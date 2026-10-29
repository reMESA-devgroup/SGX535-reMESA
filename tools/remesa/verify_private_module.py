#!/usr/bin/env python3
"""Read-only ELF checks for the offline private module, not hardware approval."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def verify(path, kernel_release, require_serialized_source_mmio=False):
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    if raw[:6] != b"\x7fELF\x01\x01" or struct.unpack_from("<H", raw, 18)[0] != 3:
        raise ValueError("expected ELF32 little-endian EM_386")
    offset = struct.unpack_from("<I", raw, 32)[0]
    stride, count, strings_index = struct.unpack_from("<3H", raw, 46)
    if stride != 40 or not count or strings_index >= count or offset + stride * count > len(raw):
        raise ValueError("invalid ELF section table")
    sections = [struct.unpack_from("<10I", raw, offset + i * stride) for i in range(count)]

    def contents(section):
        if section[4] + section[5] > len(raw):
            raise ValueError("section exceeds file")
        return raw[section[4]:section[4] + section[5]]

    def string(data, position):
        if position >= len(data):
            raise ValueError("string exceeds table")
        return data[position:].split(b"\0", 1)[0].decode()

    names = contents(sections[strings_index])
    metadata, symbols = {}, {}
    for section in sections:
        if string(names, section[0]) == ".modinfo":
            for item in contents(section).split(b"\0"):
                if b"=" in item:
                    key, value = item.split(b"=", 1)
                    if key in (b"vermagic", b"depends", b"remesa_easter_egg"):
                        metadata[key.decode()] = value.decode()
        if section[1] != 2:
            continue
        if section[9] != 16 or section[6] >= count or section[5] % 16:
            raise ValueError("invalid ELF symbol table")
        strings = contents(sections[section[6]])
        data = contents(section)
        for at in range(0, len(data), 16):
            name, value, size, info, other, index = struct.unpack_from("<3I2BH", data, at)
            if name:
                symbols[string(strings, name)] = (index, info & 15, size)
    required = ("remesa_gem_private_execute", "remesa_mmu_private_bind_locked",
                "remesa_mmu_private_restore_locked", "remesa_mmu_private_quarantine_locked",
                "sgx535_gma500_fixed_backend_begin_private", "remesa_twod_copy_ioctl",
                "remesa_twod_fill_ioctl")
    if require_serialized_source_mmio:
        required += ("remesa_private_source_write",)
    for name in required:
        index, kind, size = symbols.get(name, (0, 0, 0))
        if not 0 < index < count or kind != 2 or not size or not sections[index][2] & 4:
            raise ValueError(f"missing defined executable function: {name}")
    controls = ("memory_enabled", "retained_lab_enabled", "twod_copy_enabled", "twod_fill_enabled")
    if "private_probe_enabled" in symbols:
        controls += ("private_probe_enabled", "private_probe_consumed")
        index, kind, size = symbols.get("remesa_private_probe_ioctl", (0, 0, 0))
        if not 0 < index < count or kind != 2 or not size or not sections[index][2] & 4:
            raise ValueError("missing private probe handler")
    for name in controls:
        index, kind, size = symbols.get(name, (0, 0, 0))
        if not 0 < index < count or sections[index][1] != 8 or size != 1:
            raise ValueError(f"control is not a zero-initialized BSS bool: {name}")
    for name, (index, kind, size) in symbols.items():
        if not index and name.startswith(("remesa_gem_private_", "remesa_mmu_private_", "remesa_twod_", "remesa_private_source_")):
            raise ValueError(f"unresolved private driver symbol: {name}")
    if not metadata.get("vermagic", "").startswith(kernel_release + " "):
        raise ValueError("unexpected target kernel release")
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise RuntimeError("module changed during read-only verification")
    return dict(status="FINAL_ELF_AND_PRIVATE_SYMBOLS_PASS", module=str(path),
                module_sha256=digest, ELF="ELF32 little-endian EM_386",
                functions=list(required), disabled_zero_bss_controls=list(controls),
                modinfo=metadata, file_unchanged=True, GPU_operations=0, installed=False)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("module", type=Path)
    p.add_argument("--kernel-release", required=True)
    p.add_argument("--report", type=Path)
    p.add_argument("--require-serialized-source-mmio", action="store_true",
                   help="Require the new private source/MMIO function (symbol check, not runtime proof)")
    a = p.parse_args()
    if a.report and a.report.resolve() == a.module.resolve():
        p.error("report must not overwrite the module")
    report = json.dumps(verify(a.module, a.kernel_release, a.require_serialized_source_mmio), indent=2) + "\n"
    if a.report:
        a.report.write_text(report)
    print(report, end="")
