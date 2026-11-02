#!/usr/bin/env python3
"""Compare actual archived FAST DPM construction with the general constructor.

No device is opened. The archive is read-only; generated files live in a new
output directory. A match does not admit warm memory to the general executor.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--retained-module', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--i486-cc', type=Path)
    parser.add_argument('--qemu-i386', type=Path)
    args = parser.parse_args()
    if bool(args.i486_cc) != bool(args.qemu_i386):
        parser.error('both i486 options are required')
    root = Path(__file__).resolve().parents[2]
    module = args.retained_module.resolve()
    source = module / 'remesa_stream_contract.c'
    # Include the actual private producer with its own namespace/header/profile,
    # rather than reconstructing its 29 actions in a second handwritten oracle.
    if not source.is_file():
        parser.error('retained module must contain remesa_stream_contract.c')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    wrapper = output / 'retained_wrapper.c'
    wrapper.write_text('#include ' + json.dumps(str(source)) + '\n' + r'''
int retained_dpm_actions(uint64_t table, uint64_t parameter, void *out, size_t bytes);
int retained_dpm_actions(uint64_t table, uint64_t parameter, void *out, size_t bytes)
{
 static const uint64_t addresses[11] = {
  0x20000000,0x20080000,0x20028000,0x30000000,0,
  0x40001000,0x407d1000,0x407ef000,0x30100000,0,0x403e9000
 };
 struct sgx535_frozen_bo bos[SGX535_BO_COUNT];
 _Static_assert(SGX535_BO_COUNT == 11, "expected retained depth profile");
 if (bytes != 29 * sizeof(struct sgx535_frozen_reg_action)) return -1;
 for (unsigned i=0;i<SGX535_BO_COUNT;i++) {
  struct sgx535_frozen_requirement r;
  if (sgx535_frozen_get_requirement(i,&r)) return -2;
  bos[i]=(struct sgx535_frozen_bo){i,r.domain,r.size,addresses[i],i+1};
 }
 bos[SGX535_BO_TA_PAGE_TABLE].gpu_va=table;
 bos[SGX535_BO_TA_PARAMETER].gpu_va=parameter;
 return sgx535_frozen_ta_load_plan(bos,SGX535_BO_COUNT,0xe0000000ULL,out,29);
}
''')
    test = root / 'mesa/sgx535/tests/test_lab_dpm_retained.c'
    actions = root / 'tools/remesa/lab_scene_actions.c'
    variants = [('native', 'cc', ['-O2'], []),
                ('ubsan', 'cc', ['-O1', '-fsanitize=undefined',
                                  '-fno-sanitize-recover=all'], [])]
    if args.i486_cc:
        variants.append(('i486', str(args.i486_cc), ['-O2', '-static', '-march=i486'],
                         [str(args.qemu_i386)]))
    commands = []

    def limit(ceiling):
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (ceiling,)*2)

    for name, cc, flags, runner in variants:
        exe = output / name
        argv = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                '-DREMESA_LAB_DEPTH1280_PROFILE=1', *flags,
                '-I'+str(root/'include'), str(wrapper), str(test), str(actions),
                '-o', str(exe)]
        for label, command in [(name+'-build', argv),
                               (name+'-run', runner+[str(exe)])]:
            ceiling = 6*1024**3 if runner and label.endswith('-run') else 768*1024**2
            result = subprocess.run(command, capture_output=True, text=True,
                                    timeout=120, preexec_fn=lambda: limit(ceiling))
            (output/(label+'.log')).write_text(result.stdout+result.stderr)
            commands.append(dict(label=label, argv=command, exit=result.returncode))
            (output/'COMMANDS.json').write_text(json.dumps(commands, indent=2)+'\n')
            if result.returncode or result.stderr:
                raise RuntimeError(label + ': ' + result.stderr[-3000:])
    files = [Path(__file__).resolve(), test, actions, wrapper,
             root/'include/remesa/scene_dpm.h', root/'include/remesa/scene_layout.h']
    # Pin all local preprocessing inputs, including the archived contract header,
    # aliases, relocation/initial tables and its private scene-layout include.
    files += sorted(module.glob('*.h')) + sorted(module.glob('*.inc')) + [source]
    result = dict(status='RETAINED_GENERAL_DPM_CONSTRUCTION_EQUIVALENT',
                  GPU_operations=0, physical_reuse_qualified=False,
                  relocated_cases_per_variant=16, actions_per_case=29,
                  variants=[v[0] for v in variants], commands=commands,
                  scope='CPU producer equivalence only; no warm-memory admission, '
                        'coherency, retirement or hardware execution inferred',
                  sources={str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in files})
    (output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k: result[k] for k in ['status', 'GPU_operations', 'variants']}))


if __name__ == '__main__':
    main()
