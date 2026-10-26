#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Link the LAB default-drawable CPU test to the actual Mesa i386 archive.

No X connection, hardware access or GPU submissions. Run after staging/building
the current driver into the supplied Mesa25.2 cache. No source is rebuilt or
installed by this runner except its private test executable.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--cache", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--qemu", type=Path, required=True)
p.add_argument("--plan-only", action="store_true")
p.add_argument("--ubsan", action="store_true",
               help="Instrument the frontend and probe; the Mesa archive stays separately built")
a = p.parse_args()
r = Path(__file__).resolve().parents[2]
m = a.cache.resolve()
s, b = m / "mesa-25.2.0", m / "build-i386"
d = a.output.resolve()
d.mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env.update(json.loads((m / "build-environment.json").read_text()))
record = next(x for x in json.loads((b / "compile_commands.json").read_text())
              if x["file"].endswith("/remesa_nir.c"))
original = record.get("arguments") or shlex.split(record["command"])
command, skip = [], False
for arg in original:
    if skip:
        skip = False
        continue
    if arg in ("-o", "-MF", "-MQ"):
        skip = True
        continue
    if arg in ("-c", "-MD", "-DNDEBUG") or arg == record["file"]:
        continue
    command.append(arg)
command += ["-UNDEBUG", "-DREMESA_WITH_MESA_HEADERS", "-DREMESA_WITH_NIR",
            "-DREMESA_DRAWABLE_REAL_GL"]
if a.ubsan:
    command += ["-fsanitize=undefined", "-fno-sanitize-recover=undefined",
                "-static-libubsan"]
command += ["-I" + str(x) for x in (
    r / "mesa/sgx535", r / "tools", s / "src/gallium/auxiliary",
    s / "src/mesa", b / "src/mesa", s / "src/mapi", b / "src/mapi")]
sources = [r / "tools/remesa/test_opengl_lab_drawable.c",
           r / "tools/remesa/opengl_lab_drawable.c"]
command += list(map(str, sources)) + ["-Wl,--start-group"]
libraries = ["util/libmesa_util_simd.a", "util/libmesa_util.a", "util/libxmlconfig.a",
             "loader/libloader.a", "compiler/libcompiler.a", "mesa/libmesa_sse41.a",
             "mesa/libmesa.a", "gallium/auxiliary/libgallium.a",
             "gallium/auxiliary/libgalliumvl.a", "gallium/frontends/dri/libdri.a",
             "gallium/winsys/sw/null/libws_null.a", "gallium/winsys/sw/dri/libswdri.a",
             "gallium/winsys/sw/kms-dri/libswkmsdri.a", "gallium/winsys/sw/wrapper/libwsw.a",
             "gallium/auxiliary/pipe-loader/libpipe_loader_static.a", "compiler/nir/libnir.a",
             "compiler/spirv/libvtn.a", "compiler/glsl/libglsl.a", "compiler/glsl/glcpp/libglcpp.a",
             "mapi/shared-glapi/libglapi.a", "util/blake3/libblake3.a",
             "c11/impl/libmesa_util_c11.a", "gallium/drivers/sgx535/libremesa_loader.a"]
archives = [b / "src" / x for x in libraries]
if any(not x.is_file() for x in archives):
    raise SystemExit("A required Mesa archive is missing")
command += list(map(str, archives)) + ["-Wl,--end-group"]
exe = d / "actual-context-i386"
command += ["-L" + str(m / "i386-deps/usr/lib/i386-linux-gnu"), "-ldrm", "-lm",
            "-ldl", "-latomic", "-pthread",
            str(m / "build-tools/native/usr/i686-linux-gnu/lib/libstdc++.so.6.0.33"),
            "-Wl,-z,noexecstack", "-o", str(exe)]
runner = [str(a.qemu.resolve()), "-L", str(m / "runtime"), str(exe)]
(d / "COMMANDS.json").write_text(json.dumps({"compile": command, "run": runner}, indent=2) + "\n")
if a.plan_only:
    raise SystemExit(0)
identity_paths = sources + [r / "tools/remesa/opengl_lab_drawable.h"] + archives
identities = {str(x): hashlib.sha256(x.read_bytes()).hexdigest() for x in identity_paths}
fresh = subprocess.run(["ninja", "-C", str(b), "-n",
                        "src/gallium/drivers/sgx535/libremesa_loader.a"],
                       env=env, capture_output=True, text=True)
(d / "archive-freshness.log").write_text(fresh.stdout + fresh.stderr)
if fresh.returncode or "no work to do" not in fresh.stdout:
    raise SystemExit("Driver archive needs rebuilding; refusing a stale test")
compiled = subprocess.run(command, cwd=b, env=env, capture_output=True, text=True)
(d / "compile.log").write_text(compiled.stdout + compiled.stderr)
if compiled.returncode:
    raise SystemExit(compiled.returncode)
run = subprocess.run(runner, env=env, capture_output=True, text=True, timeout=120)
(d / "run.log").write_text(run.stdout + run.stderr)
if any(hashlib.sha256(Path(x).read_bytes()).hexdigest() != value
       for x, value in identities.items()):
    raise SystemExit("Inputs changed during test")
(d / "TEST_RESULTS.json").write_text(json.dumps({
    "exit": run.returncode, "stdout": run.stdout, "stderr": run.stderr,
    "input_sha256": identities, "GPU_submissions": 0,
    "frontend_probe_ubsan": a.ubsan,
    "scope": "Actual Mesa LAB context/default framebuffer; CPU clear and mock publication, no GLX"}, indent=2) + "\n")
print(run.stdout, end="")
raise SystemExit(run.returncode)
