#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual OpRegion callback/init/fini, scheduled owner-lookup seam, GPU0."""
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
    test=root/'kernel/remesa/gma500/tests/test_private_acpi_notifier.c'
    rows=[];sources=[test,Path(__file__).resolve(),root/'tools/remesa/test_private_source_mmio.py']
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound))
        resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for label,module in [('baseline',a.baseline_module),('fixed',a.module)]:
        dst=output/label;dst.mkdir();source=module/'opregion.c';sources.append(source)
        for name,function in [('notifier_callback_actual.inc','psb_intel_opregion_video_event'),('notifier_init_actual.inc','psb_intel_opregion_init'),('notifier_fini_actual.inc','psb_intel_opregion_fini')]:
            (dst/name).write_text(extract(source.read_text(),function)+'\n')
        for variant,flags in variants:
            exe=dst/variant
            command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,
                     *(['-static','-march=i486'] if a.static_i486 else []),
                     *(['-DEXPECT_NOTIFIER_RACE=1'] if label=='baseline' else []),
                     '-I'+str(dst),str(test),'-pthread','-o',str(exe)]
            for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
                r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
                (dst/(name+'.log')).write_text(r.stdout+r.stderr)
                rows.append(dict(label=label+'-'+name,argv=argv,exit=r.returncode))
                if r.returncode or r.stderr:raise RuntimeError(label+'-'+name+': '+r.stderr)
    report=dict(status='ACPI_NOTIFIER_OWNER_CLEAR_COUNTEREXAMPLE_AND_JOIN_MODEL_PASS',GPU_operations=0,
        physically_qualified=False,scope='Actual callback/init/fini; callback owner reads instrumented at macro seam, pthread blocking-notifier contract, kernel APIs modeled; NULL lookup recorded via safe fallback, not machine-code or physical proof',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
