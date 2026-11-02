#!/usr/bin/env python3
"""Stage the bounded 2D producer-reservation patch; never install or overwrite.

The input is the retained qualified native source, not the dirty legacy checkout.
Normal profiles integrate producer/IRQ exclusion. The private profile also
registers a default-disabled, fixed-profile qualification ioctl; it is not a
production submission interface and this tool never installs a module.
"""
from stage_scene_service import scene_service_components
from stage_scene_backend import backend_sources, source_code
import argparse
import hashlib
import json
from pathlib import Path

PINNED = {
    'accel_2d.c': '2de5bef67e6813fa2f238a1792fab42bd5dbf8942273ba5a462bc24e3e7a5b7e',
    'psb_drv.h': '6dce38176eccc8d7037783ec6c41d101c3c614068a3b7b11f40b06e1d1ca6261',
    'gma500_fixed_backend.c': '12d908c2b99c6f89eff92b9b1c69b7d0779ca7c430bfc14347c5cc67e476afd7',
    'psb_reg.h': 'a340d4fb7bb9df2403eb196639d930b8c56463ddc7a3754d152e6ed7ab1daef7',
    'psb_irq.c': '5b9962aa29ec24f844297bb2d0e4fa9b00fc3021ed46d00afbeaa26b326b3025',
    'mmu.c': 'cc722d7566ea3b2fa77252d96bc6eaff7166bd4331592f59d5d3bbc290c1a596',
    'mmu.h': '2f5a6b3edde79741234fa08fbc0b1e14045e42ae472a6e90ea6cb64f1c8b9514',
    'remesa_gem_memory.c': '0569fb6831a33ccef68fa6ba20995688ae2891a6bf22486ec9d3adb9573216c6',
    'remesa_gem_memory.h': 'c205d6a54962aab65f9f1cfa60a38c48eb04eb2f3c46f15bc5f70d25b7c3461b',
    'psb_drv.c': 'e9ce11abe31c5adf0d0134e0d4b520455661a0a7b490bb2ed9b02e1ac62488da',
    'Makefile': '6ab5c8c5819106db5894f2c0f81df55389bf2d3f3a45a0efa4cf0f8aabdbfd9e',
}


def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError('native integration anchor is missing or ambiguous')
    return source.replace(old, new, 1)



def job_capacity_sources(source, header):
    """Expand kernel-only complete job holds; frozen native inputs stay intact."""
    replacements = {'cpu_views[16]': ('cpu_views[REMESA_GEM_JOB_BINDINGS]', 1),
                    'ids[16]': ('ids[REMESA_GEM_JOB_BINDINGS]', 1),
                    'count>16': ('count>REMESA_GEM_JOB_BINDINGS', 1),
                    'uses[16]': ('uses[REMESA_GEM_JOB_BINDINGS]', 5),
                    'added[16]': ('added[REMESA_GEM_JOB_BINDINGS]', 1)}
    for old, (new, count) in replacements.items():
        if source.count(old) != count:
            raise ValueError('retained job capacity anchor missing/ambiguous: ' + old)
        source = source.replace(old, new)
    source = replace_once(source, '#define OBJECTS 32\n',
                          '#define OBJECTS 32\n'
                          '#if REMESA_GEM_JOB_BINDINGS > OBJECTS\n'
                          '#error retained job capacity exceeds backing slots\n#endif\n')
    header = replace_once(header, '#define REMESA_GEM_MEMORY_H\n',
                          '#define REMESA_GEM_MEMORY_H\n#include "remesa_gem_job_limits.h"\n')
    return source, header


def dpm_origin_source(source):
    """Track only provable fresh zero contents; never infer GPU visibility."""
    edits = [
        ('bool live,pinned,reserved,lease,write,quarantine,initialized,lease_borrowed;',
         'bool live,pinned,reserved,lease,write,quarantine,initialized,lease_borrowed;\n bool zero_pristine;'),
        ('unsigned command_bytes,count;u32 generation,ids[REMESA_GEM_JOB_BINDINGS];',
         'unsigned command_bytes,count;u32 generation,ids[REMESA_GEM_JOB_BINDINGS];u32 dpm_mask,region_mask;\n u64 dpm_previous_operation;const void *private_scene_owner;'),
        ('b->live=false;b->initialized=false;b->lease_borrowed=false;',
         'b->live=false;b->initialized=false;b->lease_borrowed=false;b->zero_pristine=false;'),
        ('b->initialized=true;b->ta_cpu_cached=true;b->ta_cpu_dirty=true;',
         'b->initialized=true;b->zero_pristine=true;b->ta_cpu_cached=true;b->ta_cpu_dirty=true;'),
        ('b->pages=NULL;b->pinned=false;b->lease_borrowed=false;',
         'b->pages=NULL;b->pinned=false;b->lease_borrowed=false;b->zero_pristine=false;'),
        ('b->lease=true;b->write=r->flags;return 0;',
         'b->lease=true;b->write=r->flags;if(b->write)b->zero_pristine=false;return 0;'),
        ('j->command_bytes=bytes;j->count=count;j->live=true;',
         'j->command_bytes=bytes;j->count=count;j->dpm_mask=0;j->region_mask=0;j->dpm_previous_operation=0;j->private_scene_owner=NULL;j->live=true;'),
        ('if(capacity<j->count)return -ENOSPC;\n e=verify_job_bindings',
         'if(capacity<j->count)return -ENOSPC;\n if(j->dpm_mask||j->region_mask)return -EACCES;\n e=verify_job_bindings'),
        ('out[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,j->cpu_views[i]};',
         'b->zero_pristine=false;\n  out[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,j->cpu_views[i]};'),
        ('j->state=REMESA_JOB_ISSUED;\n /* Possible GPU writes',
         'for(slot=0;slot<j->count;slot++)s->bo[j->ids[slot]&31].zero_pristine=false;\n j->state=REMESA_JOB_ISSUED;\n /* Possible GPU writes'),
    ]
    for old, new in edits:
        source = replace_once(source, old, new)
    return source


def scene_action_components(root, native):
    """Share the proven CPU constructor unchanged; only include paths differ."""
    import re
    reference = (root / 'tools/psb-dri-re/frozen_kernel_contract.h').read_text()
    target = (native / 'frozen_kernel_contract.h').read_text()
    for kind in ['enum sgx535_frozen_reg_action_kind', 'struct sgx535_frozen_reg_action']:
        pattern = re.escape(kind) + r' \{.*?\n\};'
        a, b = re.search(pattern, reference, re.S), re.search(pattern, target, re.S)
        if not a or not b or a.group() != b.group():
            raise ValueError('scene action ABI mismatch: ' + kind)
    source = (root / 'tools/remesa/lab_scene_actions.c').read_text()
    for old, new in [('#include "lab_scene_actions.h"', '#include "remesa_scene_actions.h"'),
                     ('"remesa/scene_layout.h"', '"remesa_scene_layout.h"'),
                     ('"remesa/scene_dpm.h"', '"remesa_scene_dpm.h"')]:
        source = replace_once(source, old, new)
    header = (root / 'tools/remesa/lab_scene_actions.h').read_text()
    header = replace_once(header, '"../psb-dri-re/frozen_kernel_contract.h"', '"frozen_kernel_contract.h"')
    controls = (root / 'include/remesa/scene_control_words.h').read_text()
    controls = replace_once(controls, '"scene_layout.h"', '"remesa_scene_layout.h"')
    references = (root / 'include/remesa/ta_references.h').read_text()
    references = replace_once(references, '"scene_layout.h"', '"remesa_scene_layout.h"')
    upload = (root / 'include/remesa/state_upload.h').read_text()
    upload = replace_once(upload, '"ta_references.h"', '"remesa_ta_references.h"')
    vertex = (root / 'include/remesa/vertex_fetch.h').read_text()
    vertex = replace_once(vertex, '"ta_references.h"', '"remesa_ta_references.h"')
    fragment = (root / 'include/remesa/fragment_fetch.h').read_text()
    fragment = replace_once(fragment, '"ta_references.h"', '"remesa_ta_references.h"')
    background = (root / 'include/remesa/background_fetch.h').read_text()
    background = replace_once(background, '"fragment_fetch.h"', '"remesa_fragment_fetch.h"')
    auxiliary = (root / 'include/remesa/scene_auxiliary.h').read_text()
    auxiliary = replace_once(auxiliary, '"ta_references.h"', '"remesa_ta_references.h"')
    event = (root / 'include/remesa/event_fetch.h').read_text()
    event = replace_once(event, '"ta_references.h"', '"remesa_ta_references.h"')
    bodies = (root / 'include/remesa/scene_use_bodies.h').read_text()
    bodies = replace_once(bodies, '"scene_auxiliary.h"', '"remesa_scene_auxiliary.h"')
    initial=(root / 'include/remesa/scene_initial.h').read_text().replace('"scene_layout.h"','"remesa_scene_layout.h"')
    return {'remesa_scene_initial.h': initial.encode(), 'remesa_scene_use_bodies.h': bodies.encode(), 'remesa_scene_auxiliary.h': auxiliary.encode(), 'remesa_event_fetch.h': event.encode(), 'remesa_background_fetch.h': background.encode(), 'remesa_fragment_fetch.h': fragment.encode(), 'remesa_vertex_fetch.h': vertex.encode(), 'remesa_state_upload.h': upload.encode(), 'remesa_ta_references.h': references.encode(), 'remesa_scene_control_words.h': controls.encode(),
            'remesa_scene_actions.c': source.encode(), 'remesa_scene_actions.h': header.encode(),
            'remesa_scene_layout.h': (root / 'include/remesa/scene_layout.h').read_bytes(),
            'remesa_scene_dpm.h': (root / 'include/remesa/scene_dpm.h').read_bytes()}


