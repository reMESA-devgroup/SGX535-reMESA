// SPDX-License-Identifier: GPL-2.0-only
/* Independent passive producer. No ioctl, command or execution entry point. */
#include <linux/module.h>
#include <linux/tracepoint.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <crypto/hash.h>
#include <asm/syscall.h>
#include <asm/unistd.h>
#include "gma500_capsule_observer.h"
#include "gma500_fixed_uapi.h"
#include "frozen_capsule.h"

static DEFINE_SPINLOCK(capsule_lock);
static struct sgx535_capsule capsule;
static struct task_struct *call_task;
static struct tracepoint *enter_point, *exit_point;
static struct proc_dir_entry *evidence_entry;
static struct proc_dir_entry *source_entry;
static struct sgx535_source_guard source_guard;
static struct drm_device *source_device, *operation_device;
static struct module *source_module;
static sgx535_source_reader source_reader;

void sgx535_provenance_source_attach(struct drm_device *dev, struct module *module,
    sgx535_source_reader reader)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!dev || !module || !reader || source_device) sgx535_source_lost(&source_guard);
    else {
        source_device = dev;
        source_module = module;
        source_reader = reader;
    }
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_attach);

void sgx535_provenance_source_observe(struct drm_device *dev,
    sgx535_source_snapshot_fn snapshot)
{
    unsigned long flags;
    struct sgx535_source_facts facts;
    /* Driver callback already owns the fixed IRQ lock. This lock order
     * matches IRQ sampling/admission. Teardown/PM marks lost before freeing
     * or changing the mapped device; never dereference after a lost lifetime. */
    spin_lock_irqsave(&capsule_lock, flags);
    if (dev && dev == source_device && snapshot &&
        capsule.state == SGX535_CAP_UNUSED && !call_task &&
        source_guard.state == SGX535_SOURCE_UNUSED &&
        sgx535_source_boot_valid(&source_guard)) {
        snapshot(dev, &facts);
        facts.coverage_complete = 1;
        sgx535_source_probe(&source_guard, &facts);
    }
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_observe);

void sgx535_provenance_source_startup(struct drm_device *dev,
    const struct sgx535_source_facts *facts)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!dev || dev != source_device || call_task ||
        capsule.state != SGX535_CAP_UNUSED)
        sgx535_source_lifecycle_lost(&source_guard);
    sgx535_source_boot_start(&source_guard, facts);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_startup);

void sgx535_provenance_source_reset_begin(struct drm_device *dev)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    /* Only the single initialization reset, before DRM registration, can
     * establish history. Later reset activity invalidates; never re-arms. */
    if (!dev || dev != source_device ||
        source_guard.boot != SGX535_BOOT_RESET_EXPECTED)
        sgx535_source_lifecycle_lost(&source_guard);
    sgx535_source_producer(&source_guard, 1);
    if (source_guard.state == SGX535_SOURCE_BOUNDARY)
        capsule.invalid = 1;
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_reset_begin);

void sgx535_provenance_source_reset_assert(struct drm_device *dev, u32 reset)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!dev || dev != source_device) sgx535_source_lifecycle_lost(&source_guard);
    sgx535_source_boot_assert(&source_guard, reset);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_reset_assert);

void sgx535_provenance_source_reset_end(struct drm_device *dev,
    const struct sgx535_source_facts *facts)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!dev || dev != source_device) sgx535_source_lifecycle_lost(&source_guard);
    sgx535_source_boot_finish(&source_guard, facts);
    sgx535_source_producer(&source_guard, 0);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_reset_end);

void sgx535_provenance_source_lifecycle(struct drm_device *dev)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    /* Power/teardown discontinuity cannot supply a new seed. */
    sgx535_source_lifecycle_lost(&source_guard);
    if (!dev || dev != source_device) sgx535_source_lost(&source_guard);
    if (source_guard.state == SGX535_SOURCE_BOUNDARY) capsule.invalid = 1;
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_lifecycle);

void sgx535_provenance_source_pending(struct drm_device *dev, u32 s1, u32 s2)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!dev || dev != source_device) sgx535_source_lost(&source_guard);
    /* Retain before the existing IRQ ACK, including events delivered late by
     * the IRQ/service path. Master/ordinary 2D are not TA/3D completions. */
    sgx535_source_pending(&source_guard, (s1 & ~((1U << 31) | (1U << 27))) | s2);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_pending);

