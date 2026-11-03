#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Actual general source routing/capsule, modeled device/locks and job shell.
No device opened, no MMIO executed, no source authority supplied to hardware.
"""
import argparse,hashlib,json,subprocess
from pathlib import Path
from stage_twod_reservation import patched_sources,replace_once

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 w=Path(__file__).resolve().parents[2];n=a.native.resolve();o=a.output.resolve();o.mkdir(parents=True,exist_ok=False);(o/'module').mkdir();(o/'tests').mkdir()
 sources=patched_sources(n,private=True)
 (o/'module/remesa_stream_source.c').write_text(sources['remesa_stream_source.c'])
 (o/'module/remesa_stream_source.h').write_bytes((n/'remesa_stream_source.h').read_bytes())
 (o/'module/remesa_twod.h').write_bytes((w/'mesa/sgx535/remesa_twod.h').read_bytes())
 for name in ['frozen_capsule.h','frozen_source_guard.h','frozen_fixed_service.h']:(o/'module'/name).write_bytes((n/name).read_bytes())
 shim=(n.parent/'tests/source_test_shim.h').read_text()
 shim=replace_once(shim,'struct sgx535_gma500_owner {','''#define REMESA_GEM_SCENE_JOB_H
#include "remesa_scene_service.h"
struct remesa_gem_scene_job {struct {struct drm_device *dev;} snapshot;struct sgx535_frozen_session session;struct remesa_scene_service service;};
struct drm_psb_private {void *sgx_reg;};
struct sgx535_gma500_owner {''')
 shim=replace_once(shim,' struct sgx535_gma500_owner *owner;bool active,power_held,fire_possible;',' struct sgx535_gma500_owner *owner;struct remesa_gem_scene_job *general_scene;void *private_loan;unsigned long deadline;bool prefire_load_active;bool active,power_held,fire_possible;')
 shim=replace_once(shim,'#define DEFINE_SPINLOCK(name) int name','''static unsigned long jiffies;
#define time_after_eq(a,b) ((long)((a)-(b))>=0)
#define PSB_SGX_SIZE 0x8000
#define PSB_WSGX32(value,offset) ((void)(value),(void)(offset),abort()) /* no actual MMIO in these tests */
#define DEFINE_SPINLOCK(name) int name''')
 (o/'tests/source_test_shim.h').write_text(shim)
 test=(n.parent/'tests/test_source.c').read_text()
 legacy=test.replace('remesa_route_source_pending(&dev,0,1);','b.prefire_load_active=true;remesa_route_source_pending(&dev,0,1);b.prefire_load_active=false;').replace('  remesa_route_issued(&b);','  b.fire_possible=true;remesa_route_issued(&b);')
 (o/'tests/test_legacy.c').write_text('#define REMESA_GENERAL_SCENE 1\n'+legacy)
 test=test.replace('struct sgx535_gma500_owner owner={0};','struct remesa_gem_scene_job scene={0};').replace('memset(&owner,0,sizeof(owner))','memset(&scene,0,sizeof(scene))')
 test=replace_once(test,'owner.dev=&dev;owner.initialized=true;owner.device_claimed=true;','scene.snapshot.dev=&dev;')
 test=test.replace('owner.session','scene.session').replace('owner.fixed_service','scene.service').replace('b.owner=&owner','b.general_scene=&scene')
 for name in ['service','before','after','terminal']:test=test.replace('remesa_route_'+name+'(','remesa_route_scene_'+name+'(')
 test=replace_once(test,'remesa_route_source_pending(&dev,0,1);','b.prefire_load_active=true;remesa_route_source_pending(&dev,0,1);b.prefire_load_active=false;')
 test=replace_once(test,'  remesa_route_issued(&b);','  b.fire_possible=true;remesa_route_issued(&b);')
 anchor='  remesa_route_scene_service(&b,&scene.service,1);'
 test=replace_once(test,anchor,anchor+'''
  if(!strcmp(scenario,"wrongservice")){struct remesa_scene_service alien={.session=&scene.session};remesa_route_scene_service(&b,&alien,1);assert(!remesa_route_source_write(&b));assert(legacy_calls==before);return 0;}
  if(!strcmp(scenario,"wrongsession")){struct sgx535_frozen_session alien=scene.session;scene.service.session=&alien;remesa_route_scene_after(&b,&scene.service);assert(!remesa_route_source_write(&b));return 0;}
''')
 anchor='  if(!strcmp(scenario,"nohash"))'
 test=replace_once(test,anchor,'''  if(!strcmp(scenario,"wrongowner")){struct remesa_gem_scene_job alien=scene;b.general_scene=&alien;assert(remesa_stream_source_finish_nohash(&lease_token,o));return 0;}
  if(!strcmp(scenario,"prefix")){assert(remesa_stream_source_finish(&lease_token,o,(char[4096]){0})==-EINVAL);return 0;}
'''+anchor)
 test=replace_once(test,'if(!strcmp(scenario,"nohash")) {','if(!strcmp(scenario,"nohash")||!strcmp(scenario,"latepending")) {')
 (o/'tests/test_source.c').write_text('#define REMESA_GENERAL_SCENE 1\n'+test)
 scenarios=['nohash','pending','overflow','wrongseq','producer','wrongtask','terminalpending','nohash_wrongledger','nohash_wrongphase','nohash_pending','latepending','wrongservice','wrongsession','wrongowner','prefix']
 rows=[]
 legacy_scenarios=['twoframes',*scenarios[:11]]
 for profile,case_names in [('general',scenarios),('legacy',legacy_scenarios)]:
  for variant,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
   variant=profile+'-'+variant;exe=o/variant;command=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',*flags,'-I'+str(o/'module'),'-I'+str(n),'-I'+str(w/'kernel/remesa/gma500'),'-I'+str(w/'include'),str(o/('tests/test_source.c' if profile=='general' else 'tests/test_legacy.c')),str(n/'frozen_capsule.c'),str(n/'frozen_source_guard.c'),'-o',str(exe)]
   for label,argv in [(variant+'-build',command)]+[(variant+'-'+s,[str(exe),s]) for s in case_names]:
    q=subprocess.run(argv,capture_output=True,text=True,timeout=60);(o/(label+'.log')).write_text(q.stdout+q.stderr);rows.append({'label':label,'argv':argv,'exit':q.returncode})
    if q.returncode or q.stderr:raise RuntimeError(label+': '+q.stderr[-1600:])
 paths=[Path(__file__).resolve(),w/'tools/remesa/stage_twod_reservation.py',w/'tools/remesa/stage_scene_backend.py',w/'kernel/remesa/gma500/remesa_scene_backend.h',w/'kernel/remesa/gma500/remesa_scene_source.inc',w/'kernel/remesa/gma500/remesa_owned_fill_source.inc',w/'kernel/remesa/gma500/remesa_owned_fill_source.h',w/'kernel/remesa/gma500/remesa_private_source_mmio.inc',n/'frozen_capsule.c',n/'frozen_source_guard.c']
 report={'status':'GENERAL_SOURCE_IDENTITIES_AND_NOHASH_CLOSURE_MODEL_PASS','GPU_operations':0,'scenarios_per_variant':len(scenarios),'variants':2,'legacy_scenarios_per_variant':len(legacy_scenarios),'scope':'Actual generated source routes and original capsule/source guard; reduced job shell/device/locks, not physical attribution','sources':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},'generated':{str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(o.glob('*/*')) if f.is_file()},'commands':rows}
 (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(report['status'])
if __name__=='__main__':main()
