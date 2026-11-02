# OpenGL on the native resource frontend

The first real frontend test found a concrete admission gap: the resource
screen created a Mesa context and a complete FBO, but its zero VS input limit
made program linking fail with `too many vertex shader inputs (max 0)`.
Enabling the context-local draw adapter alone did not change that limit.

`remesa_screen_enable_homogeneous_frontend_lab` adds a separate, named LAB
setup before children or resources exist, after linked attachment storage is
validated. The provider must have a native scene dispatcher configured and
must not be marked failed. This is a pure host configuration check, not GPU
identity or execution evidence.

The setup declares the implemented canonical shader interface: two P/N inputs,
POSITION/COLOR0 output storage, one color varying, nine coefficient float4
vectors and the 40-instruction vertex body. The fragment interface remains
one color input and one output. Sampling is still unavailable. Renderbuffer
dimensions use a power-of-two bound within the provider's storage limits and
the existing 2048-pixel typed target contract. New contexts opt into the cold
scene draw adapter.

Default screens are unchanged. The accelerated capability remains false in
this LAB frontend. No OpenGL version is forced: the actual Mesa context still
computes desktop version 1.3 and GLSL version 0. These numbers describe that
context's computed limits, not general OpenGL conformance or GLES 2.0 support.

## Actual fixed-function API inputs

The test uses real OpenGL matrix, material, lighting, normal-array and
vertex-array calls, followed by `glDrawArrays(GL_TRIANGLES, 0, 3)`. At both
1280×800 and 640×640, Mesa generates an `ff-vs` with two attributes, nine state
vectors, POSITION, COLOR0 and constant point size 1. The existing VS and FS
recognizers accept the shaders. Encoding the actual generated VS produces
exactly the same 320 bytes as the physically qualified native witness:
`b60249a06a8fbed9f8d235a06cac8f7511cbafd9f1344a076a24535859d713fd`.
No shader encoder or shader arithmetic was replaced.

Existing binding snapshots also extract geometry, coefficients, viewport and
opaque depth/ISP state successfully. The viewport bytes match the previously
qualified state at each size. The coefficient upload reserves 160 bytes:
four bytes of staging self-pointer, 144 coefficient bytes and twelve padding
bytes. This is data capacity, not extra CODE instructions.

The real API data is not byte-identical to the earlier hand-authored witness:
three light-product alpha components are 1 rather than 0, and the existing
three-component normal format expansion fills its fourth component with 1.
Those components do not contribute to the recognized NIR's outputs. They are
still part of the actual data upload; new physical equivalence is not claimed.
The test preserves and records them instead of rewriting the application data
to fit the old catalogue. The old catalogue and its integrity gates remain
unchanged.

## Current refusal and safety

The real raster state requests half-pixel centers, the bottom edge rule,
near/far depth clipping, CCW orientation and vertex-color clamping. The current
native adapter does not admit that complete state contract, so the API draw
still returns a driver refusal before reaching the native dispatcher.
No pixels are presented or accepted as GPU output in this test.

The adapter now requires bound blend, depth and raster objects, preserving
refusals after rejected state creation or NULL unbind. The existing shader,
address, ownership, fence, retirement and uncertainty checks are unchanged.
The next integration must account for the actual raster request using the
existing demonstrated gears/native components; dropping its flags would not
establish correct rendering. Cold-pass preservation, continuous production
admission and broader API coverage remain separate work.

Evidence is under
`/home/gama/sgx535-offline/opengl-resource-frontend-20261102/v1`.
It records the initial linker refusal, real API-generated NIR/CODE and bound
data, frontend admission/lifetime tests and legacy regressions. These tests
model GEM and native receipts and perform zero GPU operations. Earlier
physical `glDrawArrays`, GLXGears and native homogeneous scene results remain
valid within their recorded scopes.

## Cold clear scheduling

The explicit native draw adapter now queues a full-target zero COLOR / unit Z
clear against fresh owned attachments. It reuses the clear already emitted by
the native scene; it does not map or fill either attachment on the CPU. Both
plans are validated before either resource acquires a pending owner. Partial,
nonzero, stencil and load/preserve requests remain refused.

Pending intent owns references independently of framebuffer bindings. Normal
and unsynchronized CPU maps, transfer/native-copy access and other contexts
cannot observe unissued pixels. A successful native submission consumes the
intent only after its existing authenticated retirement/publication receipt.
A definite nonissue keeps it pending for a safe later submission; uncertainty
keeps the existing sticky failure and quarantine rules. Destroying the owning
context cancels unissued intent and marks affected resources `ECANCELED`, so
shared users cannot mistake old backing for cleared pixels.

A flush containing only pending clear work returns no completion fence and
records `ENOTSUP`: a standalone clear executor is still needed. It cannot reuse
an older fence to claim completion. Existing draw/raster admission remains
independent, and repeated passes with prior contents remain refused. This is
an integration component, not a claim of complete GL clear semantics.

The actual Mesa test calls `glClear(COLOR|DEPTH)` before `glDrawArrays` at both
1280×800 and 640×640. It asserts zero CPU clears and blocked access to both
supplied attachments. The drawable must declare
`ST_ATTACHMENT_DEPTH_STENCIL_MASK`; without it Mesa can create an internal Z
buffer instead of requesting the supplied one. The corrected fixture includes
this attachment and preserves the failed diagnostic runs.

Evidence: `/home/gama/sgx535-offline/native-clear-integration-20261102/v1`.
All new runs are offline/GPU0. Successful modeled draws are compared against
the unchanged direct-scene packet and all 13 READ images; earlier physical
clear/submission evidence is preserved, not counted as a new API test.
