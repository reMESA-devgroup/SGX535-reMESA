from pathlib import Path
import json,subprocess,hashlib
D=Path(__file__).resolve().parent;O=D.parent;M=O/'phase9-real-mesa-20261008';Q=O/'phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386';src=D/'tests/test_ta_cache_domains.c';s=src.read_text();marker='static void initialize(bool domains)';assert s.count(marker)==1
s=s.replace(marker,'static struct psb_mmu_pd combined_pd;\nstatic int combined_guard(u64 off,const void*p,unsigned n){(void)off;(void)p;(void)n;return 0;}\n'+marker,1)
marker='if(domains)assert(!remesa_gem_stream_enable_ta_domains(&s));';assert s.count(marker)==1
s=s.replace(marker,'if(domains){s.pd=&combined_pd;s.pds_write_guard=combined_guard;assert(!remesa_gem_stream_enable_ta_domains(&s));}',1)
marker='static void read_ta(unsigned role){';assert s.count(marker)==1
s=s.replace(marker,marker+'unsigned maps_before=vmap_calls,unmaps_before=vunmap_calls;void*cached_before=s.bo[role].cached_job_view;assert(cached_before);',1)
marker='assert(s.bo[role].ta_cpu_cached&&role_flush[role]==before+2);}';assert s.count(marker)==1
s=s.replace(marker,'assert(s.bo[role].ta_cpu_cached&&role_flush[role]==before+2);assert(vmap_calls==maps_before&&vunmap_calls==unmaps_before&&s.bo[role].cached_job_view==cached_before&&!s.bo[role].lease_borrowed);}',1)
marker='return 0;}';at=s.rindex(marker);s=s[:at]+'assert(batch_verify_calls>0);printf("PASS combined sealed PDS/PTE routing with TA cache+borrowed READ leases: batches=%u, no new READTA vmap/vunmap, all subsequent frames/checks/faults preserved; modeled CPU GPU0\\n",batch_verify_calls);'+s[at:]
p=D/'tests/test_combined_cache_pte.c';p.write_text(s);rows=[]
for mode,cc,flags,runner in [('native','cc',['-O2'],[]),('ubsan','cc',['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',str(M/'wrappers/cc'),['-O2','-static','-march=i486'],[str(Q)])]:
 exe=D/('test-combined-cache-pte-'+mode);cmd=[cc,'-std=c99','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-I'+str(D/'tests'),'-I/home/gama/sgx535-gfx/include',*flags,str(p),'-o',str(exe)];r=subprocess.run(cmd,capture_output=True);(D/(mode+'-combined-build.log')).write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr.decode()[-2000:]
 r=subprocess.run([*runner,str(exe)],capture_output=True);(D/(mode+'-combined-run.log')).write_bytes(r.stdout+r.stderr);assert not r.returncode,r.stderr.decode()[-2000:];print(mode,r.stdout.decode().strip(),flush=True);rows.append({'mode':mode,'exit':0,'command':cmd,'stdout':r.stdout.decode()})
(D/'COMBINED_CACHE_PTE_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'remote_calls':0,'scope':'Actual K provider/cache/owner bridge with synthetic PFN/cache/vmap model, authenticated-input seal modeled by dummy PDS guard. Actual MMU batch separately differential-tested; no hardware coherence proof.','base_C2_test_SHA256':hashlib.sha256(src.read_bytes()).hexdigest(),'combined_test_SHA256':hashlib.sha256(p.read_bytes()).hexdigest(),'tests':rows},indent=2)+'\n')
