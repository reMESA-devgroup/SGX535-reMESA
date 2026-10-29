# Local evidence index — not part of the external RFC

This appendix identifies local artifacts used for the source audit and RFC.
The external architecture draft is `sgx535-kernel-rfc.md`; it does not depend on
access to these absolute paths. No report below is evidence of physical private
MMU execution. Artifacts were read, not rebuilt or installed during this audit.

- Source worktree: `/home/gama/sgx535-worktrees/driver-resume-20261018`;
  reviewed runtime HEAD `6f841fb57b7fc7e52c30e86a200857929c4525dd`.
- Full private module reports:
  `/home/gama/sgx535-offline/private-module-build-20261029/v3/RESULT.json`,
  `MODULE_VERIFICATION.json`, `PROBE_PROFILE_VERIFICATION.json` in that directory.
- Staged kernel source used for line references:
  `/home/gama/sgx535-offline/private-module-build-20261029/v3/module/`.
- Controller model report:
  `/home/gama/sgx535-offline/private-probe-controller-20261029/v4/RESULT.json`.
  It records 36 scenarios across native/UBSan/i386; GEM, crypto, grammar,
  borrower, source and private execution dependencies are modeled, GPU0.
- Local kernel API implementation inspected:
  `/home/gama/sgx535-offline/antix-kbuild-preparation-20260930/source-unpack/linux-5.10.240-antix.1-486-smp/`;
  specifically `include/drm/drm_drv.h` and `drivers/gpu/drm/drm_drv.c`.

The handler test and the userspace dry-run client are coordinating-agent work;
this audit did not modify them or execute them. Their changing working-tree
status must not be conflated with the reviewed runtime HEAD or final linked
module. N/N boot settings, rollback and protected checkpoints remain untouched.
