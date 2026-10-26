from pathlib import Path
import subprocess,json
D=Path(__file__).resolve().parent;rows=[]
for n in ['TEST_NOHASH_TRUSTED.py','TEST_NOHASH_OUTPUT.py','TEST_CACHE_REGRESSIONS.py','TEST_CACHE_DOMAINS.py','TEST_BORROWED_TA_LEASE.py','TEST_PTE_BATCH.py','TEST_PTE_PROVIDER.py','TEST_COMBINED_CACHE_PTE.py','TEST_FROZEN_INPUT_CACHE.py']:
 p=subprocess.run(['python3',str(D/n)],capture_output=True);(D/(n+'.run.log')).write_bytes(p.stdout+p.stderr);print(n,p.returncode,flush=True);assert not p.returncode,p.stderr.decode()[-2500:];rows.append({'script':n,'exit':0})
(D/'REGRESSION_SUITE_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'tests':rows},indent=2)+'\n')
