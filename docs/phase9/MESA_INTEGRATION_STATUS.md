# Mesa/Gallium integration status

**IMPLEMENTED_OFFLINE**, Mesa25.2.0. The new CODE subset is available through
explicit host pipeline preparation, with the existing shader lifecycle and
ownership checks. A complete executable PDS and functional OpenGL context are
not claimed.

## Actual path

```text
bound pipe_context VS/FS NIR states
 -> existing remesa restricted shader_program/USSE translation
 -> existing candidate VS and constant green/magenta FS bodies
 -> remesa_context_prepare_pds_structure_pipeline
 -> remesa_shader_host_pipeline_create_pds_structure
 -> existing image / USE relocation / DATA / owner validation
 -> new immutable calculated CODE + CODE mask + DATA mask
 -> existing draw-preparation contract, GPU execution still ENOTSUP
```

The request supplies symbolic R/T inputs and the existing allocation/USE
bindings. The fragment DATA recipe is derived from this shape; a caller's
fragment recipe is explicitly ignored, while its VS recipe is validated and
copied. Layout sizes must match the actual DATA and slot counts. CODE must
follow DATA with the admitted alignment. The psb background is derived from
R/T domains (0,80000,100000,180000), rather than assuming the separate Xpsb
constant-pixel recipe's unconditional100000 background.

Texture address fields stay symbolic. No new physical texture relocation
semantics are invented. The existing lower19 USE fixup and bank validation
remain in use. The attached canonical structure retains the symbolic records.

`remesa_shader_host_pipeline_revalidate_pds_structure()` and its context
wrapper require the current bound shader pair, descriptor recipe, sizes,
ownership and mapping generations to agree. The pipeline owns all bytes;
source IR/recipe lifetimes may end after construction. Failed allocation does
not publish an object, leak storage or corrupt the previously bound shader.

The immutable-store format `HOST_PDS_STRUCTURE` admits four-byte units but
`remesa_shader_storage_validate_execution()` still returns ENOTSUP. Existing
inspection and execution-contract outputs reject overlap with CODE and both
masks, as well as the previous DATA/USSE stores. The pipeline reports
`structure_error=0`, independently from `pds_code_error=ENOTSUP` and
`execution_error=ENOTSUP`.

## Build and tests

`prepare_mesa_loader.py` stages the structure module in the generated Meson
driver, and affected qualification source lists link it. The shader IR runner
also gets the missing repository include path and common shader-request source.
Its older builder fixture now checks the already-supported constant magenta FS
instead of incorrectly expecting ENOTSUP, retaining every positive/negative case.
No renderer/resources/fences/
framebuffer/transfer subsystem was replaced.

The final real archive and DRI results, frozen artifacts and source freshness
receipt are recorded under `mesa-build/RESULTS.json`. The archive is qualified
by tests linked against the real Meson objects, not replacement driver sources.
Previous staged source, archive, thin-archive objects and DRI are backed up
before rebuilding. The real archive tests passed1396 structural pipeline,
340 pixel task,207 host pipeline and32 NIR builder checks. The new DRI is not
installed into the system.

Ninja's combined DRI dry-run initially looked dirty because the PHONY
git_sha1.h generator cannot be restatted by a dry-run. The runner rejected
that result and preserved the diagnostic. A subsequent real -j1 build ran
only that generator; restat cancelled the dependent work and hashes of891
objects/archives/shared objects stayed identical. The driver archive's own
dry-run reports no work. This actual quiescence, not a claimed DRI dry-run
PASS, is the final freshness evidence.

The linker emitted a LOAD RWX warning. The previous preserved DRI and the new
DRI both have the same PT_LOAD flag sequence4,5,4,7; this round did not change
those segment permissions or linker flags. This build receipt is not a claim
that the cached loader is ready for installation or a GPU execution test.

Default `draw_vbo` behavior and transport admission are unchanged. The new
helper uses the same bound shader preparation boundary but does not convert
a sparse CODE artifact into something that draw can submit. Reaching a
working OpenGL2.1 context still requires original initial/task/end instruction
rules, executable VS export, and separately authorized hardware validation.
