#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage a pinned Mesa DRI/pipe-loader overlay in a NEW directory; never install.
This registers a fail-closed reMESA descriptor, not OpenGL capability.
Input may be a read-only Mesa source tree or the extracted contract subset.
"""
import argparse,hashlib,json,shutil
from pathlib import Path

def prepare(mesa,repo,output,egl_mesa=None):
    pin=json.loads((repo/'mesa/sgx535/mesa-loader-pin.json').read_text())
    originals={}
    for rel,h in pin['files'].items():
        b=(mesa/rel).read_bytes()
        if hashlib.sha256(b).hexdigest()!=h:raise ValueError('Mesa contract mismatch: '+rel)
        originals[rel]=b.decode()
    if output.exists():raise ValueError('Output must be new; no overwrite')
    texts=dict(originals)
    def replace(rel,old,new):
        if texts[rel].count(old)!=1:raise ValueError('Nonunique integration anchor: '+rel)
        texts[rel]=texts[rel].replace(old,new)
    replace('src/gallium/auxiliary/pipe-loader/pipe_loader_drm.c',
            'static const struct drm_driver_descriptor *driver_descriptors[] = {',
            'static const struct drm_driver_descriptor *driver_descriptors[] = {\n   &gma500_driver_descriptor,')
    replace('src/gallium/auxiliary/target-helpers/drm_helper_public.h',
            'extern const struct drm_driver_descriptor i915_driver_descriptor;',
            'extern const struct drm_driver_descriptor gma500_driver_descriptor;\nextern const struct drm_driver_descriptor i915_driver_descriptor;')
    replace('src/gallium/auxiliary/target-helpers/drm_helper.h',
            '/* kmsro should be the last entry in the file. */',
            '#ifdef GALLIUM_SGX535\n#include "sgx535/remesa_dri.h"\n#else\nDRM_DRIVER_DESCRIPTOR_STUB(gma500)\n#endif\n\n/* kmsro should be the last entry in the file. */')
    replace('src/gallium/meson.build',
            "if with_gallium_softpipe\n  subdir('drivers/softpipe')",
            "libremesa_loader = []\nif with_gallium_sgx535\n  subdir('drivers/sgx535')\nendif\nif with_gallium_softpipe\n  subdir('drivers/softpipe')")
    replace('src/gallium/targets/dri/meson.build',
            "  files('dri_target.c'),", "  files('dri_target.c'),\n  c_args : with_gallium_sgx535 ? ['-DGALLIUM_SGX535'] : [],")
    replace('src/gallium/targets/dri/meson.build',
            'link_whole : [libdri, gallium_dri_link_whole]',
            'link_whole : [libdri, libremesa_loader, gallium_dri_link_whole]')
    replace('meson.options',
            "'softpipe', 'svga', 'tegra', 'v3d', 'vc4', 'virgl', 'zink',",
            "'softpipe', 'svga', 'tegra', 'v3d', 'vc4', 'virgl', 'zink', 'sgx535',")
    replace('meson.build',
            "with_gallium_asahi = gallium_drivers.contains('asahi')",
            "with_gallium_asahi = gallium_drivers.contains('asahi')\nwith_gallium_sgx535 = gallium_drivers.contains('sgx535')")
    # DRIL uses GBM, not Xlib; build the compatibility artifact for the SGX535
    # DRM-only configuration too. This does not expose an OpenGL context.
    replace('src/meson.build',
            "if with_glx == 'dri' or with_platform_x11 or with_platform_xcb",
            "if with_glx == 'dri' or with_platform_x11 or with_platform_xcb or with_gallium_sgx535")
    replace('src/gallium/targets/dril/dril_target.c',
            'DEFINE_LOADER_DRM_ENTRYPOINT(i915)',
            'DEFINE_LOADER_DRM_ENTRYPOINT(gma500)\nDEFINE_LOADER_DRM_ENTRYPOINT(i915)')
    replace('src/gallium/targets/dril/meson.build',
            "[with_gallium_i915, 'i915_dri.so'],",
            "[with_gallium_i915, 'i915_dri.so'],\n             [with_gallium_sgx535, 'gma500_dri.so'],")
    if egl_mesa is not None:
        from prepare_egl_primary import PIN, render
        primary_texts = render(egl_mesa)
        if texts.keys() & primary_texts.keys():
            raise ValueError('Conflicting Mesa loader/EGL overlay ownership')
        texts.update(primary_texts)
    output.mkdir(parents=True)
    for rel,text in texts.items():
        p=output/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    dst=output/'src/gallium/drivers/sgx535';dst.mkdir(parents=True)
    sources=['remesa_retained_transport.c','remesa_retained_transport.h','remesa_persistent.c','remesa_persistent.h','remesa_linux_memory.c','remesa_linux_memory.h','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_render_pass.h','remesa_sampler_state.h','remesa_texture_binding.h','remesa_texture_descriptor.h','remesa_fixed_state.c','remesa_fixed_state.h','remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c','remesa_nir.h','remesa_shader_ir.h','remesa_usse.h','remesa_pds.h','remesa_pds_structure.h','remesa_shader_program.h','remesa_fragment_body.h','remesa_shader_storage.h','remesa_pds_candidate.h','remesa_program_registers.h','remesa_nir_uniform.h','remesa_resource_ref.h','remesa_resource_layout.h','remesa_screen.c','remesa_screen.h','remesa_draw.c','remesa_draw.h','remesa_linux.c','remesa_linux.h','remesa_device.h','remesa_loader.c','remesa_loader.h','remesa_dri.c','remesa_dri.h']
    sources += ['remesa_present.c', 'remesa_present.h','remesa_render_target.c','remesa_render_target.h','remesa_ta_index.c','remesa_ta_index.h','remesa_ta_scene.c','remesa_ta_scene.h','remesa_usse_arithmetic.c','remesa_usse_arithmetic.h','remesa_usse_fragment_color.c','remesa_usse_fragment_color.h','remesa_ta_scalar.c','remesa_ta_scalar.h','remesa_ta_zclear.c','remesa_ta_zclear.h','remesa_depth_scene_image.c','remesa_depth_scene_image.h']
    sources += ['remesa_twod.c', 'remesa_twod.h']
    for n in sources:shutil.copyfile(repo/'mesa/sgx535'/n,dst/n)
    shutil.copyfile(repo/'common/remesa/draw_packet.c',dst/'draw_packet.c')
    shutil.copyfile(repo/'common/remesa/shader_request.c',dst/'shader_request.c')
    shutil.copyfile(repo/'common/remesa/retained_evidence.c',dst/'retained_evidence.c')
    for n in ['registry.c','resource.c','provider.c']:
        shutil.copyfile(repo/'kernel/remesa'/n,dst/n)
    inc=dst/'include/remesa';inc.mkdir(parents=True)
    for n in ['scene_dpm.h','scene_layout.h','native_uapi.h','draw_packet.h','shader_uapi.h','shader_request.h','memory_uapi.h','twod_copy_uapi.h','retained_uapi.h','retained_evidence.h','provider.h','registry.h','resource.h']:shutil.copyfile(repo/'include/remesa'/n,inc/n)
    shim=dst/'include/drm';shim.mkdir();(shim/'drm.h').write_text('/* SPDX-License-Identifier: MIT */\n#include <drm.h>\n')
    (dst/'meson.build').write_text("""# SPDX-License-Identifier: MIT
