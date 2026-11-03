#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Actual authority/MMIO fragment with a pthread source lock and recorded MMIO.
CPU-only reduced state; no device opened or physical synchronization attested.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import resource
import subprocess


def extract(source, name):
    match = re.search(r"^[ \t]*[A-Za-z_][\w \t*]*?\b"+re.escape(name)+
                      r"[ \t]*\([^;{}]*\)[ \t\n]*\{", source, re.M)
    if not match:raise ValueError("missing C function definition: "+name)
    start, brace = match.start(), match.end()-1
    depth, end = 1, brace+1
    while depth:
        depth += (source[end] == "{")-(source[end] == "}")
        end += 1
    return source[start:end]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--baseline-module", type=Path, required=True)
    p.add_argument("--legacy-fixed-module", type=Path, help="Unmodified fixed_write baseline for the historical race comparison")
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = a.output.resolve();output.mkdir(parents=True, exist_ok=False)
    source = (a.baseline_module / "remesa_stream_source.c").read_text()
    fixed_path = (a.legacy_fixed_module or a.baseline_module) / "gma500_fixed_backend.c"
    fixed = fixed_path.read_text()
    # This older MMIO linearization model has no owned clear predecessor.
    # Use the actual inactive-lineage helpers; the active ledger has its own
    # actual-code/receipt-corruption tests in test_owned_fill_source.py.
    helpers = ""
    owned = a.baseline_module / "remesa_owned_fill_source.inc"
    if owned.exists():
        inactive = owned.read_text().split("#else\n", 1)[1]
        helpers = "\n".join(extract(inactive, n) for n in
                            ("source_lineage", "source_available"))+"\n"
    (output / "source_authority_actual.inc").write_text(helpers+"\n".join(extract(source, n) for n in
        ("valid", "lost", "remesa_route_source_write", "remesa_stream_source_poison")))
    (output / "old_write_actual.inc").write_text(extract(fixed, "fixed_write"))
    test = root / "kernel/remesa/gma500/tests/test_private_source_mmio.c"
    rows = []
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (768*1024**2,)*2)
    for name, flags in (("native", ["-O2"]), ("ubsan", ["-O1", "-fsanitize=undefined", "-fno-sanitize-recover=all"]), ("general", ["-O1", "-DREMESA_MMIO_GENERAL_MODEL=1", "-fsanitize=undefined", "-fno-sanitize-recover=all"])):
        executable = output / name
        command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", *flags,
                   "-I"+str(output), "-I"+str(a.baseline_module.resolve()),
                   "-I"+str(root / "kernel/remesa/gma500"), str(test),
                   str(a.baseline_module / "frozen_source_guard.c"),
                   "-pthread", "-o", str(executable)]
        for label, argv in ((name+"-build", command), (name+"-run", [str(executable)])):
            result = subprocess.run(argv, capture_output=True, text=True, timeout=60, preexec_fn=limits)
            (output / (label+".log")).write_text(result.stdout+result.stderr)
            rows.append(dict(label=label, argv=argv, exit=result.returncode))
            if result.returncode or result.stderr:raise RuntimeError(label+": "+result.stderr)
    paths = [test, root / "kernel/remesa/gma500/remesa_scene_backend.h", root / "kernel/remesa/gma500/remesa_scene_service.h", root / "kernel/remesa/gma500/remesa_private_source_mmio.inc",
             a.baseline_module / "remesa_stream_source.c", fixed_path,
             a.baseline_module / "frozen_source_guard.c"]
    if owned.exists():paths.append(owned)
    report = dict(status="PRIVATE_SOURCE_CPU_WRITE_LINEARIZATION_PASS", GPU_operations=0,
                  scope="Actual extracted authority/poison/old write and new fragment, real pthread locking; owner and MMIO modeled",
                  sources={str(f):hashlib.sha256(f.read_bytes()).hexdigest() for f in paths},commands=rows)
    (output / "RESULT.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(status=report["status"],GPU_operations=0)))


if __name__ == "__main__":main()
