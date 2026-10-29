#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual standard-GEM lifetime/constructors; modeled kernel APIs, pthread join."""
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
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    module=a.module.resolve();output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    for target,source,function in [
        ('gtt_free_actual.inc','gtt.c','psb_gtt_free_range'),
        ('gem_free_live_actual.inc','gem.c','remesa_private_gem_free_object_live'),
        ('gem_create_actual.inc','gem.c','psb_gem_create'),
        ('fb_alloc_actual.inc','framebuffer.c','psbfb_alloc')]:
        (output/target).write_text(extract((module/source).read_text(),function)+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_gem_lifetime.c';rows=[]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for name,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
        exe=output/name
        command=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,
                 '-I'+str(output),'-I'+str(module),str(test),'-pthread','-o',str(exe)]
        for label,argv in [(name+'-build',command),(name+'-run',[str(exe)]),
                           (name+'-unpin-failure',[str(exe),'unpin-failure']),
                           (name+'-other-pin',[str(exe),'other-pin']),
                           (name+'-invalid-pin',[str(exe),'invalid-pin']),
                           (name+'-nested-loss',[str(exe),'nested-loss'])]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits)
            (output/(label+'.log')).write_text(r.stdout+r.stderr)
            rows.append(dict(label=label,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
    files=[test,Path(__file__).resolve(),root/'tools/remesa/test_private_source_mmio.py']
    files += [module/n for n in ['gem.c','gtt.c','gtt.h','framebuffer.c',
                               'remesa_private_gem_lifetime.h','remesa_private_gem_lifetime.inc',
                               'remesa_private_gtt_guard.inc']]
    report=dict(status='PRIVATE_GEM_JOIN_AND_RETENTION_MODEL_PASS',GPU_operations=0,
        driver_physically_qualified=False,
        scope='Actual tracker/destructor/GTT entry wrappers, original destructor/GTT-free and GEM/console constructors; native GTT/DRM/list APIs modeled, pthread joining',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
