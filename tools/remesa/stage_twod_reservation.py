#!/usr/bin/env python3
"""Stage the bounded 2D producer-reservation patch; never install or overwrite.

The input is the retained qualified native source, not the dirty legacy checkout.
This integrates producer/IRQ exclusion; it does not register a GPU executor.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

PINNED = {
    'accel_2d.c': '2de5bef67e6813fa2f238a1792fab42bd5dbf8942273ba5a462bc24e3e7a5b7e',
    'psb_drv.h': '6dce38176eccc8d7037783ec6c41d101c3c614068a3b7b11f40b06e1d1ca6261',
    'gma500_fixed_backend.c': '12d908c2b99c6f89eff92b9b1c69b7d0779ca7c430bfc14347c5cc67e476afd7',
    'psb_reg.h': 'a340d4fb7bb9df2403eb196639d930b8c56463ddc7a3754d152e6ed7ab1daef7',
    'psb_irq.c': '5b9962aa29ec24f844297bb2d0e4fa9b00fc3021ed46d00afbeaa26b326b3025',
    'mmu.c': 'cc722d7566ea3b2fa77252d96bc6eaff7166bd4331592f59d5d3bbc290c1a596',
    'mmu.h': '2f5a6b3edde79741234fa08fbc0b1e14045e42ae472a6e90ea6cb64f1c8b9514',
    'remesa_gem_memory.c': '0569fb6831a33ccef68fa6ba20995688ae2891a6bf22486ec9d3adb9573216c6',
    'remesa_gem_memory.h': 'c205d6a54962aab65f9f1cfa60a38c48eb04eb2f3c46f15bc5f70d25b7c3461b',
    'psb_drv.c': 'e9ce11abe31c5adf0d0134e0d4b520455661a0a7b490bb2ed9b02e1ac62488da',
    'Makefile': '6ab5c8c5819106db5894f2c0f81df55389bf2d3f3a45a0efa4cf0f8aabdbfd9e',
}


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError('native integration anchor is missing or ambiguous')
    return source.replace(old, new, 1)


def patched_sources(native, lab=False, vendor=False, copy=False, fill=False, private=False):
    if fill and not copy:
        raise ValueError("native fill requires the separate normal copy profile")
    if copy and (lab or vendor):
        raise ValueError('normal copy and LAB profiles are separate builds')
    if vendor and not lab:
        raise ValueError('vendor stream requires the isolated LAB ioctl build')
    for name, digest in PINNED.items():
        if hashlib.sha256((native / name).read_bytes()).hexdigest() != digest:
            raise ValueError('unreviewed native input: ' + name)
    header = replace_once((native / 'psb_drv.h').read_text(),
        '\tuint8_t panel_type;\n};',
        '\tuint8_t panel_type;\n'
        '\t/* Appended: existing private-field offsets are unchanged. */\n'
        '\tstruct remesa_twod_reservation *remesa_twod_owner;\n};')
    accel = replace_once((native / 'accel_2d.c').read_text(),
        '#include "gma500_source_snapshot.h"\n',
        '#include "gma500_source_snapshot.h"\n'
        '#include "remesa_twod_reservation.inc"\n'
        '#include "remesa_twod_native.inc"\n')
    accel = replace_once(accel,
        '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n\twhile (size > 0) {',
        '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n'
        '\t/* Native 2D owns the request base until safe retirement. */\n'
        '\tif (dev_priv->remesa_twod_owner) {\n'
        '\t\tret = -EBUSY;\n\t\tgoto reserved_out;\n\t}\n'
        '\twhile (size > 0) {')
    accel = replace_once(accel,
        '\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n'
        '\tremesa_route_source_producer(0);',
        'reserved_out:\n\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n'
        '\tremesa_route_source_producer(0);')
    # The original console caller ignores submission errors. EBUSY here is
    # guaranteed before any native command write, so retain its existing CPU
    # console behavior. Never replay an uncertain/partially emitted EIO blit.
    accel = replace_once(accel, '\tuint32_t dst_format;\n',
                         '\tuint32_t dst_format;\n\tint ret;\n')
    accel = replace_once(accel, '\tpsb_accel_2d_copy(dev_priv,\n',
                         '\tret = psb_accel_2d_copy(dev_priv,\n')
    accel = replace_once(accel,
        '\t\t\t  a->sx, a->sy, a->dx, a->dy, a->width, a->height);\n'
        '\tgma_power_end(dev);',
        '\t\t\t  a->sx, a->sy, a->dx, a->dy, a->width, a->height);\n'
        '\tif (ret == -EBUSY)\n\t\tdrm_fb_helper_cfb_copyarea(info, a);\n'
        '\tgma_power_end(dev);')
    fixed = replace_once((native / 'gma500_fixed_backend.c').read_text(),
        'static struct sgx535_gma500_fixed_backend *fixed_irq_owner;\n',
        'static struct sgx535_gma500_fixed_backend *fixed_irq_owner;\n'
        '#include "remesa_twod_irq.inc"\n')
    fixed = replace_once(fixed,
        '    remesa_route_source_pending(dev, status1, status2);\n',
        '    remesa_route_source_pending(dev, status1, status2);\n'
        '    remesa_twod_irq_capture_locked(dev, status1, status2);\n')
    fixed = replace_once(fixed,
        'if (fixed_irq_owner || !remesa_route_source_begin(backend, owner->dev))',
        'if (fixed_irq_owner || remesa_twod_irq_busy_locked() ||\n'
        '        !remesa_route_source_begin(backend, owner->dev))')
    fixed = replace_once(fixed, '    if (fixed_irq_owner) {\n',
        '    if (fixed_irq_owner || remesa_twod_irq_busy_locked()) {\n')
    mmu = (native / 'mmu.c').read_text() + '\n#include "remesa_mmu_verify_access.inc"\n#include "remesa_mmu_default_root.inc"\n'
    gem = (native / 'remesa_gem_memory.c').read_text() + '\n#include "remesa_twod.c"\n#include "remesa_gem_twod.inc"\n#include "remesa_gem_twod_job.inc"\n#ifdef REMESA_TWOD_LAB_IOCTL\n#include "remesa_twod_lab.inc"\n#endif\n#ifdef REMESA_TWOD_COPY_IOCTL\n#include "remesa_twod_copy.inc"\n#endif\n'
    driver = replace_once((native / 'psb_drv.c').read_text(),
        '#include "remesa_retained_uapi.h"\n',
        '#include "remesa_retained_uapi.h"\n#include "remesa_twod_lab.h"\n#include "remesa_twod_lab_uapi.h"\n#include "remesa_twod_copy.h"\n#include "remesa_twod_copy_uapi.h"\n')
    driver = replace_once(driver, 'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
        'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
        '#ifdef REMESA_TWOD_LAB_IOCTL\n'
        '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_LAB, remesa_twod_lab_ioctl, DRM_ROOT_ONLY),\n'
        '#endif\n'
        '#ifdef REMESA_TWOD_COPY_IOCTL\n'
        '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_COPY, remesa_twod_copy_ioctl, DRM_ROOT_ONLY),\n'
        '#endif\n')
    result = {'psb_drv.h': header, 'accel_2d.c': accel,
            'gma500_fixed_backend.c': fixed, 'mmu.c': mmu,
            'remesa_gem_memory.c': gem, 'psb_drv.c': driver}
    if lab:
        result['Makefile'] = (native / 'Makefile').read_text() + '\n# Explicit isolated 2D LAB candidate; runtime gate defaults N.\nccflags-y += -DREMESA_TWOD_LAB_IOCTL=1\n'
        if vendor:
            result['Makefile'] += 'ccflags-y += -DREMESA_TWOD_LAB_VENDOR_STREAM=1\n'
    if copy:
        result['Makefile'] = (native / 'Makefile').read_text() + '\n# Typed copy profile; no LAB diagnostics; runtime defaults N.\nccflags-y += -DREMESA_TWOD_COPY_IOCTL=1 -DREMESA_TWOD_VENDOR_STREAM=1\n'
    if fill:
        result['Makefile'] += 'ccflags-y += -DREMESA_TWOD_FILL_STREAM=1\n'
        result['remesa_gem_memory.c'] += '\n#ifdef REMESA_TWOD_FILL_STREAM\n#include "remesa_twod_fill.inc"\n#endif\n'
        result['psb_drv.c'] = '#include "remesa_twod_fill.h"\n#include "remesa_twod_fill_uapi.h"\n' + result['psb_drv.c']
        result['psb_drv.c'] = replace_once(result['psb_drv.c'],
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
            '#ifdef REMESA_TWOD_FILL_STREAM\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_FILL, remesa_twod_fill_ioctl, DRM_ROOT_ONLY),\n'
            '#endif\n')
    if private:
        result['mmu.c'] += '\n#include "remesa_mmu_confinement.inc"\n#include "remesa_mmu_private.inc"\n#include "remesa_mmu_switch.inc"\n'
        result['remesa_gem_memory.c'] += '\n#include "remesa_gem_private.inc"\n'
    return result


def stage(native, output, lab=False, vendor=False, copy=False, fill=False, private=False):
    if output.exists():
        raise FileExistsError('refusing to overwrite ' + str(output))
    patched = patched_sources(native, lab, vendor, copy, fill, private)
    root = Path(__file__).resolve().parents[2]
    additions = root / 'kernel/remesa/gma500'
    components = {name: (additions / name).read_bytes() for name in
                  ['remesa_twod_reservation.h', 'remesa_twod_reservation.inc',
                   'remesa_twod_irq.h', 'remesa_twod_irq.inc']}
    for name in ['remesa_twod_native.h', 'remesa_twod_native.inc',
                 'remesa_mmu_default_root.h', 'remesa_mmu_default_root.inc',
                 'remesa_mmu_verify_access.h', 'remesa_mmu_verify_access.inc',
                 'remesa_gem_twod.h', 'remesa_gem_twod.inc',
                 'remesa_gem_twod_job.h', 'remesa_gem_twod_job.inc']:
        components[name] = (additions / name).read_bytes()
    if private:
        for name in ['remesa_mmu_confinement.h', 'remesa_mmu_confinement.inc',
                     'remesa_mmu_private.h', 'remesa_mmu_private.inc',
                     'remesa_mmu_switch.h', 'remesa_mmu_switch.inc',
                     'remesa_gem_private.h', 'remesa_gem_private.inc']:
            components[name] = (additions / name).read_bytes()
    for name in ['remesa_twod.c', 'remesa_twod.h']:
        components[name] = (root / 'mesa/sgx535' / name).read_bytes()
    for name in ['remesa_twod_lab.h', 'remesa_twod_lab.inc', 'remesa_twod_copy.h', 'remesa_twod_copy.inc']:
        components[name] = (additions / name).read_bytes()
    components['remesa_twod_lab_uapi.h'] = (root / 'include/remesa/twod_lab_uapi.h').read_bytes()
    components['remesa_twod_copy_uapi.h'] = (root / 'include/remesa/twod_copy_uapi.h').read_bytes()
    if fill:
        for name in ['remesa_twod_fill.h', 'remesa_twod_fill.inc']:
            components[name] = (additions / name).read_bytes()
        components['remesa_twod_fill_uapi.h'] = (root / 'include/remesa/twod_fill_uapi.h').read_bytes()
    output.mkdir(parents=True)
    for source in native.iterdir():
        if source.is_file() and (source.suffix in ['.c', '.h', '.inc'] or
                                 source.name in ['Makefile', 'Kconfig']):
            shutil.copyfile(source, output / source.name)
    for name, source in patched.items():
        (output / name).write_text(source)
    for name, source in components.items():
        (output / name).write_bytes(source)
    manifest = {
        'native_source': str(native.resolve()), 'pinned_inputs': PINNED,
        'output_sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
                          for name in [*patched, *components]},
        'lab_ioctl_compiled': lab,
        'vendor_stream_lab_compiled': vendor,
        'normal_copy_compiled': copy,
        'native_fill_compiled': fill,
        'private_mmu_preparation_compiled': private,
        'scope': 'Native 2D candidate; optional root-only gated LAB ioctl; never installed by stager',
    }
    (output / 'TWOD_RESERVATION_STAGE.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--with-lab-ioctl', action='store_true')
    parser.add_argument('--with-lab-vendor-stream', action='store_true')
    parser.add_argument('--with-native-copy', action='store_true')
    parser.add_argument('--with-native-fill', action='store_true')
    parser.add_argument('--with-private-mmu-preparation', action='store_true',
                        help='Compile trusted GEM-to-unbound-MMU preparation; no new ioctl/admission')
    args = parser.parse_args()
    print(json.dumps(stage(args.native_source, args.output, args.with_lab_ioctl,
                           args.with_lab_vendor_stream, args.with_native_copy, args.with_native_fill,
                           args.with_private_mmu_preparation), indent=2))
