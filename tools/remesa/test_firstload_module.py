#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise real module metadata plus RAM-only corruptions; never insmod."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import verify_firstload_module as v


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--known-good', type=Path, required=True)
    p.add_argument('--missing-profile', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--matrix-profile', type=Path, help='Additionally test eight-control ordinary profile and missing-gate refusal')
    a = p.parse_args()
    good, missing = a.known_good.read_bytes(), a.missing_profile.read_bytes()
    rows = []
    result = v.inspect(good)
    assert result['status'] == 'FIRSTLOAD_MODULE_CONTROLS_PASS' and len(result['controls']) == 7
    rows.append({'case': 'known-good seven registered default-zero controls', 'pass': True})
    result = v.inspect(missing)
    assert set(result['missing']) == {'remesa_twod_copy_enabled', 'remesa_twod_fill_enabled'}
    rows.append({'case': 'actual incomplete profile refused', 'missing': result['missing'], 'pass': True})
    off = struct.unpack_from('<I', good, 32)[0]
    ent, count, st = struct.unpack_from('<3H', good, 46)
    sections = [struct.unpack_from('<10I', good, off + i * ent) for i in range(count)]
    names = good[sections[st][4]:sections[st][4] + sections[st][5]]
    ids = {names[s[0]:].split(b'\0', 1)[0].decode(): i for i, s in enumerate(sections)}
    tab = sections[ids['.symtab']]
    strings = sections[tab[6]]
    strings = good[strings[4]:strings[4] + strings[5]]
    symbols = [struct.unpack_from('<IIIBBH', good, i) for i in range(tab[4], tab[4] + tab[5], 16)]
    symbols = {strings[s[0]:].split(b'\0', 1)[0].decode(): s for s in symbols}
    bad = bytearray(good)
    desc = symbols['__param_remesa_twod_copy_enabled']
    struct.pack_into('<H', bad, sections[desc[5]][4] + desc[1] + 12, 0o644)
    mutations = [('wrong parameter permissions', bytes(bad)), ('truncated ELF', good[:100]),
                 ('wrong ELF class', good[:4] + b'\x02' + good[5:])]
    bad = bytearray(good)
    at = good.index(b'parmtype=remesa_twod_copy_enabled:bool')
    start = at + len(b'parmtype=remesa_twod_copy_enabled:')
    bad[start:start + 4] = b'uint'
    mutations.append(('wrong registered type', bytes(bad)))
    bad = bytearray(good)
    value = symbols['memory_enabled']
    section = sections[value[5]]
    assert section[1] == 8 and value[1] < section[5]
    struct.pack_into('<I', bad, off + value[5] * ent + 4, 1)  # materialize BSS in fixture
    struct.pack_into('<I', bad, off + value[5] * ent + 16, len(good))
    bad.extend(bytes(section[5]))
    bad[len(good) + value[1]] = 1
    mutations.append(('enabled static control refused', bytes(bad)))
    for label, data in mutations:
        try:
            v.inspect(data)
        except ValueError as e:
            rows.append({'case': label, 'error': str(e), 'pass': True})
        else:
            raise AssertionError(label)
    if a.matrix_profile:
        matrix = a.matrix_profile.read_bytes()
        checked = v.inspect(matrix, True)
        assert checked['status'] == 'FIRSTLOAD_MODULE_CONTROLS_PASS' and len(checked['controls']) == 8
        assert v.inspect(good, True)['missing'] == ['remesa_matrix_submit_enabled']
        rows.append({'case':'ordinary eight controls and old profile missing ordinary gate', 'pass':True})
        # Locate this profile's symbol/descriptor through its own ELF tables.
        moff = struct.unpack_from('<I', matrix, 32)[0]
        ment, mcount, mst = struct.unpack_from('<3H', matrix, 46)
        ms = [struct.unpack_from('<10I', matrix, moff+i*ment) for i in range(mcount)]
        mn = matrix[ms[mst][4]:ms[mst][4]+ms[mst][5]]
        symtab = next(s for s in ms if mn[s[0]:].split(b'\0',1)[0] == b'.symtab')
        strings = matrix[ms[symtab[6]][4]:ms[symtab[6]][4]+ms[symtab[6]][5]]
        symbols = [struct.unpack_from('<IIIBBH', matrix, i) for i in range(symtab[4], symtab[4]+symtab[5], 16)]
        symbols = {strings[s[0]:].split(b'\0',1)[0].decode():s for s in symbols}
        desc = symbols['__param_remesa_matrix_submit_enabled']
        bad_perm = bytearray(matrix)
        struct.pack_into('<H', bad_perm, ms[desc[5]][4]+desc[1]+12, 0o644)
        bad_type = matrix.replace(b'parmtype=remesa_matrix_submit_enabled:bool', b'parmtype=remesa_matrix_submit_enabled:uint')
        value = symbols['matrix_submit_enabled']
        section = ms[value[5]]
        assert section[1] == 8
        bad_zero = bytearray(matrix)
        struct.pack_into('<I', bad_zero, moff+value[5]*ment+4, 1)
        struct.pack_into('<I', bad_zero, moff+value[5]*ment+16, len(matrix))
        bad_zero.extend(bytes(section[5]))
        bad_zero[len(matrix)+value[1]] = 1
        for label, data in [('ordinary gate permissions', bad_perm), ('ordinary gate type', bad_type), ('ordinary gate default enabled', bad_zero)]:
            try:
                v.inspect(data, True)
            except ValueError:
                rows.append({'case':label, 'pass':True})
            else:
                raise AssertionError(label)
        assert a.matrix_profile.read_bytes() == matrix
    assert hashlib.sha256(a.known_good.read_bytes()).digest() == hashlib.sha256(good).digest()
    assert hashlib.sha256(a.missing_profile.read_bytes()).digest() == hashlib.sha256(missing).digest()
    result = {'status': 'FIRSTLOAD_MODULE_METADATA_TESTS_PASS', 'cases': rows,
              'GPU_operations': 0, 'original_modules_unchanged': True,
              'scope': 'ELF parameter registration/type/default metadata only; runtime N remains required'}
    with a.output.open('x') as f:
        f.write(json.dumps(result, indent=2) + '\n')
    print(f'PASS {len(rows)} module metadata cases, no module load or GPU')


if __name__ == '__main__':
    main()
