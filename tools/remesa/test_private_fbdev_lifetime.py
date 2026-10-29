#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual private fbdev fragments vs native mmap; modeled APIs and pthread join."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from test_private_source_mmio import extract


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module',type=Path,required=True)
    p.add_argument('--baseline-module',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();module=a.module.resolve();baseline=a.baseline_module.resolve()
    root=Path(__file__).resolve().parents[2];out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    fb=(module/'framebuffer.c').read_text();old=(baseline/'framebuffer.c').read_text()
    (out/'fb_destroy_actual.inc').write_text(extract(fb,'psb_fbdev_destroy')+'\n')
    (out/'fb_init_actual.inc').write_text(extract(fb,'psb_fbdev_init')+'\n')
    (out/'config_native_actual.inc').write_text(extract((module/'gma_display.c').read_text(),'remesa_private_fb_config_live')+'\n')
    (out/'baseline_fb_fault.inc').write_text(extract(old,'psbfb_vm_fault').replace('psbfb_vm_fault(', 'baseline_fb_fault(')+'\n')
    (out/'baseline_fb_mmap.inc').write_text(extract(old,'psbfb_mmap').replace('psbfb_mmap(', 'baseline_fb_mmap(').replace('psbfb_vm_ops','baseline_fb_vm_ops')+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_fbdev_lifetime.c';rows=[]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for name,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
        exe=out/name;cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,
            '-I'+str(out),'-I'+str(module),str(test),'-pthread','-o',str(exe)]
        for label,argv in [(name+'-build',cmd),(name+'-run',[str(exe)])]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits)
            (out/(label+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=label,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
    files=[test,Path(__file__).resolve(),root/'tools/remesa/test_private_source_mmio.py',baseline/'framebuffer.c']
    files += [module/n for n in ['framebuffer.c','gma_display.c','psb_drv.c','psb_drv.h',
        'remesa_private_fbdev.h','remesa_private_fbdev_map.inc','remesa_private_fbdev_ops.inc',
        'remesa_private_fbdev_detach.inc','remesa_private_fbdev_config.inc']]
    report=dict(status='PRIVATE_FBDEV_BOUNDS_VMA_AND_FINAL_FILE_MODEL_PASS',GPU_operations=0,
        driver_physically_qualified=False,
        scope='Actual private fbdev fragments and native destroy/config vs native mmap/fault; fb/VFS/GEM/PFN/teardown APIs modeled, pthread entry join',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (out/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
