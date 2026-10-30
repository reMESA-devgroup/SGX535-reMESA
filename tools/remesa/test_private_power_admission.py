#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual private power admission/error fixtures. Never a driver safety pass."""
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
    p.add_argument('--pci-helpers',action='store_true')
    p.add_argument('--native-counterexample',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    source=a.module/'power.c';text=source.read_text()
    if a.native_counterexample and not a.pci_helpers:
        p.error('--native-counterexample requires --pci-helpers')
    if a.pci_helpers:
        fragment = source.read_text() if a.native_counterexample else (a.module/'remesa_private_pci_suspend.inc').read_text()+'\n'+(a.module/'remesa_private_power.inc').read_text()
        text='\n'.join(extract(fragment,n) for n in ('gma_suspend_pci','gma_resume_pci'))
        test=root/'kernel/remesa/gma500/tests/test_private_pci_errors.c'
    else:
        fragment=(a.module/'remesa_private_power.inc').read_text()
        text='\n'.join(line for line in fragment.splitlines() if not line.startswith('#include'))
        test=root/'kernel/remesa/gma500/tests/test_private_power_admission.c'
    (output/'power_actual.inc').write_text(text+'\n');rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-DCHECKED_PCI='+str(int(not a.native_counterexample)),
                 '-I'+str(output),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),source]
    if not a.native_counterexample:
        files.append(a.module/'remesa_private_power.inc')
        if a.pci_helpers:files.append(a.module/'remesa_private_pci_suspend.inc')
    status=('PCI_HELPER_COUNTEREXAMPLES_CONFIRMED' if a.native_counterexample else 'PRIVATE_PCI_ERRORS_MODEL_PASS') if a.pci_helpers else 'PRIVATE_POWER_ADMISSION_MODEL_PASS'
    report=dict(status=status,driver_safety_pass=False,
        physically_qualified=False,GPU_operations=0,scope='Actual private power or PCI helper bodies; PCI/MMIO/PM/task dependencies modeled; not physical S3, GPU stop or runtime-PM proof',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],driver_safety_pass=False,GPU_operations=0)))


if __name__=='__main__':main()
