#!/usr/bin/env python3
"""Stage the bounded 2D producer-reservation patch; never install or overwrite.

The input is the retained qualified native source, not the dirty legacy checkout.
This only integrates producer exclusion; it does not register a GPU executor.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

PINNED = {
    'accel_2d.c': '2de5bef67e6813fa2f238a1792fab42bd5dbf8942273ba5a462bc24e3e7a5b7e',
    'psb_drv.h': '6dce38176eccc8d7037783ec6c41d101c3c614068a3b7b11f40b06e1d1ca6261',
}


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError('native integration anchor is missing or ambiguous')
    return source.replace(old, new, 1)


def patched_sources(native):
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
        '#include "remesa_twod_reservation.inc"\n')
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
    return {'psb_drv.h': header, 'accel_2d.c': accel}


def stage(native, output):
    if output.exists():
        raise FileExistsError('refusing to overwrite ' + str(output))
    patched = patched_sources(native)
    root = Path(__file__).resolve().parents[2]
    additions = root / 'kernel/remesa/gma500'
    components = {name: (additions / name).read_bytes() for name in
                  ['remesa_twod_reservation.h', 'remesa_twod_reservation.inc']}
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
        'scope': 'Producer reservation only; no executor, ioctl or installation',
    }
    (output / 'TWOD_RESERVATION_STAGE.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.native_source, args.output), indent=2))