# Isolated experimental integration; DRI constructor currently rejects GL.
libremesa_loader = static_library('remesa_loader',
  files('remesa_retained_transport.c','retained_evidence.c','remesa_persistent.c','remesa_linux_memory.c','registry.c','resource.c','provider.c','remesa_render_pass.c','remesa_sampler_state.c','remesa_texture_binding.c','remesa_texture_descriptor.c','remesa_fixed_state.c','remesa_nir.c','remesa_shader_ir.c','remesa_usse.c','remesa_pds.c','remesa_pds_structure.c','remesa_shader_program.c','remesa_fragment_body.c','remesa_shader_storage.c','remesa_program_registers.c','remesa_nir_uniform.c','remesa_screen.c', 'remesa_draw.c', 'remesa_linux.c',
        'remesa_loader.c', 'remesa_dri.c', 'remesa_present.c', 'remesa_render_target.c', 'remesa_ta_index.c', 'remesa_ta_scene.c', 'remesa_usse_arithmetic.c', 'remesa_usse_fragment_color.c', 'remesa_ta_scalar.c', 'remesa_ta_zclear.c', 'remesa_depth_scene_image.c', 'remesa_twod.c', 'draw_packet.c', 'shader_request.c'),
  include_directories : [inc_include, inc_src, inc_gallium, inc_nir, inc_compiler,
                        include_directories('include')],
  dependencies : [dep_libdrm],
  c_args : ['-DREMESA_WITH_MESA_HEADERS', '-DREMESA_WITH_NIR'],
  install : false,
)
""")
    manifest={'mesa_version':pin['version'],'scope':'Isolated source overlay only; no installation or hardware execution; OpenGL admission remains closed',
              'inputs':pin['files'],'files':{str(p.relative_to(output)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.rglob('*')) if p.is_file()}}
    (output/'integration.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if egl_mesa is not None:
        # Keep the separately pinned EGL contract usable by the actual EGL
        # builder, while the combined manifest continues to bind all sources.
        egl_manifest={'mesa':'25.2.0','originals':PIN,
                      'scope':'Pinned EGL component of combined Mesa overlay',
                      'files':{rel:hashlib.sha256((output/rel).read_bytes()).hexdigest()
                               for rel in primary_texts}}
        (output/'egl-primary-integration.json').write_text(json.dumps(egl_manifest,indent=2)+'\n')
    return manifest
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--mesa',type=Path,required=True);p.add_argument('--repo',type=Path,default=Path(__file__).resolve().parents[2]);p.add_argument('--output',type=Path,required=True);p.add_argument('--egl-mesa',type=Path,help='Pinned full Mesa tree for strict primary-only EGL integration')
    a=p.parse_args();m=prepare(a.mesa,a.repo,a.output,a.egl_mesa);print('PASS: pinned isolated overlay, '+str(len(m['files']))+' files; OpenGL unavailable')
