# Complete captured MVP position body — CPU encoding only

`remesa_usse_cpu_mvp_position_body_encode` constructs the POSITION arithmetic
found in both captured original gears vertex shaders: four independent vector
multiplications of matrix rows by replicated inputX/Y/Z/W and three separately
rounded additions, preserving their NIR order. It emits104bytes:7 real ALU
records and6 necessary B5 repeat-state changes. No fused MAD, selector13,
artificial CODE padding or original shader template is used.

The mapping preserves all9/10logical uniform vectors; row slots4..7 refer to
source words17..32 after the self prefix. Declared effective source spans are
37/41words, which can be initialized and explicitly transferred as40/44words
using the separately implemented source builder and single-descriptor PDS
encoder. Uploading those sizes is not physically qualified by the20-word test.

Evidence: `/home/gama/sgx535-offline/mvp-position-graph-20261024T072000Z/`.
Thirteen original CPU writer records match every byte of the body. The actual
capture1/capture3 NIR POSITION DAGs compare symbolically with its schedule,
including uniform mapping and temporary liveness. This is neither numerical
floating-point simulation nor GPU execution. Native, UBSan, i386/QEMU and the
actual Mesa archive tests pass;21 old arithmetic regressions pass in three
environments. Static and DRI builds pass with the existing RWX LOAD warning.

This is a body, not a complete admitted shader. It requires4inputs,8temporary
scalars and4outputs. Its export tail, resource planner/task fields, secondary
constant delivery for later indices and complete launch remain unqualified.
The full NIR shader also writes COL0/PSIZ and contains live normalization,
material/light arithmetic; existing whole-shader admission and VS constant
buffer rejection were preserved. Do not execute a partial position body as
full glxgears, replace lighting with CPU output or reuse the small-profile
PDS/TA launch merely because a code allocation has spare capacity.

The next minimum CPU experiment is metadata-to-task planning through original
constructor4252d for a valid public program with the corresponding temporary
requirements. The retained optimized matrix IR can reveal a planner baseline
without decoding selector13, but optimization may reduce its temporary count.
Any changed metadata or controlled leaf must be labeled as such. A physical
experiment needs a separately justified complete launch and nondegenerate
matrix geometry in a fresh boot; current MADshift boot is consumed.
