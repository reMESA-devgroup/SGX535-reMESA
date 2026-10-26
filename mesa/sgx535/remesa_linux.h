/* SPDX-License-Identifier: MIT */
#ifndef REMESA_LINUX_H
#define REMESA_LINUX_H
#include "remesa_device.h"
#include "remesa_retained_transport.h"
#include "remesa/retained_evidence.h"
/* Single-attempt DRM hardware transport; no CPU rasterization or retries. */
int remesa_linux_open(struct remesa_device *, const char *render_node);
/* Duplicates a writable loader fd with CLOEXEC; never closes the caller fd.
 * DRM name validation is discovery, not proof of native-ioctl support. */
int remesa_linux_open_fd(struct remesa_device *, int borrowed_fd);
void remesa_linux_close(struct remesa_device *);
/* Close refuses to discard an uncertain original-program storage endpoint.
 * Shader destruction likewise retains the participating source token and its
 * pinned program/geometry inventory during active or uncertain retained work.
 * The void compatibility wrapper leaves the device intact on such a failure. */
/* Live fences keep the endpoint open. wait/destroy accept only this endpoint's
 * currently owned opaque fences; unknown/foreign destruction is a no-op. */
int remesa_linux_close_checked(struct remesa_device *);
struct remesa_persistent_config;
/* Explicitly attach an owned resource-only staging endpoint before any draw
 * preparation. The caller retains its authenticated provider until close.
 * No service discovery, MEM_OPEN, GPU mapping or hardware execution is done
 * here. Normal DRM open never enables this optional resource service. */
int remesa_linux_enable_shader_staging(struct remesa_device *,
 const struct remesa_persistent_config *);
struct remesa_linux_memory;
struct remesa_linux_fragment_evidence {
 struct remesa_evidence_expected expected;
 struct remesa_evidence_response response;
 struct remesa_evidence_observer observer;
 struct remesa_evidence_ownership ownership;
};
/* Expected inventory for a protected FS-only capture adapter. These are live
 * CPU backing identities, not GPU addresses or evidence of execution. The two
 * additional allocations are the original program and its raw VB/IB snapshot;
 * the ten mapped carrier allocations remain in the retained request. */
struct remesa_linux_fragment_capture {
 uint64_t owner,generation;
 uint32_t program_handle,geometry_handle,source_offset,argb;
 uint64_t program_bytes,geometry_bytes;
 unsigned char source[8],preimage[8];
 bool program_changed;
 /* Optional exact POSITION4 source delivery; zero in the FS-only profile. */
 bool vertex_staged;
 uint32_t vertex_source_handle,vertex_source_offset,vertex_use_offset;
 unsigned char vertex_source[16],vertex_preimage[16];
};
struct remesa_linux_fragment_carrier {
 struct remesa_linux_memory *memory; /* same endpoint as shader staging */
 const struct remesa_retained_request *job; /* copied before first shader */
 remesa_retained_recorder record;
 /* Trusted capture adapter must authenticate complete raw observer/kernel
  * records for this exact response. Normalized consistency alone is not
  * hardware authentication. No default/permissive implementation is supplied. */
 int (*authenticate)(void *,const struct remesa_retained_request *,
                     struct remesa_linux_fragment_evidence *);
 void *priv; /* caller retained until close; callbacks cannot mutate/reenter
              * this device, its tokens, or the memory endpoint */
 /* Optional replacement for authenticate, not an additional trust source.
  * Exactly one authentication callback must be supplied. The capture is a
  * synchronous immutable expectation, to be bound to authenticated raw records
  * by the adapter; do not retain its pointer after the callback returns.
  * Before invoking this callback, the transport verifies the CPU-only program
  * and geometry backing and records INFO for all twelve objects after the
  * nominal retired response. The protected caller must attach a memory recorder
  * before preparation; these observations are not hardware authentication. */
 int (*authenticate_fragment)(void *,const struct remesa_retained_request *,
              const struct remesa_linux_fragment_capture *,
              struct remesa_linux_fragment_evidence *);
 /* An opt-in differential experiment must reject byte-identical replacement
  * before writing USE or issuing. This does not admit a new color/profile. */
 bool require_program_change;
 /* Zero retains the historical magenta transport control. Green selects only
  * the exact differential candidate and requires authenticate_fragment and
  * require_program_change. The matching separately qualified kernel profile
  * is still required; this field cannot select or authorize a kernel profile. */
 uint32_t fragment_argb;
 /* Default-off exact green lab extension. Requires explicit numeric mapping
  * 2:c -> 1:c, owned NIR POSITION4 VS16, authenticate_fragment, and matching
  * preinstalled VS16 at USE+0x40. Copies the token's generated VS bytes, with
  * complete preimage/readback checks. No new instruction profile is admitted. */
 bool independent_vertex;
 /* Explicit finite quarter-window FBO experiment, never normal GL admission.
  * Requires independent_vertex, exact green+authentication+program change and
  * numeric mapping2:c->1:c. Backend still admits exact32x32/three vertices. */
 bool gl_fbo_half_pixel_center;
};
/* Explicit single-attempt fragment-stage laboratory. Original NIR FS is
 * delivered to USE+0; historical VS/PDS/state remain the externally prepared,
 * exact FIRE3 carrier, checked again by the kernel's retained profile.
 * Does not generate/copy carrier programs, open another SGX owner, enable a
 * kernel gate or qualify execution. Never enabled by ordinary loader/context
 * creation. Call before creating a screen and before preparing any shader.
 * Only the two explicit constant candidates/32x32/exact geometry are supported.
 * Historical magenta remains the default; green requires the separate exact
 * candidate kernel profile and fresh experiment qualification. Externally
 * serialize access and retain all ten mapped BOs and the memory endpoint
 * through authenticated retirement, or indefinitely on uncertain failure. */