int sgx535_provenance_source_write(struct sgx535_gma500_fixed_backend *b)
{
    unsigned long flags;
    int valid;
    spin_lock_irqsave(&capsule_lock, flags);
    valid = sgx535_source_valid(&source_guard, b) && !capsule.invalid &&
        capsule.backend == b && b && capsule.owner == b->owner &&
        current == call_task;
    if (!valid) {
        sgx535_source_lost(&source_guard);
        capsule.invalid = 1;
    }
    spin_unlock_irqrestore(&capsule_lock, flags);
    return valid;
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_write);

void sgx535_provenance_source_producer(int begin)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    sgx535_source_producer(&source_guard, begin);
    if (source_guard.state == SGX535_SOURCE_BOUNDARY && source_guard.reasons)
        capsule.invalid = 1;
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_producer);

int sgx535_provenance_source_begin(struct sgx535_gma500_fixed_backend *b,
    struct drm_device *dev)
{
    unsigned long flags;
    int result;
    spin_lock_irqsave(&capsule_lock, flags);
    result = sgx535_source_begin(&source_guard, b);
    if (result) operation_device = dev;
    if (!source_device || source_device != dev || current != call_task ||
        capsule.state != SGX535_CAP_CALL)
        sgx535_source_lost(&source_guard);
    spin_unlock_irqrestore(&capsule_lock, flags);
    return result;
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_begin);

int sgx535_provenance_source_boundary(struct sgx535_gma500_fixed_backend *b,
    const struct sgx535_source_facts *observed)
{
    struct sgx535_source_facts facts;
    unsigned long flags;
    int result;
    if (!observed) return 0;
    facts = *observed;
    spin_lock_irqsave(&capsule_lock, flags);
    facts.coverage_complete = source_device && source_device == operation_device;
    result = sgx535_source_boundary(&source_guard, b, &facts);
    if (!result) capsule.invalid = 1;
    spin_unlock_irqrestore(&capsule_lock, flags);
    return result;
}
EXPORT_SYMBOL_GPL(sgx535_provenance_source_boundary);

static void syscall_enter(void *unused, struct pt_regs *regs, long id)
{
    unsigned long args[6], flags;

    if (id != __NR_ioctl)
        return;
    syscall_get_arguments(current, regs, args);
    if (args[1] != DRM_IOCTL_PSB_FIXED_TRIANGLE)
        return;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!call_task) {
        get_task_struct(current);
        call_task = current;
        /* A possibly issued attempt cannot unload/rearm this producer. */
        __module_get(THIS_MODULE);
    }
    sgx535_capsule_call(&capsule, current);
    spin_unlock_irqrestore(&capsule_lock, flags);
}

static void syscall_exit(void *unused, struct pt_regs *regs, long result)
{
    unsigned long flags;

    spin_lock_irqsave(&capsule_lock, flags);
    if (current == call_task && capsule.state != SGX535_CAP_CLOSED &&
        syscall_get_nr(current, regs) == __NR_ioctl) {
        if (source_guard.state == SGX535_SOURCE_BOUNDARY &&
            !sgx535_source_close(&source_guard, capsule.backend))
            capsule.invalid = 1;
        sgx535_capsule_close(&capsule, current, (int)result);
    }
    spin_unlock_irqrestore(&capsule_lock, flags);
}

/* Hash the actual OS-retained executable, not a supplied pathname. This is
 * sleepable backend admission, never a tracepoint/IRQ allocation. */
static int approved_executable(void)
{
    static const u8 expected[32] = {
        0x2f,0x84,0x91,0x7f,0x96,0xdb,0x28,0x59,
        0x67,0x87,0x97,0xa3,0x27,0xd4,0x0d,0x96,
        0x38,0x32,0x5e,0x5c,0x5e,0xfb,0x6e,0x0d,
        0xfc,0xce,0xf4,0x4d,0x75,0x6b,0x48,0x35
    };
    struct file *file = get_task_exe_file(current);
    struct crypto_shash *tfm = NULL;
    struct shash_desc *desc = NULL;
    u8 *buffer = NULL, digest[32];
    loff_t position = 0;
    ssize_t count;
    int result = 0;

    if (!file)
        return 0;
    if (!S_ISREG(file_inode(file)->i_mode) ||
        i_size_read(file_inode(file)) != 775264 ||
        !uid_eq(file_inode(file)->i_uid, GLOBAL_ROOT_UID) ||
        (file_inode(file)->i_mode & 0022) ||
        file_inode(file)->i_nlink != 1)
        goto out;
    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm)) { tfm = NULL; goto out; }
    desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
    buffer = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!desc || !buffer)
        goto out;
    desc->tfm = tfm;
    if (crypto_shash_init(desc))
        goto out;
    while (position < 775264) {
        count = kernel_read(file, buffer,
            min_t(size_t, PAGE_SIZE, 775264 - position), &position);
        if (count <= 0 || crypto_shash_update(desc, buffer, count))
            goto out;
    }
    if (i_size_read(file_inode(file)) == 775264 &&
        !crypto_shash_final(desc, digest) && !memcmp(digest, expected, 32))
        result = 1;
