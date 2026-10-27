#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate shader callbacks, lifetime and bounded original FS bytes; no submissions."""
import json,os,subprocess,argparse,shlex,hashlib,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--cache',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--test',choices=['test_psb_event_data.c','test_displaylist_storage.c','test_nir_selected_pipeline.c','test_nir_fragment_program.c','test_nir_white_lambert.c','test_pds_event_data.c','test_fragment_color_input.c','test_usse_position_color.c','test_lab_dpm.c','test_ta_compiled_vertex.c','test_usse_normalize_vertex.c','test_pds_compiled_vertex.c','test_usse_normalize_scale.c','test_usse_cpu_arithmetic.c','test_usse_lighting_body.c','test_usse_components.c','test_usse_max.c','test_usse_coefficient.c','test_lab_control_stage.c','test_ta_control.c','test_lab_raster_control.c','test_raster_control.c','test_lab_scene_storage.c','test_va_placement.c','test_background_object.c','test_background_geometry.c','test_large_depth_resources.c','test_packed_depth_owner.c','test_depth_scene_image.c','test_owned_depth_scene.c','test_pds_parameter_image.c','test_ta_draw_state.c','test_depth_prefix.c','test_ta_zclear.c','test_ta_scalar.c','test_ta_window_viewport.c','test_usse_arithmetic.c','test_ta_fragment_binding.c','test_pds_vertex_image.c','test_ta_scene_close.c','test_ta_index.c','test_pds_state_image.c','test_ta_state_reference.c','test_isp_state_data.c','test_isp_color_control.c','test_depth_clear_geometry.c','test_render_target.c','test_persistent.c','test_rect_renderbuffer.c','test_supplied_shader_staging.c','test_pds_task_output_encode.c','test_pds_r_allocator.c','test_pds_class64_sources.c','test_pds_structure_pipeline.c','test_pds_pixel_pipeline.c','test_pds_pixel_task.c','test_shader_execution.c','test_pds_contract.c','test_usse_export_contract.c','test_host_shader_pipeline.c','test_pds_host_builder.c','test_shader_compile.c','test_context_upload.c','test_fragment_encode.c','test_fragment_body.c','test_nir_builder.c','test_shader_program.c','test_native_shader.c','test_shader_submit.c','test_uniform_pipeline.c','test_clip_pipeline.c','test_driver_nir.c','test_persistent_shader.c','test_linked_program_owner.c','test_linked_geometry_pds_owner.c','test_linked_draw_state.c','test_linked_vertex_inputs.c','test_linked_body.c','test_zclear_prefix.c','test_linked_ta.c','test_linked_targets.c','test_linked_background.c','test_shader_memory_transport.c','test_pds_data_backing.c','test_fragment_constant.c','test_retained_fragment.c'],default='test_shader_program.c');p.add_argument('--cases',help='Comma-separated retained fragment cases for focused regression');p.add_argument('--archive-only',action='store_true',help='Run only the newly built Mesa archive test; avoid unchanged source recompilations');p.add_argument('--source-only',action='store_true',help='Run native/UBSan/i386 source tests without requiring the separately built Mesa archive');a=p.parse_args()
if a.source_only and a.archive_only:p.error('--source-only conflicts with --archive-only')
if a.test in ('test_nir_builder.c','test_nir_white_lambert.c','test_nir_fragment_program.c','test_nir_selected_pipeline.c') and not a.archive_only:p.error('The NIR builder test requires actual Mesa libraries; use --archive-only')
repo=Path(__file__).resolve().parents[3];m=a.cache;s=m/'mesa-25.2.0';b=m/'build-i386';a.output.mkdir(parents=True,exist_ok=True)
env=dict(os.environ);env.update(json.loads((m/'build-environment.json').read_text()))
base=['-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-std=c11','-D_GNU_SOURCE','-DHAVE_ENDIAN_H','-DHAVE_SECURE_GETENV','-DHAVE_PTHREAD','-DHAVE_STRUCT_TIMESPEC','-DREMESA_WITH_MESA_HEADERS','-DREMESA_WITH_NIR']
base+=['-I'+str(x) for x in [repo/'include',repo/'mesa/sgx535',s/'include',s/'src',s/'src/gallium/include',s/'src/gallium/auxiliary',s/'src/compiler/nir',b/'src/compiler/nir',b/'src/compiler',b/'src']]
sources=[repo/'mesa/sgx535'/x for x in ['remesa_depth_scene_image.c','remesa_ta_zclear.c','remesa_ta_scalar.c','remesa_usse_arithmetic.c','remesa_ta_scene.c','remesa_ta_index.c','remesa_render_target.c','remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_usse_fragment_color.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c','remesa_fixed_state.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_screen.c','remesa_draw.c','remesa_retained_transport.c','remesa_linux.c','remesa_persistent.c','remesa_linux_memory.c']]+[repo/'kernel/remesa'/x for x in ['registry.c','resource.c','provider.c']]+[repo/'common/remesa/draw_packet.c',repo/'common/remesa/retained_evidence.c',repo/'common/remesa/shader_request.c',b/'src/compiler/nir/nir_intrinsics.c',s/'src/util/ralloc.c',s/'src/util/u_printf.c',s/'src/gallium/auxiliary/util/u_upload_mgr.c']
q='/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c','test_lab_scene_storage.c'):sources.append(repo/'tools/remesa/lab_scene_storage.c')
if a.test=='test_rect_renderbuffer.c':sources.append(repo/'mesa/sgx535/remesa_present.c')
if a.test in ('test_usse_normalize_scale.c','test_usse_cpu_arithmetic.c','test_usse_lighting_body.c','test_usse_components.c','test_usse_max.c','test_usse_coefficient.c'):sources=[repo/'mesa/sgx535/remesa_usse_arithmetic.c']
if a.test in ('test_pds_event_data.c','test_psb_event_data.c'):sources=[repo/'mesa/sgx535/remesa_pds_structure.c']
if a.test=='test_fragment_color_input.c':sources=[repo/'mesa/sgx535'/f for f in ['remesa_usse.c','remesa_pds_structure.c','remesa_ta_scene.c','remesa_render_target.c','remesa_ta_scalar.c']]
if a.test in ('test_usse_position_color.c','test_usse_normalize_vertex.c'):sources=[repo/'mesa/sgx535/remesa_usse_arithmetic.c',repo/'mesa/sgx535/remesa_usse.c']
if a.test=='test_ta_compiled_vertex.c':sources=[repo/'mesa/sgx535/remesa_ta_index.c',repo/'mesa/sgx535/remesa_render_target.c']
if a.test=='test_pds_compiled_vertex.c':sources=[repo/'mesa/sgx535/remesa_pds_structure.c']
if a.test in ('test_ta_control.c','test_raster_control.c'):sources=[repo/'mesa/sgx535/remesa_render_target.c']
results={}
wrapper_names={'test_nir_selected_pipeline.c':['calloc','malloc','free'],'test_pds_structure_pipeline.c':['calloc','malloc','free'],'test_pds_pixel_pipeline.c':['calloc','malloc','free'],'test_shader_execution.c':['calloc','malloc','free'],'test_host_shader_pipeline.c':['calloc','malloc','free'],'test_fragment_encode.c':['calloc'],'test_context_upload.c':['u_upload_create','u_upload_destroy'],'test_shader_program.c':['calloc'],'test_native_shader.c':['calloc','malloc','free'],'test_uniform_pipeline.c':['malloc'],'test_shader_memory_transport.c':['ioctl','open','open64','close'],'test_retained_fragment.c':['ioctl','open','open64','close'],'test_pds_data_backing.c':['ioctl','open','open64','close','malloc','calloc','free']}
wrap=['-Wl,--wrap='+n for n in wrapper_names.get(a.test,[])]
cases=['good','split','entry-corrupt','alloc','budget','create','pin','lease','initial-corrupt','alias','write','unlease','corrupt'] if a.test=='test_pds_data_backing.c' else [None]
carrier_data=sys.stdin.buffer.read() if a.test in ('test_psb_event_data.c','test_pds_event_data.c','test_fragment_color_input.c','test_usse_position_color.c','test_lab_dpm.c','test_ta_compiled_vertex.c','test_usse_normalize_vertex.c','test_pds_compiled_vertex.c','test_usse_normalize_scale.c','test_usse_cpu_arithmetic.c','test_usse_lighting_body.c','test_usse_components.c','test_usse_max.c','test_usse_coefficient.c','test_ta_control.c','test_lab_raster_control.c','test_raster_control.c','test_background_object.c','test_background_geometry.c','test_retained_fragment.c','test_pds_state_image.c','test_pds_vertex_image.c','test_ta_scalar.c','test_ta_zclear.c','test_depth_prefix.c','test_pds_parameter_image.c') else None
if a.test=='test_retained_fragment.c' and len(carrier_data)!=180224:raise SystemExit('Expected exact six-role opaque carrier input on stdin')
if a.test=='test_pds_state_image.c' and len(carrier_data)!=180:raise SystemExit('Expected three60byte state-upload carrier objects on stdin')
if a.test=='test_pds_vertex_image.c' and len(carrier_data)!=128:raise SystemExit('Expected two64byte primary vertex carrier objects on stdin')
if a.test=='test_ta_scalar.c' and len(carrier_data)!=64:raise SystemExit('Expected four16byte scalar group oracles on stdin')
if a.test=='test_ta_zclear.c' and len(carrier_data)!=456:raise SystemExit('Expected six76byte original Z-clear geometry oracles on stdin')
if a.test=='test_depth_prefix.c' and len(carrier_data)!=640:raise SystemExit('Expected ten64byte original depth prefix oracles on stdin')
if a.test=='test_pds_parameter_image.c' and len(carrier_data)!=180:raise SystemExit('Expected three60byte original parameter PDS objects on stdin')
if a.test=='test_retained_fragment.c':cases=['gl-quarter','gl-quarter-wrong-geometry','arrays-candidate','arrays-preimage','arrays-preserve','vertex-candidate','vertex-preimage','upload-green','upload-index','upload-position','good','capture','green-candidate','green-preimage','capture-post-source','capture-post-info','unchanged','capture-inventory','disabled','late','green','shape','source-corrupt','write-fail','info-stale','ioctl-fail','auth-fail','color-mismatch']
if a.cases:
 if a.test!='test_retained_fragment.c':raise SystemExit('--cases requires the retained-fragment test')
 selected=a.cases.split(',')
 if len(set(selected))!=len(selected) or any(c not in cases for c in selected):raise SystemExit('Unknown or duplicate retained case')
 cases=selected
