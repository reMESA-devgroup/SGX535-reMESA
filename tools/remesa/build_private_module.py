#!/usr/bin/env python3
"""Build the private candidate in copied kernel output; never install it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import subprocess
import time

from stage_twod_reservation import stage


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(native, kernel, command_file, environment_file, symbols, output):
    inputs = [p.resolve() for p in (native, kernel, command_file, environment_file, symbols)]
    native, kernel, command_file, environment_file, symbols = inputs
    output = output.resolve()
    if output.exists():
        raise FileExistsError(f"refusing to overwrite {output}")
    if any(output == p or output.is_relative_to(p) for p in inputs):
        raise ValueError("output must be outside the input trees")
    command = json.loads(command_file.read_text())["command"]
    if not command or command[0] != "make" or command[-1] != "modules":
        raise ValueError("expected an existing make modules qualification command")
    watched = [kernel / n for n in (".config", "Module.symvers", "include/config/kernel.release")]
    originals = {str(p): sha(p) for p in watched}
    originals[str(symbols)] = sha(symbols)
    output.mkdir(parents=True)
    module = output / "module"
    stage(native, module, copy=True, fill=True, private=True)
    copied_kernel = output / "kernel-output"
    shutil.copytree(kernel, copied_kernel, symlinks=True)
    copied_symbols = output / "observer.symvers"
    shutil.copy2(symbols, copied_symbols)
    command = [x for x in command if not x.startswith(("O=", "M=", "KBUILD_EXTRA_SYMBOLS=", "-j")) and x != "modules"]
    command += [f"O={copied_kernel}", "-j1", f"M={module}",
                f"KBUILD_EXTRA_SYMBOLS={copied_symbols}", "modules"]
    env = dict(os.environ)
    env.update(json.loads(environment_file.read_text()))
    env.pop("MAKEFLAGS", None)
    env.pop("MFLAGS", None)

    def limits():
        # Bound each build process on the tablet; never spawn parallel compilers.
        resource.setrlimit(resource.RLIMIT_AS, (768 * 1024**2, 768 * 1024**2))
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

    started = time.monotonic()
    (output / "COMMAND.json").write_text(json.dumps(command, indent=2) + "\n")
    timed_out = False
    with (output / "build.log").open("wb") as log:
        process = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT,
                                   preexec_fn=limits, start_new_session=True)
        try:
            returncode = process.wait(timeout=900)
        except BaseException as error:
            # Include compiler/linker descendants; do not leave an orphan build.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            if not isinstance(error, subprocess.TimeoutExpired):
                raise
            timed_out, returncode = True, 124
    elapsed = time.monotonic() - started
    unchanged = all(sha(Path(p)) == digest for p, digest in originals.items())
    artifact = module / "gma500_gfx.ko"
    report = dict(status="FULL_MODULE_BUILT_UNQUALIFIED" if returncode == 0 else "BUILD_FAILED",
                  exit=returncode, timed_out=timed_out, elapsed_seconds=elapsed, command=command,
                  input_hashes=originals, original_kernel_and_symbols_unchanged=unchanged,
                  jobs=1, per_process_address_space_limit_bytes=768 * 1024**2,
                  module=str(artifact), module_sha256=sha(artifact) if artifact.is_file() else None,
                  GPU_operations=0, installed=False)
    (output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n")
    if not unchanged:
        raise RuntimeError("original kernel/symbol inputs changed during build")
    if returncode:
        raise RuntimeError(f"module build failed; inspect {output / 'build.log'}")
    print(json.dumps(report, indent=2))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("native", "kernel", "command-file", "environment-file", "symbols", "output"):
        p.add_argument("--" + name, required=True, type=Path)
    a = p.parse_args()
    build(a.native, a.kernel, a.command_file, a.environment_file, a.symbols, a.output)


if __name__ == "__main__":
    main()