def scene_use_components(root):
    """Build the existing integer USE planner with kernel-compatible type names.
    No arithmetic or program bytes change. The lone C99 loop declaration is
    moved before the loop for older kernel compilers.
    """
    import re
    header = (root / 'mesa/sgx535/remesa_program_registers.h').read_text()
    source = (root / 'mesa/sgx535/remesa_program_registers.c').read_text()
    types = '''#ifdef __KERNEL__
#include <linux/types.h>
typedef u32 remesa_reg_u32;
typedef u64 remesa_reg_u64;
typedef unsigned long remesa_reg_uptr;
#else
#include <stdint.h>
typedef uint32_t remesa_reg_u32;
typedef uint64_t remesa_reg_u64;
typedef uintptr_t remesa_reg_uptr;
#endif
#define REMESA_REG_U32_C(x) x##U
#define REMESA_REG_U64_C(x) x##ULL
#define REMESA_REG_U32_MAX (~(remesa_reg_u32)0)
#define REMESA_REG_UPTR_MAX (~(remesa_reg_uptr)0)'''
    header = replace_once(header, '#include <stdint.h>', '@REMESA_TYPES@')
    source = replace_once(source, '#include <errno.h>\n#include <stddef.h>',
        '#ifdef __KERNEL__\n#include <linux/errno.h>\n#include <linux/stddef.h>\n#define ENOTSUP EOPNOTSUPP\n#else\n#include <errno.h>\n#include <stddef.h>\n#endif')
    for old, new in [('uint32_t', 'remesa_reg_u32'), ('uint64_t', 'remesa_reg_u64'),
                     ('uintptr_t', 'remesa_reg_uptr'), ('UINT32_C', 'REMESA_REG_U32_C'),
                     ('UINT64_C', 'REMESA_REG_U64_C'), ('UINT32_MAX', 'REMESA_REG_U32_MAX'),
                     ('UINTPTR_MAX', 'REMESA_REG_UPTR_MAX')]:
        header = re.sub(r'\b' + old + r'\b', new, header)
        source = re.sub(r'\b' + old + r'\b', new, source)
    header = replace_once(header, '@REMESA_TYPES@', types)
    source = replace_once(source, ' for (unsigned i = 0; i < 2; ++i) {',
        ' unsigned i;\n for (i = 0; i < 2; ++i) {')
    return {'remesa_program_registers.h': header.encode(),
            'remesa_program_registers.c': source.encode(),
            'remesa_scene_use_limits.h': (root / 'include/remesa/scene_use_limits.h').read_bytes()}


def private_mmu_header_source(header):
    return replace_once(header, '\tstruct drm_device *dev;\n};',
        '\tstruct drm_device *dev;\n'
        '\t/* MMU writer semaphore protects this retained root owner. */\n'
        '\tstruct remesa_mmu_private *remesa_private_owner;\n};')


def private_mmu_flush_source(mmu):
    """Keep original flush writes; provide an already-locked private variant."""
    start = mmu.index('void psb_mmu_flush(struct psb_mmu_driver *driver)\n')
    end = mmu.index('\nvoid psb_mmu_set_pd_context(', start)
    original = mmu[start:end]
    body = replace_once(original,
        'void psb_mmu_flush(struct psb_mmu_driver *driver)',
        'static void remesa_mmu_original_flush_locked(struct psb_mmu_driver *driver)')
    body = replace_once(body, '\tdown_write(&driver->sem);\n', '')
    body = replace_once(body, '\tup_write(&driver->sem);\n', '')
    wrapper = ('void psb_mmu_flush(struct psb_mmu_driver *driver)\n{\n'
               '\tdown_write(&driver->sem);\n'
               '\t/* A queued default flush must not touch a retained private root. */\n'
               '\tif (driver->remesa_private_owner)\n'
               '\t\tatomic_set(&driver->needs_tlbflush, 1);\n'
               '\telse\n'
               '\t\tremesa_mmu_original_flush_locked(driver);\n'
               '\tup_write(&driver->sem);\n}\n')
    result = mmu[:start] + body + '\n' + wrapper + mmu[end:]
    return replace_once(result, '\tdriver->dev = dev;\n',
        '\tdriver->dev = dev;\n\tdriver->remesa_private_owner = NULL;\n')


def private_fixed_sources(fixed, header):
    """Preserve original begin/source/retirement gates, add an owned IRQ loan."""
    fixed = '#include "remesa_private_source_mmio.h"\n' + fixed
    fixed = replace_once(fixed,
        'static int fixed_write(void *context, sgx535_u32 offset, sgx535_u32 value)\n'
        '{\n'
        '    struct sgx535_gma500_fixed_backend *backend = context;\n'
        '    struct drm_psb_private *dev_priv = backend->owner->dev->dev_private;\n',
        'static int fixed_write(void *context, sgx535_u32 offset, sgx535_u32 value)\n'
        '{\n'
        '    struct sgx535_gma500_fixed_backend *backend = context;\n'
        '    struct drm_psb_private *dev_priv;\n'
        '\n'
        '    if (backend && backend->private_loan)\n'
        '        return remesa_private_source_write(backend, offset, value);\n'
        '    dev_priv = backend->owner->dev->dev_private;\n')
    fixed = replace_once(fixed,
        'int sgx535_gma500_fixed_backend_begin(\n'
        '    struct sgx535_gma500_fixed_backend *backend,\n'
        '    struct sgx535_gma500_owner *owner)',
        'static int remesa_fixed_backend_begin(\n'
        '    struct sgx535_gma500_fixed_backend *backend,\n'
        '    struct sgx535_gma500_owner *owner,\n'
        '    struct remesa_twod_reservation *guard, const void *token)')
    ready = '(guard ? remesa_twod_irq_handoff_ready_locked(guard, token, owner->dev) : remesa_twod_irq_busy_locked())'
    fixed = replace_once(fixed,
        'if (fixed_irq_owner || remesa_twod_irq_busy_locked() ||\n'
        '        !remesa_route_source_begin(backend, owner->dev))',
        'if (fixed_irq_owner || ' + ready + ' ||\n'
        '        !remesa_route_source_begin(backend, owner->dev))')
    fixed = replace_once(fixed,
        '    if (fixed_irq_owner || remesa_twod_irq_busy_locked()) {\n',
        '    if (fixed_irq_owner || ' + ready + ') {\n')
    fixed = replace_once(fixed, '    fixed_irq_owner = backend;\n',
        '    if (guard) {\n'
        '        int e = remesa_twod_irq_handoff_fixed_locked(guard, token, backend, owner->dev);\n'
        '        if (e) {\n'
        '            backend->power_held = false; backend->active = false; backend->owner = NULL;\n'
        '            sgx535_gma500_fixed_irq_unlock(flags); return e;\n'
        '        }\n'
        '    } else fixed_irq_owner = backend;\n')
    wrappers = (
        'int sgx535_gma500_fixed_backend_begin(\n'
        '    struct sgx535_gma500_fixed_backend *b, struct sgx535_gma500_owner *o)\n'
        '{ return remesa_fixed_backend_begin(b, o, NULL, NULL); }\n'
        'int sgx535_gma500_fixed_backend_begin_guarded(\n'
        '    struct sgx535_gma500_fixed_backend *b, struct sgx535_gma500_owner *o,\n'
        '    struct remesa_twod_reservation *guard, const void *token)\n'
        '{ if (!guard || !token) return -EINVAL;\n'
        '  return remesa_fixed_backend_begin(b, o, guard, token); }\n\n')
    fixed = replace_once(fixed, 'int sgx535_gma500_fixed_backend_end(\n',
                         wrappers + 'int sgx535_gma500_fixed_backend_end(\n')
    fixed = replace_once(fixed,
        '    struct sgx535_gma500_fixed_backend *backend)\n{\n    unsigned long flags;\n',
        '    struct sgx535_gma500_fixed_backend *backend)\n{\n    unsigned long flags;\n    int loan_error;\n')
    fixed = replace_once(fixed, '    fixed_irq_owner = NULL;\n',
        '    loan_error = remesa_twod_irq_return_fixed_locked(backend);\n'
        '    if (loan_error) { sgx535_gma500_fixed_irq_unlock(flags); return loan_error; }\n')
    fixed = replace_once(fixed,
        '        backend->pending_status1 |= status1 & ~fixed_benign_status1;\n',
        '        backend->pending_status1 |= status1 & ~fixed_benign_status1;\n'
        '#ifdef REMESA_PRIVATE_IRQ_HANDOFF\n'
        '        backend->pending_status1 |= remesa_twod_irq_guarded_extra_locked(backend, status1);\n'
        '#endif\n')
    header = replace_once(header, '\n#endif\n',
        '\n/* Trusted kernel caller retains exact guarded MMU loan and original owner.\n'
        ' * Original source-history admission and retirement gates remain required. */\n'
        'struct remesa_twod_reservation;\n'
        'int sgx535_gma500_fixed_backend_begin_guarded(\n'
        ' struct sgx535_gma500_fixed_backend *, struct sgx535_gma500_owner *,\n'
        ' struct remesa_twod_reservation *, const void *);\n#endif\n')
    header = replace_once(header, '#include "gma500_bo_owner.h"\n',
        '#include "gma500_bo_owner.h"\n#include "remesa_gem_memory.h"\n'
        'struct remesa_gem_private;\n')
    header = replace_once(header, '    unsigned long deadline;\n',
        '    unsigned long deadline;\n'
        '    /* Private integration only; views copied, backing remains loan-owned. */\n'
        '    struct remesa_gem_private *private_loan;\n'
        '    struct remesa_memory_view private_views[SGX535_BO_COUNT];\n'
        '    bool prefire_load_active; /* fixed_irq_lock, not last_stage */\n')
    fixed = replace_once(fixed,
        '        ret = fixed_actions(backend, payload, expected, 29);\n',
        '        {\n'
        '            unsigned long flags = sgx535_gma500_fixed_irq_lock();\n'
        '            backend->prefire_load_active = true;\n'
        '            sgx535_gma500_fixed_irq_unlock(flags);\n'
        '            ret = fixed_actions(backend, payload, expected, 29);\n'
        '            flags = sgx535_gma500_fixed_irq_lock();\n'
        '            backend->prefire_load_active = false;\n'
        '            sgx535_gma500_fixed_irq_unlock(flags);\n'
        '        }\n')
    header = replace_once(header, '\n#endif\n',
        '\n/* Caller holds endpoint mutex THEN MMU write semaphore throughout all\n'
        ' * stages and retains whole-device exclusion through retirement/restore.\n'
        ' * Exact borrowed scene only; no executor registration or root bind. */\n'
        'int sgx535_gma500_fixed_backend_begin_private(\n'
        ' struct sgx535_gma500_fixed_backend *, struct sgx535_gma500_owner *,\n'
        ' struct remesa_gem_private *, const struct remesa_memory_view *, unsigned);\n'
        '#endif\n')
    fixed = replace_once(fixed, 'static int fixed_run(void *context,',
        '#include "remesa_fixed_private.inc"\n\nstatic int fixed_run(void *context,')
    fixed = replace_once(fixed,
        '        ret = (remesa_stream_source_owned(backend) ? remesa_stream_owner_flush(owner) : sgx535_gma500_owner_flush_user_images(owner));',
        '        ret = backend->private_loan ? remesa_fixed_private_run(backend, stage) :\n'
        '            (remesa_stream_source_owned(backend) ? remesa_stream_owner_flush(owner) : sgx535_gma500_owner_flush_user_images(owner));')
    for case in ('PUBLISH_TRANSLATIONS', 'DEVICE_MAINTAIN'):
        marker = '    case SGX535_FIXED_' + case + ':\n'
        fixed = replace_once(fixed, marker, marker +
            '        if (backend->private_loan) {\n'
            '            if (payload || count) break;\n'
            '            ret = remesa_fixed_private_run(backend, stage);\n'
            '            break;\n'
            '        }\n')
    fixed = replace_once(fixed,
        '    backend->power_held = false;\n    backend->active = false;\n    return 0;\n}',
        '    backend->power_held = false;\n    backend->active = false;\n'
        '    backend->private_loan = NULL;\n'
        '    memset(backend->private_views, 0, sizeof(backend->private_views));\n'
        '    return 0;\n}')
    return fixed, header


