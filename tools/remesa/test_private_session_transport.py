#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""C private-session transport/fault tests with interposed ioctl; never a device.

Optional captured wires are replayed byte for byte. Provider metadata remains
modeled; replay is not new physical completion or pixel approval.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--output',type=Path,required=True)
    a.add_argument('--capture',type=Path)
    a.add_argument('--i486-cc',type=Path)
    a.add_argument('--qemu-i386',type=Path)
    args=a.parse_args();w=Path(__file__).resolve().parents[2];o=args.output.resolve()
    o.mkdir(parents=True,exist_ok=False)
    (o/'drm').mkdir();(o/'drm/drm.h').write_text("#include <sys/ioctl.h>\n#define DRM_COMMAND_BASE 0x40\n#define DRM_IOWR(n,t) _IOWR('d',n,t)\n")
    variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
    if args.i486_cc or args.qemu_i386:
        if not(args.i486_cc and args.qemu_i386):a.error('both i486 options required')
        variants.append(('i486',str(args.i486_cc),['-O2','-static','-march=i486'],[str(args.qemu_i386)]))
    sources=[w/'mesa/sgx535'/n for n in ('remesa_linux_memory.c','remesa_retained_transport.c')]
    tests=['test_private_session_transport.c','test_general_session_transport.c','test_general_parameter_transport.c','test_general_parameter_dispatch.c','test_general_scene_dispatch.c','test_memory_upload_pages.c','test_native_copy_transport.c','test_native_fill_transport.c','test_retained_transport.c']
    files=sources+[w/'mesa/sgx535/tests'/n for n in tests]+[Path(__file__).resolve(),w/'mesa/sgx535/remesa_linux_memory.h',w/'include/remesa/private_session_lab_uapi.h',w/'include/remesa/general_session_uapi.h',w/'mesa/sgx535/remesa_linux_general_session.inc',w/'tools/remesa/prepare_mesa_loader.py']
    capture_hashes={}
    files.append(w/'include/remesa/general_parameter_uapi.h')
    if args.capture:
        for stem in ('OPEN','frame-000001','frame-000002','CLOSE'):
            for suffix in ('REQUEST','RESPONSE'):
                p=args.capture/(stem+'-'+suffix+'.bin');blob=p.read_bytes()
                if len(blob)!=136:raise ValueError('not a complete captured session envelope')
                capture_hashes[str(p.resolve())]=hashlib.sha256(blob).hexdigest()
    commands=[]
    for name,cc,flags,runner in variants:
        for target in tests:
            exe=o/(name+'-'+Path(target).stem)
            cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,'-I'+str(o),'-I'+str(w/'include'),'-I'+str(w/'mesa/sgx535'),*[str(p) for p in sources],str(w/'mesa/sgx535/tests'/target),'-Wl,--wrap=ioctl','-o',str(exe)]
            argv=runner+[str(exe)]+([str(args.capture.resolve())] if args.capture and target==tests[0] else [])
            for label,command in [('build',cmd),('run',argv)]:
                def limit():
                    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
                    n=6*1024**3 if runner and label=='run' else 768*1024**2
                    resource.setrlimit(resource.RLIMIT_AS,(n,n))
                r=subprocess.run(command,capture_output=True,timeout=90,preexec_fn=limit)
                stem=name+'-'+Path(target).stem+'-'+label
                (o/(stem+'.stdout')).write_bytes(r.stdout);(o/(stem+'.stderr')).write_bytes(r.stderr)
                commands.append(dict(label=stem,argv=command,exit=r.returncode))
                (o/'COMMANDS.json').write_text(json.dumps(commands,indent=2)+'\n')
                if r.returncode or r.stderr:raise RuntimeError(stem+' failed; inspect saved output')
    report=dict(status='C_PRIVATE_SESSION_TRANSPORT_AND_EXISTING_TRANSPORTS_PASS',GPU_operations=0,variants=len(variants),commands=commands,source_sha256={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in files},captured_wire_sha256=capture_hashes,scope='interposed calls, modeled memory and optional real wire replay; not new physical rendering')
    (o/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],variants=len(variants),test_targets=len(tests),GPU_operations=0)))

if __name__=='__main__':main()
