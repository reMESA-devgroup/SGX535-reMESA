"""Offline-only generator. Typed C grammar performs PDS masking; no Python substitute."""
from pathlib import Path
import subprocess,json,hashlib,tempfile
C=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as t:
 t=Path(t);src=t/'mask.c';exe=t/'mask';masked=t/'masked.bin'
 src.write_text('''#include <stdbool.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include "remesa_stream_trusted.h"\n#include "stream_profile_asset.h"\n#include "remesa_stream_grammar.h"\nint main(int argc,char **argv){unsigned char p[131072];FILE*f;if(argc!=3)return 2;f=fopen(argv[1],"rb");if(!f||fread(p,1,sizeof(p),f)!=sizeof(p)||fclose(f))return 3;if(remesa_stream_mask_sources(gears_stream_profile_asset.batches,gears_stream_profile_asset.count,p,sizeof(p),gears_stream_profile_asset.bindings[0].va))return 4;f=fopen(argv[2],"wb");return !f||fwrite(p,1,sizeof(p),f)!=sizeof(p)||fclose(f);}\n''')
 subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-I'+str(C/'module'),str(src),str(C/'module/remesa_stream_grammar.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(C/'stream-profile/role0.bin'),str(masked)],check=True)
 data=[masked.read_bytes()]+[(C/'stream-profile'/f'role{i}.bin').read_bytes()for i in range(1,5)]
 parts=['/* Offline generated from qualified CODE336 profile; PDS typed masker is authoritative. */','#ifndef REMESA_CANONICAL_INPUTS_H','#define REMESA_CANONICAL_INPUTS_H']
 for i,b in enumerate(data):
  parts.append('static const unsigned char remesa_canonical_role%d[%d] = {'%(i,len(b)))
  parts += [','.join('0x%02x'%x for x in b[j:j+24])+','for j in range(0,len(b),24)];parts.append('};')
 parts+=['static const unsigned char *const remesa_canonical_inputs[5] = {'+','.join('remesa_canonical_role%d'%i for i in range(5))+'};','static const unsigned remesa_canonical_input_sizes[5] = {'+','.join(str(len(b))for b in data)+'};','#endif']
 (C/'module/remesa_canonical_inputs.h').write_text('\n'.join(parts)+'\n')
 (C/'CANONICAL_INPUTS_GENERATION.json').write_text(json.dumps({'GPU_operations':0,'method':'existing remesa_stream_mask_sources C function and exact qualified role1..4 files','masked_and_exact_sha256':[hashlib.sha256(b).hexdigest()for b in data],'raw_asset_sha256':[hashlib.sha256((C/'stream-profile'/f'role{i}.bin').read_bytes()).hexdigest()for i in range(5)]},indent=2)+'\n')