if a.test=='test_retained_fragment.c':base=[('-isystem'+x[2:] if x.startswith('-I'+str(m)) else x) for x in base]
carrier_input=carrier_data.decode('latin1') if carrier_data is not None else None
variants=[] if a.archive_only else [('native',['cc'],['-O2'],[]),('ubsan',['cc'],['-O1','-fsanitize=undefined','-fno-sanitize-recover=all'],[]),('i386',[str(m/'wrappers/cc')],['-O2','-march=i486','-static'],[q])]
for name,cc,flags,run in variants:
 exe=a.output/name;cmd=cc+base+flags+list(map(str,sources))+[str(repo/'mesa/sgx535/tests'/a.test)]+wrap+['-o',str(exe)]
 r=subprocess.run(cmd,env=env,text=True,capture_output=True);(a.output/(name+'-compile.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(name+' compilation failed')
 runs=[]
 for case in cases:
  runner=run+[str(exe)]+([case] if case else [])
  r=subprocess.run(runner,env=env,text=True,capture_output=True,timeout=60,encoding='latin1',input=carrier_input)
  (a.output/(name+('-'+case if case else '')+'-run.log')).write_text(r.stdout+r.stderr)
  if r.returncode:raise SystemExit(name+' execution failed: '+str(case))
  runs.append({'case':case,'command':runner,'exit':r.returncode,'output':r.stdout.strip()})
 results[name]={'pass':True,'command':cmd,'output':'\n'.join(x['output'] for x in runs),'runs':runs}
 (a.output/'result.partial.json').write_text(json.dumps(results,indent=2)+'\n')
if a.source_only:
 (a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2));raise SystemExit(0)
# Exercise the actual Mesa-built driver objects too. The preceding variants
# intentionally compile sources for native/UBSan coverage; they do not alone
# prove that the Meson archive contains the same callback implementation.
record=next(x for x in json.loads((b/'compile_commands.json').read_text()) if x['file'].endswith('/remesa_nir.c'))
args=record.get('arguments') or shlex.split(record['command']);flags=[];skip=False
for arg in args:
 if skip:skip=False;continue
 if arg in ('-o','-MF','-MQ'):skip=True;continue
 if arg in ('-c','-MD','-DNDEBUG') or arg==record['file']:continue
 flags.append(arg)
flags.insert(1,'-I'+str(repo/'mesa/sgx535'));flags+=['-UNDEBUG','-DREMESA_WITH_MESA_HEADERS','-DREMESA_WITH_NIR']
archive=b/'src/gallium/drivers/sgx535/libremesa_loader.a'
if not archive.is_file():raise SystemExit('Actual Mesa driver archive missing')
# Do not silently test stale cache sources or rebuild/reselect an artifact.
# Match the staged sources to this checkout, then require Ninja to consider
# the actual driver archive current. Its digest is bound into the receipt.
driver=s/'src/gallium/drivers/sgx535';archive_inputs={}
for path in sorted(driver.rglob('*')):
 if not path.is_file() or path.suffix not in ('.c','.h'):continue
 relative=path.relative_to(driver)
 if str(relative) in ('draw_packet.c','shader_request.c','retained_evidence.c'):original=repo/'common/remesa'/relative.name
 elif str(relative) in ('registry.c','resource.c','provider.c'):original=repo/'kernel/remesa'/relative.name
 elif relative.parts[:2]==('include','remesa'):original=repo/'include/remesa'/relative.name
 elif str(relative)=='include/drm/drm.h':
  if path.read_text()!='/* SPDX-License-Identifier: MIT */\n#include <drm.h>\n':raise SystemExit('Actual Mesa DRM shim mismatch')
  archive_inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest();continue
 else:original=repo/'mesa/sgx535'/relative
 if not original.is_file() or path.read_bytes()!=original.read_bytes():raise SystemExit('Actual Mesa archive source mismatch: '+str(relative))
 archive_inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
freshness=['ninja','-C',str(b),'-n','src/gallium/drivers/sgx535/libremesa_loader.a']
r=subprocess.run(freshness,env=env,text=True,capture_output=True);(a.output/'archive-freshness.log').write_text(r.stdout+r.stderr)
if r.returncode or 'no work to do' not in r.stdout:raise SystemExit('Actual Mesa archive is stale; qualify a rebuilt artifact first')
archive_identity=hashlib.sha256(archive.read_bytes()).hexdigest()
libraries=['src/gallium/auxiliary/libgallium.a','src/compiler/nir/libnir.a','src/compiler/libcompiler.a','src/util/libmesa_util.a','src/util/libmesa_util_simd.a','src/util/blake3/libblake3.a','src/util/format/libmesa_format.a','src/c11/impl/libmesa_util_c11.a']
exe=a.output/'actual-archive-i386'
# The release Mesa archive omits the debug NIR validator. Compile the pinned
# upstream validator with assertions for builder fixtures; do not replace or
# weaken nir_validate_shader, and do not rebuild the driver with test sources.
test_sources=[repo/'mesa/sgx535/tests'/a.test]
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c','test_lab_scene_storage.c'):test_sources.append(repo/'tools/remesa/lab_scene_storage.c')
if a.test in ('test_nir_builder.c','test_nir_white_lambert.c','test_nir_fragment_program.c','test_nir_selected_pipeline.c'):test_sources.append(s/'src/compiler/nir/nir_validate.c')
test_inputs={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in test_sources}
if a.test in ('test_linked_program_owner.c','test_linked_geometry_pds_owner.c','test_linked_draw_state.c','test_linked_vertex_inputs.c','test_linked_body.c','test_zclear_prefix.c','test_linked_ta.c','test_linked_targets.c','test_linked_background.c'):
 fixture=repo/'mesa/sgx535/tests/test_shader_linkage.c'
 test_inputs[str(fixture)]=hashlib.sha256(fixture.read_bytes()).hexdigest()
if a.test=='test_linked_body.c':
 fixture=repo/'mesa/sgx535/tests/test_linked_draw_state.c'
 test_inputs[str(fixture)]=hashlib.sha256(fixture.read_bytes()).hexdigest()
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c'):
 path=repo/'mesa/sgx535/tests/test_lab_scene_storage.c'
 test_inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c','test_lab_scene_storage.c'):
 path=repo/'tools/remesa/lab_scene_storage.h'
 test_inputs[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c','test_lab_scene_storage.c','test_va_placement.c','test_owned_depth_scene.c','test_packed_depth_owner.c','test_large_depth_resources.c','test_linked_program_owner.c','test_linked_geometry_pds_owner.c','test_linked_draw_state.c','test_linked_vertex_inputs.c','test_linked_body.c','test_zclear_prefix.c','test_linked_ta.c','test_linked_targets.c','test_linked_background.c'):
 fixture=repo/'mesa/sgx535/tests/test_persistent_shader.c'
 test_inputs[str(fixture)]=hashlib.sha256(fixture.read_bytes()).hexdigest()
if a.test in ('test_lab_dpm.c','test_lab_control_stage.c','test_lab_raster_control.c','test_lab_scene_storage.c','test_va_placement.c'):
 fixture=repo/'mesa/sgx535/tests/test_large_depth_resources.c'
 test_inputs[str(fixture)]=hashlib.sha256(fixture.read_bytes()).hexdigest()
cmd=flags+list(map(str,test_sources))+wrap+['-Wl,--start-group',str(archive)]+[str(b/n) for n in libraries if (b/n).exists()]+['-Wl,--end-group','-L'+str(m/'i386-deps/usr/lib/i386-linux-gnu'),'-ldrm','-lm','-ldl','-latomic','-pthread',str(m/'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),'-o',str(exe)]
r=subprocess.run(cmd,cwd=b,env=env,text=True,capture_output=True);(a.output/'actual-archive-compile.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit('Actual Mesa archive callback link failed')
runner=[q,'-L',str(m/'runtime'),str(exe)]
runs=[]
for case in cases:
 case_runner=runner+([case] if case else [])
 r=subprocess.run(case_runner,env=env,text=True,capture_output=True,timeout=60,encoding='latin1',input=carrier_input)
 (a.output/('actual-archive'+('-'+case if case else '')+'-run.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit('Actual Mesa archive callback execution failed: '+str(case))
 runs.append({'case':case,'command':case_runner,'exit':r.returncode,'output':r.stdout.strip()})
if hashlib.sha256(archive.read_bytes()).hexdigest()!=archive_identity:raise SystemExit('Actual Mesa archive changed during qualification')
if any(hashlib.sha256(Path(path).read_bytes()).hexdigest()!=value for path,value in archive_inputs.items()):raise SystemExit('Actual Mesa staged inputs changed during qualification')
if any(hashlib.sha256(Path(path).read_bytes()).hexdigest()!=value for path,value in test_inputs.items()):raise SystemExit('Test/validator sources changed during qualification')
results['actual-archive-i386']={'pass':True,'command':cmd,'runner':runner,'output':'\n'.join(x['output'] for x in runs),'runs':runs,'archive_sha256':archive_identity,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'archive_inputs':archive_inputs,'test_inputs':test_inputs,'freshness_command':freshness,'scope':'Actual Meson-built driver archive and Mesa NIR libraries; offline candidate storage/callback fixtures, no GPU submission'}
if carrier_data is not None:results['carrier_input_sha256']=hashlib.sha256(carrier_data).hexdigest()
(a.output/'result.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(results,indent=2))