def private_stream_source(source):
    source = replace_once(source, '#endif\nstatic DEFINE_SPINLOCK(source_lock);',
        '#endif\n#include "remesa_private_prefire_irq.h"\nstatic DEFINE_SPINLOCK(source_lock);')
    source = replace_once(source, ' else if(lease&&pending) {\n',
        ' else if(lease&&(pending||(a&(1U<<27)))) {\n')
    source = replace_once(source,
        '  if(operation->finished || operation->evidence.state>=SGX535_CAP_TERMINAL)lost();\n',
        '  if(operation->finished || operation->evidence.state>=SGX535_CAP_TERMINAL)lost();\n'
        '  /* IRQ capture owns fixed_irq_lock; preserve unexpected prefire\n'
        '   * events before the existing IRQ path acknowledges the status. */\n'
        '  if(!operation->backend->fire_possible &&\n'
        '     remesa_private_prefire_unexpected(a,d,operation->backend->prefire_load_active))lost();\n')
    return source + '\n#include "remesa_private_source_mmio.inc"\n'


def trap_gem_unmap_source(source):
    # Retained native psb_driver_load constructs the default PD with trap=1:
    # an invalid PTE returns -EINVAL. Any successful lookup after removal,
    # including a foreign PFN, must retain the VA/backing instead of freeing it.
    return replace_once(source,
        '   if((e && e!=-EINVAL) || (!e && pfn==page_to_pfn(b->pages[page]))) {b->quarantine=true;return -EIO;}\n',
        '   if(e!=-EINVAL) {b->quarantine=true;return -EIO;}\n')


def private_entry_sources(driver, memory):
    driver = '#include "remesa_private_file_lost.h"\n#include "remesa_private_gem_lifetime.h"\n#include "remesa_private_fbdev.h"\n' + driver
    driver = replace_once(driver,
        '\tstruct drm_psb_private *dev_priv = dev->dev_private;\n\n\tif (dev_priv) {',
        '\tstruct drm_psb_private *dev_priv = dev->dev_private;\n\tbool retained = false;\n\n'
        '\t/* FB registration can precede DRM registration. Once modeset\n'
        '\t * lists exist, error-unload must also join published CPU entries. */\n'
        '\tif (dev_priv && dev_priv->modeset && !drm_dev_is_unplugged(dev)) {\n'
        '\t\tdrm_dev_unplug(dev);\n\t\tremesa_private_stream_device_lost(dev);\n\t}\n'
        '\tif (dev_priv) {\n'
        '\t\tretained = remesa_private_memory_device_retained(dev)||\n'
        '\t\t           remesa_private_gem_device_retained(dev);')
    driver = replace_once(driver, '\t\tpsb_modeset_cleanup(dev);\n',
        '\t\tif (retained)\n\t\t\tremesa_private_fbdev_detach(dev);\n'
        '\t\telse\n\t\t\tpsb_modeset_cleanup(dev);\n')
    driver = replace_once(driver, '\t\tif (dev_priv->backlight_device)\n\t\t\tgma_backlight_exit(dev);',
        '\t\t/* IRQ uninstall has joined the only ASLE work producer. Join\n'
        '\t\t * queued/running brightness work before releasing its backlight.\n'
        '\t\t * A published header implies asle_work was initialized. Keep\n'
        '\t\t * the mapping until chip teardown has stopped the lid timer. */\n'
        '#ifdef CONFIG_ACPI\n'
        '\t\tif (dev_priv->opregion.header)\n'
        '\t\t\tcancel_work_sync(&dev_priv->opregion.asle_work);\n'
        '#endif\n'
        '\t\t/* The private lid worker may perform I2C brightness updates. */\n'
        '\t\tpsb_lid_timer_takedown(dev_priv);\n'
        '\t\tif (dev_priv->backlight_device)\n\t\t\tgma_backlight_exit(dev);')
    driver = replace_once(driver,
        '\t\tpsb_intel_opregion_fini(dev);\n\n\t\tif (dev_priv->pf_pd) {',
        '\t\tpsb_intel_opregion_fini(dev);\n\n'
        '\t\t/* Lost endpoints/GEM owners retain module/device references.\n'
        '\t\t * Keep the remaining SGX/MMU/GTT dependency graph;\n'
        '\t\t * IRQ/work/client teardown above is not a GPU stop receipt. */\n'
        '\t\tif (retained) {\n'
        '\t\t\tgma_power_uninit(dev);\n'
        '\t\t\treturn;\n'
        '\t\t}\n\n\t\tif (dev_priv->pf_pd) {')
    driver = replace_once(driver,
        '\tret = pci_enable_device(pdev);\n',
        '\t/* A retained global endpoint forbids rebinding before any PCI\n'
        '\t * enable, register write, reset or new MMU construction. */\n'
        '\tif (remesa_private_memory_rebind_blocked()||remesa_private_gem_rebind_blocked())\n'
        '\t\treturn -EBUSY;\n\n\tret = pci_enable_device(pdev);\n')
    driver = replace_once(driver, 'static void psb_pci_remove(struct pci_dev *pdev)\n',
        '#include "remesa_private_entry_guard.inc"\n\n'
        'static void psb_pci_remove(struct pci_dev *pdev)\n')
    driver = replace_once(driver,
        '\tdrm_dev_unregister(dev);\n\tpsb_driver_unload(dev);\n',
        '\t/* No endpoint/MMU lock across the SRCU join. This joins guarded\n'
        '\t * CPU entry only; retained GPU resources still need detach policy. */\n'
        '\tdrm_dev_unplug(dev);\n'
        '\t/* CPU entries have joined; revoke private ownership even when\n'
        '\t * userspace still holds its file. This does not stop SGX DMA. */\n'
        '\tremesa_private_stream_device_lost(dev);\n'
        '\tpsb_driver_unload(dev);\n')
    for old, new in [('\t.unlocked_ioctl = psb_unlocked_ioctl,',
                      '\t.unlocked_ioctl = remesa_private_unlocked_ioctl,'),
                     ('\t.compat_ioctl = drm_compat_ioctl,',
                      '\t.compat_ioctl = remesa_private_compat_ioctl,'),
                     ('\t.mmap = remesa_gem_color_mmap,',
                      '\t.mmap = remesa_private_mmap,'),
                     ('\t.fault = psb_gem_fault,',
                      '\t.fault = remesa_private_gem_fault,'),
                     ('\t.postclose = remesa_stream_postclose,',
                      '\t.postclose = remesa_private_postclose,'),
                     ('\t.lastclose = drm_fb_helper_lastclose,',
                      '\t.lastclose = remesa_private_lastclose,')]:
        driver = replace_once(driver, old, new)
    start = memory.index('static vm_fault_t color_fault(struct vm_fault *vmf)\n')
    end = memory.index('static const struct vm_operations_struct color_vm_ops=', start)
    fault = memory[start:end]
    fault = replace_once(fault, ' vm_fault_t result=VM_FAULT_SIGBUS;\n',
        ' vm_fault_t result=VM_FAULT_SIGBUS;int entry_index;\n')
    fault = replace_once(fault, ' mutex_lock(&s->lock);\n',
        ' if(!drm_dev_enter(s->dev,&entry_index))return result;\n'
        ' mutex_lock(&s->lock);\n')
    fault = replace_once(fault, ' mutex_unlock(&s->lock);return result;\n',
        ' mutex_unlock(&s->lock);drm_dev_exit(entry_index);return result;\n')
    return driver, memory[:start]+fault+memory[end:]


def private_opregion_source(source):
    # ACPI uses a blocking notifier chain on this target. Unregister joins
    # its readers; keep the callback's global owner valid until that join.
    return replace_once(source,
        '\t\tsystem_opregion = NULL;\n\t\tunregister_acpi_notifier(&psb_intel_opregion_notifier);',
        '\t\tunregister_acpi_notifier(&psb_intel_opregion_notifier);\n'
        '\t\t/* No notifier reader remains after unregister returns. */\n'
        '\t\tsystem_opregion = NULL;')