out:
    kfree(buffer);
    kfree(desc);
    if (tfm) crypto_free_shash(tfm);
    fput(file);
    return result;
}

void sgx535_provenance_admit(struct sgx535_gma500_fixed_backend *b)
{
    struct sgx535_gma500_owner *o = b->owner;
    unsigned long flags;
    int approved = approved_executable();

    spin_lock_irqsave(&capsule_lock, flags);
    if (!sgx535_source_valid(&source_guard, b)) capsule.invalid = 1;
    sgx535_capsule_admit(&capsule, current, b, &o->fixed_service, o,
        o->session.phase, o->session.observed_events, o->session.sequence,
        approved && current == call_task && b->active && b->power_held &&
        !b->fire_possible && !b->pending_status1 && !b->pending_status2 &&
        o->initialized && o->device_claimed &&
        o->fixed_service.session == &o->session);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_admit);

static int same_service(struct sgx535_gma500_fixed_backend *b,
    struct sgx535_fixed_service *s)
{
    return current == call_task && b && b->owner &&
        s == &b->owner->fixed_service && s->session == &b->owner->session;
}

void sgx535_provenance_service(struct sgx535_gma500_fixed_backend *b,
    struct sgx535_fixed_service *s, int matched)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    sgx535_capsule_service(&capsule, b, s, matched && same_service(b, s));
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_service);

void sgx535_provenance_issued(struct sgx535_gma500_fixed_backend *b)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (current != call_task) capsule.invalid = 1;
    sgx535_capsule_issued(&capsule, b);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_issued);

void sgx535_provenance_sample(struct sgx535_gma500_fixed_backend *b, u32 q,
    u32 a, u32 d, int exclusive)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (current != call_task) capsule.invalid = 1;
    sgx535_capsule_sample(&capsule, b, q, a, d, exclusive);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_sample);

void sgx535_provenance_before(struct sgx535_gma500_fixed_backend *b,
    struct sgx535_fixed_service *s, u32 q, u32 a, u32 d, int exclusive)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!same_service(b, s)) capsule.invalid = 1;
    sgx535_capsule_before(&capsule, b, s, q, a, d, exclusive);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_before);

void sgx535_provenance_after(struct sgx535_gma500_fixed_backend *b,
    struct sgx535_fixed_service *s)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!same_service(b, s)) capsule.invalid = 1;
    else sgx535_capsule_after(&capsule, b, s,
        s->session->observed_events, s->session->phase);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_after);

void sgx535_provenance_terminal(struct sgx535_gma500_fixed_backend *b,
    struct sgx535_fixed_service *s, int result)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (!same_service(b, s)) capsule.invalid = 1;
    else sgx535_capsule_terminal(&capsule, b, s, result,
        s->session->phase, s->session->observed_events);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_terminal);

void sgx535_provenance_color(struct sgx535_gma500_owner *o, const void *bytes)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    if (current != call_task) capsule.invalid = 1;
    sgx535_capsule_color(&capsule, o, o->session.phase, bytes, PAGE_SIZE);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_color);

void sgx535_provenance_release(struct sgx535_gma500_owner *o)
{
    unsigned long flags;
    spin_lock_irqsave(&capsule_lock, flags);
    sgx535_capsule_release(&capsule, o);
    spin_unlock_irqrestore(&capsule_lock, flags);
}
EXPORT_SYMBOL_GPL(sgx535_provenance_release);

static int evidence_open(struct inode *inode, struct file *file)
{
    unsigned long flags;
    void *bytes = kmalloc(SGX535_CAPSULE_BYTES, GFP_KERNEL);
    if (!bytes) return -ENOMEM;
    spin_lock_irqsave(&capsule_lock, flags);
    sgx535_capsule_encode(&capsule, bytes, SGX535_CAPSULE_BYTES);
    spin_unlock_irqrestore(&capsule_lock, flags);
    file->private_data = bytes;
    return 0;
}
static ssize_t evidence_read(struct file *file, char __user *buffer,
    size_t count, loff_t *offset)
{
    return simple_read_from_buffer(buffer, count, offset, file->private_data,
        SGX535_CAPSULE_BYTES);
}
static int evidence_release(struct inode *inode, struct file *file)
{
    kfree(file->private_data);
    return 0;
}
static const struct proc_ops evidence_ops = {
    .proc_open = evidence_open, .proc_read = evidence_read,
    .proc_lseek = default_llseek, .proc_release = evidence_release
};

