#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile existing wire envelopes with chosen DRM headers; never open a device."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def wire(layout):
    # C object alignment may vary by architecture without changing wire bytes.
    return {name:{k:v for k,v in item.items() if k!='alignment'}
            for name,item in layout['wire'].items()}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--drm-headers',type=Path,required=True,help='Directory containing original drm.h and drm_mode.h')
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--reference',type=Path,help='Prior report with the approved wire layout')
    p.add_argument('--cc',default='cc')
    p.add_argument('--runner',nargs='+',default=[])
    p.add_argument('--static-i486',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    headers=output/'headers/drm';headers.mkdir(parents=True)
    sources=[]
    for name in ['drm.h','drm_mode.h']:
        src=a.drm_headers/name;data=src.read_bytes();(headers/name).write_bytes(data);sources.append(src)
    src=root/'tools/remesa/uapi_layout_probe.c';sources.append(src)
    sources.extend(sorted((root/'include/remesa').glob('*uapi.h')))
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    exe=output/'probe'
    command=[a.cc,'-std=c11','-O2','-Wall','-Wextra','-Werror',
             *(['-static','-march=i486'] if a.static_i486 else []),
             '-I'+str(output/'headers'),'-I'+str(root/'include'),str(src),'-o',str(exe)]
    rows=[];layout=None
    for label,argv,emulated in [('build',command,False),('run',a.runner+[str(exe)],bool(a.runner))]:
        r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
        (output/(label+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=label,argv=argv,exit=r.returncode))
        if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
        if label=='run':layout=json.loads(r.stdout)
    equal=None
    if a.reference:
        previous=json.loads(a.reference.read_text())['layout'];equal=wire(layout)==wire(previous)
    report=dict(status='WIRE_LAYOUT_MATCH' if equal else 'WIRE_LAYOUT_MISMATCH' if equal is False else 'WIRE_LAYOUT_RECORDED',
        layout=layout,reference=str(a.reference) if a.reference else None,wire_matches_reference=equal,
        scope='Selected envelope sizes, ioctl values, versions and listed field offsets only; no kernel module build, endpoint registration, driver compatibility or acceleration qualification',
        GPU_operations=0,device_opened=False,sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in sources},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    if equal is False:raise SystemExit('wire layout mismatch; inspect RESULT.json')
    print(json.dumps(dict(status=report['status'],word_bits=layout['word_bits'],envelopes=len(layout['wire']),GPU_operations=0)))


if __name__=='__main__':main()
