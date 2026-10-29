#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual orphaning fragments and close with modeled metadata/mutexes, GPU0."""
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
    source=(a.module/'remesa_gem_memory.c').read_text()
    (output/'memory_close_actual.inc').write_text(extract(source,'remesa_gem_memory_close'))
    (output/'native_postclose_actual.inc').write_text(extract((a.module/'remesa_stream_bridge.c').read_text(),'remesa_stream_postclose'))
    test=root/'kernel/remesa/gma500/tests/test_private_file_lost.c';rows=[]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for name,flags in [('native',['-O2']),('ubsan',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'])]:
        exe=output/name;command=['cc','-std=c11','-Wall','-Wextra','-Werror',*flags,
             '-I'+str(output),'-I'+str(a.module.resolve()),str(test),'-o',str(exe)]
        for label,argv in [(name+'-build',command),(name+'-run',[str(exe)])]:
            r=subprocess.run(argv,capture_output=True,text=True,timeout=30,preexec_fn=limits)
            (output/(label+'.log')).write_text(r.stdout+r.stderr)
            rows.append(dict(label=label,argv=argv,exit=r.returncode))
            if r.returncode or r.stderr:raise RuntimeError(label+': '+r.stderr)
    files=[test,a.module/'remesa_gem_memory.c',a.module/'remesa_stream_bridge.c']+[a.module/n for n in
          ('remesa_private_file_lost.h','remesa_private_trusted_file_lost.inc',
           'remesa_private_stream_file_lost.inc','remesa_private_memory_file_lost.inc')]
    report=dict(status='PRIVATE_FILE_LOST_RETENTION_MODEL_PASS',GPU_operations=0,
        scope='Actual orphan helpers, native bridge postclose and memory close; mutex/backing/source-poison/native callbacks/memory-postclose modeled',
        sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},commands=rows)
    (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(dict(status=report['status'],GPU_operations=0)))


if __name__=='__main__':main()
