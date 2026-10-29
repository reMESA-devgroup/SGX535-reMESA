#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual native GTT functions/guards, modeled APIs/MMIO, pthread unplug join."""
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
    a=p.parse_args();module=a.module.resolve();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    source=(module/'gtt.c').read_text()
    (output/'gtt_native_actual.inc').write_text('\n'.join(extract(source,n) for n in
        ['remesa_private_gtt_pin_live','remesa_private_gtt_unpin_live','remesa_private_gtt_roll_live'])+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_gtt_guard.c';rows=[]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for name,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
        exe=output/name;command=['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,
            '-I'+str(output),'-I'+str(module),str(test),'-pthread','-o',str(exe)]
        for label,argv in [(name+'-build',command),(name+'-run',[str(exe)])]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits)
            (output/(label+'.log')).write_text(r.stdout+r.stderr)
            rows.append(dict(label=label,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
    files=[test,Path(__file__).resolve(),root/'tools/remesa/test_private_source_mmio.py',
           module/'gtt.c',module/'remesa_private_gtt_guard.inc']
    report=dict(status='PRIVATE_GTT_ENTRY_JOIN_AND_ROLL_BOUNDS_MODEL_PASS',GPU_operations=0,
        driver_physically_qualified=False,
        scope='Actual pin/unpin/roll and guards; reduced metadata, MMIO/page/kernel APIs modeled; pthread join',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
