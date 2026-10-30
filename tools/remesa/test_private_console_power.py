#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual console sync/submit power fixtures. CPU models, not GPU retirement proof."""
import argparse
import hashlib
import json
from pathlib import Path
from test_private_source_mmio import extract
import resource
import subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--cc',default='cc')
    p.add_argument('--runner',nargs='+',default=[])
    p.add_argument('--static-i486',action='store_true')
    p.add_argument('--ubsan',action='store_true')
    p.add_argument('--native-counterexample',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    source=a.module/'accel_2d.c';text=source.read_text()
    (output/'console_actual.inc').write_text('\n'.join(extract(text,n) for n in ('psb_2d_wait_available','psbfb_2d_submit','psbfb_sync'))+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_console_power.c';rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-sign-compare',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-DCHECKED_CONSOLE='+str(int(not a.native_counterexample)),
                 '-I'+str(output),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),source]
    status='CONSOLE_SYNC_POWER_COUNTEREXAMPLES_CONFIRMED' if a.native_counterexample else 'PRIVATE_CONSOLE_SYNC_POWER_GUARDS_MODEL_PASS'
    report=dict(status=status,driver_safety_pass=False,async_console_PM_counterexample_remains=True,
        frozen_jiffies_child_controls=True,FIFO_reset_dependency_modeled=True,
        physically_qualified=False,GPU_operations=0,scope='Actual console sync and submit bodies; PM admission/clock/FIFO/MMIO dependencies modeled with pthread scheduled closure. Asynchronous queued-work counterexample remains in both modes; not GPU retirement or kernel PM proof.',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],driver_safety_pass=False,GPU_operations=0)))


if __name__=='__main__':main()
