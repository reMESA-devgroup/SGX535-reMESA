#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exact console stolen-mapping verifier, reduced CPU page-table dependencies. GPU0."""
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
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    output=a.output.resolve();output.mkdir(parents=True,exist_ok=False)
    import re
    source=a.module/'mmu.c';header=a.module/'psb_drv.h'
    text=source.read_text();defs=header.read_text()
    names=['PSB_MMU_CACHED_MEMORY','PSB_MMU_RO_MEMORY','PSB_MMU_WO_MEMORY','PSB_PTE_VALID','PSB_PTE_WO','PSB_PTE_RO','PSB_PTE_CACHED']
    constants=[]
    for name in names:
        matches=re.findall(r'^#define[ \t]+'+name+r'[ \t]+(0x[0-9a-fA-F]+)\b',defs,re.M)
        if len(matches)!=1:raise ValueError('unreviewed PTE macro: '+name)
        constants.append('#define '+name+' '+matches[0])
    (output/'pte_constants.inc').write_text('\n'.join(constants)+'\n')
    (output/'pte_actual.inc').write_text(extract(text,'psb_mmu_mask_pte')+'\n')
    test=root/'kernel/remesa/gma500/tests/test_console_mapping.c';rows=[]
    def limits(emulated=False):
        resource.setrlimit(resource.RLIMIT_CORE,(0,0));bound=6*1024**3 if emulated else 768*1024**2
        resource.setrlimit(resource.RLIMIT_AS,(bound,bound));resource.setrlimit(resource.RLIMIT_CPU,(60,60))
    variants=[('native',['-O2'])]
    if a.ubsan:variants.append(('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all']))
    for variant,flags in variants:
        exe=output/variant
        command=[a.cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-sign-compare',*flags,
                 *(['-static','-march=i486'] if a.static_i486 else []),
                 '-I'+str(a.module.resolve()),
                 '-I'+str(output),str(test),'-pthread','-o',str(exe)]
        for name,argv,emulated in [(variant+'-build',command,False),(variant+'-run',a.runner+[str(exe)],bool(a.runner))]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=lambda:limits(emulated))
            (output/(name+'.log')).write_text(r.stdout+r.stderr);rows.append(dict(label=name,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(name+': '+r.stderr)
    files=[test,Path(__file__).resolve(),source,header,a.module/'remesa_console_mapping.inc',a.module/'remesa_console_mapping.h']
    status='CONSOLE_STOLEN_MAPPING_MODEL_PASS'
    report=dict(status=status,driver_safety_pass=False,physically_qualified=False,GPU_operations=0,
        scope='Actual new verifier plus unchanged native PTE encoder/constants; default-PD/page/locking dependencies reduced. No pin/ownership transfer/visibility/retirement proof.',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(status=report['status'],driver_safety_pass=False,GPU_operations=0)))


if __name__=='__main__':main()
