#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual GEM issue source-owner transport; upstream validation is modeled."""
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
    p.add_argument('--i486-cc',type=Path)
    p.add_argument('--qemu-i386',type=Path)
    a=p.parse_args()
    if bool(a.i486_cc)!=bool(a.qemu_i386):p.error('both i486 tools required')
    w=Path(__file__).resolve().parents[2];a.output.mkdir(parents=True,exist_ok=False)
    source=a.module/'remesa_gem_twod_job.inc';actual=a.output/'owned_gem_issue_actual.inc'
    actual.write_text(extract(source.read_text(),'remesa_gem_twod_issue')+'\n')
    test=w/'kernel/remesa/gma500/tests/test_owned_fill_gem.c';rows=[]
    variants=[('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[])]
    if a.i486_cc:variants.append(('i486',str(a.i486_cc),['-O2','-static','-march=i486'],[str(a.qemu_i386)]))
    for name,cc,flags,runner in variants:
        exe=a.output/name;cmd=[cc,'-std=c11','-Wall','-Wextra','-Werror',*flags,'-DREMESA_GENERAL_SCENE',
            '-I'+str(a.output),'-I'+str(a.module),'-I'+str(w/'include'),str(test),'-o',str(exe)]
        for label,argv in [(name+'-build',cmd)]+[(name+'-'+str(i),runner+[str(exe),str(i)]) for i in range(17)]:
            def limits():
                resource.setrlimit(resource.RLIMIT_CORE,(0,0))
                resource.setrlimit(resource.RLIMIT_AS,((6*1024**3 if runner else 768*1024**2),)*2)
            q=subprocess.run(argv,capture_output=True,timeout=60,preexec_fn=limits)
            (a.output/(label+'.stdout')).write_bytes(q.stdout);(a.output/(label+'.stderr')).write_bytes(q.stderr)
            rows.append(dict(label=label,argv=argv,exit=q.returncode));(a.output/'COMMANDS.json').write_text(json.dumps(rows,indent=2)+'\n')
            if q.returncode or q.stderr:raise RuntimeError(label+': '+q.stderr.decode()[-2000:])
    files=[Path(__file__),test,source,actual,a.module/'remesa_gem_twod_job.h']
    result=dict(status='ACTUAL_OWNED_FILL_GEM_LAUNCH_OFFLINE_PASS',GPU_operations=0,physical_qualification=False,
                scope='Actual GEM issue including owner derivation, flush/attach/issue/error ordering; upstream validated plan, retained objects/locks/flush/native callback modeled.',
                variants=len(variants),scenarios=17,commands=rows,sources={str(f.resolve()):hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (a.output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(result['status'])


if __name__=='__main__':main()
