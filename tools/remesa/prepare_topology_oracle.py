#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate six unmodified index translators from pinned public Mesa25.2.0.

Oracle only, not driver source or rendering. Input starts are normalized to
zero after selecting the source slice; triangle parity is relative to the draw.
"""
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path

PIN='9798646139782c2b28bdb37aedf4de4171a69a13739d3200d4d11eaf6afdffba'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mesa',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();source=a.mesa/'src/gallium/auxiliary/indices/u_indices_gen.py'
    if hashlib.sha256(source.read_bytes()).hexdigest()!=PIN:raise ValueError('unreviewed Mesa index generator')
    spec=importlib.util.spec_from_file_location('pinned_mesa_indices',source);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
    text=io.StringIO();text.write(m.copyright+'\n#include <stdint.h>\n#include <stdbool.h>\n#include <stdlib.h>\n')
    names={}
    for mode,method in [('MESA_PRIM_TRIANGLES','tris'),('MESA_PRIM_TRIANGLE_STRIP','tristrip'),('MESA_PRIM_TRIANGLE_FAN','trifan')]:
        names[mode]={}
        for pv in (m.FIRST,m.LAST):
            getattr(m,method)(text,m.UINT32,m.UINT16,pv,pv,m.PRDISABLE)
            names[mode][pv]=m.name(m.UINT32,m.UINT16,pv,pv,m.PRDISABLE,method,m.OUT_TRIS)
    text.write('static void mesa_topology_expected(unsigned mode,bool first,const uint32_t *indices,unsigned input_count,unsigned count,uint16_t *out)\n{\nswitch(mode){\n')
    for mode,n in names.items():
        text.write(f'case {mode}:\n if(first){n[m.FIRST]}(indices,0,input_count,count,0,out);\n else {n[m.LAST]}(indices,0,input_count,count,0,out);\n break;\n')
    text.write('default:abort();\n}\n}\n')
    with a.output.open('x') as f:f.write(text.getvalue())
    print(json.dumps({'status':'PINNED_MESA_INDEX_ORACLE_GENERATED','generator_sha256':PIN,'functions':6,'GPU_operations':0}))

if __name__=='__main__':main()
