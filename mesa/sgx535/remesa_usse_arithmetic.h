/* SPDX-License-Identifier: MIT */
#ifndef REMESA_USSE_ARITHMETIC_H
#define REMESA_USSE_ARITHMETIC_H
#include <stddef.h>
#include <stdint.h>
/* Restricted ORIGINAL CPU-compiler encoding recovered from valid public Mesa
 * ADD/MUL vertex programs. This is not physical arithmetic qualification or a
 * complete VS: uniform banks, task resources, export and termination are separate.
 * Numeric source class2, destination class1/index0, identity vec4, flags0x41;
 * no modifiers, indexing, saturation, caller-selected controls or B5 setup.
 * Source bases are aligned four-component spans in the seven-bit CPU namespace.
 * These numeric classes are not architectural bank names. */
enum remesa_usse_arithmetic_op {
 REMESA_USSE_ARITHMETIC_ADD=1, REMESA_USSE_ARITHMETIC_MULTIPLY=2,
 REMESA_USSE_ARITHMETIC_MAXIMUM=3
};
struct remesa_usse_arithmetic {
 uint32_t op, left_base, right_base, reserved;
};
/* Exactly eight little-endian bytes. All outputs unchanged on error. Aliases
 * among request, output span and written are rejected. No submission admission. */
int remesa_usse_arithmetic_encode(const struct remesa_usse_arithmetic *,
 void *output,size_t capacity,size_t *written);
/* CPU-compiler numeric operands, not architectural bank names. Adds only the
 * demonstrated class2/index0 + class3/index1 pair and its commuted public order.
 * Class2/class2 preserves the existing aligned-span profile and adds MAX.
 * MAX with coefficients remains unsupported; the old API remains ADD/MUL only. A coefficient is
 * not embedded here: its owned upload, preload and task contract are separate.
 * No NIR instruction or physical execution admission is promoted. */
struct remesa_usse_arithmetic_source {uint32_t cpu_class,base;};
struct remesa_usse_arithmetic_sources {
 uint32_t op;
 struct remesa_usse_arithmetic_source left,right;
 uint32_t reserved;
};
int remesa_usse_arithmetic_sources_encode(const struct remesa_usse_arithmetic_sources *,
 void *output,size_t capacity,size_t *written);
/* Whole independently generated original CPU VS for POSITION=P*PARAM0 or P+PARAM0.
 * Selected dense numeric bindings class2/base0 and class3/base1 only.
 * Selected ADD with owned PARAM0.x=0/2.125 was physically qualified against
 * exact translated-pixel oracles (docs/phase9/VERTEX-PARAMETER-DELIVERY-PHYSICAL-20261024.md).
 * Other components/bindings and MUL remain physically unqualified. SA names
 * and preload mechanism are unknown; no NIR or GPU admission is granted. */
struct remesa_usse_cpu_parameter_vertex {
 uint32_t position_base,parameter_base,op,reserved;
};
#define REMESA_USSE_CPU_PARAMETER_VERTEX_BYTES 16u
int remesa_usse_cpu_parameter_vertex_encode(
 const struct remesa_usse_cpu_parameter_vertex *,void *,size_t,size_t *written);
/* Restricted whole original linked VS for authored public IR:
 * MUL temporary,P,PARAM0; ADD POSITION,temporary,PARAM1; END.
 * The original compiler combines that graph into common numeric kind0, not
 * selector0x13. Fixed bindings class2/base0 and class3/bases1,5 consume the
 * two dense vec4 values after the source prefix. source_dwords declares the
 * initialized source span INCLUDING that prefix (9..128); allocation, upload,
 * task resources and lifetime remain caller obligations. It is not CODE size
 * or proof that every declared word is read by the shader. A separate physical
 * MAD pair qualified PARAM0=(1,1,1,1), PARAM1.x=0/2.125 with transfer20,
 * producing the same two-column shift (parameter-mad-physical-pair-20261024T071000Z).
 * Non-unit multipliers, other components and full transforms remain unqualified. */
