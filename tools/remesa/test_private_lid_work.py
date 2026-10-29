#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual old timer/new lid work with modeled APIs and pthread join, GPU0."""
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
    p.add_argument('--cc',default='cc')
    p.add_argument('--runner',nargs='+',default=[])
    p.add_argument('--static-i486',action='store_true')
    p.add_argument('--ubsan',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    baseline=a.baseline_module/'psb_lid.c';text=baseline.read_text()
    (output/'baseline_lid_actual.inc').write_text('\n'.join(extract(text,n) for n in ['psb_lid_timer_func','psb_lid_timer_init','psb_lid_timer_takedown'])+'\n')
    for name in ['linux/workqueue.h','drm/drm_drv.h']:
        f=output/name;f.parent.mkdir(exist_ok=True);f.write_text('/* APIs modeled by the fixture. */\n')
    test=root/'kernel/remesa/gma500/tests/test_private_lid_work.c';rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-I'+str(output),'-I'+str(a.module.resolve()),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),baseline,a.module/'psb_lid.c',a.module/'psb_drv.h',a.module/'remesa_private_lid.inc']
    report=dict(status='PRIVATE_LID_WORK_AND_NATIVE_TIMER_COUNTEREXAMPLES_MODEL_PASS',GPU_operations=0,
        physically_qualified=False,scope='Actual old timer/new work; stable-register spin and NULL witnesses intercepted with longjmp; MMIO/DRM/power/workqueue modeled; pthread work join, not kernel or physical proof',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
