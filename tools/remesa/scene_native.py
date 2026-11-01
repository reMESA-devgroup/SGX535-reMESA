#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Closed LAB: real Mesa constructors/C provider; Python collects retired GEM.
No node open without --execute. Does not install, enable gates or present.
Fixture images are reference comparisons ONLY; Python never uploads them.
"""
import argparse,fcntl,json,os,struct,subprocess,time
from pathlib import Path
import private_probe as p
import scene_probe as scene
import scene_trace

def collect(device,output,plan,images,client,expected_packet,trace=False,region_full=False):
 report={'status':'PREPARING','identity':p.device_identity(device),'plan':plan,'probe_ioctl_calls':0,'hardware_driver_qualified':False,'requires_independent_pixel_oracle':True,'submission_path':'ACTUAL_MESA_CONSTRUCTORS_LINUX_RESOURCE_PROVIDER_C_SCENE_TRANSPORT','reference_images_uploaded_by_collector':False,'native_client_sha256':p.sha(client.read_bytes())}
 def save():(output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
 save();fd=os.open(device,os.O_RDWR|os.O_CLOEXEC);memory=None;complete=False;objects=[]
 try:
  start=time.monotonic()
  with (output/'native.stdout').open('xb') as out,(output/'native.stderr').open('xb') as err:
   q=subprocess.run([str(client),str(fd),plan['size'],str(output),str(expected_packet)],stdout=out,stderr=err,pass_fds=(fd,),timeout=120)
  report['native_client_wall_seconds']=time.monotonic()-start;report['native_client_exit']=q.returncode
  if q.returncode:raise RuntimeError('native owner/transport failed; suppress all cleanup, no retry')
  req=(output/'REQUEST.bin').read_bytes();res=(output/'RESPONSE.bin').read_bytes();request=scene.PROBE.unpack(req)
  report['probe_ioctl_calls']=1;context=request[2];ids=list(request[3:20]);report['receipt']=scene.receipt(bytearray(res),context,plan['profile'],ids,1000)
  packet=(output/'packet.bin').read_bytes();placement=(output/'placement.bin').read_bytes()
  if len(packet)!=2264 or len(placement)!=776 or struct.unpack_from('<II',placement)!=(1,17) or struct.unpack_from('<Q',packet)[0]!=context:raise RuntimeError('native capture record changed')
  for i,b in enumerate(plan['bindings']):
   if struct.unpack_from('<QQII',placement,8+24*i)!=(b['va'],b['bytes'],ids[i],b['access']) or struct.unpack_from('<II',packet,32+8*i)!=(ids[i],b['access']):raise RuntimeError('native placement differs from closed profile')
  guard_ids=struct.unpack('<4I',(output/'GUARD_IDS.bin').read_bytes());guard_va=struct.unpack('<4Q',(output/'GUARD_VA.bin').read_bytes())
  extras=list(struct.iter_unpack('<QQII',(output/'EXTRAS.bin').read_bytes()))
  if len(extras)!=2 or any(access or not va or not size or size&4095 for va,size,rid,access in extras):raise RuntimeError('invalid CPU-only allocation inventory')
  # Native owners retain two CPU-only GEM allocations that the old exported
  # GPU-only fixture did not allocate. Guards must enclose ALL real objects.
  bounds=list(plan['bindings'])+[{'va':va,'bytes':size} for va,size,_,_ in extras];expected_guards=[]
  for lo,hi in [(0x20000000,0x28000000),(0x30000000,0x40000000),(0x40000000,0xe0000000)]:
   group=[b for b in bounds if lo<=b['va']<hi]
   if not group:continue
   for va in [min(b['va'] for b in group)-4096,max(b['va']+b['bytes'] for b in group)]:
    if lo<=va and va+4096<=hi and not any(va<b['va']+b['bytes'] and b['va']<va+4096 for b in bounds) and va not in expected_guards:expected_guards.append(va)
  if list(guard_va)!=expected_guards or len(expected_guards)!=4:raise RuntimeError('native guards overlap actual retained allocations')
  plan['guard_va']=expected_guards;report['native_packet_sha256']=p.sha(packet)
  objects=[{'id':rid,'va':b['va'],'bytes':b['bytes']} for rid,b in zip(ids,plan['bindings'])]+[{'id':rid,'va':va,'bytes':4096} for rid,va in zip(guard_ids,guard_va)]+[{'id':rid,'va':va,'bytes':size} for va,size,rid,_ in extras]
  if len({o['id'] for o in objects})!=23:raise RuntimeError('native allocation IDs alias')
  memory=p.Memory(fd,context);memory.ids={o['id'] for o in objects}
  info=memory.info();report['provider_info']=list(info)
  if info[3:5]!=(23,23) or any(info[i] for i in (6,7,8)):raise RuntimeError('native resources/jobs/leases/quarantine mismatch')
  report['object_info']=[]
  for o in objects:
   info=memory.info(o['id']);report['object_info'].append({'id':o['id'],'info':list(info)})
   if info[1:3]!=(o['bytes'],o['va']) or any(info[k] for k in (6,7,8,11)) or info[9]!=1 or info[10]!=info[12] or info[10]!=o['bytes']//4096:raise RuntimeError('native restored object mapping mismatch')
  complete=True
  t=struct.unpack('<3Q',(output/'TIMES.bin').read_bytes());report['native_construct_and_LAB_prepare_ns']=t[1]-t[0];report['probe_ioctl_wall_seconds']=(t[2]-t[1])/1e9
  if trace:
   query=bytearray(scene_trace.QUERY.pack(1,0,context,bytes(4256)));before=bytes(query);(output/'TRACE-REQUEST.bin').write_bytes(before);report['trace_ioctl_calls']=1
   try:
    if fcntl.ioctl(fd,p.ioctl_number(12,len(query)),query,True):raise RuntimeError('trace ioctl failed')
   finally:(output/'TRACE-RESPONSE.bin').write_bytes(query)
   if query[:16]!=before[:16]:raise RuntimeError('trace owner changed')
   report['trace']=scene_trace.decode(bytes(query[16:]),context,report['receipt'],plan)
   if [b['id'] for b in report['trace']['bindings']]!=ids:raise RuntimeError('trace does not describe C owner handles')
   expected=(output/'CONTROLS.bin').read_bytes()
   if tuple(report['trace']['raster']+report['trace']['ta'])!=struct.unpack('<66I',expected):raise RuntimeError('real Mesa control snapshot differs from executed closed plan')
  start=time.monotonic();report['readbacks']={}
  for i in sorted(set(images)|{plan['color_index'],plan['depth_index']}):
   data=memory.read(ids[i],plan['bindings'][i]['bytes']);(output/f'bo-{i}-readback.bin').write_bytes(data);report['readbacks'][str(i)]={'bytes':len(data),'sha256':p.sha(data)}
   if i in images and data!=images[i]:raise RuntimeError('real constructor READ image differs from qualified bytes')
  report['readback_wall_seconds']=time.monotonic()-start
  for i,rid in enumerate(guard_ids):
   data=memory.read(rid,4096);(output/f'guard-{i}-readback.bin').write_bytes(data)
   if data!=b'\xa5'*4096:raise RuntimeError('guard modified')
  report['guards_intact']=True
  if region_full:
   i=plan['region_index'];data=memory.read(ids[i],plan['bindings'][i]['bytes']);(output/'region-full-readback.bin').write_bytes(data);report['region_readback']={'index':i,'bytes':len(data),'nonzero_bytes':sum(x!=0 for x in data),'sha256':p.sha(data)}
  report['status']='RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING'
 except BaseException as e:report['status']='FAILED';report['error']=type(e).__name__+': '+str(e);raise
 finally:
  if complete and memory is not None and not memory.poisoned:
   try:
    for o in reversed(objects):
     memory.request(p.UNMAP,o['id']);memory.request(p.UNPIN,o['id']);memory.request(p.DESTROY,o['id'])
    memory.request(p.CLOSE);report['memory_cleanup']='complete'
   except Exception as e:report['memory_cleanup']='failed';report['cleanup_error']=str(e);report['status']='FAILED'
  else:report['memory_cleanup']='suppressed_after_uncertain_memory_or_execution'
  try:os.close(fd)
  finally:save()
 if report.get('memory_cleanup')=='failed':raise RuntimeError('native cleanup failed')
 return report

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--catalogue',type=Path,required=True);parser.add_argument('--size',choices=['1280x800','640x640'],required=True);parser.add_argument('--asset-root',type=Path);parser.add_argument('--client',type=Path,required=True);parser.add_argument('--device',type=Path,default=Path('/dev/dri/card0'));parser.add_argument('--output',type=Path,required=True);parser.add_argument('--budget-ms',type=int,default=1000);parser.add_argument('--execute',action='store_true');parser.add_argument('--trace',action='store_true');parser.add_argument('--region-full',action='store_true');a=parser.parse_args()
 if a.budget_ms!=1000:parser.error('native closed client uses the pinned1000ms budget')
 plan,images=scene.load_profile(a.catalogue,a.size,a.asset_root)
 if plan['profile'] not in (5,6):parser.error('native constructor qualification requires initialized+finalized profiles')
 a.output.mkdir(parents=True,exist_ok=False)
 if not a.execute:(a.output/'PLAN.json').write_text(json.dumps(plan,indent=2)+'\n');print('PLAN_ONLY_NO_DEVICE_OPEN');return
 profile=next(f for f in json.loads(a.catalogue.read_text())['profiles'] if f['size']==a.size)
 expected=a.asset_root/a.size/'packet.bin' if a.asset_root else Path(profile['packet_path'])
 report=collect(a.device,a.output,plan,images,a.client.resolve(),expected,a.trace,a.region_full);print(report['status'])
if __name__=='__main__':main()