/* Snapshot of retained facts only. Opening this file performs no MMIO,
 * admission, hardware state change or proof injection. Same release/read API. */
static int source_open(struct inode *inode, struct file *file)
{
    unsigned long flags;
    struct module *module;
    struct drm_device *dev;
    sgx535_source_reader reader;
    char *bytes = kzalloc(SGX535_CAPSULE_BYTES, GFP_KERNEL);
    (void)inode;
    if (!bytes) return -ENOMEM;
    spin_lock_irqsave(&capsule_lock, flags);
    module = source_module; dev = source_device; reader = source_reader;
    if (capsule.state != SGX535_CAP_UNUSED || call_task ||
        source_guard.state != SGX535_SOURCE_UNUSED ||
        !sgx535_source_boot_valid(&source_guard)) reader = NULL;
    if (reader && !try_module_get(module)) {
        spin_unlock_irqrestore(&capsule_lock, flags);
        kfree(bytes);
        return -EAGAIN;
    }
    spin_unlock_irqrestore(&capsule_lock, flags);
    if (reader) {
        reader(dev);
        module_put(module);
    }
    spin_lock_irqsave(&capsule_lock, flags);
    scnprintf(bytes, SGX535_CAPSULE_BYTES,
        "SGXSOURCE2\nstate=%u\nreasons=%u\nproducers_active=%u\n"
        "driver_attached=%u\ncapsule_state=%u\nboot=%u\nprepared=%u\nasserted_reset=%u\nreleased_reset=%u\n"
        "startup_pending=%u\nstartup_reads=%u\nstartup_fault=%u\nstartup_autonomous=%u\n"
        "delayed_exclusion=%s\n",
        source_guard.state, source_guard.reasons, source_guard.producers_active,
        source_device != NULL, capsule.state, source_guard.boot,
        source_guard.prepared, source_guard.asserted_reset,
        source_guard.released_reset, source_guard.startup_pending,
        source_guard.startup_reads, source_guard.startup_fault,
        source_guard.startup_autonomous,
        sgx535_source_boot_valid(&source_guard) ? "STARTUP_LIFECYCLE" : "UNPROVEN");
    spin_unlock_irqrestore(&capsule_lock, flags);
    file->private_data = bytes;
    return 0;
}
static ssize_t source_read(struct file *file, char __user *buffer,
    size_t count, loff_t *offset)
{
    return simple_read_from_buffer(buffer, count, offset, file->private_data,
        strlen(file->private_data));
}
static const struct proc_ops source_ops = {
    .proc_open = source_open, .proc_read = source_read,
    .proc_lseek = default_llseek, .proc_release = evidence_release
};

static void find_syscalls(struct tracepoint *tp, void *unused)
{
    if (!strcmp(tp->name, "sys_enter")) enter_point = tp;
    if (!strcmp(tp->name, "sys_exit")) exit_point = tp;
}
static int __init provenance_init(void)
{
    int result;
    for_each_kernel_tracepoint(find_syscalls, NULL);
    if (!enter_point || !exit_point) return -ENODEV;
    result = tracepoint_probe_register(enter_point, syscall_enter, NULL);
    if (result) return result;
    result = tracepoint_probe_register(exit_point, syscall_exit, NULL);
    if (result) goto undo_enter;
    evidence_entry = proc_create("sgx535_current_operation", 0400, NULL,
        &evidence_ops);
    if (evidence_entry) {
        source_entry = proc_create("sgx535_source_guard", 0400, NULL, &source_ops);
        if (source_entry) return 0;
        proc_remove(evidence_entry);
    }
    tracepoint_probe_unregister(exit_point, syscall_exit, NULL);
    result = -ENOMEM;
undo_enter:
    tracepoint_probe_unregister(enter_point, syscall_enter, NULL);
    tracepoint_synchronize_unregister();
    return result;
}
static void __exit provenance_exit(void)
{
    proc_remove(source_entry);
    proc_remove(evidence_entry);
    tracepoint_probe_unregister(exit_point, syscall_exit, NULL);
    tracepoint_probe_unregister(enter_point, syscall_enter, NULL);
    tracepoint_synchronize_unregister();
    if (call_task) put_task_struct(call_task);
}
module_init(provenance_init);
module_exit(provenance_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SGX535 passive single-use current-operation evidence");
