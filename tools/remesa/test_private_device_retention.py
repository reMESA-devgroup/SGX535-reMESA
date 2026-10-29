#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual private unload/probe, modeled APIs; not GPU stop or GEM safety proof."""
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
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    driver=a.module.resolve()/'psb_drv.c'
    source=driver.read_text()
    for name,function in [('unload_actual.inc','psb_driver_unload'),('probe_actual.inc','psb_pci_probe')]:
        (output/name).write_text(extract(source,function)+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_device_retention.c';rows=[]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for name,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
        exe=output/name
        command=['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,
                 '-I'+str(output),str(test),'-o',str(exe)]
        for label,argv in [(name+'-build',command),(name+'-run',[str(exe)])]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits)
            (output/(label+'.log')).write_text(r.stdout+r.stderr)
            rows.append(dict(label=label,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
    files=[test,driver,Path(__file__).resolve(),root/'tools/remesa/test_private_source_mmio.py']
    report=dict(status='PRIVATE_RETAINED_ENDPOINT_UNLOAD_AND_REBIND_MODEL_PASS',GPU_operations=0,
        driver_physically_qualified=False,scope='Actual unload/probe bodies; endpoint query and resource/CPU teardown APIs modeled',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
