from pathlib import Path
import hashlib,json,importlib.util,re,subprocess,tempfile
D=Path(__file__).resolve().parent;spec=importlib.util.spec_from_file_location('cpio','/home/gama/sgx535-offline/boot-provenance-readonly-20261001T025947Z/read_initramfs.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);sha=lambda b:hashlib.sha256(b).hexdigest()
rows,_=m.read_image((D/'depth1280-private-initrd.img').read_bytes());data={x['name']:x['data']for x in rows};init=data['init'];script=data['scripts/sgx535-first-load'];ko=data['usr/lib/sgx535-first-load/gma500_gfx.ko'];expected=re.search(rb'firstload_hash%% \*\}" != ([0-9a-f]{64})',init)[1].decode();assert expected==sha(script);assert sha(ko).encode()in script;assert expected!=sha(script+b'\n#tamper');assert sha(ko+b'\0').encode()not in script
results=[{'case':'v2 full parent-script-module identity chain','pass':True},{'case':'changed script rejected by unchanged parent','pass':True},{'case':'changed module rejected by script identity binding','pass':True}]
# Exercise the original shell's hash/OR condition in a tempfile harness.
# The payload is harmless and only creates a marker. No initramfs, module,
# device, GPU or real /scripts path is executed.
condition=re.search(rb'if \[ "\$firstload_status".*?; then',init).group().decode()
with tempfile.TemporaryDirectory(prefix='sgx-hash-test-')as tmp:
 p=Path(tmp);payload=p/'payload';marker=p/'called';payload.write_text('printf CALLED > '+str(marker)+'\n');pin=sha(payload.read_bytes());cond=condition.replace('/scripts/sgx535-first-load',str(payload)).replace(expected,pin)
 for label,change,want in [('valid',False,0),('tampered',True,77)]:
  if marker.exists():marker.unlink()
  if change:payload.write_text(payload.read_text()+'# changed\n')
  shell='firstload_hash=$(/bin/sha256sum '+str(payload)+')\nfirstload_status=$?\n'+cond+'\nexit77=77; exit "$exit77"\nfi\nexit 0\n'
  r=subprocess.run(['/bin/sh','-c',shell],capture_output=True);assert r.returncode==want,(label,r.stderr);assert marker.exists()==(not change);results.append({'case':'original shell condition '+label,'exit':r.returncode,'payload_executed':marker.exists(),'pass':True})
(D/'INIT_CHAIN_TEST_RESULTS.json').write_text(json.dumps({'classification':'CPU_ONLY_IDENTITY_CHAIN_TEST_PASS','cases':results,'deployed':False,'booted':False,'GPU_submissions':0},indent=2)+'\n');print(results)
