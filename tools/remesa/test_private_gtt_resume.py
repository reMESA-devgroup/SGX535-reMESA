#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual GTT resume failure/success fixtures. CPU models, never hardware approval."""
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
    source=a.module/'gtt.c';text=source.read_text()
    names=['psb_gtt_mask_pte','psb_gtt_entry','psb_gtt_insert','psb_gtt_alloc','psb_gtt_takedown']
    bodies=[extract(text,n) for n in names]
    bodies[2]=bodies[2].replace('psb_gtt_insert(', 'actual_gtt_insert(',1)
    if a.native_counterexample:
        bodies.extend(extract(text,n) for n in ('psb_gtt_init','psb_gtt_restore'))
    else:
        bodies.append(extract(text,'remesa_private_gtt_init_live'))
        bodies.append((a.module/'remesa_private_gtt_resume.inc').read_text())
    (output/'gtt_actual.inc').write_text('\n'.join(bodies)+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_gtt_resume.c';rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-DCHECKED_GTT='+str(int(not a.native_counterexample)),
                 '-I'+str(output),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe),str(output/(variant+'-success-table.bin'))],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),source]
    if not a.native_counterexample:files.append(a.module/'remesa_private_gtt_resume.inc')
    status='NATIVE_GTT_RESUME_COUNTEREXAMPLES_CONFIRMED' if a.native_counterexample else 'PRIVATE_GTT_RESUME_MODEL_PASS'
    report=dict(status=status,driver_safety_pass=False,
        physically_qualified=False,GPU_operations=0,scope='Actual GTT init/restore/entry/insert/PTE/takedown bodies with reduced PCI/resource/MMIO dependencies. Invalid insertion records failure instead of dereferencing a missing mapping; valid insert delegates actual body. Not physical PM or GPU-stop proof.',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],driver_safety_pass=False,GPU_operations=0)))


if __name__=='__main__':main()
