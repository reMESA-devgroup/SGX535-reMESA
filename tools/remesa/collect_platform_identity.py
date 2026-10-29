#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read-only platform inventory; never opens DRM, PCI resource or MMIO devices."""
import argparse
import json
import os
from pathlib import Path
import re


def read(path):
    try:
        return path.read_text().strip()
    except (OSError, UnicodeError):
        return None


def children(path):
    try:
        return sorted(path.iterdir())
    except OSError:
        return None


def collect():
    uname = os.uname()
    release = {}
    for line in (read(Path('/etc/os-release')) or '').splitlines():
        key, separator, value = line.partition('=')
        if separator and key in ('ID', 'VERSION_ID', 'PRETTY_NAME'):
            release[key] = value.strip('"')
    cpu = {}
    for line in (read(Path('/proc/cpuinfo')) or '').splitlines():
        key, separator, value = line.partition(':')
        key = key.strip()
        if separator and key in ('model name', 'flags', 'Features') and key not in cpu:
            cpu[key] = value.strip()
    memory = None
    for line in (read(Path('/proc/meminfo')) or '').splitlines():
        if line.startswith('MemTotal:'):
            memory = line.partition(':')[2].strip()
            break
    dmi = {name: read(Path('/sys/class/dmi/id') / name) for name in
           ('sys_vendor', 'product_name', 'product_version', 'board_name',
            'bios_vendor', 'bios_version', 'bios_date')}
    display = []
    pci_entries = children(Path('/sys/bus/pci/devices'))
    if pci_entries is not None:
        for device in pci_entries:
            class_id = read(device / 'class')
            try:
                if int(class_id or '0', 16) >> 16 != 3:
                    continue
            except ValueError:
                continue
            item = {'pci_address': device.name}
            item.update({name: read(device / name) for name in
                         ('vendor', 'device', 'revision', 'class',
                          'subsystem_vendor', 'subsystem_device', 'boot_vga')})
            try:
                item['bound_driver'] = (device / 'driver').resolve(strict=True).name
            except OSError:
                item['bound_driver'] = None
            display.append(item)
    cards = []
    drm_entries = children(Path('/sys/class/drm'))
    if drm_entries is not None:
        for card in drm_entries:
            if not re.fullmatch(r'card[0-9]+', card.name):
                continue
            try:
                owner = (card / 'device').resolve(strict=True).name
            except OSError:
                owner = None
            cards.append({'name': card.name, 'device': owner})
    module = Path('/sys/module/gma500_gfx')
    controls = {name: read(module / 'parameters' / name) for name in
                ('remesa_memory_enabled', 'remesa_retained_lab_enabled',
                 'remesa_twod_copy_enabled', 'remesa_twod_fill_enabled',
                 'remesa_private_probe_enabled')}
    return {
        'format': 'SGX535_PLATFORM_IDENTITY_V1',
        'scope': 'Identity only; no driver installation, device ioctl, MMIO, rendering or acceleration qualification',
        'GPU_operations': 0,
        'kernel': {'sysname': uname.sysname, 'release': uname.release,
                   'version': uname.version, 'machine': uname.machine},
        'distribution': release, 'dmi': dmi, 'cpu': cpu, 'memory_total': memory,
        'display_pci_devices': display, 'drm_cards': cards,
        'pci_inventory_readable': pci_entries is not None,
        'drm_inventory_readable': drm_entries is not None,
        'poulsbo_pci_candidates': [item['pci_address'] for item in display
                                  if item['vendor'] == '0x8086' and item['device'] == '0x8108'],
        'SGX_core_revision': 'NOT_READ; PCI revision does not establish SGX535 core/revision',
        'gma500_module_loaded': module.is_dir(),
        'gma500_module_version': read(module / 'version'),
        'remesa_controls_read_if_available': controls,
        'limitations': ['Unavailable/protected files are reported as null.',
                        'Missing parameter files do not prove live N/N controls.',
                        'No hostname, serial number, UUID, network address, command line or full boot log is collected.'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='New JSON file; refuses to overwrite an existing file')
    args = parser.parse_args()
    report = json.dumps(collect(), ensure_ascii=False, indent=2) + '\n'
    if args.output:
        with args.output.open('x', encoding='utf-8') as file:
            file.write(report)
        print('Identidade salva em ' + str(args.output))
    else:
        print(report, end='')


if __name__ == '__main__':
    main()