struct remesa_usse_cpu_parameter_mad_vertex {
 uint32_t position_base,multiply_parameter_base,add_parameter_base;
 uint32_t source_dwords,reserved;
};
#define REMESA_USSE_CPU_PARAMETER_MAD_VERTEX_BYTES 16u
#define REMESA_USSE_CPU_PARAMETER_MAD_MIN_SOURCE_DWORDS 9u
int remesa_usse_cpu_parameter_mad_vertex_encode(
 const struct remesa_usse_cpu_parameter_mad_vertex *,void *,size_t,size_t *written);
/* First real term of the captured MVP expression, not a complete matrix:
 * POSITION=INPUT2.xxxx*PARAM0.xyzw. Whole original linked profile consists
 * of B5 repeat deltas(1,0,1,0), existing MUL body and existing typed tail.
 * Fixed numeric bindings class2/base0, class3/base1; source_dwords includes
 * prefix (5..128). The original compiler reports one live input component,
 * four output components and auxiliary_count0: task resource construction is
 * separate and must not inherit a four-input-component resource claim from
 * the identity MUL. CPU encoding only, no matrix/NIR/hardware admission.
 * A triangle transformed by this term alone is rank1 and can degenerate;
 * an empty readback would not establish correct execution. */
struct remesa_usse_cpu_parameter_first_term_vertex {
 uint32_t position_base,parameter_base,source_dwords,reserved;
};
#define REMESA_USSE_CPU_PARAMETER_FIRST_TERM_VERTEX_BYTES 24u
int remesa_usse_cpu_parameter_first_term_vertex_encode(
 const struct remesa_usse_cpu_parameter_first_term_vertex *,void *,size_t,
 size_t *written);
/* Complete POSITION expression from both captured original gears VS graphs:
 * u4*P.xxxx + u5*P.yyyy + u6*P.zzzz + u7*P.wwww, in original SSA order
 * (latest product is the first ADD operand). Four MUL and three separate ADD
 * records preserve the graph's rounding boundaries; no MAD contraction.
 * Logical uniform slots stay dense, source word1+4*slot+component after prefix.
 * uniform_vectors9/10 preserves the ENTIRE corresponding captured state span,
 * including uniforms consumed later by lighting, rather than compacting MVP.
 * Fixed position numeric base0. source_dwords includes prefix, must cover all
 * uniform vectors and be <=128; the caller supplies initialized/owned backing.
 *
 * This is only a104-byte arithmetic BODY: no tail, PDS, launch or full shader
 * acceptance. Required numeric spans:4 input words,8 temporary words(class0
 * product0..3 and accumulator4..7),4 POSITION output words. The task-resource
 * planner for those spans is not qualified; do not reuse a simpler task's fixed
 * resources. Does not emit COLOR0/lighting/PSIZ or admit the full gears NIR.
 * Original parameterized CPU writers corroborate each emitted record; no
 * original optimized whole-shader equivalence or physical execution is claimed.
 */
struct remesa_usse_cpu_mvp_position_body {
 uint32_t position_base,uniform_vectors,source_dwords,reserved;
};
#define REMESA_USSE_CPU_MVP_POSITION_BODY_BYTES 104u
#define REMESA_USSE_CPU_MVP_POSITION_INPUT_DWORDS 4u
#define REMESA_USSE_CPU_MVP_POSITION_TEMPORARY_DWORDS 8u
#define REMESA_USSE_CPU_MVP_POSITION_OUTPUT_DWORDS 4u
int remesa_usse_cpu_mvp_position_body_encode(
 const struct remesa_usse_cpu_mvp_position_body *,void *,size_t,size_t *written);
/* Complete captured gears POSITION+COLOR0 expression body, CPU ONLY. It
 * composes the MVP body above with normalization, material/light, max0 and
 * float SLT(0,dot), preserving scenecolor.w as COLOR0 alpha. Bindings match
 * the two real captures: attribute normal ->9uniforms, state normal ->10.
 * source_dwords includes the initialized prefix+full uniform state, not DMA
 * capacity. Requires11 temporary scalar words, input4/8 and output8; no tail,
 * PDS, launch, clipping, viewport, full-NIR or GPU execution admission.
 * PSIZ=1 is unconsumed only for explicit triangle topology; other primitives
 * reject. No CPU-transformed vertices or software colors are generated.
 * All outputs remain unchanged on failure. */
