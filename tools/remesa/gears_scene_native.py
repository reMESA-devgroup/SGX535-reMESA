#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Closed bound-Gallium LAB supervisor. Default plan only; no device opened.
Uses actual C Gallium/NIR/Linux memory client and saved retired readbacks.
Never uploads reference images, enables gates, installs modules or reboots.
"""
import argparse,json,os,struct,subprocess,time
from pathlib import Path
import private_probe as p
import gears_scene_profile as scene
import gears_scene_trace as scene_trace

def inventory(raw,context,count):
 if len(raw)!=784 or struct.unpack_from('<QII',raw)!=(context,1,count):raise ValueError('inventory header')
 entries=[struct.unpack_from('<QQII',raw,16+i*24) for i in range(count)]
 if any(not va or va&4095 or not n or n&4095 or n>33554432 or va+n>0x100000000 or rid<32 or flags!=1 for va,n,rid,flags in entries):raise ValueError('inventory bounds/ownership')
 if len({e[2] for e in entries})!=count or sum(e[1] for e in entries)>128*1024*1024:raise ValueError('inventory identity/capacity')
 for i,(va,n,_,_) in enumerate(entries):
  if any(va<other+size and other<va+n for other,size,_,_ in entries[:i]):raise ValueError('inventory VA overlap')
 if any(raw[16+24*count:]):raise ValueError('inventory reserved')
 return entries

def packet_matches(raw,expected,context,ids):
 if len(raw)!=2264 or len(expected)!=2264:raise ValueError('packet size')
 normalized=bytearray(expected);old=[struct.unpack_from('<I',expected,32+i*8)[0] for i in range(18)];mapping=dict(zip(old,ids))
 if len(mapping)!=18:raise ValueError('profile IDs alias')
 struct.pack_into('<Q',normalized,0,context)
 offsets=[32+i*8 for i in range(18)]+[304,308,312,352,356,360,372,384]
 uses,windows=struct.unpack_from('<II',expected,24)
 if (uses,windows)!=(11,2):raise ValueError('packet topology')
 offsets += [400+i*24 for i in range(uses)]
 offsets += [1720+i*32+k for i in range(windows) for k in [0,12]]
 for at in offsets:struct.pack_into('<I',normalized,at,mapping[struct.unpack_from('<I',expected,at)[0]])
 if normalized!=raw:raise ValueError('bound packet differs from exact profile')

def collect(device,output,plan,images,client,expected_fixture,client_sha):
 if plan['profile'] not in (1,2):raise ValueError('bound Gallium closed gears version2 profiles1/2 required')
 if p.sha(client.read_bytes())!=client_sha:raise ValueError('native client binary changed')
 report={'status':'PREPARING','identity':p.device_identity(device),'plan':plan,'hardware_driver_qualified':False,'requires_independent_pixel_oracle':True,'submission_path':'BOUND_GALLIUM_NIR_REAL_LINUX_MEMORY_PROVIDER','native_client_sha256':client_sha,'reference_images_uploaded_by_collector':False,'probe_ioctl_calls':0}
 def save():(output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
 save();fd=os.open(device,os.O_RDWR|os.O_CLOEXEC);memory=None;complete=False;remaining=[]
 try:
  report['probe_ioctl_calls']=None;save();start=time.monotonic()
  with (output/'native.stdout').open('xb') as out,(output/'native.stderr').open('xb') as err:
   q=subprocess.run([str(client),str(fd),plan['size'],str(output),str(expected_fixture),'--execute'],stdout=out,stderr=err,pass_fds=(fd,),timeout=180)
  report['native_client_wall_seconds']=time.monotonic()-start;report['native_client_exit']=q.returncode
  report['probe_ioctl_calls']=1 if (output/'RESPONSE.bin').exists() else None
  if q.returncode:raise RuntimeError('native client failed; no cleanup/retry')
  response=(output/'RESPONSE.bin').read_bytes();request=scene.PROBE.unpack(response);context=request[2];ids=list(request[3:21])
  report['receipt']=scene.receipt(response,context,plan['profile'],ids,1000);report['probe_ioctl_calls']=1
  if (output/'RESULT.bin').read_bytes()!=bytes(4):raise RuntimeError('native frontend rejected submission')
  raw=(output/'packet.bin').read_bytes();packet_matches(raw,(expected_fixture/'packet.bin').read_bytes(),context,ids)
  placement=(output/'placement.bin').read_bytes()
  if len(placement)!=776 or struct.unpack_from('<II',placement)!=(1,18):raise ValueError('placement header')
  bindings=[(b['va'],b['bytes'],ids[i],b['access']) for i,b in enumerate(plan['bindings'])]
  if [struct.unpack_from('<QQII',placement,8+i*24) for i in range(18)]!=bindings:raise ValueError('placement differs from closed profile')
  owners=inventory((output/'INVENTORY-OWNERS.bin').read_bytes(),context,21)
  before=(output/'INVENTORY-BEFORE.bin').read_bytes();all_objects=inventory(before,context,25)
  if before!=(output/'INVENTORY-AFTER.bin').read_bytes():raise ValueError('collection ownership changed')
  byid={e[2]:e for e in all_objects}
  if any(byid.get(rid)!=(va,n,rid,1) for va,n,rid,_ in bindings+owners):raise ValueError('held owners differ from bindings')
  extras=[e for e in owners if e[2] not in ids]
  if len(extras)!=3:raise ValueError('missing body/DPM/source VB')
  vb=[e for e in extras if e[0]==0x20000000 and e[1]==4096]
  if len(vb)!=1:raise ValueError('source VB ownership')
  guard_ids=struct.unpack('<4I',(output/'GUARD_IDS.bin').read_bytes());guard_va=struct.unpack('<4Q',(output/'GUARD_VA.bin').read_bytes())
  expected_guards=[]
  for lo,hi in [(0x20000000,0x28000000),(0x30000000,0x40000000),(0x40000000,0xe0000000)]:
   group=[e for e in owners if lo<=e[0]<hi]
   if not group:continue
   for va in [min(e[0] for e in group)-4096,max(e[0]+e[1] for e in group)]:
    if lo<=va and va+4096<=hi:expected_guards.append(va)
  if list(guard_va)!=expected_guards or len(set(guard_ids))!=4:raise ValueError('guards do not enclose actual graph')
  if {e[2] for e in all_objects}!={e[2] for e in owners}|set(guard_ids):raise ValueError('unaccounted allocations')
  if any(byid.get(rid)!=(va,4096,rid,1) for rid,va in zip(guard_ids,guard_va)):raise ValueError('guard inventory mismatch')
  plan['guard_va']=expected_guards
  report['readbacks']={}
  for i in sorted(set(images)|{plan['color_index'],plan['depth_index']}):
   rid=ids[i];data=(output/f'after-id-{rid}.bin').read_bytes()
   if len(data)!=plan['bindings'][i]['bytes']:raise ValueError('readback size')
   if i in images and (data!=images[i] or (output/f'before-id-{rid}.bin').read_bytes()!=images[i]):raise ValueError('READ image changed')
   (output/f'bo-{i}-readback.bin').write_bytes(data);report['readbacks'][str(i)]={'bytes':len(data),'sha256':p.sha(data)}
  for _,n,rid,_ in extras:
   a=(output/f'before-id-{rid}.bin').read_bytes();b=(output/f'after-id-{rid}.bin').read_bytes()
   if len(a)!=n or a!=b:raise ValueError('CPU-only/source buffer changed')
  for i,rid in enumerate(guard_ids):
   for when in ['before','after']:
    if (output/f'{when}-id-{rid}.bin').read_bytes()!=b'\xa5'*4096:raise ValueError('guard changed')
   (output/f'guard-{i}-readback.bin').write_bytes(b'\xa5'*4096)
  report['guards_intact']=True;report['non_GPU_or_source_buffers_intact']=3
  remaining=inventory((output/'INVENTORY-REMAINING.bin').read_bytes(),context,7)
  expected_remaining=set(guard_ids)|{ids[plan['color_index']],ids[plan['depth_index']],vb[0][2]}
  if {e[2] for e in remaining}!=expected_remaining or any(byid[rid]!=(va,n,rid,flags) for va,n,rid,flags in remaining):raise ValueError('unexpected remaining ownership')
  memory=p.Memory(fd,context);memory.ids=expected_remaining
  info=memory.info();report['provider_info']=list(info)
  if info[3:5]!=(7,7) or any(info[k] for k in [6,7,8]):raise RuntimeError('remaining endpoint holds')
  report['object_info']=[]
  for va,n,rid,_ in remaining:
   info=memory.info(rid);report['object_info'].append({'id':rid,'info':list(info)})
   if info[1:3]!=(n,va) or any(info[k] for k in [6,7,8,11]) or info[9]!=1 or info[10]!=n//4096 or info[12]!=n//4096:raise RuntimeError('remaining object mapping mismatch')
  t=struct.unpack('<5Q',(output/'TIMES.bin').read_bytes())
  if any(b<a for a,b in zip(t,t[1:])):raise ValueError('phase clock order')
  report['timings_ns']=dict(zip(['frontend_prepare','LAB_pre_reads','native_ioctl_wall','LAB_retired_reads'],[b-a for a,b in zip(t,t[1:])]))
  # The native callback queried BEFORE consuming/freeing the capture. Reuse
  # that complete raw snapshot; never query a second time for this report.
  query=(output/'TRACE-RESPONSE.bin').read_bytes();original=(output/'TRACE-REQUEST.bin').read_bytes()
  if len(query)!=4296 or original!=scene_trace.QUERY.pack(2,0,context,bytes(4280)) or query[:16]!=original[:16] or (output/'TRACE-RESULT.bin').read_bytes()!=bytes(4):raise ValueError('trace request/owner/status')
  report['trace']=scene_trace.decode(query[16:],context,report['receipt'],plan)
  if [b['id'] for b in report['trace']['bindings']]!=ids:raise ValueError('trace binding identity')
  complete=True;report['status']='RECEIPT_AND_READBACK_COLLECTED_ORACLE_PENDING'
 except BaseException as e:report['status']='FAILED';report['error']=type(e).__name__+': '+str(e);raise
 finally:
  if complete and memory is not None and not memory.poisoned:
   try:
    for _,_,rid,_ in reversed(remaining):
     memory.request(p.UNMAP,rid);memory.request(p.UNPIN,rid);memory.request(p.DESTROY,rid)
    memory.request(p.CLOSE);report['memory_cleanup']='complete'
   except Exception as e:report['memory_cleanup']='failed';report['status']='FAILED';report['cleanup_error']=str(e)
  else:report['memory_cleanup']='suppressed_after_uncertain_or_invalid_evidence'
  try:os.close(fd)
  finally:save()
 if report.get('memory_cleanup')!='complete':raise RuntimeError('incomplete memory cleanup')
 return report

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--catalogue',type=Path,required=True);parser.add_argument('--size',choices=['1280x800','640x640'],required=True);parser.add_argument('--asset-root',type=Path);parser.add_argument('--client',type=Path,required=True);parser.add_argument('--client-sha256',required=True);parser.add_argument('--device',type=Path,default=Path('/dev/dri/card0'));parser.add_argument('--output',type=Path,required=True);parser.add_argument('--execute',action='store_true');a=parser.parse_args()
 plan,images=scene.load_profile(a.catalogue,a.size,a.asset_root)
 if plan['profile'] not in (1,2):parser.error('bound gears version2 1/2 catalogue required')
 a.output.mkdir(parents=True,exist_ok=False)
 if not a.execute:(a.output/'PLAN.json').write_text(json.dumps(plan,indent=2)+'\n');print('PLAN_ONLY_NO_DEVICE_OPEN');return
 entry=next(f for f in json.loads(a.catalogue.read_text())['profiles'] if f['size']==a.size)
 fixture=a.asset_root/a.size if a.asset_root else Path(entry['packet_path']).parent
 print(collect(a.device,a.output,plan,images,a.client.resolve(),fixture,a.client_sha256)['status'])
if __name__=='__main__':main()
