#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Qualify the retained SGX535 CPU compiler oracle in a NEW private directory.

No GPU/GL/EGL API or installation. Does not ship/download the original compiler.
The original library's CPU initializer runs against memory/locale-only services;
its empty constructor list and API contract were inspected with REA. Output USP
objects contain metadata/deferred work and MUST NOT be submitted as USSE CODE.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import struct
import subprocess

PIN = '3aeb3a45b89b6e292875f84acd427930ce2e6170653970082d22e8123584a0dd'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(args):
    compiler = args.compiler.resolve()
    original = compiler.read_bytes()
    if hashlib.sha256(original).hexdigest() != PIN:
        raise ValueError('Unreviewed compiler artifact')
    output = args.output.absolute()
    output.mkdir(mode=0o700, exist_ok=False)
    sources = Path(__file__).resolve().with_name('compiler_oracle')
    inputs = {}
    for name in ('services_cpu.c', 'compiler_cpu.c'):
        src = sources / name
        (output / name).write_bytes(src.read_bytes())
        inputs[str(src)] = sha(src)
    library = output / 'original-compiler.so'
    library.write_bytes(original)
    library.chmod(0o400)
    runtime = args.runtime.resolve()
    cc, qemu = str(args.cc.resolve()), str(args.qemu.resolve())
    commands = []
    env = {k: v for k, v in os.environ.items()
           if k not in ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'LD_AUDIT')}

    def execute(label, command, cwd, timeout):
        try:
            proc = subprocess.run(command, cwd=cwd, env=env,
                                  capture_output=True, text=True,
                                  timeout=timeout, preexec_fn=no_core)
            stdout, stderr, code = proc.stdout, proc.stderr, proc.returncode
        except subprocess.TimeoutExpired as error:
            def decode(value):
                return value.decode(errors='replace') if isinstance(value, bytes) else value or ''
            stdout, stderr, code = decode(error.stdout), decode(error.stderr), 'TIMEOUT'
        (cwd / (label + '.stdout')).write_text(stdout)
        (cwd / (label + '.stderr')).write_text(stderr)
        record = {'label': label, 'command': command, 'cwd': str(cwd),
                  'returncode': code}
        commands.append(record)
        (output / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
        if code != 0:
            raise RuntimeError('Failed ' + label + ': ' + str(code))
        return stdout, stderr

    common = [cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
    execute('services-build', common + ['-fPIC', '-shared',
            str(output / 'services_cpu.c'), '-Wl,-soname,libemgdsrv_um.so',
            '-o', str(output / 'libemgdsrv_um.so')], output, 60)
    execute('oracle-build', common + [str(output / 'compiler_cpu.c'),
            '-L' + str(output), '-Wl,-rpath,' + str(output),
            '-l:libemgdsrv_um.so', '-ldl', '-o', str(output / 'compiler_cpu')], output, 60)
    cases = []
    for shader, padding in [('constant', '0'), ('texture', '0'),
                            ('texture', '0xdeadbeef')]:
        case = output / (shader + '-pad' + padding)
        case.mkdir(mode=0o700)
        command = [qemu, '-strace', str(runtime / 'ld-linux.so.2'),
                   '--library-path', str(output) + ':' + str(runtime),
                   str(output / 'compiler_cpu'), str(library), shader,
                   '1', '64', padding]
        stdout, trace = execute('compile', command, case, 30)
        forbidden = [line for line in trace.splitlines()
                     if re.search(r'/dev/(?:dri|mem|pvrsrv)', line)
                     or re.search(r'\b(?:ioctl|socket|connect)\(', line)]
        if forbidden:
            raise RuntimeError('Unexpected device/network syscall: ' + repr(forbidden))
        if ('TARGET core=4 rev=121' not in stdout or
                'RESULT stage=1 flags=2 success=1' not in stdout):
            raise RuntimeError('Missing original compiler success/target')
        allocation_lines = [line for line in stdout.splitlines()
                            if line.startswith('ALLOCATIONS ')]
        if not allocation_lines or not re.search(r'live=0 bytes=0\b', allocation_lines[-1]):
            raise RuntimeError('Compiler allocations remain live')
        blobs = {}
        for kind in ('primary', 'alternate'):
            path = case / (kind + '-output-allocation.bin')
            data = path.read_bytes()
            if len(data) < 40:
                raise RuntimeError('Truncated USP allocation')
            magic, version, body, first = struct.unpack_from('<4I', data)
            core, revision = struct.unpack_from('<2I', data, 32)
            if (magic, version, first, core, revision) != (0x55535020, 31, 1, 4, 121):
                raise RuntimeError('Wrong USP prefix/target')
            if body != len(data) - 12:
                raise RuntimeError('USP declared length/allocation mismatch')
            blobs[kind] = {'bytes': len(data), 'sha256': sha(path),
                           'scope': 'USP allocation, not executable USSE CODE'}
        cases.append({'shader': shader, 'unwritten_format_word9': padding,
                      'blobs': blobs, 'guards': 'intact', 'live_allocations': 0})
    if cases[1]['blobs'] != cases[2]['blobs']:
        raise RuntimeError('Unwritten-word probe changed this shader output')
    if sha(compiler) != PIN or sha(library) != PIN:
        raise RuntimeError('Original compiler changed')
    result = {'status': 'ORIGINAL_SGX535_REV121_CPU_COMPILER_ORACLE_PASS',
              'compiler_sha256': PIN, 'source_sha256': inputs, 'cases': cases,
              'compiler_register_budget': 64,
              'budget_scope': 'Explicit CPU compiler input, not measured GPU task capacity',
              'format_table_bytes': 320, 'format_table_entries': 8,
              'format_table_read_only_end_guarded': True,
              'GPU_operations': 0, 'shader_admission_changed': False,
              'module_or_library_installed': False,
              'limitations': ['Static native defaults, not a captured live device context.',
                             'Only USP prefix/length/target checked; full block grammar unqualified.',
                             'No physical shader/texture result or OpenGL capability approval.']}
    (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cc', type=Path, required=True)
    parser.add_argument('--qemu', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    result = run(parser.parse_args())
    print(result['status'] + '; 3 cases; no GPU operations')
