#!/usr/bin/env python3
"""Stage the bounded 2D producer-reservation patch; never install or overwrite.

The input is the retained qualified native source, not the dirty legacy checkout.
Normal profiles integrate producer/IRQ exclusion. The private profile also
registers a default-disabled, fixed-profile qualification ioctl; it is not a
production submission interface and this tool never installs a module.
"""
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


def private_file_lost_sources(memory):
    memory = replace_once(memory,
        ' int (*pds_write_guard)(u64,const void *,unsigned);\n',
        ' int (*pds_write_guard)(u64,const void *,unsigned);\n'
        ' bool private_device_lost; /* monotonic; do not reclaim after unplug */\n')
    memory = replace_once(memory,
        ' unsigned i;if(!s) return -EINVAL;mutex_lock(&s->lock);\n',
        ' unsigned i;if(!s) return -EINVAL;mutex_lock(&s->lock);\n'
        ' if(s->private_device_lost){mutex_unlock(&s->lock);return -ENODEV;}\n')
    return memory + '\n#include "remesa_private_memory_file_lost.inc"\n'


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
    result = {'psb_drv.h': header, 'accel_2d.c': accel,
            'gma500_fixed_backend.c': fixed, 'mmu.c': mmu,
            'remesa_gem_memory.c': gem, 'psb_drv.c': driver}
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
        private_header = native / 'gma500_fixed_backend.h'
        if hashlib.sha256((native / 'stream_profile_asset.h').read_bytes()).hexdigest() != 'e21ceda020131e56c93271b995af0df590c8eecbe87e18d97579fa25bf8edf6e':
            raise ValueError('unreviewed private qualification profile')
        if hashlib.sha256(private_header.read_bytes()).hexdigest() != '97c2f97e00d61970195b79cb7ddb4acc1a5c4b314c739f707ded12df3cc1260f':
            raise ValueError('unreviewed private backend header')
        result['gma500_fixed_backend.c'], result['gma500_fixed_backend.h'] = private_fixed_sources(
            result['gma500_fixed_backend.c'], private_header.read_text())
        result['Makefile'] = result.get('Makefile', (native / 'Makefile').read_text()) + '\nccflags-y += -DREMESA_PRIVATE_IRQ_HANDOFF=1\n'
        source = (native / 'remesa_stream_source.c').read_text()
        if hashlib.sha256(source.encode()).hexdigest() != '45fbe31cfdf36c96433ffaef97167477a5f7ad090fa2f5040943aa64ac600bca':
            raise ValueError('unreviewed private stream source')
        result['remesa_stream_source.c'] = private_stream_source(source)
        result['mmu.c'] = private_mmu_flush_source(result['mmu.c'])
        result['mmu.h'] = private_mmu_header_source((native / 'mmu.h').read_text())
        result['mmu.c'] += '\n#include "remesa_mmu_confinement.inc"\n#include "remesa_mmu_private.inc"\n#include "remesa_mmu_switch.inc"\n'
        result['remesa_gem_memory.c'] += '\n#include "remesa_gem_private.inc"\n#include "remesa_gem_private_execute.inc"\n#include "remesa_private_probe.inc"\n'
        result['psb_drv.c'] = replace_once(result['psb_drv.c'],
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n',
            '#include "remesa_private_probe.h"\n#include "remesa_private_probe_uapi.h"\n'
            'static const struct drm_ioctl_desc psb_ioctls[] = {\n'
            '\tDRM_IOCTL_DEF_DRV(REMESA_PRIVATE_PROBE, remesa_private_probe_ioctl, DRM_ROOT_ONLY),\n')
        result['psb_drv.c'] += '\n#include "remesa_easter_egg.h"\n'
        result['psb_drv.c'], result['remesa_gem_memory.c'] = private_entry_sources(
            result['psb_drv.c'], result['remesa_gem_memory.c'])
        result['remesa_gem_memory.c'] = private_file_lost_sources(result['remesa_gem_memory.c'])
        result.update(private_gem_lifetime_sources(native))
        display=(native/'gma_display.c').read_bytes()
        if hashlib.sha256(display).hexdigest()!='877140046fcea01a5e8f0face75384a3dfe2d34983539de76bcfa94ef9f5f2c4':
            raise ValueError('unreviewed fbdev legacy modeset input')
        result['psb_drv.h'],result['framebuffer.c'],result['gma_display.c']=private_fbdev_sources(
            result['psb_drv.h'],result['framebuffer.c'],display.decode())
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