int remesa_linux_enable_retained_fragment(struct remesa_device *,
 const struct remesa_linux_fragment_carrier *);
struct remesa_linux_fragment_receipt {
 uint64_t owner,generation,use_va;
 uint32_t source_handle,source_offset,use_handle;
 unsigned char source[8],preimage[8];
 bool staged,outside_fragment_unchanged,attempted,authenticated,quarantined;
 bool program_changed;
 /* Optional exact POSITION4 source delivery; zero in the FS-only profile. */
 bool vertex_staged;
 uint32_t vertex_source_handle,vertex_source_offset,vertex_use_offset;
 unsigned char vertex_source[16],vertex_preimage[16];
};
int remesa_linux_last_retained_fragment(const struct remesa_device *,
 struct remesa_linux_fragment_receipt *);
struct remesa_native_draw;
struct remesa_shader_image;
struct remesa_shader_request;
struct remesa_shader_bindings;
struct remesa_shader_draw_programs;
struct remesa_usse_vertex_mapping;
/* Explicit offline numeric VS candidate preparation. No bank mapping is
 * selected implicitly; no PDS/termination is synthesized. Returns ENOTSUP and
 * an owned token on successful storage preparation. With resource staging,
 * both original VS/FS CPU ranges share checked, pinned, byte-verified backing.
 * Destroy through device.ops->destroy_shader_programs; output must be empty.
 * This does not change the ordinary Gallium callback or hardware admission. */
int remesa_linux_prepare_vertex_candidate(struct remesa_device *,
 const struct remesa_shader_draw_programs *,const struct remesa_usse_vertex_mapping *,void **);
/* Explicit CPU-only staging through the already-configured resource endpoint.
 * Owns independent image/storage references. Requires empty output and existing
 * shader staging configuration. ENOTSUP+token is not execution admission. */
int remesa_linux_prepare_image(struct remesa_device *,const struct remesa_shader_image *,void **);
/* Explicit offline CPU profile for subsequent ordinary Gallium preparation.
 * Copies checked numeric assignments by value, before the first candidate.
 * Does not select hardware banks, provide termination, enable PDS generation,
 * change admission, or authorize a draw. Execution preparation remains ENOTSUP.
 * The profile is frozen once any candidate has been retained; externally
 * serialize configuration with context creation/draws. Unchanged on error. */
int remesa_linux_set_vertex_candidate_mapping(struct remesa_device *,
 const struct remesa_usse_vertex_mapping *);
/* Pure wire construction; no ioctl or device access. */
int remesa_linux_request_build(const struct remesa_draw_packet *,struct remesa_native_draw *);
/* Original-program staging, separate from the diagnostic color packet. Copies
 * owned candidate bytes into a versioned bounded request. No ioctl number is
 * assigned to this request and no GPU upload/submission is performed. Binding
 * values identify the receiving software lifetime; they are not authentication. */
int remesa_linux_shader_request_build(const struct remesa_shader_image *,
 uint64_t owner,uint64_t generation,struct remesa_shader_request *);
/* Retained CPU snapshot from the last successful candidate preparation, even
 * though execution preparation returns ENOTSUP. This is NOT a kernel response,
 * proof of upload, or permission to execute. Output unchanged if absent. */
