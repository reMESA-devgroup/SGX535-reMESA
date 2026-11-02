/* SPDX-License-Identifier: MIT
 * Actual Mesa GL -> Gallium NIR observation. Recording backend only; GPU0.
 * The cap toggle is confined to this test; production caps stay unchanged.
 */
int inherited_driver_fixture(void);
#define main inherited_driver_fixture
#include "test_driver_nir.c"
#undef main
#undef remesa_bind_diagnostic_pipeline
#include "remesa/opengl_lab_context.h"
#include "remesa_resource_ref.h"
#include "GL/gl.h"
uint32_t nir_debug=NIR_DEBUG_EXTENDED_VALIDATION;

static struct pipe_context *(*original_context_create)(struct pipe_screen *,void *,unsigned);
static void *(*original_create_vs)(struct pipe_context *,const struct pipe_shader_state *);
static unsigned selected_cap,observed_vs,viewport_loads,reciprocals;

static void *observe_vs(struct pipe_context *ctx,const struct pipe_shader_state *state)
{
 assert(state&&state->type==PIPE_SHADER_IR_NIR&&state->ir.nir);
 nir_shader *shader=state->ir.nir;
 assert(shader->info.stage==MESA_SHADER_VERTEX);
 observed_vs++;
 nir_foreach_function_impl(impl,shader) {
  nir_foreach_block(block,impl) nir_foreach_instr(instr,block) {
   if(instr->type==nir_instr_type_intrinsic) {
    nir_intrinsic_op op=nir_instr_as_intrinsic(instr)->intrinsic;
    viewport_loads+=op==nir_intrinsic_load_viewport_scale||op==nir_intrinsic_load_viewport_offset;
   }else if(instr->type==nir_instr_type_alu)
    reciprocals+=nir_instr_as_alu(instr)->op==nir_op_frcp;
  }
 }
 char name[80];snprintf(name,sizeof name,"api-vs-cap%u-%u.nir.txt",selected_cap,observed_vs);
 FILE *file=fopen(name,"wx");assert(file);nir_print_shader(shader,file);assert(!fclose(file));
 return original_create_vs(ctx,state);
}

static struct pipe_context *observe_context(struct pipe_screen *screen,void *priv,unsigned flags)
{
 struct pipe_context *ctx=original_context_create(screen,priv,flags);assert(ctx);
 original_create_vs=ctx->create_vs_state;ctx->create_vs_state=observe_vs;return ctx;
}

int main(void)
{
 for(selected_cap=0;selected_cap<2;selected_cap++) {
  observed_vs=viewport_loads=reciprocals=0;
  struct recorder rec={.submit_error=ENOTSUP};
  struct remesa_device device={.ops=&ops,.priv=&rec,.offline_test_backend=true};
  struct pipe_screen *screen=remesa_screen_create(&device);assert(screen);
  /* Controlled comparison of the state tracker's response to this hint.
   * No graphics/shader/version limits, NIR or raster guards are widened. */
  struct pipe_caps caps=screen->caps;caps.viewport_transform_lowered=selected_cap;
  memcpy((void *)&screen->caps,&caps,sizeof caps);
  original_context_create=screen->context_create;screen->context_create=observe_context;
  struct pipe_resource template={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
   .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
  struct pipe_resource *color=screen->resource_create(screen,&template);assert(color);
  struct remesa_opengl_lab_report report={0};
  int error=remesa_opengl_lab_fbo_arrays(screen,color,&report);
  assert(!error&&report.context_created&&report.program_linked&&report.program_draw_called);
  assert(!report.program_draw_indexed&&!report.program_draw_gl_error);
  assert(observed_vs&&!viewport_loads&&!reciprocals);
  printf("Observed cap=%u VS_error=%u VSbytes=%u pipe_error=%u\n",selected_cap,
   report.program_vs_error,report.program_vs_bytes,report.program_draw_pipe_error);fflush(stdout);
  /* Recognition succeeds here. Binary delivery is a separate backend
   * contract and is deliberately not inferred from this recording screen. */
  assert(!report.program_vs_error);
  assert(report.program_draw_pipe_error==ENOTSUP&&!rec.draws&&!rec.shader_submits);
  printf("PASS actual GL draw -> NIR cap=%u VS=%u viewport_loads=%u rcp=%u VSbytes=%u draw_refused=%u GPU0\n",
   selected_cap,observed_vs,viewport_loads,reciprocals,report.program_vs_bytes,report.program_draw_pipe_error);
  remesa_resource_reference(&color,NULL);screen->destroy(screen);
 }
 return 0;
}