def private_async_power_sources(irq, device, opregion):
    if hashlib.sha256(device.encode()).hexdigest() != 'c02f8c825350ed89675d7618e158d503fbb92169a7f2f8aba4255b353de9a493':
        raise ValueError('unreviewed private platform brightness input')
    start=irq.index('irqreturn_t psb_irq_handler(')
    end=irq.index('\nvoid psb_irq_preinstall(',start)
    body=irq[start:end]
    body=replace_once(body, '\tint handled = 0;\n',
        '\tint handled = 0;\n\n'
        '\t/* Cover every raw read/capture/ACK, including shared IRQs.\n'
        '\t * Denial leaves hardware status untouched; it is no completion. */\n'
        '\tif (!gma_power_begin(dev, false))\n\t\treturn IRQ_NONE;\n')
    body=replace_once(body, '\tif (!handled)\n',
        '\tgma_power_end(dev);\n\n\tif (!handled)\n')
    irq=irq[:start]+body+irq[end:]
    start=device.index('static int psb_set_brightness(')
    end=device.index('\nstatic const struct backlight_ops',start)
    body=device[start:end]
    body=replace_once(body, '\tint level = bd->props.brightness;\n',
        '\tint level = bd->props.brightness;\n\n'
        '\t/* Sysfs callbacks are joined by backlight unregister. Internal\n'
        '\t * ASLE/lid producers have their own worker joins. */\n'
        '\tif (!gma_power_begin(dev, false))\n\t\treturn -EAGAIN;\n')
    body=replace_once(body, '\tpsb_brightness = level;\n',
        '\tpsb_brightness = level;\n\tgma_power_end(dev);\n')
    device='#include "power.h"\n'+device[:start]+body+device[end:]
    start=opregion.index('static u32 asle_set_backlight(')
    end=opregion.index('\nstatic void psb_intel_opregion_asle_work(',start)
    body=opregion[start:end]
    body=replace_once(body, '\tstruct backlight_device *bd = dev_priv->backlight_device;\n',
        '\tstruct backlight_device *bd = dev_priv->backlight_device;\n\tint entry_index;\n')
    body=replace_once(body, '\tgma_backlight_set(dev, bclp * bd->props.max_brightness / 255);',
        '\tif (!drm_dev_enter(dev, &entry_index))\n\t\treturn ASLE_BACKLIGHT_FAILED;\n'
        '\tif (!gma_power_begin(dev, false)) {\n'
        '\t\tdrm_dev_exit(entry_index);\n\t\treturn ASLE_BACKLIGHT_FAILED;\n\t}\n'
        '\t/* This outer claim keeps the nested platform callback admitted.\n'
        '\t * A nested DRM entry there would reject a pre-unplug operation. */\n'
        '\tgma_backlight_set(dev, bclp * bd->props.max_brightness / 255);')
    body=replace_once(body, '\treturn 0;\n',
        '\tgma_power_end(dev);\n\tdrm_dev_exit(entry_index);\n\treturn 0;\n')
    opregion='#include <drm/drm_drv.h>\n#include "power.h"\n'+opregion[:start]+body+opregion[end:]
    return irq, device, opregion


def private_console_power_source(source):
    # FIFO room is not retirement of any earlier console/3D transaction.
    # Preserve the timeout/error; never reset shared engines as a recovery guess.
    start=source.index('static int psb_2d_wait_available(')
    end=source.index('\n/**',start)
    body=replace_once(source[start:end], '\t\t\tpsb_spank(dev_priv);\n',
        '\t\t\t/* Retain outstanding hardware state; no unproved reset. */\n')
    body=replace_once(body, '\tunsigned long t = jiffies + HZ;\n',
        '\tu64 start = ktime_get_mono_fast_ns();\n')
    body=replace_once(body, '\t\tif (time_after(jiffies, t)) {\n',
        '\t\tif (ktime_get_mono_fast_ns() - start >= NSEC_PER_SEC) {\n')
    body=replace_once(body, '\t\t\treturn -EIO;\n\t\t}\n',
        '\t\t\treturn -EIO;\n\t\t}\n\t\tcpu_relax();\n')
    source=source[:start]+body+source[end:]
    start=source.index('int psbfb_sync(struct fb_info *info)\n')
    body=source[start:]
    body=replace_once(body, '\tstruct drm_fb_helper *fb_helper = info->par;\n',
        '\tstruct drm_fb_helper *fb_helper = info ? info->par : NULL;\n')
    body=replace_once(body, '\tstruct drm_framebuffer *fb = fb_helper->fb;\n',
        '\tstruct drm_framebuffer *fb = fb_helper ? fb_helper->fb : NULL;\n')
    body=replace_once(body, '\tstruct drm_device *dev = fb->dev;\n',
        '\tstruct drm_device *dev = fb ? fb->dev : NULL;\n')
    body=replace_once(body, '\tstruct drm_psb_private *dev_priv = dev->dev_private;\n',
        '\tstruct drm_psb_private *dev_priv = dev ? dev->dev_private : NULL;\n')
    body=replace_once(body, '\tunsigned long _end = jiffies + HZ;\n',
        '\tu64 start = ktime_get_mono_fast_ns();\n')
    if body.count('!time_after_eq(jiffies, _end)') != 2:
        raise ValueError('unreviewed console sync timeout loops')
    body=body.replace('!time_after_eq(jiffies, _end)',
        'ktime_get_mono_fast_ns() - start < NSEC_PER_SEC')
    body=replace_once(body, '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n',
        '\t/* DRM lifetime is covered by the private fbops entry wrapper.\n'
        '\t * An offline device is not evidence that pending 2D work retired. */\n'
        '\tif (!dev_priv)\n\t\treturn -ENODEV;\n'
        '\tif (!gma_power_begin(dev, false))\n\t\treturn -EAGAIN;\n'
        '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n')
    body=replace_once(body, '\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n',
        '\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n'
        '\tgma_power_end(dev);\n')
    return '#include <linux/ktime.h>\n'+source[:start]+body


def private_power_sources(header, source):
    # The retained native runtime-PM enable is disabled. Offline forced claims
    # cannot safely restore KMS state while callers may hold modeset locks.
    if hashlib.sha256(source.encode()).hexdigest() != '8d16ef8434418afe6a286f8f978f0ee1cc4a369874198c15311e685e581e16a6':
        raise ValueError('unreviewed private power input')
    header = replace_once(header, '\tbool remesa_private_fbdev_unregistered;\n',
        '\tbool remesa_private_fbdev_unregistered;\n'
        '\tbool remesa_private_power_transition, remesa_private_power_owner_usable;\n'
        '\tstruct task_struct *remesa_private_power_owner;\n')
    source = replace_once(source, '\tdev_priv->display_count = 0;\t/* Currently no users */\n',
        '\tdev_priv->display_count = 0;\t/* Currently no users */\n'
        '\tdev_priv->remesa_private_power_transition = false;\n'
        '\tdev_priv->remesa_private_power_owner_usable = false;\n'
        '\tdev_priv->remesa_private_power_owner = NULL;\n')
    start=source.index('static void gma_suspend_display(')
    end=source.index('int psb_runtime_suspend(')
    return header, source[:start]+'#include "remesa_private_pci_suspend.inc"\n#include "remesa_private_power.inc"\n\n'+source[end:]


def private_lid_sources(header, source):
    header = '#include <linux/workqueue.h>\n'+header
    header = replace_once(header, '\tbool remesa_private_fbdev_unregistered;\n',
        '\tbool remesa_private_fbdev_unregistered;\n'
        '\tstruct delayed_work remesa_private_lid_work;\n'
        '\tbool remesa_private_lid_initialized, remesa_private_lid_stopping;\n'
        '\tbool remesa_private_lid_pending;\n\tu32 remesa_private_lid_target;\n')
    start=source.index('static void psb_lid_timer_func(struct timer_list *t)\n')
    return header, source[:start]+'#include "remesa_private_lid.inc"\n'


