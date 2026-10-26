# Background object CPU recovery

IMPLEMENTED_OFFLINE; GPU semantics remain unqualified for the new profile.

The original psb_dri.so constructor ELF 0x2ee34, reached from its original caller 0x2f317, commits 68 bytes through 0x384ab. Nine bounded original-x86 cases execute the caller, constructor, relocation helpers and commit. Allocation, lock, map and validation ABI boundaries are explicit cuts recorded by the replay; this is not execution of the whole compiler.

Primary metadata is DATA12/control1, secondary DATA0/control0. Control is `primary.control10 | 0x30000 | (((secondary.control10+127)<<11)&0x01fc0000)`. References use address>>4 and DATA count contribution, checked against BO extents and a common 256MiB window. The supported API requires primary64/secondary4 bytes, disjoint aligned storage. Canonical field14=0 is a host API policy, not a recovered hardware restriction. Coefficients come from the caller; the constructor supplies tag2/zero and six geometry words. No GPU instruction body is incorporated.

The typed encoder builds all17 DWORDs transactionally, reusing the independently recovered non-square extent encoder. Eight initial original-x86 oracles pass native, UBSan, i386/QEMU and the actual Mesa archive. The ninth replay matches the first68 bytes of the preserved physical-reference input role3.bin; all four compiled tests also pass with this input substituted. This authenticates a historical INPUT descriptor comparison, not new hardware consumption or depth execution. Existing fragment-binding61 and full-size owned-resource88 checks pass against the rebuilt archive. Static Mesa and DRI build succeeds.

No GPU submissions, SSH, MMIO, kernel loading or gate changes. Background USE body remains an explicitly retained LAB dependency. Next: per-purpose bounded VA allocation and full native resource composition at1280x800; the old exact32 admission cannot admit that job.