#define REMESA_USSE_CPU_GEARS_VERTEX_BODY_BYTES 312u
#define REMESA_USSE_CPU_GEARS_TEMPORARY_DWORDS 11u
#define REMESA_USSE_CPU_GEARS_TRIANGLES 1u
struct remesa_usse_cpu_gears_vertex_body {
 uint32_t normal_from_attribute,uniform_vectors,source_dwords,primitive,reserved;
};
int remesa_usse_cpu_gears_vertex_body_encode(
 const struct remesa_usse_cpu_gears_vertex_body *,void *,size_t,size_t *written);
/* Existing component control + max(COLOR0,0) + min(COLOR0,1), four lanes.
 * Numeric output class1/base4; no temporary, tail or physical qualification.
 * Reuses the checked POSITION/COLOR clamp records. Transactional. */
#define REMESA_USSE_CPU_COLOR_OUTPUT_CLAMP_BYTES 24u
int remesa_usse_cpu_color_output_clamp_encode(void *,size_t,size_t *written);
/* Original CPU writer selectorB5, selected profile with flags0x200 and
 * zero trailing offsets. Four class8 signed scalar fields or class15 four-
 * component selectors. Field modes follow the corresponding demonstrated
 * format. Names describe CPU operands, not architectural execution semantics.
 * No task, NIR, resource, export or execution admission is provided. */
enum remesa_usse_component_format {
 REMESA_USSE_COMPONENT_SIGNED=8, REMESA_USSE_COMPONENT_SELECTORS=15
};
struct remesa_usse_component_operand {
 uint32_t format;
 int32_t value;
 uint32_t selector[4];
};
struct remesa_usse_component_control {
 struct remesa_usse_component_operand operand[4];
 uint32_t reserved;
};
int remesa_usse_component_control_encode(const struct remesa_usse_component_control *,
 void *output,size_t capacity,size_t *written);
/* Restricted original CPU arithmetic bodies needed by the valid lighting
 * probes. DOT3: class2 aligned vec3 spans, destination class1/index0, flags31.
 * RSQ: class2 scalar index0..3, modifier2, destination class1/index0, flags201.
 * These8-byte bodies exclude the B5/MOVE records surrounding them in complete
 * original programs. No physical bank, GLSL admission or task completion. */
enum remesa_usse_lighting_body_op {REMESA_USSE_LIGHTING_DOT3=1,REMESA_USSE_LIGHTING_RSQ=2};
struct remesa_usse_lighting_body {uint32_t op,left_base,right_base,reserved;};
int remesa_usse_lighting_body_encode(const struct remesa_usse_lighting_body *,
 void *output,size_t capacity,size_t *written);
/* Selected CPU record forms0/2/5/11 and restricted COLOR0 clamp12/13, adding class0 intermediates needed
 * by real multi-operation normalize probes. Also accepts the exact kind0,
 * flags41, dst(1,0), sources(2,0)/(3,1)/(3,5) two-parameter graph above.
 * Also four exact parameter MUL records for the captured MVP position body:
 * source0(class2,channel0..3), source1(class3,17+4*channel), source2(class6,48),
 * flags41, destination(class0,channel==0?4:0). Other coefficient tuples reject.
 * Numeric classes are not physical
 * bank names. Destination class0/1/2 numeric index fields; three-source inputs class0/2
 * and demonstrated class6 constants48/52 in slots1/2. Single-source kind5
 * permits class0/2 and modifier2. Exact flags31/41/201; unused fields must be0.
 * Kind11 is restricted DOT3, flags31, destination class1/base0 or2 and two
 * aligned class2 inputs; source0 is unused. Clamp13 additionally admits the
 * checked class1/base4 color intermediate. This emits one8-byte body, not a
 * task, complete shader, B5 recipe or NIR cap. */