def private_file_lost_sources(memory):
    memory = replace_once(memory,
        ' int (*pds_write_guard)(u64,const void *,unsigned);\n',
        ' int (*pds_write_guard)(u64,const void *,unsigned);\n'
        ' const void *private_session; /* backing lease, not source authority */\n'
        ' bool private_source_held, private_source_poisoned;\n'
        ' u32 private_input_ids[5];\n'
        ' u32 private_scene_input_ids[32],private_scene_input_mask;\n'
        ' unsigned private_scene_binding_count;\n'
        ' u32 private_work_ids[6]; /* kernel-owned COLOR/TA/DPM/Z roles */\n'
        ' bool private_work_owned;\n'
        ' u32 private_dpm_ids[2]; /* general table/parameter write exclusion */\n'
        ' u64 private_dpm_retired_operation;bool private_dpm_ready;\n'
        ' bool private_device_lost; /* monotonic; do not reclaim after unplug */\n')
    memory = replace_once(memory,
        'static bool ta_domain(const struct remesa_gem_memory *s,const struct backing *b)\n'
        '{return s->ta_cache_domains && s->ta_pools_sealed && is_managed_ta_pool(b);}',
        '#define REMESA_PRIVATE_DPM_CACHE 1\n'
        'static bool private_dpm_domain(const struct remesa_gem_memory *,const struct backing *);\n'
        'static bool ta_domain(const struct remesa_gem_memory *s,const struct backing *b)\n'
        '{return (s->ta_cache_domains && s->ta_pools_sealed && is_managed_ta_pool(b)) || private_dpm_domain(s,b);}')
    memory = replace_once(memory,
        'static int act(struct remesa_gem_memory *s,struct remesa_memory_request *r)\n',
        '#include "remesa_private_dpm_cache.inc"\n'
        'static int act(struct remesa_gem_memory *s,struct remesa_memory_request *r)\n')
    memory = replace_once(memory,
        ' unsigned i;if(!s) return -EINVAL;mutex_lock(&s->lock);\n',
        ' unsigned i;if(!s) return -EINVAL;mutex_lock(&s->lock);\n'
        ' if(s->private_device_lost){mutex_unlock(&s->lock);return -ENODEV;}\n'
        ' if(s->private_session){mutex_unlock(&s->lock);return -EBUSY;}\n')
    memory = replace_once(memory, ' if(s->stream_bound) {\n',
        ' if(s->stream_bound || s->private_session) {\n')
    memory = replace_once(memory, ' if(s->stream_bound || s->executor){e=-EBUSY;goto unlock;}\n',
        ' if(s->stream_bound || s->private_session || s->executor){e=-EBUSY;goto unlock;}\n')
    memory = replace_once(memory, ' if(endpoint->stream_bound){e=-EBUSY;goto done;}\n',
        ' if(endpoint->stream_bound || endpoint->private_session){e=-EBUSY;goto done;}\n')
    # The legacy fixed-size COLOR mapping has no session-wide CPU/GPU lease.
    memory = replace_once(memory,
        ' if(!b || !color_readable(s,b)){e=-EBUSY;goto unlock;}\n',
        ' if(s->private_session || !b || !color_readable(s,b)){e=-EBUSY;goto unlock;}\n')
    memory = replace_once(memory,
        ' if(b->input_seal==2 && (r->action==REMESA_MEM_WRITE ||\n',
        ' /* A private source lease owns its six GPU-written work roles.\n'
        '  * READ leases remain guarded by ordinary job/CPU exclusion. */\n'
        ' if(r->action==REMESA_MEM_WRITE || r->action==REMESA_MEM_SYNC ||\n'
        '    (r->action==REMESA_MEM_LEASE && r->flags))\n'
        '  for(page=0;page<6;page++)if(s->private_work_ids[page]==r->id)return -EPERM;\n'
        ' if(r->action==REMESA_MEM_WRITE || r->action==REMESA_MEM_SYNC ||\n'
        '    (r->action==REMESA_MEM_LEASE && r->flags))\n'
        '  for(page=0;page<2;page++)if(s->private_dpm_ids[page]==r->id)return -EPERM;\n'
        ' if(r->action==REMESA_MEM_WRITE || r->action==REMESA_MEM_SYNC ||\n'
        '    (r->action==REMESA_MEM_LEASE && r->flags))\n'
        '  for(page=0;page<32;page++)if(s->private_scene_input_ids[page]==r->id)return -EPERM;\n'
        ' if(b->input_seal==2 && (r->action==REMESA_MEM_WRITE ||\n')
    return memory + '\n#include "remesa_private_scene_inputs.inc"\n#include "remesa_private_memory_file_lost.inc"\n#include "remesa_private_session.inc"\n#include "remesa_private_session_dpm.inc"\n#include "remesa_private_session_source.inc"\n#include "remesa_private_session_scene.inc"\n#include "remesa_private_inputs.inc"\n'


def private_gem_lifetime_sources(native):
    anchors = {'gem.c': '2e47b9c3df078c49693dbee1f82e6660cb76a876db0ad3156ee4091c999eca43',
               'framebuffer.c': 'ec0a8e81d02e6246600e14ed112bd3224ec07db7ee440e78bcae53b4b01541b6',
               'gtt.c': '388c5b251cfa17545031c1841a4ef6afcf1bc7385a424e34221948959e15818d',
               'gtt.h': '2407ff8bdbdba056e0583982e94b77da8c665bdd797d3fb3444918566abdc72f'}
    sources = {}
    for name, digest in anchors.items():
        raw = (native/name).read_bytes()
        if hashlib.sha256(raw).hexdigest() != digest:
            raise ValueError('unreviewed standard GEM lifetime input: '+name)
        sources[name] = raw.decode()
    gem = '#include <linux/module.h>\n#include <drm/drm_drv.h>\n'+sources['gem.c']
    gem = replace_once(gem, 'void psb_gem_free_object(struct drm_gem_object *obj)\n',
        'static void remesa_private_gem_free_object_live(struct drm_gem_object *obj)\n')
    gem = replace_once(gem, '\nint psb_gem_get_aperture(',
        '\n#include "remesa_private_gem_lifetime.inc"\n\nint psb_gem_get_aperture(')
    gem = replace_once(gem, '\t/* Limit the object to 32bit mappings */\n',
        '\tremesa_private_gem_track(&r->gem);\n\t/* Limit the object to 32bit mappings */\n')
    gem = replace_once(gem, '\t\tdrm_gem_object_release(&r->gem);\n\t\tpsb_gtt_free_range(dev, r);\n',
        '\t\tdrm_gem_object_put(&r->gem);\n')
    fb = '#include "remesa_private_gem_lifetime.h"\n'+sources['framebuffer.c']
    fb = replace_once(fb, '\t\tdrm_gem_private_object_init(dev, &backing->gem, aligned_size);\n',
        '\t\tdrm_gem_private_object_init(dev, &backing->gem, aligned_size);\n'
        '\t\tremesa_private_gem_track(&backing->gem);\n')
    fb = replace_once(fb, '\tpsb_gtt_free_range(dev, backing);\n',
        '\tdrm_gem_object_put(&backing->gem);\n')
    header = replace_once(sources['gtt.h'], '#include <drm/drm_gem.h>\n',
        '#include <drm/drm_gem.h>\n#include <linux/list.h>\n')
    header = replace_once(header, '\tint roll;\t\t\t/* Roll applied to the GTT entries */\n',
        '\tint roll;\t\t\t/* Roll applied to the GTT entries */\n'
        '\t/* Private profile only; preserve existing field offsets. */\n'
        '\tstruct list_head private_lifetime_link;\n'
        '\tbool private_tracked, private_orphaned;\n')
    gtt = '#include <drm/drm_drv.h>\n'+sources['gtt.c']
    for old, new in [
        ('int psb_gtt_pin(struct gtt_range *gt)\n',
         'static int remesa_private_gtt_pin_live(struct gtt_range *gt)\n'),
        ('void psb_gtt_unpin(struct gtt_range *gt)\n',
         'static void remesa_private_gtt_unpin_live(struct gtt_range *gt)\n'),
        ('void psb_gtt_roll(struct drm_device *dev, struct gtt_range *r, int roll)\n',
         'static void remesa_private_gtt_roll_live(struct drm_device *dev, struct gtt_range *r, int roll)\n')]:
        gtt = replace_once(gtt, old, new)
    gtt += '\n#include "remesa_private_gtt_guard.inc"\n'
    gtt = replace_once(gtt, 'int psb_gtt_init(struct drm_device *dev, int resume)\n',
        'static int remesa_private_gtt_init_live(struct drm_device *dev, int resume)\n')
    start=gtt.index('int psb_gtt_restore(struct drm_device *dev)\n')
    end=gtt.index('\n#include "remesa_private_gtt_guard.inc"',start)
    gtt=gtt[:start]+'#include "remesa_private_gtt_resume.inc"\n'+gtt[end:]
    return {'gem.c': gem, 'framebuffer.c': fb, 'gtt.h': header, 'gtt.c': gtt}


def private_fbdev_sources(driver_header, fb, display):
    driver_header = replace_once(driver_header,
        '\tstruct remesa_twod_reservation *remesa_twod_owner;\n',
        '\tstruct remesa_twod_reservation *remesa_twod_owner;\n'
        '\tbool remesa_private_fbdev_unregistered;\n')
    fb = '#include <drm/drm_drv.h>\n#include <drm/drm_framebuffer.h>\n#include "remesa_private_fbdev.h"\n'+fb
    start = fb.index('static vm_fault_t psbfb_vm_fault(struct vm_fault *vmf)\n')
    end = fb.index('static const struct fb_ops psbfb_ops = {', start)
    fb = fb[:start]+'#include "remesa_private_fbdev_map.inc"\n#include "remesa_private_fbdev_ops.inc"\n\n'+fb[end:]
    if fb.count('DRM_FB_HELPER_DEFAULT_OPS,')!=3:
        raise ValueError('unexpected fbdev operation tables')
    fb = fb.replace('DRM_FB_HELPER_DEFAULT_OPS,','REMESA_PRIVATE_FB_DEFAULT_OPS,\n\t.fb_destroy = remesa_private_fb_destroy_info,')
    for old, new in [('psbfb_setcolreg','remesa_fb_color'),('drm_fb_helper_cfb_fillrect','remesa_fb_fill'),
                     ('drm_fb_helper_cfb_copyarea','remesa_fb_copy'),('psbfb_copyarea','remesa_fb_native_copy'),
                     ('drm_fb_helper_cfb_imageblit','remesa_fb_image'),('psbfb_mmap','remesa_private_fb_mmap'),
                     ('psbfb_sync','remesa_fb_sync'),('psbfb_pan','remesa_fb_roll')]:
        # Only table bindings: original functions and the new macro wrappers
        # retain their names for source review and independent test extraction.
        import re
        fb = re.sub(r'(\.fb_\w+\s*=\s*)'+re.escape(old)+r'\b',r'\g<1>'+new,fb)
    fb = replace_once(fb, '\tdrm_fb_helper_unregister_fbi(fb_helper);\n\n\tdrm_fb_helper_fini(fb_helper);\n',
        '\t/* Called by core fb_destroy after the final put_fb_info. */\n'
        '\tdrm_fb_helper_fini(fb_helper);\n')
    fb = replace_once(fb,
        '\tdrm_framebuffer_unregister_private(fb);\n\tdrm_framebuffer_cleanup(fb);\n\n'
        '\tif (fb->obj[0])\n\t\tdrm_gem_object_put(fb->obj[0]);\n\tkfree(fb);\n',
        '\t/* Planes/clients may still own framebuffer references. */\n'
        '\tif (fb)\n\t\tdrm_framebuffer_put(fb);\n')
    start=fb.index('static void psb_fbdev_fini(struct drm_device *dev)\n')
    end=fb.index('\nstatic const struct drm_mode_config_funcs',start)
    fb=fb[:start]+'#include "remesa_private_fbdev_detach.inc"\n'+fb[end:]
    fb=replace_once(fb, 'free:\n\tkfree(fb_helper);\n\treturn ret;\n',
        'free:\n\tkfree(fb_helper);\n\tdev_priv->fb_helper = NULL;\n\treturn ret;\n')
    fb=replace_once(fb, 'fini:\n\tdrm_fb_helper_fini(fb_helper);\n',
        'fini:\n\tif (fb_helper->fb) {\n'
        '\t\tstruct drm_framebuffer *fb = fb_helper->fb;\n'
        '\t\tfb_helper->fb = NULL;\n\t\tdrm_framebuffer_put(fb);\n\t}\n'
        '\tdrm_fb_helper_fini(fb_helper);\n')
    fb=replace_once(fb, '\tret = drm_fb_helper_initial_config(fb_helper, 32);\n\tif (ret)\n\t\tgoto fini;\n',
        '\tret = drm_fb_helper_initial_config(fb_helper, 32);\n'
        '\tif (ret) {\n'
        '\t\t/* On this target register_framebuffer can return a fbcon\n'
        '\t\t * error after publishing the slot and initializing count. */\n'
        '\t\tif (fb_helper->fbdev && atomic_read(&fb_helper->fbdev->count)) {\n'
        '\t\t\tdrm_dev_unplug(dev);\n\t\t\tremesa_private_fbdev_detach(dev);\n'
        '\t\t\treturn ret; /* helper may have been destroyed */\n'
        '\t\t}\n\t\tgoto fini;\n\t}\n')
    display=replace_once(display,
        'int gma_crtc_set_config(struct drm_mode_set *set,\n',
        'static int remesa_private_fb_config_live(struct drm_mode_set *set,\n')
    display=replace_once(display, 'int gma_crtc_cursor_set(struct drm_crtc *crtc,\n',
        'static int remesa_private_fb_cursor_set_live(struct drm_crtc *crtc,\n')
    display=replace_once(display, 'int gma_crtc_cursor_move(struct drm_crtc *crtc, int x, int y)\n',
        'static int remesa_private_fb_cursor_move_live(struct drm_crtc *crtc, int x, int y)\n')
    display='#include <drm/drm_drv.h>\n'+display+'\n#include "remesa_private_fbdev_config.inc"\n'
    return driver_header,fb,display

