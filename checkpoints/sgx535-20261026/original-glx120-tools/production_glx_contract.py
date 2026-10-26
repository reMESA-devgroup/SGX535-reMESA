"""Offline aggregate production evidence checks; no receipt/readback assertions."""
import re

def validate_production_run(result,stderr,stdout,capture_entries):
 assert result['exit']==0 and not result.get('error') and result['maximum_frames']==120 and result['continuous']
 assert result['production_FAST'] and result['checkpoints']is False and result['driver_hashes']is False and result['per_frame_diagnostics']is False
 assert result['legacy_capsule_unchanged'] and all(x=='N'for x in result['controls_after'].values())
 assert not capture_entries and not result.get('capture_files'), 'Production capture directory must remain empty'
 assert result['client_sha256']=='159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37'
 finals=[x for x in stderr.splitlines()if x.startswith('GEARS_ENGINE_FINAL ')]
 assert len(finals)==1
 counts={k:int(v)for k,v in re.findall(r'(\w+)=(\d+)',finals[0])}
 assert all(counts.get(k)==v for k,v in {'frames':120,'GPU_submissions':120,'queued':0,'legacy_submits':0,'error':0,'offline':0}.items())
 assert 'GEARS_BOOTSTRAP_CLOSE error=0 offline=0' in stderr and 'GEARS_PROFILE_BOUND_REACHED swaps=120 signal=0' in stderr
 assert 'CPU_RECORD_ONLY=0'in stderr and 'GL_RENDERER   = reMESA SGX535'in stdout
 banner='GEARS_PROFILE FAST; FAST is not LAB approval'
 assert stderr.splitlines().count(banner)==1,'Expected one exact FAST startup notice'
 allowed=('GEARS_INSTALL ','GEARS_PROFILE_BOUND_REACHED ','GEARS_ENGINE_FINAL ','GEARS_BOOTSTRAP_CLOSE ')
 assert all(not line.strip() or line==banner or line.startswith(allowed)for line in stderr.splitlines()),'Unexpected per-frame diagnostic/error stderr'
 fps=[{'frames':int(n),'seconds':float(s),'FPS':float(f)}for n,s,f in re.findall(r'(\d+)\s+frames in\s+([0-9.]+)\s+seconds\s*=\s*([0-9.]+)\s+FPS',stdout)]
 assert all(x['frames']>0 and x['seconds']>0 and x['FPS']>=0 for x in fps)
 return {'engine_final':counts,'original_glxgears_FPS_samples':fps,'archived_GPU_receipts':0,'archived_raw_frames':0,'capture_empty':True,'evidence_scope':'Runtime aggregate120 GPU submissions and clean close; no per-frame receipt/raw/archive proof. Separate six-operation physical qualifier verifies full COLOR and defined Z24; VOID X8 raw differences remain recorded.'}
