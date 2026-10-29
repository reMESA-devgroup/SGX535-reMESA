#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual IRQ/brightness power counterexamples or private controls. GPU0."""
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
    p.add_argument('--cc',default='cc')
    p.add_argument('--runner',nargs='+',default=[])
    p.add_argument('--static-i486',action='store_true')
    p.add_argument('--ubsan',action='store_true')
    p.add_argument('--expect-guards',action='store_true')
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    specs={'psb_irq.c':['psb_irq_handler'],
           'psb_intel_lvds.c':['psb_intel_lvds_set_brightness'],
           'psb_device.c':['psb_set_brightness'],
           'backlight.c':['do_gma_backlight_set','gma_backlight_set'],
           'opregion.c':['asle_set_backlight']}
    bodies=[]
    for name,names in specs.items():
        bodies.extend(extract((a.module/name).read_text(),n) for n in names)
    (output/'async_actual.inc').write_text('\n'.join(bodies)+'\n')
    test=root/'kernel/remesa/gma500/tests/test_private_async_power.c';rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-DEXPECT_GUARDS='+str(int(a.expect_guards)),'-I'+str(output),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),*[a.module/n for n in specs]]
    report=dict(status=('PRIVATE_ASYNC_POWER_GUARDS_MODEL_PASS' if a.expect_guards else 'PRIVATE_ASYNC_POWER_COUNTEREXAMPLES_CONFIRMED'),driver_safety_pass=False,
        physically_qualified=False,GPU_operations=0,scope='Actual IRQ handler and ASLE/backlight/platform/LVDS dispatch chain; PM/DRM/source capture and hardware dependencies modeled; pthread scheduled interleavings, not kernel PM/unplug or GPU-stop proof',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],driver_safety_pass=False,GPU_operations=0)))


if __name__=='__main__':main()
