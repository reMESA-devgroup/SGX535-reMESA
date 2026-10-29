# Guarded reservation to fixed3D IRQ handoff

The opt-in private-MMU staging profile now supplies
`sgx535_gma500_fixed_backend_begin_guarded`. It transfers the existing unissued
reservation's IRQ ledger to the already validated fixed3D backend under the
shared IRQ lock, retaining the console/native2D producer reservation throughout.
The original begin/source-history and fixed-service release gates remain in
place. No private-root publication, new renderer, ioctl or GPU issue was added.

Evidence: `/home/gama/sgx535-offline/irq-handoff-20261029/v1/`.
`GUARD_RESULT.json`, `GUARD_COMMANDS.json`, `LEGACY_STAGING.json` and the logs
bind source models, real kernel compilation and partial link. The stager pins
the original backend header SHA256
`97c2f97e00d61970195b79cb7ddb4acc1a5c4b314c739f707ded12df3cc1260f` before
adding the guarded prototype. Qualified source/checkpoint files are not edited.

## Atomic ownership and existing gates

The original begin function is factored in the staged copy into a common
implementation. Its normal public wrapper passes no guard/token, preserving the
normal admission path. Its new guarded wrapper requires both. Both original
IRQ-lock admission points recheck the exact held reservation, device, MMU token,
unissued ledger and absence of another fixed owner. Source begin, source facts,
source boundary, owner initialization, power and host-image checks are retained.
They may still reject a candidate; this patch does not reinterpret rejection
as approval or replace retained source history with an idle sample.

Only after those checks does one IRQ-lock interval replace the reservation
ledger owner with the fixed backend and record its lender/token. The device's
producer reservation remains held. During the unlocked portions of original
begin, that reservation still blocks console/native2D and competing fixed
admission. The trusted caller must serialize the stable loan and backend.
A borrowed ledger also keeps general IRQ availability false if ownership
metadata ever becomes inconsistent.

End still requires the original fixed-service `may_release` gate and no USE
reservation. That gate must represent either an unissued service or its owned
completion/retirement lifecycle. A guarded end additionally refuses pending
status1/status2, a lost/mismatched lender token, an issued lender ledger or
inconsistent producer ownership. It returns the ledger to the lender under
the same IRQ lock before clearing the fixed backend's active/power bookkeeping.
The MMU token remains claimed; ordinary producer release/2D issue is still
blocked until private-owner cleanup and guard release succeed. No ACK, reset,
retry, MMIO root write or manufactured retirement occurs in the handoff.

The normal fixed path historically treats2D completion as display-owned benign
status. In the guarded path2D production is excluded, so bit27 is instead
retained in the fixed pending ledger. The unchanged service/status contract
can reject that unexpected event, and end cannot silently drop pending status.
The normal unguarded mask is preserved. All status2 bits remain retained.

## Qualification

`test_irq_handoff.c` compiles the actual staged common begin, wrappers and end,
and actual IRQ capture/handoff code, against modeled source-history/lifecycle
and kernel operations. Native, UBSan and i386 each pass123 parent checks plus3
joined retained-fault models. Cases cover normal entry refusal while reserved,
wrong token, original initialization/source-begin/source-boundary rejections,
successful transfer/return, console producer blocking, early end refusal,
unexpected2D completion, status2 faults and lost lender token. Fault children
retain their model ownership; no event clear or fake retirement is used to
make their cleanup pass. The successful guarded scenario is unissued.

Reservation, IRQ and native2D regressions pass641,980 and3306 parent checks
respectively in each variant, with their existing joined fault cases. Inherited
reservation checks are not independent unique coverage and must not be summed.
These are CPU source models, not actual GPU completion evidence.

Six real antiX units compile and partially link with copy/vendor/fill and
`REMESA_PRIVATE_IRQ_HANDOFF` enabled: accel_2d, psb_drv, gma500_fixed_backend,
psb_irq, mmu and remesa_gem_memory. New driver symbols resolve. This is not a
full module link, installation or physical qualification. The private stager
adds the required compile flag; no runtime knob selects this path.
All five old generated native source replacement profiles remain byte-identical
with private=False. Shared include text changes are tested by the regressions;
that comparison does not claim the entire old module is byte-identical.
All31 protected files remain unchanged.

## Next integration requirement

The kernel can now transfer IRQ ownership while preserving the borrowed
producer reservation and original scene gates. Default-MMU writers must still
be excluded across the actual root bind, issue, owned completion/visibility/
retirement and restore interval. The current GEM preparation bridge does not
call the guarded backend or publish a root. Complete indirect resource closure,
private-PD-aware backend validation and physical textured drawing remain open.
No general GL/GLES2, desktop2D, FPS gain or installable-driver completion is
claimed by this component.
