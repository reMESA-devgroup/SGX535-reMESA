#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Replay the pinned original rev121 USP finalizer, entirely offline.

Inputs are the two primary USP objects retained by the GLSL compiler qualifier.
Outputs are a selected fragment reference, NOT permission to execute a shader.
Original library/decoder binaries remain external, unmodified dependencies.
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

PIN = '7859b0fa0cd3cd44ed18a0c056607066c07312307f8c6d86fd8e211f9a617ed0'
DECODER_PIN = '9349ed4d7643d81cb638b41eb41b69507cc2f9f15bae14c8f2116e79f70e7bd9'
SHIM_PIN = '77feb27c20772f0ab32d14a60602ddc10389dce0a440947d78d99d6e2ed62b73'
BLOBS = {
    'constant': '34bc07006c386c28d3cc9cb70f2792acebac4ac281c19daa703a1953578c3e92',
    'texture': '03dd81c5cf66c3e43cde6a004f6232ea87bfed99c4c8ed6357cdf6fa01b12a5b',
}


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run(args):
    library = args.gles2.resolve()
    decoder, shim = args.decoder.resolve(), args.decoder_shim.resolve()
    for p, pin in ((library, PIN), (decoder, DECODER_PIN), (shim, SHIM_PIN)):
        if sha(p) != pin:
            raise ValueError('Unreviewed artifact: ' + str(p))
    blobs = {}
    for name, pin in BLOBS.items():
        p = args.usp_directory.resolve() / (name + '-pad0') / 'primary-output-allocation.bin'
        if sha(p) != pin:
            raise ValueError('USP object differs from qualified input: ' + name)
        blobs[name] = p
    output = args.output.absolute()
    output.mkdir(mode=0o700, exist_ok=False)
    source = Path(__file__).resolve().with_name('compiler_oracle')
    sources = {}
    for name in ('services_cpu.c', 'service_traps.c', 'egl_traps.c', 'finalizer_cpu.c'):
        p = source / name
        (output / name).write_bytes(p.read_bytes())
        sources[str(p)] = sha(p)
    private_library = output / 'original-gles2.so'
    private_library.write_bytes(library.read_bytes())
    private_library.chmod(0o400)
    cc, qemu = str(args.cc.resolve()), str(args.qemu.resolve())
    runtime = args.runtime.resolve()
    env = {k: v for k, v in os.environ.items()
           if k not in ('LD_PRELOAD', 'LD_LIBRARY_PATH', 'LD_AUDIT')}
    commands = []

    def execute(label, command, cwd=output):
        try:
            p = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                               text=True, timeout=40, preexec_fn=no_core)
            stdout, stderr, code = p.stdout, p.stderr, p.returncode
        except subprocess.TimeoutExpired as error:
            def decode(v):
                return v.decode(errors='replace') if isinstance(v, bytes) else v or ''
            stdout, stderr, code = decode(error.stdout), decode(error.stderr), 'TIMEOUT'
        (cwd / (label + '.stdout')).write_text(stdout)
        (cwd / (label + '.stderr')).write_text(stderr)
        commands.append(dict(label=label, command=command, cwd=str(cwd), returncode=code))
        (output / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
        if code != 0:
            raise RuntimeError('Failed ' + label + ': ' + str(code))
        return stdout, stderr

    common = [cc, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror']
    execute('services-build', common + ['-shared', '-fPIC', str(output / 'services_cpu.c'),
            str(output / 'service_traps.c'), '-Wl,-soname,libemgdsrv_um.so',
            '-o', str(output / 'libemgdsrv_um.so')])
    execute('egl-build', common + ['-shared', '-fPIC', str(output / 'egl_traps.c'),
            '-Wl,-soname,libEMGDegl.so', '-o', str(output / 'libEMGDegl.so')])
    execute('harness-build', common + [str(output / 'finalizer_cpu.c'), '-L' + str(output),
            '-Wl,-rpath,' + str(output), '-l:libemgdsrv_um.so', '-ldl',
            '-o', str(output / 'finalizer_cpu')])
    cases, corpus = [], bytearray()
    for name, blob in blobs.items():
        for fmt in (20, 21):
            case = output / (name + '-format' + str(fmt))
            case.mkdir()
            stdout, trace = execute('finalize', [qemu, '-strace', str(runtime / 'ld-linux.so.2'),
                '--library-path', str(output) + ':' + str(runtime),
                str(output / 'finalizer_cpu'), str(private_library), str(blob), str(fmt)], case)
            if not re.search(r'ALLOCATIONS .*live=0 bytes=0\b', stdout):
                raise RuntimeError('Leaked allocations')
            if re.search(r'/dev/(?:dri|mem|pvrsrv)|\b(?:ioctl|socket|connect)\(', trace):
                raise RuntimeError('Unexpected device/network syscall')
            raw = (case / 'raw-use-code.bin').read_bytes()
            terminated = (case / 'native-terminated-use-code.bin').read_bytes()
            expected = bytes.fromhex('800400e001108128' if name == 'constant' else '000000a00100a128')
            if raw != expected or terminated != raw[:4] + struct.pack('<I', struct.unpack_from('<I', raw, 4)[0] | (1 << 18)):
                raise RuntimeError('Unexpected effective CODE/termination')
            inputs = (case / 'input-records.bin').read_bytes()
            if len(inputs) != 28 or struct.unpack_from('<I', inputs)[0] != (name == 'texture'):
                raise RuntimeError('Unexpected selected input record')
            samples = case / 'sample-records.bin'
            if samples.exists() != (name == 'texture') or (samples.exists() and samples.stat().st_size != 28):
                raise RuntimeError('Unexpected sample record count')
            corpus.extend(raw + terminated)
            cases.append(dict(shader=name, numeric_format=fmt, effective_code_bytes=len(raw),
                raw_sha256=sha(case / 'raw-use-code.bin'), input_records=1,
                sample_records=int(samples.exists()), live_allocations_after_cleanup=0))
    (output / 'oracle-input.bin').write_bytes(corpus)
    stdout, stderr = execute('decode', [qemu, str(runtime / 'ld-linux.so.2'), '--library-path',
        str(runtime), '--preload', str(shim), str(decoder), str(output / 'oracle-input.bin')])
    lines = stdout.splitlines()
    if stderr or len(lines) != 9 or not lines[0].startswith('SDK SGX535 revision121 '):
        raise RuntimeError('Decoder trace/identity')
    for i, line in enumerate(lines[1:]):
        m = re.fullmatch(r'(\d+) ([0-9a-f]{8}) ([0-9a-f]{8}) result=(-?\d+) (.*)', line)
        register = 'sa9' if i < 4 else 'pa0'
        expected = 'mov.skipinv' + ('.end' if i % 2 else '') + ' o0, ' + register
        if not m or m[5].strip() != expected or int(m[4]) != i % 2:
            raise RuntimeError('Decoded reference differs')
        if struct.pack('<II', int(m[2], 16), int(m[3], 16)) != corpus[i*8:i*8+8]:
            raise RuntimeError('Decoder input mismatch')
    # Reuse the existing reMESA numeric MOVE encoder; compare against the
    # independently produced primary texture CODE, before and after native END.
    stage = Path(__file__).resolve().parents[2] / 'mesa' / 'sgx535'
    probe = source / 'prefetched_move.c'
    (output / probe.name).write_bytes(probe.read_bytes())
    sources[str(probe)] = sha(probe)
    sources[str(stage / 'remesa_usse.c')] = sha(stage / 'remesa_usse.c')
    sources[str(stage / 'remesa_usse.h')] = sha(stage / 'remesa_usse.h')
    execute('move-build', common + ['-ffunction-sections', '-fdata-sections',
        '-I' + str(stage), str(output / probe.name), str(stage / 'remesa_usse.c'),
        '-Wl,--gc-sections', '-o', str(output / 'prefetched_move')])
    # The existing execute() keeps text logs; this small binary result is
    # serialized separately and must exactly equal the original reference.
    command = [qemu, str(runtime / 'ld-linux.so.2'), '--library-path', str(runtime),
               str(output / 'prefetched_move')]
    p = subprocess.run(command, cwd=output, env=env, capture_output=True,
                       timeout=15, preexec_fn=no_core)
    (output / 'independent-move.bin').write_bytes(p.stdout)
    (output / 'independent-move.stderr').write_bytes(p.stderr)
    commands.append(dict(label='move-run', command=command, returncode=p.returncode))
    (output / 'COMMANDS.json').write_text(json.dumps(commands, indent=2) + '\n')
    if p.returncode or p.stderr or p.stdout != corpus[32:48]:
        raise RuntimeError('Existing independent MOVE encoder differs from finalizer')
    result = dict(status='ORIGINAL_REV121_USP_FINALIZER_CPU_PASS', gles2_sha256=PIN,
        source_sha256=sources, input_sha256=BLOBS, cases=cases, decoder_instructions=8, existing_move_encoder_matches=True,
        GPU_operations=0, runtime_admission_changed=False, installed=False,
        limitations=['Selected primary fragment programs only; not arbitrary USP grammar.',
                     'Input/sample records include uninitialized padding; no whole-record identity claim.',
                     'PDS sampling/interpolation and retained resource closure still need integration.',
                     'No physical textured draw or full OpenGL capability validation.'])
    for p, pin in ((library, PIN), (private_library, PIN), (decoder, DECODER_PIN), (shim, SHIM_PIN)):
        if sha(p) != pin:
            raise RuntimeError('Artifact changed during replay')
    (output / 'RESULT.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('gles2', 'usp-directory', 'cc', 'qemu', 'runtime', 'decoder', 'decoder-shim', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    result = run(parser.parse_args())
    print(result['status'] + '; 4 cases, 8 decoded instructions; no GPU operations')
