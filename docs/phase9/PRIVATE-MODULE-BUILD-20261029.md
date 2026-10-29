# Full private kernel-module build

The complete opt-in private candidate now builds through all translation units,
modpost and final `gma500_gfx.ko` linking. It is an offline candidate for
`5.10.240-antix.1-486-smp`, not an installed or physically qualified driver.
No new private-execution ioctl is registered.

The builder stages pinned qualified native sources plus the reviewed private
changes. It copies the prepared kernel output before building, uses one make job,
and caps each build process at 768 MiB of virtual address space. Original kernel
configuration, kernel symbol versions and observer symbol inputs are hashed and
checked unchanged. Existing code, checkpoints, Codex data and toolchains are not
removed. The tool never installs modules or changes Mini12 boot state. Future
build timeouts/interruption terminate the make/compiler process group.

The initial full build used the preserved `BUILD_TOOL_INITIAL.py` at
`/home/gama/sgx535-offline/private-module-build-20261029/v1/`. It completed in
152.455 seconds of make time after staging/copying. Its module SHA256 is
`930f4b19adab6e207571b06f68bb00ef9a9e5393c04724e88d60e934736be14d`.
That artifact precedes the final source-settling fix and is preserved as historical
build evidence. v2 rebuilds the complete module with that fix using the same
private copied kernel cache; it preserves the v1 module and key kernel inputs.
Final results and module path are in v2/RESULT.json. The final build took
100.601 seconds and its module SHA256 is
`ea66ad988d29cbc7cc7958d747e7eb340afdb51349f864b33a0480a145d9e06e`. No claim of hardware approval
follows from either build.

The final ELF verification checks 32-bit little-endian EM_386, target vermagic,
new private/2D function definitions and absence of unresolved new driver symbols.
Memory, retained LAB, typed-copy and fill controls are zero-initialized BSS. This
proves compiled defaults, not the values of a loaded module or boot command line.
The retained compiled stream profile's 5844-byte symbol is compared with the
original qualified assets by the existing ELF profile oracle; matching it
preserves that asset, without transferring prior physical approval to new code.
The module retains its original observer/DRM/video/I2C dependencies.

The build log contains existing C90 mixed-declaration warnings in BLAKE3. No new
private-driver compilation failure was ignored. The end result must pass modpost
and final linking; individual object or partial-link success is insufficient.

## Reproduce offline

From the repository worktree, select a fresh output path:

```sh
python3 tools/remesa/build_private_module.py \
  --native /home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/module \
  --kernel /home/gama/sgx535-offline/antix-kbuild-successor-20260930/output \
  --command-file /home/gama/sgx535-offline/phase9-fs-differential-handoff-20261014/green-module/BUILD.json \
  --environment-file /home/gama/sgx535-offline/phase9-real-mesa-20261008/build-environment.json \
  --symbols /home/gama/sgx535-offline/gears-depth-less-single-clear-20261026T160400Z/observer/Module.symvers \
  --output /home/gama/sgx535-offline/private-module-build-20261029/reproduction
```

The command refuses existing output directories. `COMMAND.json`, `build.log` and
`RESULT.json` retain the actual build and input hashes. These dependencies are the
existing local cross-toolchain and prepared kernel; this is not yet a portable
package recipe for other distros or kernels.

Next work is full private executor/backend/source composition and controlled
hardware qualification, followed by actual userspace admission/resource/shader
integration. Complete desktop OpenGL/GLES2, accelerated desktop 2D, VA-API and a
qualified installable driver remain unfinished. The `.ko` artifact alone does
not meet that objective.

## Inspection audit

The initial ad hoc metadata fallback invoked llvm-objcopy with only an input
file. Although extracting .modinfo, that invocation also rewrote the v1 module,
changing its SHA256 to
`32d0ecb343c498f09d4cc58ee4826da10336b9f757683c0b604e50404a6641bc`.
That inspected copy is preserved. The original linker command was replayed into
a separate file and reproduced the initial
`930f4b19adab6e207571b06f68bb00ef9a9e5393c04724e88d60e934736be14d`
hash exactly. The byte-exact reconstructed original and report are at
`v1/original-link-reconstructed-v2/`; v1's initial command-parser error before
that successful replay did not modify any input. No Mini12 operation occurred.

`tools/remesa/verify_private_module.py` now parses the ELF directly and compares
the module's SHA256 before/after inspection. The final v2 module is unchanged by
both that verifier and the original compiled-profile oracle. Its
`MODULE_VERIFICATION.json` and `MODULE_PROFILE_VERIFICATION.json` bind the exact
final hash above. The source for the initial builder is preserved as
`v1/BUILD_TOOL_INITIAL.py`; the repository builder additionally includes process-
group cleanup for timeout/interruption, without changing the successful build
command or source staging.

`v2/FINAL_GATE_RELOCATION.json` also locates the call to
`remesa_stream_source_settled` inside the final compiled
`remesa_gem_private_execute` function, following restore and source unbind. This
binds the integration fix to the final binary without claiming it was executed
on hardware.
