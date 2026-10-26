from pathlib import Path
import json,hashlib,subprocess,difflib
D=Path(__file__).resolve().parent;O=D.parent;X=O/'gears-x11-batch-production-20261026T003000Z';Y=O/'gears-x11-bulk-clear-combined-20261026T030000Z';R=O/'gears-readback-target-production-20261026T003000Z';B=O/'gears-host-clear-bulk-20261026T013000Z';M=O/'phase9-real-mesa-20261008';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
r=json.loads((D/'RESULTS.json').read_text());assert all(row['exit']==0 for row in r['commands']) and len(r['commands'])==8
old=json.loads((Y/'COMPOSITION_BUILD_RESULTS.json').read_text());cmd=list(next(row['command']for row in r['commands']if row['label']=='frontend-link'));expected=[str(D/'remesa_xcb.o')if x==str(X/'remesa_xcb.o')else x for x in old['command']];expected[expected.index('-o')+1]=str(D/'libGL-gears-runtime.so.1');assert cmd==expected
assert sha(D/'libGL-gears-runtime.so.1')==r['libGL_sha256']
for p,h in old['inputs_unchanged'].items():assert sha(p)==h,p
assert sha(R/'gears-bootstrap.so')=='fefe373e29dbc3250d3f60a7cd97eb29a6e98c980f73a5c9a28dd170f3ca7c45'
pinned=json.loads((O/'handoff-resume-20261024T071200Z/PROTECTED.json').read_text())['expected_sha256']
for p,h in pinned.items():assert sha(p)==h,p
r.update(source_base=str(X),link_composition_base=str(Y),bulk_archive=str(B/'libremesa_loader-bulk.a'),normal_FAST_synchronous_barriers_before=5,normal_FAST_synchronous_barriers_after=3,physical_gain_demonstrated=False,physical_deployment=False,geometry_shaders_kernel_resources_unchanged=True,only_link_object_replaced='remesa_xcb.o')
(D/'RESULTS.json').write_text(json.dumps(r,indent=2)+'\n')
(D/'remesa_xcb.c.patch').write_text(''.join(difflib.unified_diff((X/'remesa_xcb.c').read_text().splitlines(True),(D/'remesa_xcb.c').read_text().splitlines(True),fromfile=str(X/'remesa_xcb.c'),tofile=str(D/'remesa_xcb.c'))))
for mode in ['native','ubsan','i386']:
 text=(D/f'test-{mode}-run.log').read_text();assert 'MOCK CPU/GPU0' in text and 'paired-query errors drained' in text
files=[p for p in D.iterdir()if p.is_file()and p.name!='DELIVERY_MANIFEST.json'];out={'status':'SEALED_OFFLINE_ONLY_PHYSICAL_FPS_UNPROVEN','GPU_operations':0,'X_server_operations':0,'normal_synchronous_barriers':[5,3],'runtime_replace_only':str(D/'libGL-gears-runtime.so.1'),'required_existing_bootstrap':str(R/'gears-bootstrap.so'),'baseline':str(Y),'baseline_inputs_verified_unchanged':old['inputs_unchanged'],'protected_previous_files_verified_unchanged':len(pinned),'tests':['native','UBSan','i386/qemu'],'negative_scope':['all16 image error positions and multiple errors','Geometry,Attributes,both errors and NULL replies consumed','image+FreeGC simultaneous error priority','zero image cookie partial queues','zero FreeGC cookie no retry','connection failure beforeFreeGC and duringimagechecks','sequencewrap insideimages and exactlyFreeGC','cap64/fallback65','LAB serial','held resource untilallchecks','one reused stagingallocation120frames'],'files':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)}for p in sorted(files)}}
(D/'DELIVERY_MANIFEST.json').write_text(json.dumps(out,indent=2)+'\n');print('SEALED',r['libGL_sha256'],'onlypublisherobject; tests3modes; physicalgainUNPROVEN')
