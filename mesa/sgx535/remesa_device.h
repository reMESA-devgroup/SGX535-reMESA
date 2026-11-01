/* SPDX-License-Identifier: MIT */
#ifndef REMESA_DEVICE_H
#define REMESA_DEVICE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "remesa/draw_packet.h"
struct remesa_device;
struct remesa_linked_scene_request;struct remesa_scene_capture;
struct remesa_gears_runtime_ops;
struct remesa_shader_draw_programs;
struct remesa_shader_draw_inputs;
/* Internal serialized userspace seam, not a kernel ABI. Submit snapshots the
 * packet before returning and retains backing through retirement. Allocation
 * destruction must defer release of pending or quarantined backing. All map calls must be nonblocking; reads
 * must fail until the backend establishes CPU publication. The Linux backend performs one synchronous native DRM draw; recording tests
 * must opt in explicitly and cannot establish hardware rendering. */
struct remesa_device_ops {
 /* Any returned BO transfers ownership, even when allocation reports error. */
 int (*allocate)(void *, size_t, void **);
 void (*destroy_bo)(void *, void *);
 /* Zero means one acquired CPU-map lease. A non-NULL returned pointer also
  * transfers that lease on error. Frontend releases it exactly once. Zero with
  * NULL is a protocol error and still requires lease cleanup, never access. */
 int (*map)(void *, void *, bool write, void **);
 void (*unmap)(void *, void *, bool write);
 /* Any non-NULL returned fence transfers ownership, including on error.
  * An error after possible issuance must quarantine backing in the backend;
  * destroying that fence must not release uncertain device resources. */
 int (*submit)(void *, void *target, const struct remesa_draw_packet *, void **fence);
 /* timeout_ns=0 polls; UINT64_MAX waits indefinitely. Success with
  * cpu_visible=true means all relevant writes are published and backing is
  * reusable. A fence may outlive its submitting context. This interface and
  * resources shared between contexts require external serialization. */
 int (*wait)(void *, void *, uint64_t timeout_ns, bool cpu_visible);
 void (*destroy_fence)(void *, void *);
 /* Original shader path, separate from the retained diagnostic submit above.
  * The three callbacks must be provided together. Preparation receives owned
  * NIR-derived IR snapshots, not a color-profile selector. It must compile
  * these programs independently, validate encoding/termination/PDS launch,
  * and own/pin all program backing before returning a token. It must reject
  * unqualified forms; substituting a historical fixed shader is forbidden.
  * Explicit retained_fragment_carrier mode is a separately named FS-only
  * laboratory: preparation covers original FS and declares retained VS/PDS;
  * it never represents the full original pipeline or advertises acceleration.
  * Any non-NULL token transfers ownership even on failure. No work may be
  * issued by preparation. Input snapshots are borrowed only for this call. */
 int (*prepare_shader_programs)(void *,const struct remesa_shader_draw_programs *,void **);
 /* Snapshots original VB/IB inputs and draw state before the CPU map leases
  * end. Packet positions have already been projected/clipped on the host;
  * they must never replace the original vertex shader inputs. Packet indices
  * and primitive counts can also change during clipping/culling/remapping;
  * all executable topology must come from the raw input start/count/indices.
  * The packet is
  * a preparation reference, not a GPU stream or fixed-shader selector.
  * Retains program/target backing until verified
  * retirement, including uncertain failure. The backend enforces its exact
  * hardware admission/profile; a token is not permission to bypass a guard.
  * Non-NULL fences transfer ownership on error, as for diagnostic submit. */
 int (*submit_shader_programs)(void *,void *target,const struct remesa_draw_packet *,
                               const struct remesa_shader_draw_inputs *,void *programs,void **fence);
 /* Release frontend token ownership. Submitted backing must remain owned by
  * the backend through retirement/quarantine; destruction cannot imply it. */
 void (*destroy_shader_programs)(void *,void *);
 /* Optional typed attachment/scene preparation, independent of ordinary draw.
  * Use the actual owning device object so the backend verifies its identity;
  * no fabricated owner/context or generic allocation fallback is permitted.
  * These hooks allocate/prepare only, never establish graphics capabilities. */
 int (*validate_attachment_storage)(const struct remesa_device *);
 int (*allocate_color_attachment)(struct remesa_device *,uint32_t,uint32_t,void **);
 int (*allocate_depth_attachment)(struct remesa_device *,uint32_t,uint32_t,uint32_t,void **);
 int (*prepare_linked_scene)(struct remesa_device *,const struct remesa_linked_scene_request *,struct remesa_scene_capture *);
 /* Explicit scene execution: success consumes the capture; an error with no
  * fence is NOT_ISSUED and leaves it releasable. Any error fence or
  * EOWNERDEAD means uncertain execution: retain/quarantine backend ownership.
  * These hooks do not admit ordinary draws or change graphics capabilities. */
 int (*submit_linked_scene)(struct remesa_device *,struct remesa_scene_capture,void **);
 int (*release_linked_scene)(struct remesa_device *,struct remesa_scene_capture);

};
/* Explicit target load contract. Current physical executor is REPLACE only.
 * PRESERVE is permitted solely for opt-in offline fixtures until an actual
 * preserving kernel path has been independently qualified. */
enum remesa_framebuffer_mode { REMESA_FRAMEBUFFER_REPLACE=0, REMESA_FRAMEBUFFER_PRESERVE=1 };
/* Transfer storage limits, not SGX render/sampler capabilities. Zero fields
 * select conservative frontend defaults. The backing allocator may still
 * reject allocations based on its actual GEM/address-space budget. */
struct remesa_resource_limits {
 uint32_t max_width,max_height,row_alignment;
 uint64_t max_resource_bytes;
};
struct remesa_device {
 const struct remesa_device_ops *ops; void *priv;
 bool offline_test_backend, qualified_hardware_backend;
 bool resource_only_backend; /* allocation/map only; submit must refuse */
 enum remesa_framebuffer_mode framebuffer_mode;
 struct remesa_resource_limits resource_limits;
 /* Explicit laboratory FS-only route: original NIR FS, retained historical
  * VS/PDS carrier. Never an implicit fallback or an accelerated GL claim. */
 bool retained_fragment_carrier;
 /* Default-off finite laboratory raster profile, derived only by successful
  * retained-carrier configuration. Not a general half-pixel capability. */
 bool retained_gl_fbo_half_pixel_center;
 /* Explicit private whole-frame gears route; NULL preserves every legacy gate. */
 const struct remesa_gears_runtime_ops *gears_runtime;
};
struct pipe_context;
/* Diagnostic fixed program, not NIR/OpenGL support. Positions are projected
 * window XY, Z in [0,1], W=1; no vertex transformation is claimed. */
int remesa_bind_diagnostic_pipeline(struct pipe_context *, uint32_t argb);
int remesa_context_error(const struct pipe_context *);
#endif
