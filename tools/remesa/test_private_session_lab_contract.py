#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Offline real initial authentication + native/i486 wire bytes. No device."""
import argparse,hashlib,json,resource,subprocess
from pathlib import Path
from test_private_source_mmio import extract
import private_session_lab as lab
import private_probe as p

def main():
 a=argparse.ArgumentParser(description=__doc__)
 a.add_argument('--module',type=Path,required=True);a.add_argument('--assets',type=Path,required=True)
 a.add_argument('--output',type=Path,required=True);a.add_argument('--i486-cc',type=Path);a.add_argument('--qemu-i386',type=Path)
 args=a.parse_args();w=Path(__file__).resolve().parents[2];o=args.output.resolve();m=args.module.resolve();o.mkdir(parents=True,exist_ok=False)
 (o/'auth_actual.inc').write_text(extract((m/'remesa_private_session_lab.inc').read_text(),'private_lab_authenticate'))
 (o/'drm').mkdir();(o/'drm/drm.h').write_text('#include <sys/ioctl.h>\n#define DRM_COMMAND_BASE 0x40\n#define DRM_IOWR(n,t) _IOWR(\'d\',n,t)\n')
 (o/'wire.c').write_text('''#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "remesa_private_session_lab_request.h"
_Static_assert(sizeof(struct remesa_private_session_lab_request)==136,"ABI size");
_Static_assert(offsetof(struct remesa_private_session_lab_request,operation)==88,"operation");
_Static_assert(offsetof(struct remesa_private_session_lab_request,state)==124,"state");
int main(void){struct remesa_private_session_lab_request r={.version=1,.action=1,.context=0x1122334455667788ULL,.max_jobs=2};
for(unsigned i=0;i<11;i++)r.ids[i]=32+i;
if(!remesa_private_session_lab_request_valid(&r))return 2;
struct remesa_private_session_lab_request bad=r;bad.flags=1;if(remesa_private_session_lab_request_valid(&bad))return 3;
bad=r;bad.operation=1;if(remesa_private_session_lab_request_valid(&bad))return 4;
bad=r;bad.max_jobs=65;if(remesa_private_session_lab_request_valid(&bad))return 5;
unsigned command=DRM_IOCTL_REMESA_PRIVATE_SESSION_LAB;
return fwrite(&command,1,4,stdout)!=4||fwrite(&r,1,sizeof(r),stdout)!=sizeof(r);}
''')
 (o/'auth.c').write_text('''#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "remesa_stream_trusted.h"
#define gears_stream_profile_asset private_probe_profile
#include "stream_profile_asset.h"
#undef gears_stream_profile_asset
#include "remesa_canonical_inputs.h"
#include "remesa_stream_grammar.h"
typedef uint8_t u8;
struct remesa_private_probe {struct {struct {uint64_t bytes,va;} binding;void *cpu;} views[11];bool images_authenticated;};
#define vmalloc malloc
#define vfree free
#include "auth_actual.inc"
int main(int argc,char **argv){if(argc!=2)return 2;struct remesa_private_probe p={0};char name[4096];
for(unsigned i=0;i<5;i++){
p.views[i].binding.bytes=private_probe_profile.bindings[i].bytes;p.views[i].binding.va=private_probe_profile.bindings[i].va;
p.views[i].cpu=malloc(p.views[i].binding.bytes);if(!p.views[i].cpu)return 3;
if(snprintf(name,sizeof(name),"%s/role%u.bin",argv[1],i)>=(int)sizeof(name))return 4;
FILE*f=fopen(name,"rb");if(!f)return 5;size_t n=fread(p.views[i].cpu,1,p.views[i].binding.bytes,f);int end=fgetc(f);fclose(f);if(n!=p.views[i].binding.bytes||end!=EOF)return 6;}
int e=private_lab_authenticate(&p);if(e||!p.images_authenticated){fprintf(stderr,"actual initial authentication refused native assets: %d\\n",e);return 7;}
p.images_authenticated=false;((u8*)p.views[1].cpu)[0]^=1;
if(private_lab_authenticate(&p)!=-EKEYREJECTED||p.images_authenticated)return 8;
((u8*)p.views[1].cpu)[0]^=1;((u8*)p.views[0].cpu)[0]^=1;
if(private_lab_authenticate(&p)!=-EKEYREJECTED||p.images_authenticated)return 9;
for(unsigned i=0;i<5;i++)free(p.views[i].cpu);
puts("PASS actual canonical authentication and grammar on native pose; CODE/PDS metadata corruption rejected; GPU0");return 0;}
''')
 variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
 if args.i486_cc or args.qemu_i386:
  if not(args.i486_cc and args.qemu_i386):a.error('both i486 options required')
  variants.append(('i486',str(args.i486_cc),['-O2','-static','-march=i486'],[str(args.qemu_i386)]))
 rows=[];expected=__import__('struct').pack('<I',p.ioctl_number(10,136))+bytes(lab.packet(1,0x1122334455667788,ids=list(range(32,43)),maximum=2))
 for name,cc,flags,runner in variants:
  for target in ['wire','auth']:
   exe=o/(name+'-'+target);cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(o),'-I'+str(m),'-I'+str(w/'include'),str(o/(target+'.c'))]
   if target=='auth':cmd += [str(m/'remesa_stream_grammar.c')]
   cmd += ['-o',str(exe)]
   for label,argv in [(name+'-'+target+'-build',cmd),(name+'-'+target+'-run',runner+[str(exe)]+([str(args.assets.resolve())] if target=='auth' else []))]:
    def limit():
     n=6*1024**3 if runner and label.endswith('-run') else 768*1024**2
     resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_AS,(n,n))
    r=subprocess.run(argv,capture_output=True,timeout=60,preexec_fn=limit);(o/(label+'.stdout')).write_bytes(r.stdout);(o/(label+'.stderr')).write_bytes(r.stderr)
    rows.append(dict(label=label,argv=argv,exit=r.returncode));(o/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
    if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr.decode())
    if target=='wire' and label.endswith('-run') and r.stdout!=expected:raise RuntimeError('C/Python wire mismatch')
 files=[Path(__file__).resolve(),m/'remesa_private_session_lab.inc',m/'remesa_private_session_lab_uapi.h',m/'remesa_canonical_inputs.h',m/'remesa_stream_grammar.c',o/'auth_actual.inc',o/'auth.c',o/'wire.c']
 report=dict(status='ACTUAL_INITIAL_AUTH_AND_NATIVE_I486_LAB_WIRE_PASS',GPU_operations=0,variants=len(variants),wire_bytes=136,sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
 (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'variants':len(variants)}))
if __name__=='__main__':main()
