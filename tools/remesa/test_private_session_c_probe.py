#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline C collector and real kernel PDS write-guard tests; no live node."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--output',type=Path,required=True);a.add_argument('--assets',type=Path,required=True)
    a.add_argument('--next-assets',type=Path,required=True);a.add_argument('--module',type=Path,required=True)
    a.add_argument('--i486-cc',type=Path);a.add_argument('--qemu-i386',type=Path)
    args=a.parse_args();w=Path(__file__).resolve().parents[2];o=args.output.resolve();o.mkdir(parents=True,exist_ok=False)
    (o/'drm').mkdir();(o/'drm/drm.h').write_text("#include <sys/ioctl.h>\n#define DRM_COMMAND_BASE 0x40\n#define DRM_IOWR(n,t) _IOWR('d',n,t)\n")
    guard=o/'guard.c';guard.write_text('''#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "remesa_stream_trusted.h"
#include "stream_profile_asset.h"
#include "remesa_canonical_inputs.h"
#include "remesa_pds_write_guard.h"
static unsigned char images[2][131072],bad[4096];
int main(int argc,char **argv){if(argc!=3)return 2;char path[4096];unsigned changes=0;
for(unsigned i=0;i<2;i++){
 if(snprintf(path,sizeof(path),"%s/role0.bin",argv[i+1])>=(int)sizeof(path))return 3;
 FILE*f=fopen(path,"rb");if(!f)return 4;
 if(fread(images[i],1,131072,f)!=131072||fgetc(f)!=EOF)return 5;
 fclose(f);
 for(unsigned at=0;at<131072;at+=4096)
  if(remesa_canonical_pds_page(gears_stream_profile_asset.batches,gears_stream_profile_asset.count,remesa_canonical_inputs[0],at,images[i]+at,4096))return 6;
}
for(unsigned at=0;at<131072;at+=4096)changes+=!!memcmp(images[0]+at,images[1]+at,4096);
if(changes!=2)return 7;
memcpy(bad,images[1],4096);bad[0]^=1;
if(remesa_canonical_pds_page(gears_stream_profile_asset.batches,gears_stream_profile_asset.count,remesa_canonical_inputs[0],0,bad,4096)!=-EKEYREJECTED)return 8;
unsigned absolute=gears_stream_profile_asset.batches[0].source_offset+68,at=absolute&~4095u;
memcpy(bad,images[1]+at,4096);unsigned off=absolute-at;bad[off]=0;bad[off+1]=0;bad[off+2]=0xc0;bad[off+3]=0x7f;
if(remesa_canonical_pds_page(gears_stream_profile_asset.batches,gears_stream_profile_asset.count,remesa_canonical_inputs[0],at,bad,4096)!=-ERANGE)return 9;
puts("PASS actual kernel typed guard: both real native PDS poses, two changed pages, metadata mutation and NaN rejected; GPU0");return 0;}
''')
    variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
    if args.i486_cc or args.qemu_i386:
        if not(args.i486_cc and args.qemu_i386):a.error('both i486 options required')
        variants.append(('i486',str(args.i486_cc),['-O2','-static','-march=i486'],[str(args.qemu_i386)]))
    common=[w/'mesa/sgx535/remesa_linux_memory.c',w/'mesa/sgx535/remesa_retained_transport.c']
    probe=w/'tools/remesa/private_session_c_probe.c';test=w/'tools/remesa/tests/test_private_session_c_probe.c';commands=[]
    def run(label,cmd,expected=0):
        def limit():
            resource.setrlimit(resource.RLIMIT_CORE,(0,0));n=6*1024**3 if 'i486' in label and 'run' in label else 768*1024**2
            resource.setrlimit(resource.RLIMIT_AS,(n,n))
        r=subprocess.run(cmd,capture_output=True,timeout=120,preexec_fn=limit)
        (o/(label+'.stdout')).write_bytes(r.stdout);(o/(label+'.stderr')).write_bytes(r.stderr)
        commands.append(dict(label=label,argv=cmd,exit=r.returncode,expected_exit=expected))
        (o/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n')
        if r.returncode!=expected or (r.stderr and not label.endswith('usage-run')):raise RuntimeError(label+' failed; preserved logs')
    for name,cc,flags,runner in variants:
        base=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(o),'-I'+str(w/'include'),'-I'+str(w/'mesa/sgx535')]
        model=o/(name+'-model');exe=o/(name+'-application');g=o/(name+'-guard');out=o/(name+'-model-output');out.mkdir()
        run(name+'-model-build',base+[*map(str,common),str(test),'-Wl,--wrap=ioctl','-o',str(model)])
        run(name+'-model-run',runner+[str(model),str(args.assets.resolve()),str(args.next_assets.resolve()),str(out)])
        results=[json.loads((out/f'model-{i}/RESULT.json').read_text()) for i in range(7)]
        assert results[0]['status']=='C_SESSION_COLLECTED_ORACLE_PENDING' and results[0]['frames_completed']==3 and results[0]['memory_cleanup']=='complete'
        assert all(r['status']=='FAILED' and r['memory_cleanup']=='retained' and not r['hardware_driver_qualified'] for r in results[1:])
        run(name+'-application-build',base+[*map(str,common),str(probe),'-o',str(exe)])
        run(name+'-usage-run',runner+[str(exe)],2)
        run(name+'-guard-build',base+['-I'+str(args.module.resolve()),str(guard),'-o',str(g)])
        run(name+'-guard-run',runner+[str(g),str(args.assets.resolve()),str(args.next_assets.resolve())])
    files=[probe,test,*common,w/'mesa/sgx535/remesa_linux_memory.h',Path(__file__).resolve(),guard,*[args.module/n for n in ('remesa_pds_write_guard.h','remesa_canonical_inputs.h','stream_profile_asset.h')]]
    report=dict(status='C_COLLECTOR_MODELS_AND_ACTUAL_TYPED_PDS_GUARD_PASS',GPU_operations=0,variants=len(variants),model_scenarios=7,commands=commands,source_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},scope='C collector on /dev/null with modeled kernel only; actual pure typed guard; not physical rendering')
    (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'variants':len(variants),'GPU_operations':0}))

if __name__=='__main__':main()
