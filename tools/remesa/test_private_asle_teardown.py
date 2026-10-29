#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual unload/ASLE worker regression; modeled workqueue/backlight, GPU0."""
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
    p.add_argument('--ubsan',action='store_true')
    p.add_argument('--static-i486',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    tests=root/'kernel/remesa/gma500/tests'
    fixture=tests/'test_private_asle_teardown.c';rows=[];sources=[fixture,tests/'test_private_device_retention.c',Path(__file__).resolve()]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        limit=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(limit,limit))
        resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for label,module in [('baseline',a.baseline_module),('fixed',a.module)]:
        dst=output/label;dst.mkdir()
        driver=module/'psb_drv.c';opregion=module/'opregion.c';sources.extend([driver,opregion])
        for name,function in [('unload_actual.inc','psb_driver_unload'),('probe_actual.inc','psb_pci_probe')]:
            (dst/name).write_text(extract(driver.read_text(),function)+'\n')
        for name,function in [('asle_set_actual.inc','asle_set_backlight'),('asle_work_actual.inc','psb_intel_opregion_asle_work')]:
            (dst/name).write_text(extract(opregion.read_text(),function)+'\n')
        for variant,flags in variants:
            exe=dst/variant
            command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,
                     *(['-static','-march=i486'] if a.static_i486 else []),
                     *(['-DEXPECT_ASLE_RACE=1'] if label=='baseline' else []),
                     '-I'+str(dst),str(fixture),'-pthread','-o',str(exe)]
            for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
                r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
                (dst/(name+'.log')).write_text(r.stdout+r.stderr)
                rows.append(dict(label=label+'-'+name,argv=argv,exit=r.returncode))
                if r.returncode or r.stderr:raise RuntimeError(label+'-'+name+': '+r.stderr)
    report=dict(status='ASLE_STALE_BACKLIGHT_COUNTEREXAMPLE_AND_JOIN_MODEL_PASS',GPU_operations=0,
        physically_qualified=False,scope='Actual unload, ASLE set/worker; workqueue/backlight/DRM APIs modeled, pthread active-worker join; stale witness uses allocated fixture storage',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