def patched_sources(native, lab=False, vendor=False, copy=False, fill=False, private=False):
    if fill and not copy:
        raise ValueError("native fill requires the separate normal copy profile")
    if copy and (lab or vendor):
        raise ValueError('normal copy and LAB profiles are separate builds')
    if vendor and not lab:
        raise ValueError('vendor stream requires the isolated LAB ioctl build')
    for name, digest in PINNED.items():
        if hashlib.sha256((native / name).read_bytes()).hexdigest() != digest:
            raise ValueError('unreviewed native input: ' + name)
    header = replace_once((native / 'psb_drv.h').read_text(),
        '\tuint8_t panel_type;\n};',
        '\tuint8_t panel_type;\n'
        '\t/* Appended: existing private-field offsets are unchanged. */\n'
        '\tstruct remesa_twod_reservation *remesa_twod_owner;\n};')
    accel = replace_once((native / 'accel_2d.c').read_text(),
        '#include "gma500_source_snapshot.h"\n',
        '#include "gma500_source_snapshot.h"\n'
        '#include "remesa_twod_reservation.inc"\n'
        '#include "remesa_twod_native.inc"\n')
    accel = replace_once(accel,
        '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n\twhile (size > 0) {',
        '\tspin_lock_irqsave(&dev_priv->lock_2d, flags);\n'
        '\t/* Native 2D owns the request base until safe retirement. */\n'
        '\tif (dev_priv->remesa_twod_owner) {\n'
        '\t\tret = -EBUSY;\n\t\tgoto reserved_out;\n\t}\n'
        '\twhile (size > 0) {')
    accel = replace_once(accel,
        '\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n'
        '\tremesa_route_source_producer(0);',
        'reserved_out:\n\tspin_unlock_irqrestore(&dev_priv->lock_2d, flags);\n'
        '\tremesa_route_source_producer(0);')
    # The original console caller ignores submission errors. EBUSY here is
    # guaranteed before any native command write, so retain its existing CPU
    # console behavior. Never replay an uncertain/partially emitted EIO blit.
    accel = replace_once(accel, '\tuint32_t dst_format;\n',
                         '\tuint32_t dst_format;\n\tint ret;\n')
    accel = replace_once(accel, '\tpsb_accel_2d_copy(dev_priv,\n',
                         '\tret = psb_accel_2d_copy(dev_priv,\n')
    accel = replace_once(accel,
        '\t\t\t  a->sx, a->sy, a->dx, a->dy, a->width, a->height);\n'
        '\tgma_power_end(dev);',
        '\t\t\t  a->sx, a->sy, a->dx, a->dy, a->width, a->height);\n'
        '\tif (ret == -EBUSY)\n\t\tdrm_fb_helper_cfb_copyarea(info, a);\n'
        '\tgma_power_end(dev);')
    fixed = replace_once((native / 'gma500_fixed_backend.c').read_text(),
        'static struct sgx535_gma500_fixed_backend *fixed_irq_owner;\n',
        'static struct sgx535_gma500_fixed_backend *fixed_irq_owner;\n'
        '#include "remesa_twod_irq.inc"\n')
    fixed = replace_once(fixed,
        '    remesa_route_source_pending(dev, status1, status2);\n',
        '    remesa_route_source_pending(dev, status1, status2);\n'
        '    remesa_twod_irq_capture_locked(dev, status1, status2);\n')
    fixed = replace_once(fixed,
        'if (fixed_irq_owner || !remesa_route_source_begin(backend, owner->dev))',
        'if (fixed_irq_owner || remesa_twod_irq_busy_locked() ||\n'
        '        !remesa_route_source_begin(backend, owner->dev))')
    fixed = replace_once(fixed, '    if (fixed_irq_owner) {\n',
        '    if (fixed_irq_owner || remesa_twod_irq_busy_locked()) {\n')
    mmu = (native / 'mmu.c').read_text() + '\n#include "remesa_mmu_verify_access.inc"\n#include "remesa_mmu_default_root.inc"\n'
    gem = trap_gem_unmap_source((native / 'remesa_gem_memory.c').read_text()) + '\n#include "remesa_twod.c"\n#include "remesa_gem_twod.inc"\n#include "remesa_gem_twod_job.inc"\n#ifdef REMESA_TWOD_LAB_IOCTL\n#include "remesa_twod_lab.inc"\n#endif\n#ifdef REMESA_TWOD_COPY_IOCTL\n#include "remesa_twod_copy.inc"\n#endif\n'
    driver = replace_once((native / 'psb_drv.c').read_text(),
        '#include "remesa_retained_uapi.h"\n',
        '#include "remesa_retained_uapi.h"\n#include "remesa_twod_lab.h"\n#include "remesa_twod_lab_uapi.h"\n#include "remesa_twod_copy.h"\n#include "remesa_twod_copy_uapi.h"\n')
    driver = replace_once(driver, 'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
        'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
        '#ifdef REMESA_TWOD_LAB_IOCTL\n'
        '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_LAB, remesa_twod_lab_ioctl, DRM_ROOT_ONLY),\n'
        '#endif\n'
        '#ifdef REMESA_TWOD_COPY_IOCTL\n'
        '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_COPY, remesa_twod_copy_ioctl, DRM_ROOT_ONLY),\n'
        '#endif\n')
    gem, memory_header = job_capacity_sources(gem, (native / 'remesa_gem_memory.h').read_text())
    gem = dpm_origin_source(gem) + '\n#include "remesa_gem_dpm.inc"\n'
    result = {'psb_drv.h': header, 'accel_2d.c': accel,
            'gma500_fixed_backend.c': fixed, 'mmu.c': mmu,
            'remesa_gem_memory.c': gem, 'remesa_gem_memory.h': memory_header, 'psb_drv.c': driver}
    if lab:
        result['Makefile'] = (native / 'Makefile').read_text() + '\n# Explicit isolated 2D LAB candidate; runtime gate defaults N.\nccflags-y += -DREMESA_TWOD_LAB_IOCTL=1\n'
        if vendor:
            result['Makefile'] += 'ccflags-y += -DREMESA_TWOD_LAB_VENDOR_STREAM=1\n'
    if copy:
        result['Makefile'] = (native / 'Makefile').read_text() + '\n# Typed copy profile; no LAB diagnostics; runtime defaults N.\nccflags-y += -DREMESA_TWOD_COPY_IOCTL=1 -DREMESA_TWOD_VENDOR_STREAM=1\n'
    if fill:
        result['Makefile'] += 'ccflags-y += -DREMESA_TWOD_FILL_STREAM=1\n'
        result['remesa_gem_memory.c'] += '\n#ifdef REMESA_TWOD_FILL_STREAM\n#include "remesa_twod_fill.inc"\n#endif\n'
        result['psb_drv.c'] = '#include "remesa_twod_fill.h"\n#include "remesa_twod_fill_uapi.h"\n' + result['psb_drv.c']
        result['psb_drv.c'] = replace_once(result['psb_drv.c'],
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
            '#ifdef REMESA_TWOD_FILL_STREAM\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_TWOD_FILL, remesa_twod_fill_ioctl, DRM_ROOT_ONLY),\n'
            '#endif\n')
    if private:
        # The early ASLE join relies on setup publishing header only after
        # work initialization and on unregister not clearing the owner's bd.
        for name,digest in {
            'opregion.c':'a6fd11464b3972d456dea55919cf97c3ccb26f9d5a1a690139cdf1c4c09d35d0',
            'backlight.c':'995720191e06a74df2cdb80a1bf8d77fe49aa9dd38e60139dcd15f516be63e1d',
            'psb_lid.c':'cc5660e14990d1ab7adfe69880c37125becda71927cd9e6ef7d508343dbf5bf7',
            'psb_device.c':'c02f8c825350ed89675d7618e158d503fbb92169a7f2f8aba4255b353de9a493',
            'psb_intel_lvds.c':'f74be00054701ec6297b03824f34994eb77541b9341b5ce4f266351ae8438e12',
        }.items():
            if hashlib.sha256((native/name).read_bytes()).hexdigest()!=digest:
                raise ValueError('unreviewed private ASLE lifetime input: '+name)
        private_header = native / 'gma500_fixed_backend.h'
        if hashlib.sha256((native / 'stream_profile_asset.h').read_bytes()).hexdigest() != 'e21ceda020131e56c93271b995af0df590c8eecbe87e18d97579fa25bf8edf6e':
            raise ValueError('unreviewed private qualification profile')
        if hashlib.sha256(private_header.read_bytes()).hexdigest() != '97c2f97e00d61970195b79cb7ddb4acc1a5c4b314c739f707ded12df3cc1260f':
            raise ValueError('unreviewed private backend header')
        result['gma500_fixed_backend.c'], result['gma500_fixed_backend.h'] = private_fixed_sources(
            result['gma500_fixed_backend.c'], private_header.read_text())
        header_before_general = result['gma500_fixed_backend.h']
        baseline_start = header_before_general.index('struct sgx535_gma500_fixed_backend {')
        baseline_end = header_before_general.index('};', baseline_start)+2
        result['remesa_backend_abi_baseline.h'] = header_before_general[baseline_start:baseline_end].replace('struct sgx535_gma500_fixed_backend {','struct remesa_private_baseline_backend {')+'\n'
        result['gma500_fixed_backend.c'], result['gma500_fixed_backend.h'] = backend_sources(
            result['gma500_fixed_backend.c'], result['gma500_fixed_backend.h'])
        result['Makefile'] = result.get('Makefile', (native / 'Makefile').read_text()) + '\nccflags-y += -DREMESA_PRIVATE_IRQ_HANDOFF=1 -DREMESA_GENERAL_SCENE=1\n'
        source = (native / 'remesa_stream_source.c').read_text()
        if hashlib.sha256(source.encode()).hexdigest() != '45fbe31cfdf36c96433ffaef97167477a5f7ad090fa2f5040943aa64ac600bca':
            raise ValueError('unreviewed private stream source')
        result['remesa_stream_source.c'] = source_code(private_stream_source(source))
        result['mmu.c'] = private_mmu_flush_source(result['mmu.c'])
        result['mmu.h'] = private_mmu_header_source((native / 'mmu.h').read_text())
        result['mmu.c'] += '\n#include "remesa_mmu_confinement.inc"\n#include "remesa_mmu_private.inc"\n#include "remesa_mmu_switch.inc"\n'
        result['mmu.c'] += '#include "remesa_console_mapping.inc"\n'
        result['remesa_gem_memory.c'] += '\n#include "remesa_gem_private.inc"\n#include "remesa_gem_private_execute.inc"\n#include "remesa_gem_scene_actions.inc"\n#include "remesa_gem_scene_controls.inc"\n#include "remesa_gem_scene_use.inc"\n#include "remesa_gem_scene_ta.inc"\n#include "remesa_gem_scene_states.inc"\n#include "remesa_gem_scene_vertices.inc"\n#include "remesa_gem_scene_fragments.inc"\n#include "remesa_gem_scene_background.inc"\n#include "remesa_gem_scene_event.inc"\n#include "remesa_gem_scene_job.inc"\n#include "remesa_gem_scene_packet.inc"\n#include "remesa_gem_scene_execute.inc"\n#include "remesa_private_probe.inc"\n'
        result['psb_drv.c'] = replace_once(result['psb_drv.c'],
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
            '#include "remesa_private_probe.h"\n#include "remesa_private_probe_uapi.h"\n'
            '#include "remesa_private_session_lab.h"\n#include "remesa_private_session_lab_uapi.h"\n'
            '#include "remesa_scene_probe.h"\n#include "remesa_scene_probe_uapi.h"\n'
            '#include "remesa_gears_scene_probe_uapi.h"\n'
            '#include "remesa_general_session_uapi.h"\n'
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_SCENE_PROBE, remesa_scene_probe_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_SCENE_TRACE, remesa_scene_trace_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_GEARS_SCENE_PROBE, remesa_gears_scene_probe_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_GEARS_SCENE_TRACE, remesa_gears_scene_trace_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_GENERAL_SESSION, remesa_general_session_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_PRIVATE_SESSION_LAB, remesa_private_session_lab_ioctl, DRM_ROOT_ONLY),\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_PRIVATE_PROBE, remesa_private_probe_ioctl, DRM_ROOT_ONLY),\n')
        result['psb_drv.c'] += '\n#include "remesa_easter_egg.h"\n'
        result['psb_drv.c'], result['remesa_gem_memory.c'] = private_entry_sources(
            result['psb_drv.c'], result['remesa_gem_memory.c'])
        result['opregion.c'] = private_opregion_source((native/'opregion.c').read_text())
        result['psb_irq.c'],result['psb_device.c'],result['opregion.c']=private_async_power_sources(
            result.get('psb_irq.c',(native/'psb_irq.c').read_text()),
            (native/'psb_device.c').read_text(),result['opregion.c'])
        result['remesa_gem_memory.c'] = private_file_lost_sources(result['remesa_gem_memory.c'])
        result['remesa_gem_memory.c'] += '#include "remesa_private_session_lab.inc"\n'
        result['remesa_gem_memory.c'] += '#include "remesa_scene_probe.inc"\n'
        result['remesa_gem_memory.c'] += '#include "remesa_general_session.inc"\n'
        result.update(private_gem_lifetime_sources(native))
        display=(native/'gma_display.c').read_bytes()
        if hashlib.sha256(display).hexdigest()!='877140046fcea01a5e8f0face75384a3dfe2d34983539de76bcfa94ef9f5f2c4':
            raise ValueError('unreviewed fbdev legacy modeset input')
        result['psb_drv.h'],result['framebuffer.c'],result['gma_display.c']=private_fbdev_sources(
            result['psb_drv.h'],result['framebuffer.c'],display.decode())
        result['psb_drv.h'],result['power.c']=private_power_sources(
            result['psb_drv.h'],(native/'power.c').read_text())
        result['accel_2d.c']=private_console_power_source(result['accel_2d.c'])
        result['psb_drv.h'],result['psb_lid.c']=private_lid_sources(
            result['psb_drv.h'],(native/'psb_lid.c').read_text())
        for name, digest, fragment in [
            ('remesa_stream_bridge.c', '4d0a1accdfc65caa28892facc2a2149cfc35c42360ebb7333027aa9d07ad8f4d',
             'remesa_private_stream_file_lost.inc'),
            ('remesa_stream_trusted.c', '66ee297420a35c44a130f9af6925ffa17d32bf0c85371635d5cb38c10f68d53a',
             'remesa_private_trusted_file_lost.inc')]:
            contents = (native / name).read_bytes()
            if hashlib.sha256(contents).hexdigest() != digest:
                raise ValueError('unreviewed private file-lost input: ' + name)
            result[name] = contents.decode() + '\n#include "' + fragment + '"\n'
    return result


