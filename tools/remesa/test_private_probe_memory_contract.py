#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Exercise Python packets against the actual C memory ioctl in a host shim.

CPU-only: Linux GEM/MMU/PM APIs are modeled. No device or GPU is involved.
Requires the retained native module sources and their compatible GEM test shim.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import resource
import subprocess
from unittest.mock import patch

import private_probe as probe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--shim", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    bridge = output / "memory-contract.c"
    bridge.write_text('''/* SPDX-License-Identifier: GPL-2.0-only */
/* Actual memory ioctl; only host backing/kernel APIs are modeled. */
#define REMESA_MEMORY_TEST
#include "remesa_gem_memory.c"
static struct test_mmu contract_mmu={true,{0}};
static struct drm_psb_private contract_priv={&contract_mmu,(void*)1,
 {0x28000000,32,0xe0000000}};
static struct drm_device contract_dev={&contract_priv};
static struct drm_file contract_file={1};
int contract_dispatch(void *packet) {
 memory_enabled=true;
 return remesa_gem_memory_ioctl(&contract_dev,packet,&contract_file);
}
void contract_start(void) {next_context=0x100000000ULL;}
unsigned contract_resources(void) {
 return gems+pins+ptes+device_refs+module_refs+power+claimed+(endpoint!=NULL);
}
unsigned contract_size(void) {return sizeof(struct remesa_memory_request);}
''')
    library = output / "memory-contract.so"
    argv = [args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
            "-Wno-unused-function", "-Wno-missing-field-initializers", "-shared", "-fPIC",
            "-I"+str(args.shim.resolve().parent), "-I"+str(args.native.resolve()),
            "-I"+str(root / "include"), str(bridge), "-o", str(library)]
    def limits():
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        resource.setrlimit(resource.RLIMIT_AS, (768*1024**2,)*2)
    build = subprocess.run(argv, capture_output=True, text=True, timeout=60, preexec_fn=limits)
    (output / "build.stdout").write_text(build.stdout)
    (output / "build.stderr").write_text(build.stderr)
    (output / "COMMAND.json").write_text(json.dumps(dict(argv=argv, exit=build.returncode), indent=2)+"\n")
    if build.returncode:
        raise RuntimeError(build.stderr)
    c = ctypes.CDLL(str(library))
    c.contract_dispatch.argtypes = [ctypes.c_void_p]
    c.contract_dispatch.restype = ctypes.c_int
    c.contract_size.restype = c.contract_resources.restype = ctypes.c_uint
    assert c.contract_size() == probe.MEMORY.size
    c.contract_start()
    calls = []
    def ioctl(fd, number, packet, mutate):
        assert fd == 123 and mutate and number == probe.ioctl_number(2, probe.MEMORY.size)
        before = probe.MEMORY.unpack(packet)
        buffer = (ctypes.c_ubyte * len(packet)).from_buffer(packet)
        result = c.contract_dispatch(buffer)
        calls.append(dict(action=before[1], context=before[4], result=result))
        if result < 0:
            raise OSError(-result, "actual memory ioctl rejected request")
        return result
    with patch.object(probe.fcntl, "ioctl", side_effect=ioctl):
        invalid = probe.Memory(123, 123456)
        try:
            invalid.request(probe.OPEN)
        except OSError as error:
            assert error.errno == 22
        else:
            raise AssertionError("kernel accepted userspace-chosen context")
        memory = probe.Memory(123, 0)
        memory.context = memory.request(probe.OPEN)[4]
        assert memory.context == 0x100000001
        rid = memory.request(probe.CREATE, size=8192)[2]
        memory.request(probe.PIN, rid)
        memory.request(probe.MAP, rid, va=0x40001000)
        data = bytes(range(256))*32
        memory.upload(rid, data)
        assert memory.read(rid, len(data)) == data
        info = memory.info(rid)
        assert info[1:3] == (8192, 0x40001000)
        assert info[6:9] == (0, 0, 0) and info[9:13] == (1, 2, 0, 2)
        memory.request(probe.UNMAP, rid)
        memory.request(probe.UNPIN, rid)
        memory.request(probe.DESTROY, rid)
        memory.request(probe.CLOSE)
        assert c.contract_resources() == 0
    sources = [root / "tools/remesa/private_probe.py", Path(__file__).resolve(),
               args.native / "remesa_gem_memory.c", args.native / "remesa_gem_memory.h",
               args.shim, bridge]
    report = dict(status="ACTUAL_MEMORY_IOCTL_PYTHON_CONTRACT_PASS", GPU_operations=0,
                  model_scope="Linux GEM/MMU/PM APIs only; actual memory ioctl and Python packets",
                  calls=calls, resources_after_close=0,
                  source_sha256={str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in sources})
    (output / "RESULT.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(dict(status=report["status"], GPU_operations=0, result=str(output / "RESULT.json"))))


if __name__ == "__main__":
    main()
