#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Closed general-scene LAB collector. Default: plan only, no device opened.
Does not install modules, enable controls, reboot or present an image. Execution
requires an independently pinned recoverable test boot and matching profile.
"""
import argparse,fcntl,json,os,struct,time
from pathlib import Path
import private_probe as p
PROBE=struct.Struct('<IIQ17IIQQ5Ii4I')
assert PROBE.size==144

def load_profile(catalogue,size,asset_root=None):
 c=json.loads(catalogue.read_text());assert not c['physical_qualification'] and c['GPU_operations']==0
 assert [x['size'] for x in c['profiles']]==['1280x800','640x640']
 index=next(i for i,x in enumerate(c['profiles']) if x['size']==size);profile=c['profiles'][index]
 def path(original,name):return asset_root/size/name if asset_root else Path(original)
 raw=path(profile['packet_path'],'packet.bin').read_bytes();placement=path(profile['placement_path'],'placement.bin').read_bytes()
 if len(raw)!=2264 or p.sha(raw)!=profile['packet_sha256'] or len(placement)!=776 or p.sha(placement)!=profile['placement_sha256']:raise ValueError('profile metadata changed')
 if struct.unpack_from('<4I',raw,16)!=(1,17,10,2) or len(profile['bindings'])!=17:raise ValueError('unexpected closed profile shape')
 bindings=[];images={}
 for i,b in enumerate(profile['bindings']):
  if (b['index']!=i or b['access'] not in (1,3) or b['cached']!=1 or b['gpu_va']&4095 or b['allocation_bytes']&4095 or not 0<b['allocation_bytes']<=33554432 or b['gpu_va']<0x20000000 or b['gpu_va']+b['allocation_bytes']>0xe0000000):raise ValueError('invalid closed placement')
  if struct.unpack_from('<QQII',placement,8+24*i)!=(b['gpu_va'],b['allocation_bytes'],b['captured_handle'],b['access']):raise ValueError('placement does not match catalogue')
  bindings.append({'index':i,'va':b['gpu_va'],'bytes':b['allocation_bytes'],'access':b['access']})
  if b['access']==1:
   data=path(b['path'],'bo-'+str(i)+'.bin').read_bytes()
   if len(data)!=b['allocation_bytes'] or p.sha(data)!=b['sha256']:raise ValueError('READ image changed')
   images[i]=data
 ids=[b['captured_handle'] for b in profile['bindings']]
 color,depth=struct.unpack_from('<II',raw,352);color=ids.index(color);depth=ids.index(depth)
 guards=[]
 for lower,upper in [(0x20000000,0x28000000),(0x30000000,0x40000000),(0x40000000,0xe0000000)]:
  group=[b for b in bindings if lower<=b['va']<upper]
  if not group:continue
  for va in [min(b['va'] for b in group)-4096,max(b['va']+b['bytes'] for b in group)]:
   if lower<=va and va+4096<=upper and not any(va<b['va']+b['bytes'] and b['va']<va+4096 for b in bindings) and va not in guards:guards.append(va)
 if len(bindings)+len(guards)>32:raise ValueError('guard allocation exceeds endpoint capacity')
 return {'profile':index+1,'size':size,'packet_sha256':p.sha(raw),'bindings':bindings,'color_index':color,'depth_index':depth,'guard_va':guards},images

def receipt(packet,context,profile,ids,budget):
 q=PROBE.unpack(packet)
 if list(q[:21])!=[1,profile,context,*ids,budget] or any(q[31:]):raise ValueError('scene response changed request/reserved fields')
 r=dict(zip(['operation','cookie','core','revision','pds_base','phase','ledger','error','retained','stages'],q[21:31]))
 if (not r['operation'] or r['cookie']!=r['operation'] or r['core']!=0x01130000 or r['revision']!=0x10201 or r['pds_base']!=0x20000000 or r['phase']!=9 or r['ledger']!=7 or r['error'] or r['retained'] or r['stages']!=15):raise RuntimeError('incomplete scene lifecycle receipt: '+json.dumps(r))
 return r

def collect(device,output,plan,images,budget):
 report={'status':'PREPARING','identity':p.device_identity(device),'plan':plan,'probe_ioctl_calls':0,'hardware_driver_qualified':False,'requires_independent_pixel_oracle':True}
 def save():(output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
 save();fd=os.open(device,os.O_RDWR|os.O_CLOEXEC);memory=p.Memory(fd,0)
 opened=attempted=complete=False;objects=[]
 try:
  memory.context=memory.request(p.OPEN)[4];opened=True
  start=time.monotonic()
  for i,b in enumerate(plan['bindings']):
   rid=memory.request(p.CREATE,size=b['bytes'])[2];state={'id':rid,'pinned':False,'mapped':False};objects.append(state)
   memory.request(p.PIN,rid);state['pinned']=True
   memory.request(p.MAP,rid,va=b['va']);state['mapped']=True
   if b['access']==1:memory.upload(rid,images[i])
  guards=[]
  for va in plan['guard_va']:
   rid=memory.request(p.CREATE,size=4096)[2];state={'id':rid,'pinned':False,'mapped':False};objects.append(state)
   memory.request(p.PIN,rid);state['pinned']=True;memory.request(p.MAP,rid,va=va);state['mapped']=True
   memory.upload(rid,b'\xa5'*4096);guards.append({'id':rid,'va':va})
  report['prepare_and_upload_wall_seconds']=time.monotonic()-start;ids=[b['id'] for b in objects[:17]]
  packet=bytearray(PROBE.pack(1,plan['profile'],memory.context,*ids,budget,*([0]*12)))
  (output/'REQUEST.bin').write_bytes(packet);attempted=True;report['probe_ioctl_calls']=1;save();start=time.monotonic()
  try:
   if fcntl.ioctl(fd,p.ioctl_number(11,PROBE.size),packet,True):raise RuntimeError('nonzero ioctl success return')
  finally:
   (output/'RESPONSE.bin').write_bytes(packet);report['probe_ioctl_wall_seconds']=time.monotonic()-start
  report['receipt']=receipt(packet,memory.context,plan['profile'],ids,budget)
  info=memory.info()
  if any(info[i] for i in (6,7,8)):raise RuntimeError('endpoint retains leases/jobs/quarantine')
  report['provider_info']=list(info)
  expected=[(o['id'],b['va'],b['bytes']) for o,b in zip(objects[:17],plan['bindings'])]+[(g['id'],g['va'],4096) for g in guards]
  for rid,va,n in expected:
   info=memory.info(rid)
   if info[1:3]!=(n,va) or any(info[i] for i in (6,7,8,11)) or info[9]!=1 or info[10]!=n//4096 or info[12]!=n//4096:raise RuntimeError('restored mapping/ownership mismatch')
  complete=True;start=time.monotonic();readbacks={}
  selected=sorted(set(images)|{plan['color_index'],plan['depth_index']})
  for i in selected:
   data=memory.read(ids[i],plan['bindings'][i]['bytes']);(output/('bo-'+str(i)+'-readback.bin')).write_bytes(data)
   readbacks[str(i)]={'bytes':len(data),'sha256':p.sha(data)}
   if i in images and data!=images[i]:raise RuntimeError('READ input changed: '+str(i))
  report['readbacks']=readbacks;report['readback_wall_seconds']=time.monotonic()-start
  for i,g in enumerate(guards):
   data=memory.read(g['id'],4096);(output/('guard-'+str(i)+'-readback.bin')).write_bytes(data)
   if data!=b'\xa5'*4096:raise RuntimeError('unmapped-private guard changed')
  report['guards_intact']=True;report['status']='RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING'
 except BaseException as e:report['status']='FAILED';report['error']=str(e);raise
 finally:
  if opened and not memory.poisoned and (not attempted or complete):
   try:
    for o in reversed(objects):
     if o['mapped']:memory.request(p.UNMAP,o['id'])
     if o['pinned']:memory.request(p.UNPIN,o['id'])
     memory.request(p.DESTROY,o['id'])
    memory.request(p.CLOSE);report['memory_cleanup']='complete'
   except Exception as e:report['memory_cleanup']='failed';report['cleanup_error']=str(e);report['status']='FAILED'
  else:report['memory_cleanup']='suppressed_after_uncertain_memory_or_execution'
  try:os.close(fd)
  finally:save()
 if report.get('memory_cleanup')=='failed':raise RuntimeError('memory cleanup failed')
 return report

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--catalogue',type=Path,required=True);parser.add_argument('--asset-root',type=Path);parser.add_argument('--size',choices=['1280x800','640x640'],default='1280x800');parser.add_argument('--device',type=Path,default=Path('/dev/dri/card0'));parser.add_argument('--output',type=Path,required=True);parser.add_argument('--budget-ms',type=int,default=1000);parser.add_argument('--execute',action='store_true');a=parser.parse_args()
 if not 1<=a.budget_ms<=10000:parser.error('budget must be1..10000ms')
 plan,images=load_profile(a.catalogue,a.size,a.asset_root);a.output.mkdir(parents=True,exist_ok=False)
 if not a.execute:(a.output/'PLAN.json').write_text(json.dumps({'status':'OFFLINE_PLAN_ONLY','GPU_operations':0,'plan':plan},indent=2)+'\n');return
 collect(a.device,a.output,plan,images,a.budget_ms)
if __name__=='__main__':main()