def stage(native, output, lab=False, vendor=False, copy=False, fill=False, private=False):
    if output.exists():
        raise FileExistsError('refusing to overwrite ' + str(output))
    # Inventory all copied sources, including files outside the patch anchors.
    # This records provenance; only PINNED entries are approved hash anchors.
    native_files = {source.name: source.read_bytes() for source in native.iterdir()
                    if source.is_file() and (source.suffix in ['.c', '.h', '.inc'] or
                                             source.name in ['Makefile', 'Kconfig'])}
    patched = patched_sources(native, lab, vendor, copy, fill, private)
    root = Path(__file__).resolve().parents[2]
    additions = root / 'kernel/remesa/gma500'
    components = {name: (additions / name).read_bytes() for name in
                  ['remesa_twod_reservation.h', 'remesa_twod_reservation.inc',
                   'remesa_twod_irq.h', 'remesa_twod_irq.inc']}
    for name in ['remesa_twod_native.h', 'remesa_twod_native.inc',
                 'remesa_mmu_default_root.h', 'remesa_mmu_default_root.inc',
                 'remesa_mmu_verify_access.h', 'remesa_mmu_verify_access.inc',
                 'remesa_gem_twod.h', 'remesa_gem_twod.inc',
                 'remesa_gem_twod_job.h', 'remesa_gem_twod_job.inc']:
        components[name] = (additions / name).read_bytes()
    if private:
        for name in ['remesa_mmu_confinement.h', 'remesa_mmu_confinement.inc',
                     'remesa_mmu_private.h', 'remesa_mmu_private.inc',
                     'remesa_mmu_switch.h', 'remesa_mmu_switch.inc',
                     'remesa_gem_private.h', 'remesa_gem_private.inc',
                     'remesa_gem_private_execute.inc',
                     'remesa_private_probe.h', 'remesa_private_probe.inc',
                     'remesa_private_probe_request.h',
                     'remesa_private_prefire_irq.h',
                     'remesa_private_source_mmio.h', 'remesa_private_source_mmio.inc',
                     'remesa_private_entry_guard.inc',
                     'remesa_private_gem_lifetime.h', 'remesa_private_gem_lifetime.inc',
                     'remesa_private_gtt_guard.inc',
                     'remesa_private_fbdev.h', 'remesa_private_fbdev_map.inc',
                     'remesa_private_fbdev_ops.inc', 'remesa_private_fbdev_detach.inc',
                     'remesa_private_fbdev_config.inc',
                     'remesa_private_file_lost.h', 'remesa_private_memory_file_lost.inc',
                     'remesa_private_stream_file_lost.inc', 'remesa_private_trusted_file_lost.inc',
                     'remesa_easter_egg.h', 'remesa_fixed_private.inc']:
            components[name] = (additions / name).read_bytes()
        components['remesa_private_probe_uapi.h'] = (root / 'include/remesa/private_probe_uapi.h').read_bytes()
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
    parser.add_argument('--with-private-mmu-preparation', action='store_true',
                        help='Compile private MMU/execution and the default-disabled typed qualification ioctl')
    args = parser.parse_args()
    print(json.dumps(stage(args.native_source, args.output, args.with_lab_ioctl,
                           args.with_lab_vendor_stream, args.with_native_copy, args.with_native_fill,
                           args.with_private_mmu_preparation), indent=2))
