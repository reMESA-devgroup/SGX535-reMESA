#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate the seven registered, default-zero controls of the i486 boot profile.
Reads ELF metadata only; never loads a module or opens a device. Runtime N
checks remain mandatory: a static initializer cannot attest post-init state.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

CONTROLS = ('remesa_memory_enabled', 'remesa_retained_lab_enabled',
            'remesa_twod_copy_enabled', 'remesa_twod_fill_enabled',
            'remesa_private_probe_enabled', 'remesa_stream_enabled',
            'remesa_scene_probe_enabled')


def inspect(data):
    if len(data) < 52 or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('requires little-endian ELF32')
    if struct.unpack_from('<HH', data, 16) != (1, 3):
        raise ValueError('requires i386 relocatable kernel module')
    offset = struct.unpack_from('<I', data, 32)[0]
    entry, count, strings = struct.unpack_from('<HHH', data, 46)
    if entry != 40 or not count or strings >= count or offset + count * entry > len(data):
        raise ValueError('invalid section table')
    sections = [struct.unpack_from('<10I', data, offset + i * entry) for i in range(count)]

    def body(index):
        if index >= count:
            raise ValueError('invalid section index')
        s = sections[index]
        if s[1] == 8:  # SHT_NOBITS, zero-initialized storage
            return None
        if s[4] + s[5] > len(data):
            raise ValueError('section exceeds file')
        return data[s[4]:s[4] + s[5]]

    def cstring(blob, at):
        if blob is None or at >= len(blob):
            raise ValueError('invalid string address')
        end = blob.find(b'\0', at)
        if end < 0:
            raise ValueError('unterminated string')
        return blob[at:end].decode('ascii')

    names = body(strings)
    ids = {cstring(names, s[0]): i for i, s in enumerate(sections)}
    required = ['.symtab', '.modinfo', '__param']
    if any(n not in ids for n in required):
        raise ValueError('required module metadata missing')
    symsec = sections[ids['.symtab']]
    symbols_raw, symbol_names = body(ids['.symtab']), body(symsec[6])
    if symbols_raw is None or symsec[9] != 16 or len(symbols_raw) % 16:
        raise ValueError('invalid symbol table')
    symbols = [struct.unpack_from('<IIIBBH', symbols_raw, i) for i in range(0, len(symbols_raw), 16)]
    named = {cstring(symbol_names, s[0]): s for s in symbols}
    param_index = ids['__param']
    relocs = {}
    for i, s in enumerate(sections):
        if s[1] == 9 and s[7] == param_index:  # SHT_REL / sh_info
            blob = body(i)
            if s[6] != ids['.symtab'] or s[9] != 8 or len(blob) % 8:
                raise ValueError('invalid parameter relocations')
            for at in range(0, len(blob), 8):
                target, info = struct.unpack_from('<II', blob, at)
                if target in relocs or info & 255 != 1 or info >> 8 >= len(symbols):
                    raise ValueError('invalid parameter relocation')
                relocs[target] = symbols[info >> 8]
    params = body(param_index)
    modinfo = body(ids['.modinfo'])
    if params is None or modinfo is None:
        raise ValueError('module metadata has no file contents')
    metadata = modinfo.split(b'\0')
    results, missing = {}, []
    for name in CONTROLS:
        desc = named.get('__param_' + name)
        if desc is None:
            missing.append(name)
            continue
        if sum(cstring(symbol_names, s[0]) == '__param_' + name for s in symbols) != 1:
            raise ValueError('duplicate descriptor: ' + name)
        at = desc[1]
        if desc[5] != param_index or desc[2] != 20 or at + 20 > len(params):
            raise ValueError('invalid descriptor: ' + name)
        if struct.unpack_from('<H', params, at + 12)[0] != 0o600:
            raise ValueError('unexpected parameter permissions: ' + name)
        if ('parmtype=' + name + ':bool').encode() not in metadata:
            raise ValueError('missing bool type: ' + name)
        fields = []
        for field in (0, 8, 16):
            if at + field not in relocs:
                raise ValueError('missing parameter relocation: ' + name)
            sym = relocs[at + field]
            addend = struct.unpack_from('<I', params, at + field)[0]
            fields.append((sym, sym[1] + addend))
        sym, pos = fields[0]
        if cstring(body(sym[5]), pos) != name:
            raise ValueError('descriptor name mismatch: ' + name)
        if cstring(symbol_names, fields[1][0][0]) != 'param_ops_bool':
            raise ValueError('unexpected parameter operations: ' + name)
        sym, pos = fields[2]
        if not sym[5] or sym[5] >= count or pos >= sections[sym[5]][5]:
            raise ValueError('invalid bool initializer: ' + name)
        if sections[sym[5]][1] not in (1, 8) or sections[sym[5]][2] & 3 != 3:
            raise ValueError('bool storage is not writable allocated memory: ' + name)
        blob = body(sym[5])
        if blob is not None and blob[pos] != 0:
            raise ValueError('enabled static initializer: ' + name)
        results[name] = {'registered': True, 'type': 'bool', 'permissions': '0600',
                         'static_initial_zero': True}
    return {'status': 'FIRSTLOAD_MODULE_CONTROLS_PASS' if not missing else 'FIRSTLOAD_MODULE_CONTROLS_MISSING',
            'module_sha256': hashlib.sha256(data).hexdigest(), 'controls': results,
            'missing': missing, 'runtime_N_still_required': True, 'GPU_operations': 0}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--module', type=Path, required=True)
    p.add_argument('--output', type=Path)
    a = p.parse_args()
    try:
        result = inspect(a.module.read_bytes())
    except (ValueError, IndexError, struct.error, UnicodeError) as e:
        result = {'status': 'FIRSTLOAD_MODULE_METADATA_INVALID', 'error': str(e), 'GPU_operations': 0}
    if a.output:
        with a.output.open('x') as f:
            f.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
    return 0 if result['status'] == 'FIRSTLOAD_MODULE_CONTROLS_PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
