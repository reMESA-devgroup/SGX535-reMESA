from pathlib import Path
import argparse,json,hashlib,re,traceback
from production_glx_contract import validate_production_run
N=Path(__file__).resolve().parent;H=N/'hardware-glx';p=argparse.ArgumentParser();p.add_argument('--actual',type=Path,default=H/'ended');args=p.parse_args();A=args.actual
try:
 result=json.loads((A/'RESULT.json').read_text());stderr=(A/'original.stderr').read_text();stdout=(A/'original.stdout').read_text();assert(A/'capture').is_dir()
 summary=validate_production_run(result,stderr,stdout,list((A/'capture').rglob('*')))
 staged=json.loads((H/'STAGED_NATIVE.json').read_text());assert result['manifest_sha256']==staged['manifest_sha256'] and result['client_sha256']==staged['client_sha256']
 notes={name:record['note_sha256']for name,record in json.loads((N/'INITRD_BUILD.json').read_text())['modules'].items()};assert result['notes']==notes
 assert result['boot']==(H/'OBSERVED_BOOT.txt').read_text().strip()
 assert (A/'capsule-before.bin').read_bytes()==(A/'sgx535_current_operation.raw').read_bytes()
 source=(A/'sgx535_source_guard.raw').read_text();facts=dict(line.split('=',1)for line in source.splitlines()if'='in line)
 assert all(facts.get(k)==v for k,v in {'state':'0','reasons':'0','producers_active':'0','capsule_state':'0','startup_fault':'0','startup_autonomous':'0'}.items())
 assert not re.search(r'REMESA_(?:STREAM_TIMING|STREAM_MEMORY_TIMING|GPU_WALL_TIMING|FROZEN_INPUT_AUDIT)',(A/'dmesg.after').read_text(errors='replace'))
 audit=json.loads((H/'FINAL_REMOTE_STATE.json').read_text());assert audit['boot']==result['boot'] and audit['owned_client_absent'] and audit['owned_supervisor_absent'] and audit['capture_empty'] and all(x=='N'for x in audit['controls'].values()) and audit['status']=='ENDED_PRODUCTION_GLX120_NNN_STOCK_NO_PENDING_GPU_OPERATION'
 assert audit['custom_sha256']=='d1d812f7b6dbfe5244ae94dc1e4686cb797b5fd03e5d0d4aa20ad7e73eecfd18' and audit['grub']=='saved_entry=gnulinux-5.10.240-antix.1-486-smp-advanced-6da9b4a7-ede2-4e27-bbfc-b537f568eaf1\nnext_entry=\n'
 qroot=N.parent/'gears-depth-less-single-clear-20261026T160400Z';qpath=qroot/'hardware/PHYSICAL_SINGLE_CLEAR_RESULTS.json';q=json.loads(qpath.read_text());assert q['status']=='PHYSICAL_SINGLE_CLEAR_COLOR_Z_COCOVERAGE_STABLE_REFERENCE_PASS_NOT_FULL_GL_CONFORMANCE' and q['boot']!=result['boot'] and len(q['frames'])==6
 assert all((x['phase'],x['ledger'],x['proof'])==(9,7,111) and x['COLOR_silhouette_identical_to_ALWAYS'] and x['COLOR_Z_co_coverage_exact_all_pixels'] and x['background_COLOR_RGBA_zero_and_Z24_clear_all_pixels'] and x['Z24_never_farther_on_historical_visible_geometry'] and x['analytical_stable_interior_pixels']>=100000 and x['analytical_visible_gear_class_exact_in_stable_interiors']for x in q['frames'])
 assert q['full_RAW_approval']is False and q['raw_Z_byte_exact']is False
 assert json.loads((qroot/'hardware/replay-ended6/RESULT.json').read_text())['notes']==notes
 expected={f'assets/{n}':{'sha256':hashlib.sha256((qroot/'stream-profile'/n).read_bytes()).hexdigest(),'bytes':(qroot/'stream-profile'/n).stat().st_size}for n in ['stream-profile.bin',*[f'role{i}.bin'for i in range(5)]]}
 assert staged['replaced_files']==expected and staged['whole_manifest_files_verified']==22
 summary['evidence_scope']='Runtime aggregate120 GPU submissions and clean close; no per-frame receipt/raw/archive proof. Separate physically qualified S checks exact historical COLOR silhouette, COLOR/Z co-coverage and clear background over whole image, plus nearest-depth/visible-gear stable interiors. Old wrong Z-footprint gate remains failed/preserved; not full GL shading/precision conformance.'
 report={'status':'PRODUCTION_NOHASH_ORIGINAL_GLX_120_AGGREGATE_FRAMES_PHYSICAL_PASS','boot':result['boot'],'dimensions':[1280,800],'client_sha256':result['client_sha256'],'manifest_sha256':result['manifest_sha256'],'notes':notes,**summary,'separate_single_clear_qualification':{'path':str(qpath),'offline_artifact_sha256':hashlib.sha256(qpath.read_bytes()).hexdigest(),'boot':q['boot'],'operations':6,'full_RAW_approval':False,'raw_Z_byte_exact':False,'old_Z_footprint_failure_preserved':True,'scope':'Whole-image COLOR/Z co-coverage and stable-reference qualification; old raw/oracles unmodified, no full GL/LAB approval'},'LAB_approval':False,'unbounded_continuity_qualified':False,'fps_target_claim':'Only original stdout samples establish FPS; no10/20/30/60 target is inferred from count120.'}
 (H/'PHYSICAL_PRODUCTION_GLX_RESULTS.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
except Exception:
 (H/'PRODUCTION_GLX_VERIFICATION_FAILURE.json').write_text(json.dumps({'status':'FAIL_EVIDENCE_PRESERVED','actual':str(A),'traceback':traceback.format_exc()},indent=2)+'\n');raise