def deferred_twod_cpu_source(source):
    """Defer destination CPU invalidation, never device visibility/retirement.

    The 2D issue path still flushes every source/destination before DMA. CPU
    leases, internal views and page release consume pending invalidation under
    the existing memory lock, before exposing or returning a single page.
    """
    source = replace_once(source, ' bool ta_cpu_cached,ta_cpu_dirty,ta_gpu_dirty;\n',
        ' bool ta_cpu_cached,ta_cpu_dirty,ta_gpu_dirty;\n'
        ' bool twod_cpu_invalidate_pending;\n')
    source = replace_once(source, 'static int unmap(struct remesa_gem_memory *s,struct backing *b)\n',
        'static bool twod_defer_cpu_invalidation;\n'
        'module_param_named(remesa_twod_defer_cpu_invalidation,twod_defer_cpu_invalidation,bool,0600);\n'
        'static void twod_for_cpu(struct backing *b)\n'
        '{\n'
        ' if(!b->twod_cpu_invalidate_pending)return;\n'
        ' drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);dma_rmb();\n'
        ' b->twod_cpu_invalidate_pending=false;\n'
        '}\n'
        'static int unmap(struct remesa_gem_memory *s,struct backing *b)\n')
    source = replace_once(source, ' if(b->pages) drm_gem_put_pages(&b->gem,b->pages,true,false);\n',
        ' if(b->pages) {twod_for_cpu(b);drm_gem_put_pages(&b->gem,b->pages,true,false);}\n')
    source = replace_once(source,
        '  drm_gem_put_pages(&b->gem,b->pages,true,false);b->pages=NULL;b->pinned=false;b->lease_borrowed=false;\n',
        '  twod_for_cpu(b);\n'
        '  drm_gem_put_pages(&b->gem,b->pages,true,false);b->pages=NULL;b->pinned=false;b->lease_borrowed=false;\n')
    source = replace_once(source, '  if(!b->pinned || b->lease) return -EBUSY;\n  /* A retired OPEN8',
        '  if(!b->pinned || b->lease) return -EBUSY;\n'
        '  twod_for_cpu(b); /* before any CPU mapping or color-view fault */\n  /* A retired OPEN8')
    source = replace_once(source,
        '  drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);dma_wmb();\n',
        '  drm_clflush_pages(b->pages,b->gem.size/PAGE_SIZE);dma_wmb();\n'
        '  dma_rmb();b->twod_cpu_invalidate_pending=false;\n')
    source = replace_once(source,
        '  out[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,j->cpu_views[i]};\n',
        '  twod_for_cpu(b); /* includes already cached internal views */\n'
        '  out[i]=(struct remesa_memory_view){uses[i],b->pages,b->gem.size/PAGE_SIZE,j->cpu_views[i]};\n')
    return source