struct remesa_usse_cpu_arithmetic {
 uint32_t kind,flags;
 struct remesa_usse_arithmetic_source destination,source[3];
 uint32_t modifier[3],reserved;
};
int remesa_usse_cpu_arithmetic_encode(const struct remesa_usse_cpu_arithmetic *,
 void *output,size_t capacity,size_t *written);
/* Independent selected CPU lowering of authored public Mesa IR:
 * DP3 t.x,n,n; RSQ t.x,t.xxxx; MUL output.xyzw,v,t.xxxx; END.
 * Inputs are symbolic numeric class2 bindings, identity components. This is
 * v scaled by reciprocal length of n.xyz, NOT a generic normalize4 operation.
 * Emits the complete40-byte returned original CPU CODE profile, including its
 * generated B5 control, with liveness-based temporary selection. No prelude,
 * PDS/task/resource/export architecture or GPU execution is qualified here.
 * No NIR/GLSL admission is granted. All outputs unchanged on failure. */
#define REMESA_USSE_CPU_NORMALIZE_SCALE_BYTES 40u
struct remesa_usse_cpu_normalize_scale {uint32_t normal_base,value_base,reserved;};
int remesa_usse_cpu_normalize_scale_encode(const struct remesa_usse_cpu_normalize_scale *,
 void *output,size_t capacity,size_t *written);
/* Whole original linked USE CPU allocation for the same restricted profile:
 *40-byte generated body +8-byte typed tail, auxiliary_count0. This covers
 * CPU construction only, not task resources, hardware export or termination.
 * POSITION only; no additional varyings, clipping auxiliaries or NIR admission. */
#define REMESA_USSE_CPU_NORMALIZE_SCALE_VERTEX_BYTES 48u
int remesa_usse_cpu_normalize_scale_vertex_encode(const struct remesa_usse_cpu_normalize_scale *,
 void *output,size_t capacity,size_t *written);
/* Selected original public VP MOV POSITION/inputP; MOV COLOR0/inputC with
 * a public passthrough-color FP. Complete40-byte linked USE CPU allocation,
 * generated MOVE + component control + kind13/12 color clamp + typed tail.
 * Numeric inputs class2 at0/4 (same or distinct), outputs class1 POSITION0 and
 * COLOR4. This establishes CPU construction, NOT physical color export,
 * interpolation, FS varying input, task resources or Gallium/NIR admission. */
#define REMESA_USSE_CPU_POSITION_COLOR_BYTES 40u
struct remesa_usse_cpu_position_color {uint32_t position_base,color_base,reserved;};
int remesa_usse_cpu_position_color_vertex_encode(const struct remesa_usse_cpu_position_color *,
 void *output,size_t capacity,size_t *written);
/* Selected complete CPU POSITION plus white diffuse COLOR program:
 * POSITION=P; COLOR.xyzw=clamp(dot(N.xyz,L.xyz),0,1). Alpha follows the same
 * intensity. Unit N/L are the caller's interpretation contract; no RSQ,
 * uniforms, material upload or NIR admission is provided. Five checked numeric
 * tuples (P,N,L): (0,4,0), (0,0,4), (4,0,4), (0,4,8), (0,8,4).
 * Two-input tuples explicitly alias P with N or L; three-input tuples provide
 * separate P/N/L records. Input strides are respectively8/12 DWORDs; output
 * layout is POSITION4+COLOR4. Physical arithmetic/export remains unqualified.
 * Includes generated component controls and typed tail; no shader template.
 *64 bytes are committed code, distinct from this selected CPU builder's
 *144-byte reserved USE extent and32-byte allocation alignment. */
#define REMESA_USSE_CPU_WHITE_LAMBERT_VERTEX_BYTES 64u
#define REMESA_USSE_CPU_WHITE_LAMBERT_RESERVED_BYTES 144u
#define REMESA_USSE_CPU_WHITE_LAMBERT_ALIGNMENT 32u
struct remesa_usse_cpu_white_lambert {
 uint32_t position_base,normal_base,light_base,reserved;
};
int remesa_usse_cpu_white_lambert_vertex_encode(
 const struct remesa_usse_cpu_white_lambert *,void *output,size_t capacity,size_t *written);
#endif
