#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Focused Gallium transfer and lifetime regression; never contacts hardware."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--actual-archive', action='store_true',
                        help='Also execute the rebuilt Meson driver and Mesa upload utilities')
    parser.add_argument('--archive-only', action='store_true',
                        help='Test only the rebuilt archive; do not repeat source tests')
    parser.add_argument('--targets', help='Comma-separated source test names, without test_ or .c')
    parser.add_argument('--archive-test', choices=('resource_sync', 'resource_invalidate', 'framebuffer_contract', 'transfer_flush', 'mip_transfers', 'context_texture_metadata', 'clip_pipeline'),
                        default='transfer_flush', help='Callback test linked to the actual Mesa archive')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    here = repo / 'mesa/sgx535'
    mesa = args.cache / 'mesa-25.2.0'
    build = args.cache / 'build-i386'
    pin = json.loads((here / 'mesa-api-pin.json').read_text())
    for name in ('src/gallium/include/pipe/p_context.h',
                 'src/gallium/include/pipe/p_state.h',
                 'src/gallium/include/pipe/p_defines.h'):
        if digest(mesa / name) != pin['interface_files'][name]:
            raise SystemExit('Pinned Mesa interface mismatch: ' + name)
    args.output.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    env.update(json.loads((args.cache / 'build-environment.json').read_text()))
    qemu = '/home/gama/sgx535-offline/phase8-single-color-hypothesis-20261006T034310Z/tests/qemu-root/usr/bin/qemu-i386'
    base = ['-std=c11', '-D_POSIX_C_SOURCE=200809L', '-DHAVE_ENDIAN_H',
            '-DREMESA_WITH_MESA_HEADERS', '-Wall', '-Wextra', '-Werror']
    base += ['-I' + str(p) for p in (here, repo/'include', build/'src',
                                     mesa/'src/gallium/include',
                                     mesa/'src', mesa/'include')]
    base += ['-isystem', str(mesa/'src/gallium/auxiliary')]
    sources = [here/n for n in ('remesa_fixed_state.c', 'remesa_render_pass.c',
               'remesa_sampler_state.c', 'remesa_texture_binding.c',
               'remesa_texture_descriptor.c', 'remesa_screen.c', 'remesa_draw.c',
               'remesa_present.c')] + [repo/'common/remesa/draw_packet.c']
    tests = ['test_resource_sync.c', 'test_resource_invalidate.c', 'test_transfer_flush.c', 'test_mip_transfers.c', 'test_transfer_resources.c',
             'test_framebuffer_contract.c', 'test_driver.c']
    if args.targets:
        selected = args.targets.split(',')
        names = {Path(n).stem.removeprefix('test_'): n for n in tests}
        if len(set(selected)) != len(selected) or any(n not in names for n in selected):
            raise SystemExit('Unknown or duplicate source target')
        tests = [names[n] for n in selected]
    inputs = sources + list(here.glob('*.h')) + [here/'tests'/n for n in tests]
    inputs += [here/'tests/test_transfer_resources.c']
    inputs += [here/'tests/test_resource_sync.c']
    inputs += [Path(__file__).resolve()]
    hashes = {str(p): digest(p) for p in inputs}
    results = []

    def execute(command, label, cwd=None):
        run = subprocess.run(command, env=env, cwd=cwd, text=True,
                             capture_output=True, timeout=120)
        (args.output/(label+'.log')).write_text(run.stdout + run.stderr)
        if run.returncode:
            raise SystemExit(label + ' failed (see log)')
        return run.stdout.strip()

    variants = [('native', ['cc'], ['-O2'], []),
                ('ubsan', ['cc'], ['-O1', '-fsanitize=undefined', '-fno-sanitize-recover=all'], []),
                ('i386', [str(args.cache/'wrappers/cc')], ['-O2', '-static', '-march=i486'], [qemu])]
    if args.archive_only:
        variants = []
    for variant, compiler, flags, runner in variants:
        for test in tests:
            label = variant + '-' + Path(test).stem
            executable = args.output/label
            wrap = ['-Wl,--wrap=calloc'] if test == 'test_transfer_flush.c' else []
            command = compiler + base + flags + list(map(str, sources))
            command += [str(here/'tests'/test)] + wrap + ['-lm', '-o', str(executable)]
            execute(command, label+'-build')
            output = execute(runner + [str(executable)], label+'-run')
            results.append({'name': label, 'command': command,
                            'run': runner+[str(executable)], 'output': output,
                            'sha256': digest(executable), 'status': 'PASS'})
    archive_inputs = {}
    if args.actual_archive or args.archive_only:
        # Never rebuild or silently accept stale, unrelated staged sources.
        staged = mesa/'src/gallium/drivers/sgx535'
        for path in sorted(staged.rglob('*')):
            if not path.is_file() or path.suffix not in ('.c', '.h'):
                continue
            relative = path.relative_to(staged)
            if str(relative) in ('draw_packet.c', 'shader_request.c', 'retained_evidence.c'):
                original = repo/'common/remesa'/relative.name
            elif str(relative) in ('registry.c', 'resource.c', 'provider.c'):
                original = repo/'kernel/remesa'/relative.name
            elif relative.parts[:2] == ('include', 'remesa'):
                original = repo/'include/remesa'/relative.name
            elif str(relative) == 'include/drm/drm.h':
                if path.read_text() != '/* SPDX-License-Identifier: MIT */\n#include <drm.h>\n':
                    raise SystemExit('DRM shim mismatch')
                archive_inputs[str(path)] = digest(path)
                continue
            else:
                original = here/relative
            if path.read_bytes() != original.read_bytes():
                raise SystemExit('Actual Mesa source mismatch: ' + str(relative))
            archive_inputs[str(path)] = digest(path)
        archive = build/'src/gallium/drivers/sgx535/libremesa_loader.a'
        freshness = execute(['ninja', '-C', str(build), '-n', str(archive.relative_to(build))],
                            'actual-archive-freshness')
        if 'no work to do' not in freshness:
            raise SystemExit('Actual Mesa archive is stale')
        record = next(r for r in json.loads((build/'compile_commands.json').read_text())
                      if r['file'].endswith('/remesa_nir.c'))
        command = record.get('arguments') or shlex.split(record['command'])
        flags, skip = [], False
        for word in command:
            if skip:
                skip = False
                continue
            if word in ('-o', '-MF', '-MQ'):
                skip = True
                continue
            if word in ('-c', '-MD', '-DNDEBUG') or word == record['file']:
                continue
            flags.append(word)
        flags.insert(1, '-I'+str(here))
        flags.insert(1, '-I'+str(mesa/'src/gallium/auxiliary'))
        # Drop the uncalled inherited fixture entry; keep all callbacks reached
        # by this test. Mesa's compile flags already use function sections.
        flags += ['-UNDEBUG', '-DREMESA_TEST_MESA_UPLOAD', '-Wl,--gc-sections']
        libraries = ['src/gallium/auxiliary/libgallium.a', 'src/compiler/nir/libnir.a',
                     'src/compiler/libcompiler.a', 'src/util/libmesa_util.a',
                     'src/util/libmesa_util_simd.a', 'src/util/blake3/libblake3.a',
                     'src/c11/impl/libmesa_util_c11.a']
        executable = args.output/'actual-archive-i386'
        archive_test = 'test_' + args.archive_test + '.c'
        if str(here/'tests'/archive_test) not in hashes:
            hashes[str(here/'tests'/archive_test)] = digest(here/'tests'/archive_test)
        wrap = ['-Wl,--wrap=calloc'] if args.archive_test == 'transfer_flush' else []
        command = flags + [str(here/'tests'/archive_test)] + wrap + ['-Wl,--start-group', str(archive)]
        command += [str(build/n) for n in libraries] + ['-Wl,--end-group',
                    '-L'+str(args.cache/'i386-deps/usr/lib/i386-linux-gnu'),
                    '-ldrm', '-lm', '-ldl', '-latomic', '-pthread',
                    str(args.cache/'build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33'),
                    '-o', str(executable)]
        execute(command, 'actual-archive-build', build)
        runner = [qemu, '-L', str(args.cache/'runtime'), str(executable)]
        results.append({'name': 'actual-archive-i386', 'status': 'PASS', 'command': command,
                        'run': runner, 'output': execute(runner, 'actual-archive-run'),
                        'sha256': digest(executable), 'archive_sha256': digest(archive)})
    if any(digest(Path(p)) != h for p, h in hashes.items()):
        raise SystemExit('Source changed during qualification')
    if any(digest(Path(p)) != h for p, h in archive_inputs.items()):
        raise SystemExit('Actual Mesa sources changed during qualification')
    receipt = {'status': 'PASS', 'mesa': '25.2.0', 'tests': results,
               'sources': hashes, 'actual_archive_inputs': archive_inputs,
               'hardware_submissions': 0,
               'scope': 'Real Gallium CPU transfer contracts and linked Mesa uploads; no GL context or GPU rendering claim'}
    (args.output/'result.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print('PASS', len(results), 'test processes; zero GPU submissions')

if __name__ == '__main__':
    main()