int remesa_linux_last_shader_request(const struct remesa_device *,struct remesa_shader_request *);
/* Live token-specific staging binding, unlike the retained last-request copy.
 * Requires optional resource staging and a currently owned token. Callers must
 * discard tokens at destruction. Their small identity cookies are retained
 * until successful endpoint close, preventing reuse within that lifetime.
 * Exact ID, byte ranges and provider owner/generation; no execution address.
 * No ioctl. Outputs may not alias any live program or borrowed raw VB/IB
 * snapshot on this endpoint. Unchanged on failure; serialized with destroy. */
int remesa_linux_shader_bindings(const struct remesa_device *,void *token,
 struct remesa_shader_request *,struct remesa_shader_bindings *);
/* Explicit CPU readback of this token's actual backing. No GPU activity. */
int remesa_linux_shader_verify(struct remesa_device *,void *token);
struct remesa_shader_draw_inputs;
struct remesa_draw_plan;
/* Own a bounded immutable pre-VS VB/IB/state snapshot under a live original
 * program token. No diagnostic color/geometry packet substitutes for it.
 * Owns a byte-verified, pinned CPU-GEM VB/IB allocation through the optional
 * resource-only provider. This is CPU preparation only, not GPU qualification.
 * One immutable geometry per token; a replacement returns EBUSY. Failed
 * preparation preserves the previous snapshot; uncertain provider release
 * retains its own backing in quarantine and makes close return EBUSY.
 * Caller holds source mapping leases for the call, serialized with destroy. */
int remesa_linux_shader_stage_draw(struct remesa_device *,void *token,
 const struct remesa_shader_draw_inputs *);
/* Borrowed snapshot bytes remain valid until token destruction.
 * No device syscall; outputs unchanged on failure. */
int remesa_linux_shader_draw_snapshot(const struct remesa_device *,void *token,
 struct remesa_shader_draw_inputs *,struct remesa_draw_plan *);
struct remesa_persistent_draw_backing;
/* Live provider-local geometry handle/ranges bound to the original program's
 * owner/generation. No GPU VA, execution address or job is supplied. No ioctl;
 * outputs unchanged on failure, serialized with token destruction. */
int remesa_linux_shader_draw_backing(const struct remesa_device *,void *token,
 struct remesa_persistent_draw_backing *);
struct remesa_use_entry_plan;
/* Live-token entry-address preview using its checked owned CPU backing ID,
 * exact range/length and generation. allocation_address is caller-supplied
 * symbolic GPU metadata, NOT a verified map or assigned PC. The provider still
 * must establish bank ownership, executable mapping/visibility and job holds.
 * No ioctl, mapping, instruction/task encoding or execution permission.
 * Output cannot alias any live token's program or raw VB/IB snapshot.
 * Unchanged on failure; serialized with token destruction. */
int remesa_linux_shader_preview_use_entry(const struct remesa_device *,void *token,
 uint64_t allocation_address,uint32_t bank,struct remesa_use_entry_plan *);
/* Same live-token preview, selecting REMESA_IR_VERTEX or REMESA_IR_FRAGMENT.
 * Missing stages return ENOTSUP. Exact length/range comes from the owned stage
 * binding, not caller-chosen instruction counts. No executable map or launch. */
int remesa_linux_shader_preview_stage_entry(const struct remesa_device *,void *token,
 uint32_t stage,uint64_t allocation_address,uint32_t bank,struct remesa_use_entry_plan *);
/* Atomic VS/FS entry preview from one live owned program backing. Requires
 * both original CPU candidates and banks[0]/banks[1] for VS/FS respectively.
 * Rejects conflicting USE register settings, missing stages, stale bindings
 * and lossy entries before publishing either output. allocation_address is
 * still caller-supplied symbolic metadata, never an authenticated executable
 * mapping. No provider action, task words, MMIO or execution permission.
 * Output may not overlap any live token or its immutable snapshots. Both
 * entries are unchanged on failure; externally serialized with destruction.
 */
int remesa_linux_shader_preview_entry_pair(const struct remesa_device *,void *token,
 uint64_t allocation_address,const uint32_t banks[2],struct remesa_use_entry_plan *entries);
