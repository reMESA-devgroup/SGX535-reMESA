# Owned ISP update preparation

Status: IMPLEMENTED_OFFLINE. This is preparation, not a submitted depth scene.

`remesa_persistent_isp_update_prepare()` builds the existing restricted opaque
triangle ISP word0 update, its count-dependent USE auxiliary, state-upload PDS
and TA reference in one owned page. It reuses the persistent allocator, byte
budget, reserved VA ranges, registry generation, pin/map and cleanup paths.
No GPU address is accepted from the caller. The explicit USE bank produces an
entry plan; it is neither programmed nor acquired by this API.

The allocation contains STATE8 at0, USE16 at16, PDS60 at32 and TA-reference8
at96. These are host packing offsets, not discovered hardware requirements.
The separate source, auxiliary and PDS references are derived only after the
allocator establishes the actual backing VA. The source update is header1
plus ISP word0; it does not reset or provide the other ISP/MTE state groups.
The existing typed codecs calculate every generated instruction and command.
No historical CODE input is accepted.

The page is zero-filled, written under a private provider lease, synchronized
and read back in full before publication. Unwritten PDS cells retain zero known
masks even though their initialized bytes are zero. Public write leases and
the existing color diagnostic submission explicitly reject the typed object.
Read-only mapping remains available. Owner/generation/handle, ranges, entry plan
and masks can be queried only for a live, owned, unquarantined object without
active leases. Destruction uses the existing BO destructor.

Ordinary allocation/pin/map/readback failures roll back without publishing an
output. A map error with a returned pointer, uncertain unmap or failed sync
retains quarantined backing and makes close fail; the caller cannot interpret
that as successful staging. The mock tests deliberately retain those uncertain
endpoints instead of freeing backing that may still be leased.

Native, UBSan, i386/QEMU and i386/UBSan persistent tests pass, including reserved
VA holes, derived source/USE/PDS addresses, zero tail, immutable write refusal,
color-submit refusal, failure rollback and three provider ambiguity cases.
The actual Mesa archive test and static/DRI build also pass. Existing persistent
resource/framebuffer/fence regressions are exercised by the same test.

The separately implemented TA index codec references a **vertex PDS** under the
original40355 profile. It must not be pointed at this60byte state-upload PDS.
Its raw descriptor fields are derived from a different producer. ISP setters
flush pending indices before changing state; cache state is committed only
after the state reference has been successfully written.

Remaining physical work: assemble the owned vertex/index stream, ordered state
updates and clear draws; associate the separately owned Z attachment; establish
the complete scene bounds and bank/mapping lifetime; then qualify a distinct
bounded multi-resource job. The existing persistent diagnostic dispatch admits
one target use and a fixed draw packet. It cannot submit this new object or
quietly retain it through an unrelated job. No generic submit support, GLX
capability, depth capability or ADMISSION policy is changed here.

Evidence, commands, logs, patch and SHA256 manifest:
`sgx535-offline/owned-state-upload-20261023T010000Z/`.
New GPU submissions:0. Physical depth:UNKNOWN. Original glxgears:BLOCKED.
