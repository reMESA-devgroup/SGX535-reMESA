#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual guard/removal code with modeled DRM enter/unplug and pthread joining.
This does not test kernel SRCU itself, GPU retirement or resource reclamation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from test_private_source_mmio import extract


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--module", type=Path, required=True)
    p.add_argument("--baseline-module", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = a.output.resolve();output.mkdir(parents=True, exist_ok=False)
    driver = (a.module / "psb_drv.c").read_text()
    old = (a.baseline_module / "psb_drv.c").read_text()
    memory = (a.module / "remesa_gem_memory.c").read_text()
    (output / "remove_actual.inc").write_text(extract(driver, "psb_pci_remove"))
    (output / "baseline_remove_actual.inc").write_text(extract(old, "psb_pci_remove").replace("psb_pci_remove(", "baseline_remove("))
    (output / "color_fault_actual.inc").write_text(extract(memory, "color_fault"))
    test = root / "kernel/remesa/gma500/tests/test_private_entry_guard.c"
    rows = []
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE,(0,0))
        resource.setrlimit(resource.RLIMIT_AS,(768*1024**2,)*2)
    for variant, flags in (("native", ["-O2"]), ("ubsan", ["-O1", "-fsanitize=undefined", "-fno-sanitize-recover=all"])):
        for compat in (False, True):
            name = variant+("-compat" if compat else "-no-compat")
            executable = output / name
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                       *(["-DCONFIG_COMPAT=1"] if compat else []),
                       "-I"+str(output), "-I"+str(a.module.resolve()), str(test),
                       "-pthread", "-o", str(executable)]
            for label, argv in ((name+"-build", command), (name+"-run", [str(executable)])):
                result = subprocess.run(argv,capture_output=True,text=True,timeout=60,preexec_fn=limits)
                (output / (label+".log")).write_text(result.stdout+result.stderr)
                rows.append(dict(label=label,argv=argv,exit=result.returncode))
                if result.returncode or result.stderr:raise RuntimeError(label+": "+result.stderr)
    inputs = [test,a.module / "psb_drv.c",a.module / "remesa_gem_memory.c",
              a.module / "remesa_private_entry_guard.inc",a.baseline_module / "psb_drv.c"]
    report = dict(status="PRIVATE_CPU_ENTRY_AND_UNPLUG_JOIN_MODEL_PASS",GPU_operations=0,
                  scope="Actual wrappers/remove/color fault; DRM contract and backing modeled, real pthread joining",
                  sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in inputs},commands=rows)
    (output / "RESULT.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(dict(status=report["status"],GPU_operations=0)))


if __name__ == "__main__":main()