struct remesa_pds_request;
struct remesa_shader_pds_links;
/* Connect a live original VS/FS token to two caller-retained symbolic PDS
 * CODE/DATA plans. Requests and DATA offsets are arrays of two, VS then FS.
 * PDS owner/generation must match the token; both requests share an operation.
 * The source program IDs/lengths come from verified CPU staging. PDS mappings
 * are caller assertions, not authenticated/retained GPU backing. Revalidation
 * checks fresh metadata, not task execution. No provider calls, upload, MMIO,
 * instruction/control-word generation or permission to launch. Output is
 * unchanged on error and cannot alias inputs or any live token/snapshot.
 */
int remesa_linux_shader_preview_pds_links(const struct remesa_device *,void *token,
 uint64_t allocation_address,const uint32_t banks[2],const struct remesa_pds_request *requests,
 const uint64_t data_byte_offsets[2],struct remesa_shader_pds_links *);
int remesa_linux_shader_revalidate_pds_links(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 const struct remesa_shader_pds_links *);
struct remesa_pds_record_preview;
/* Exact live-token projection of the retained symbolic PDS pair. Current PDS
 * metadata and immutable CPU backing are revalidated before publication.
 * No T+4/control fields, actual fetch PC, MMIO, mapping or task bytes exist in
 * this result; execution stays gated. Failure leaves both outputs unchanged. */
int remesa_linux_shader_preview_pds_records(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_pds_record_preview *out);
struct remesa_pds_host_data;
/* Apply the CPU entry fixup to original caller-owned HOST DATA buffers only,
 * after validating the live token/current backing and retained PDS pair.
 * Cannot overwrite any live immutable shader, token or draw resource. No
 * provider call, GPU publication, task-control construction or execution.
 * Both buffers remain unchanged on failure. Caller serializes destruction. */
int remesa_linux_shader_apply_host_pds_fixups(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 const struct remesa_pds_host_data *data);
struct remesa_pds_host_data_view;
/* Clone original HOST DATA, apply only the known entry fields, and publish
 * both immutable snapshots atomically under the existing live token. Source
 * buffers are unchanged. Partial allocation failures release temporary state.
 * A successful pair cannot be replaced; no PDS instruction or GPU backing is
 * created. Qualified executable storage remains absent. */
int remesa_linux_shader_retain_host_pds_data(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 const struct remesa_pds_host_data *data);
/* Revalidates current bindings; returns two borrowed const HOST DATA views.
 * Output may not overlap any live immutable data. Failure leaves it unchanged.
 * EAGAIN denotes absent DATA; token destruction invalidates both views. */
int remesa_linux_shader_host_pds_data(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_pds_host_data_view *out);
/* CPU-stage the already retained immutable DATA pair through the attached
 * resource owner. Exact live token/current links are revalidated first.
 * This creates provider-local pinned backing, NOT an executable PDS allocation
 * or a binding of the caller's symbolic addresses to SGX VA. No CODE, mapping,
 * cache sync, submission or historical program fallback is performed. */
int remesa_linux_shader_stage_pds_data(struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current);
struct remesa_persistent_pds_backing;
/* Exact CPU-staged ranges under this token, independent of symbolic PDS VA.
 * Fresh token/current links required. Output unchanged on failure. */
int remesa_linux_shader_pds_backing(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_persistent_pds_backing *out);
/* Retain one immutable symbolic VS/FS link plan inside a live shader token.
 * Publication is atomic; subsequent successful plans cannot replace it (EBUSY).
 * Failure preserves prior state; an initially unbound token remains unbound.
 * Retrieval requires fresh caller metadata, live backing identity and the
 * immutable program image, and leaves output unchanged
 * on failure; EAGAIN means no plan was retained. Token retirement ends the
 * plan's lifetime. Neither API allocates PDS storage, pins a mapping, writes
 * task-control/instruction bytes or admits execution. No provider calls. */
int remesa_linux_shader_retain_pds_links(const struct remesa_device *,void *token,
 uint64_t allocation_address,const uint32_t banks[2],const struct remesa_pds_request *,
 const uint64_t data_byte_offsets[2]);
int remesa_linux_shader_retained_pds_links(const struct remesa_device *,void *token,
 uint64_t allocation_address,const struct remesa_pds_request *current,
 struct remesa_shader_pds_links *);
struct remesa_linux_receipt {
 uint64_t operation;
 int ioctl_errno, operation_errno;
 uint32_t phase, events, issued, cpu_published, attempted;
};
/* Full original ioctl buffer is retained even on failed/ambiguous issuance. */
int remesa_linux_response(const struct remesa_device *,struct remesa_native_draw *);
int remesa_linux_last_receipt(const struct remesa_device *,struct remesa_linux_receipt *);
#endif