def stage(native, output, lab=False, vendor=False, copy=False, fill=False, private=False,
          defer_twod_cpu=False):
    if defer_twod_cpu and (not copy or lab or vendor or private):
        raise ValueError('deferred CPU invalidation requires isolated normal native-copy/fill profile')
    if output.exists():
        raise FileExistsError('refusing to overwrite ' + str(output))
    # Inventory all copied sources, including files outside the patch anchors.
    # This records provenance; only PINNED entries are approved hash anchors.
    native_files = {source.name: source.read_bytes() for source in native.iterdir()
                    if source.is_file() and (source.suffix in ['.c', '.h', '.inc'] or
                                             source.name in ['Makefile', 'Kconfig'])}
    patched = patched_sources(native, lab, vendor, copy, fill, private)
    if defer_twod_cpu:
        patched['remesa_gem_memory.c'] = deferred_twod_cpu_source(patched['remesa_gem_memory.c'])
        patched['Makefile'] += 'ccflags-y += -DREMESA_TWOD_DEFER_CPU_INVALIDATE=1\n'
    root = Path(__file__).resolve().parents[2]
    additions = root / 'kernel/remesa/gma500'
    components = {name: (additions / name).read_bytes() for name in
                  ['remesa_gem_dpm.h', 'remesa_gem_dpm.inc', 'remesa_gem_job_limits.h', 'remesa_twod_reservation.h', 'remesa_twod_reservation.inc',
                   'remesa_twod_irq.h', 'remesa_twod_irq.inc']}
    for name in ['remesa_twod_native.h', 'remesa_twod_native.inc',
                 'remesa_mmu_default_root.h', 'remesa_mmu_default_root.inc',
                 'remesa_mmu_verify_access.h', 'remesa_mmu_verify_access.inc',
                 'remesa_gem_twod.h', 'remesa_gem_twod.inc',
                 'remesa_gem_twod_job.h', 'remesa_gem_twod_job.inc']:
        components[name] = (additions / name).read_bytes()
    if private:
        components.update(scene_action_components(root, native))
        components.update(scene_use_components(root))
        components.update(scene_service_components(root, native))
        patched['Makefile'] += '\ngma500_gfx-y += remesa_scene_actions.o remesa_program_registers.o remesa_scene_service.o remesa_scene_backend_abi.o\n'
        for name in ['remesa_mmu_confinement.h', 'remesa_mmu_confinement.inc',
                     'remesa_mmu_private.h', 'remesa_mmu_private.inc',
                     'remesa_mmu_switch.h', 'remesa_mmu_switch.inc',
                     'remesa_gem_private.h', 'remesa_gem_private.inc',
                     'remesa_gem_private_execute.inc', 'remesa_gem_scene_actions.h', 'remesa_gem_scene_actions.inc',
                     'remesa_gem_scene_controls.h', 'remesa_gem_scene_controls.inc',
                     'remesa_gem_scene_use.h', 'remesa_gem_scene_use.inc',
                     'remesa_gem_scene_ta.h', 'remesa_gem_scene_ta.inc',
                     'remesa_gem_scene_states.h', 'remesa_gem_scene_states.inc',
                     'remesa_gem_scene_vertices.h', 'remesa_gem_scene_vertices.inc',
                     'remesa_gem_scene_fragments.h', 'remesa_gem_scene_fragments.inc',
                     'remesa_gem_scene_background.h', 'remesa_gem_scene_background.inc',
                     'remesa_gem_scene_event.h', 'remesa_gem_scene_event.inc',
                     'remesa_gem_scene_job.h', 'remesa_gem_scene_job.inc', 'remesa_gem_scene_execute.inc', 'remesa_gem_scene_packet.inc',
                     'remesa_private_probe.h', 'remesa_private_probe.inc',
                     'remesa_private_probe_request.h',
                     'remesa_private_prefire_irq.h',
                     'remesa_private_source_mmio.h', 'remesa_private_source_mmio.inc',
                     'remesa_private_entry_guard.inc',
                     'remesa_private_gem_lifetime.h', 'remesa_private_gem_lifetime.inc',
                     'remesa_private_gtt_guard.inc',
                     'remesa_private_gtt_resume.inc',
                     'remesa_console_mapping.h', 'remesa_console_mapping.inc',
                     'remesa_private_fbdev.h', 'remesa_private_fbdev_map.inc',
                     'remesa_private_fbdev_ops.inc', 'remesa_private_fbdev_detach.inc',
                     'remesa_private_fbdev_config.inc',
                     'remesa_private_lid.inc', 'remesa_private_power.inc',
                     'remesa_private_pci_suspend.inc',
                     'remesa_private_file_lost.h', 'remesa_private_memory_file_lost.inc',
                     'remesa_private_session.h', 'remesa_private_session.inc',
                     'remesa_private_session_source.inc', 'remesa_private_session_scene.inc',
                     'remesa_private_session_dpm.inc',
                     'remesa_private_scene_inputs.inc',
                     'remesa_general_session.inc',
                     'remesa_private_dpm_cache.inc',
                     'remesa_private_inputs.inc',
                     'remesa_private_session_lab.h', 'remesa_private_session_lab_request.h',
                     'remesa_private_session_lab.inc',
                     'remesa_private_stream_file_lost.inc', 'remesa_private_trusted_file_lost.inc',
                     'remesa_easter_egg.h', 'remesa_fixed_private.inc',
                     'remesa_scene_backend.h', 'remesa_scene_backend.inc', 'remesa_scene_source.inc', 'remesa_scene_backend_abi.c',
                     'remesa_scene_probe.h', 'remesa_scene_probe.inc', 'remesa_scene_probe_profiles.h', 'remesa_gears_scene_probe_profiles.h', 'remesa_gears_homogeneous_probe_profiles.h', 'remesa_gears_clamped_api_probe_profiles.h', 'remesa_scene_probe_request.h']:
            components[name] = (additions / name).read_bytes()
        components['remesa_scene_probe_uapi.h'] = (root / 'include/remesa/scene_probe_uapi.h').read_bytes()
        components['remesa_general_session_uapi.h'] = (root / 'include/remesa/general_session_uapi.h').read_bytes()
        components['remesa_gears_scene_probe_uapi.h'] = (root / 'include/remesa/gears_scene_probe_uapi.h').read_text().replace('"scene_probe_uapi.h"','"remesa_scene_probe_uapi.h"').encode()
        components['remesa_gears_scene_probe_envelope.h'] = (root / 'include/remesa/gears_scene_probe_envelope.h').read_text().replace('"gears_scene_probe_uapi.h"','"remesa_gears_scene_probe_uapi.h"').encode()
        components['remesa_scene_packet.h'] = (root / 'include/remesa/scene_packet.h').read_bytes()
        components['remesa_private_probe_uapi.h'] = (root / 'include/remesa/private_probe_uapi.h').read_bytes()
        components['remesa_private_session_lab_uapi.h'] = (root / 'include/remesa/private_session_lab_uapi.h').read_bytes()
    for name in ['remesa_twod.c', 'remesa_twod.h']:
        components[name] = (root / 'mesa/sgx535' / name).read_bytes()
    for name in ['remesa_twod_lab.h', 'remesa_twod_lab.inc', 'remesa_twod_copy.h', 'remesa_twod_copy.inc']:
        components[name] = (additions / name).read_bytes()
    components['remesa_twod_lab_uapi.h'] = (root / 'include/remesa/twod_lab_uapi.h').read_bytes()
    components['remesa_twod_copy_uapi.h'] = (root / 'include/remesa/twod_copy_uapi.h').read_bytes()
    if fill:
        for name in ['remesa_twod_fill.h', 'remesa_twod_fill.inc']:
            components[name] = (additions / name).read_bytes()
        components['remesa_twod_fill_uapi.h'] = (root / 'include/remesa/twod_fill_uapi.h').read_bytes()
    for name, contents in native_files.items():
        if (native / name).read_bytes() != contents:
            raise ValueError('native input changed during staging: ' + name)
    output.mkdir(parents=True)
    for name, contents in native_files.items():
        (output / name).write_bytes(contents)
    for name, source in patched.items():
        (output / name).write_text(source)
    for name, source in components.items():
        (output / name).write_bytes(source)
    manifest = {
        'native_source': str(native.resolve()), 'pinned_inputs': PINNED,
        'native_input_sha256': {name: hashlib.sha256(contents).hexdigest()
                                for name, contents in sorted(native_files.items())},
        'output_sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest()
                          for name in sorted(set(native_files) | set(patched) | set(components))},
        'provenance_scope': 'All copied native and staged sources inventoried; pinned_inputs lists only approved patch anchors',
        'lab_ioctl_compiled': lab,
        'vendor_stream_lab_compiled': vendor,
        'normal_copy_compiled': copy,
        'native_fill_compiled': fill,
        'private_mmu_preparation_compiled': private,
        'twod_deferred_cpu_invalidation_compiled': defer_twod_cpu,
        'scope': 'Native 2D candidate; optional root-only gated LAB ioctl; never installed by stager',
    }
    (output / 'TWOD_RESERVATION_STAGE.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--with-lab-ioctl', action='store_true')
    parser.add_argument('--with-lab-vendor-stream', action='store_true')
    parser.add_argument('--with-native-copy', action='store_true')
    parser.add_argument('--with-native-fill', action='store_true')
    parser.add_argument('--with-twod-deferred-cpu-invalidation', action='store_true',
                        help='Candidate normal 2D cache policy; requires separate physical qualification')
    parser.add_argument('--with-private-mmu-preparation', action='store_true',
                        help='Compile private MMU/execution and the default-disabled typed qualification ioctl')
    args = parser.parse_args()
    print(json.dumps(stage(args.native_source, args.output, args.with_lab_ioctl,
                           args.with_lab_vendor_stream, args.with_native_copy, args.with_native_fill,
                           args.with_private_mmu_preparation,
                           args.with_twod_deferred_cpu_invalidation), indent=2))
