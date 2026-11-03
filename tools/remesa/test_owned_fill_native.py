#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual native/source join, modeled MMIO and GEM receipt; zero GPU work."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from test_private_source_mmio import extract

FIXTURES = {'legacy_producer_actual.inc': 'f122190e32459ffb19020bff654f8ad332dea96d7a8a1c6309e7f74c20d567d9', 'native_defs_actual.h': 'd5ed7de595b18e0c433578a94de3e95313d5c5ef03678e86caaef529cc475bdc', 'native_source_defs.h': '8e0635d86c3b81ee72a6c3d0729614ae333abe743a4c0767f8456f88c0d66a73', 'fixed_capture_actual.inc': 'a980b7456639e5377353cc770cf4889e46e7f3456060866c9f25c55d841c7d84'}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module',type=Path,required=True)
    p.add_argument('--fixtures',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--i486-cc',type=Path)
    p.add_argument('--qemu-i386',type=Path)
    a=p.parse_args()
    if bool(a.i486_cc)!=bool(a.qemu_i386):p.error('both i486 tools required')
    w=Path(__file__).resolve().parents[2];a.output.mkdir(parents=True,exist_ok=False)
    for n,h in FIXTURES.items():
        raw=(a.fixtures/n).read_bytes()
        if hashlib.sha256(raw).hexdigest()!=h:raise ValueError('unreviewed native model fixture: '+n)
        (a.output/n).write_bytes(raw)
    source=(a.module/'remesa_stream_source.c').read_text()
    actual=a.output/'owned_native_source_actual.inc'
    actual.write_text(extract(source,'lost')+'\n'+(a.module/'remesa_owned_fill_source.inc').read_text()+'\n'+
        '\n'.join(extract(source,n) for n in ('remesa_route_source_producer','remesa_route_source_pending'))+'\n')
    test=w/'kernel/remesa/gma500/tests/test_owned_fill_native.c'
    variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
    if a.i486_cc:variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
    rows=[]
    for name,cc,flags,runner in variants:
        exe=a.output/name
        cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',*flags,
             '-DREMESA_TWOD_VENDOR_STREAM','-DREMESA_TWOD_FILL_STREAM',
             '-I'+str(a.output),'-I'+str(a.module),'-I'+str(w/'include'),
             '-I'+str(w/'kernel/remesa/gma500'),str(test),str(a.module/'remesa_twod.c'),
             str(a.module/'frozen_source_guard.c'),'-o',str(exe)]
        # test_twod_native.c already includes remesa_twod.c in the same TU.
        cmd.remove(str(a.module/'remesa_twod.c'))
        for label,argv in [(name+'-build',cmd)]+[(name+'-'+str(i),runner+[str(exe),str(i)]) for i in range(24)]:
            def limits():
                resource.setrlimit(resource.RLIMIT_CORE,(0,0))
                resource.setrlimit(resource.RLIMIT_AS,((6*1024**3 if runner else 768*1024**2),)*2)
            q=subprocess.run(argv,capture_output=True,timeout=90,preexec_fn=limits)
            (a.output/(label+'.stdout')).write_bytes(q.stdout);(a.output/(label+'.stderr')).write_bytes(q.stderr)
            rows.append(dict(label=label,argv=argv,exit=q.returncode))
            (a.output/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
            if q.returncode or q.stderr:raise RuntimeError(label+': '+q.stderr.decode()[-2500:])
    files=[Path(__file__),test,actual,*[a.fixtures/n for n in FIXTURES],*[w/'kernel/remesa/gma500'/n for n in
        ('tests/test_twod_native.c','tests/test_twod_reservation.c','remesa_twod_native.inc','remesa_twod_reservation.inc','remesa_twod_irq.inc')],
        *[a.module/n for n in ('remesa_stream_source.c','remesa_owned_fill_source.inc','remesa_owned_fill_source.h','remesa_gem_twod_job.h','remesa_twod.c','remesa_twod.h','frozen_source_guard.c','frozen_source_guard.h')]]
    result=dict(status='ACTUAL_OWNED_SOURCE_NATIVE_CLEAR_JOIN_OFFLINE_PASS',GPU_operations=0,physical_qualification=False,
                scope='Actual source/native/reservation/IRQ/packet constructor and frozen guard. Modeled MMIO/task/power/locks and final GEM receipt. No pixel simulation or physical retirement attestation.',
                variants=len(variants),scenarios=24,commands=rows,
                sources={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (a.output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(result['status'])


if __name__=='__main__':main()
