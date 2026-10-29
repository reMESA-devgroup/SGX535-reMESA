#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Test exact staged unmap against residual PTEs; CPU model only."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from test_screen_shared_counters import function


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--module", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    output = a.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = (a.module / "remesa_gem_memory.c").read_text()
    (output / "private_unmap_actual.inc").write_text(function(source, "unmap("))
    test = Path(__file__).resolve().parents[2] / "kernel/remesa/gma500/tests/test_private_unmap.c"
    rows = []
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (768*1024**2,)*2)
    for variant, flags in (("native", ["-O2"]), ("ubsan", ["-O1", "-fsanitize=undefined", "-fno-sanitize-recover=all"])):
        executable = output / variant
        command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                   "-I"+str(output), str(test), "-o", str(executable)]
        for label, argv in ((variant+"-build", command), (variant+"-run", [str(executable)])):
            result = subprocess.run(argv, capture_output=True, text=True, timeout=30, preexec_fn=limits)
            (output / (label+".log")).write_text(result.stdout+result.stderr)
            rows.append(dict(label=label, argv=argv, exit=result.returncode))
            if result.returncode or result.stderr:raise RuntimeError(label+": "+result.stderr)
    report = dict(status="PRIVATE_TRAP_UNMAP_MODEL_PASS", GPU_operations=0,
                  scope="Actual unmap extracted from staged source; modeled PTE/removal APIs",
                  source_sha256=hashlib.sha256(source.encode()).hexdigest(), commands=rows)
    (output / "RESULT.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(status=report["status"], GPU_operations=0)))


if __name__ == "__main__":main()
