/* SPDX-License-Identifier: MIT
 * Actual Mesa state_tracker and GL calls on the existing recording backend.
 * No fake GL version/caps, rasterizer or hardware-success observations.
 */
int inherited_driver_fixture(void);
#define main inherited_driver_fixture
#include "test_driver_nir.c"
#undef main
#undef remesa_bind_diagnostic_pipeline
#include "remesa/opengl_lab_context.h"
#include "GL/gl.h"
#include "remesa_resource_ref.h"
int main(void)
{
 struct recorder rec={.submit_error=ENOTSUP};struct remesa_device device={.ops=&ops,.priv=&rec,.offline_test_backend=true};
 struct pipe_screen *screen=remesa_screen_create(&device);assert(screen);
 struct pipe_resource template={.target=PIPE_TEXTURE_2D,.format=PIPE_FORMAT_B8G8R8A8_UNORM,
  .width0=32,.height0=32,.depth0=1,.array_size=1,.bind=PIPE_BIND_RENDER_TARGET};
 struct pipe_resource *color=screen->resource_create(screen,&template);assert(color);
 /* Required state-tracker hook accepts inactive pattern updates while
  * existing raster validation continues rejecting enabled stippling. */
 struct pipe_context *lifecycle=screen->context_create(screen,NULL,0);assert(lifecycle);
 struct pipe_poly_stipple pattern={0};assert(lifecycle->set_polygon_stipple);
 lifecycle->set_polygon_stipple(lifecycle,&pattern);assert(!remesa_context_error(lifecycle));
 lifecycle->set_polygon_stipple(lifecycle,NULL);assert(remesa_context_error(lifecycle)==EINVAL);
 struct pipe_rasterizer_state raster={.poly_stipple_enable=1};
 assert(!lifecycle->create_rasterizer_state(lifecycle,&raster));
 assert(remesa_context_error(lifecycle)==ENOTSUP);lifecycle->destroy(lifecycle);
 struct remesa_opengl_lab_report report;
 int error=remesa_opengl_lab_clear(screen,color,&report);
 printf("LAB_GL context=%u computed_version=%u GLSL=%u string=%s\n",report.context_created,report.computed_version,report.glsl_version,report.version_string);
 printf("LAB_CLEAR error=%d GL=%u pipe=%u actual_green_pixels=%u CPU_ONLY\n",error,report.gl_error,report.pipe_error,report.green_pixels);
 printf("LAB_SHADER object=%u compiled=%u error=%u log=%s\n",report.shader_object,report.shader_compiled,report.shader_error,report.shader_log);
 printf("LAB_GL21 created=%u error=%u expected_bad_version=%u\n",report.gl21_created,report.gl21_error,report.gl21_bad_version);fflush(stdout);
 printf("LAB_TRIANGLE called=%u GL=%u pipe=%u shader_prepares=%u shader_submits=%u CPU_ONLY_NO_RENDER\n",report.triangle_called,report.triangle_gl_error,report.triangle_pipe_error,rec.shader_prepares,rec.shader_submits);fflush(stdout);
 printf("LAB_COMPILE VS_attempts=%u VS_error=%u FS_attempts=%u FS_error=%u\n",report.vs_attempts,report.vs_error,report.fs_attempts,report.fs_error);fflush(stdout);
 printf("LAB_PROGRAM ES_FS=%u ES_VS=%u linked=%u GL=%u log=%s\n",report.es_fs_compiled,report.es_vs_compiled,report.program_linked,report.program_gl_error,report.program_log);fflush(stdout);
 printf("LAB_PROGRAM_DRAW called=%u GL=%u pipe=%u VS_attempts=%u VS_error=%u FS_attempts=%u FS_error=%u\n",report.program_draw_called,report.program_draw_gl_error,report.program_draw_pipe_error,report.program_vs_attempts,report.program_vs_error,report.program_fs_attempts,report.program_fs_error);fflush(stdout);
 assert(!error&&report.context_created&&report.green_pixels==1024);
 assert(report.computed_version==13&&!report.glsl_version);
 assert(!report.gl21_created&&report.gl21_error==report.gl21_bad_version);
 assert(!rec.draws);
 assert(report.es_fs_compiled&&report.es_vs_compiled&&report.program_linked&&!report.program_gl_error);
 assert(report.program_draw_called&&!report.program_draw_gl_error);
 assert(!report.program_vs_error&&!report.program_fs_error);
 assert(report.program_draw_pipe_error==ENOTSUP);
 assert(!rec.shader_prepares&&!rec.shader_submits);
 struct pipe_resource *fresh=screen->resource_create(screen,&template);assert(fresh);
 struct remesa_opengl_lab_report fresh_report;
 error=remesa_opengl_lab_triangle(screen,fresh,&fresh_report);
 printf("LAB_FRESH_DRAW setup=%d called=%u GL=%u pipe=%u VS=%u FS=%u shader_prepares=%u shader_submits=%u GPU_SUBMISSIONS=0\n",
  error,fresh_report.program_draw_called,fresh_report.program_draw_gl_error,fresh_report.program_draw_pipe_error,
  fresh_report.program_vs_error,fresh_report.program_fs_error,rec.shader_prepares,rec.shader_submits);fflush(stdout);
 assert(!error&&fresh_report.context_created&&fresh_report.program_draw_called&&!fresh_report.program_draw_gl_error);
 assert(!fresh_report.program_vs_error&&!fresh_report.program_fs_error);
 assert(!rec.draws);remesa_resource_reference(&fresh,NULL);
 struct remesa_opengl_lab_report fbo_report;
 error=remesa_opengl_lab_fbo_triangle(screen,color,&fbo_report);
 printf("LAB_FBO setup=%d created=%u status=%x GL=%u program_called=%u draw_GL=%u pipe=%u prepares=%u submits=%u GPU_SUBMISSIONS=0\n",
  error,fbo_report.fbo_created,fbo_report.fbo_status,fbo_report.fbo_gl_error,fbo_report.program_draw_called,
  fbo_report.program_draw_gl_error,fbo_report.program_draw_pipe_error,rec.shader_prepares,rec.shader_submits);fflush(stdout);
 printf("LAB_RB target=%u width=%u height=%u internal_format=%x samples=%u\n",fbo_report.internal_target,fbo_report.fbo_width,fbo_report.fbo_height,fbo_report.fbo_internal_format,fbo_report.fbo_samples);fflush(stdout);
 assert(!error&&fbo_report.context_created);
 assert(!rec.draws);
 assert(fbo_report.fbo_created&&fbo_report.fbo_status==GL_FRAMEBUFFER_COMPLETE);
 assert(fbo_report.fbo_width==32&&fbo_report.fbo_height==32&&fbo_report.fbo_internal_format==GL_RGBA8&&!fbo_report.fbo_samples);
 assert(!fbo_report.fbo_gl_error&&fbo_report.program_draw_gl_error==GL_NO_ERROR);
 assert(fbo_report.program_draw_pipe_error==ENOTSUP&&fbo_report.program_draw_indexed);
 struct remesa_opengl_lab_report arrays_report;
 error=remesa_opengl_lab_fbo_arrays(screen,color,&arrays_report);
 printf("LAB_GL_ARRAYS setup=%d fbo=%x called=%u indexed=%u GL=%u pipe=%u prepares=%u submits=%u GPU_SUBMISSIONS=0\n",
  error,arrays_report.fbo_status,arrays_report.program_draw_called,arrays_report.program_draw_indexed,
  arrays_report.program_draw_gl_error,arrays_report.program_draw_pipe_error,rec.shader_prepares,rec.shader_submits);fflush(stdout);
 assert(!error&&arrays_report.fbo_status==GL_FRAMEBUFFER_COMPLETE&&arrays_report.program_draw_called&&!arrays_report.program_draw_indexed);
 assert(!arrays_report.program_draw_gl_error&&arrays_report.program_draw_pipe_error==ENOTSUP);
 assert(!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
 unsigned char quarter_pixels[4096],quarter_saved[4096];memset(quarter_pixels,0xa5,sizeof(quarter_pixels));memcpy(quarter_saved,quarter_pixels,sizeof(quarter_pixels));
 struct remesa_opengl_lab_report quarter_report;
 error=remesa_opengl_lab_draw_arrays_quarter(screen,color,&quarter_report,quarter_pixels,sizeof(quarter_pixels));
 printf("LAB_GL_QUARTER setup=%d fbo=%x called=%u indexed=%u GL=%u pipe=%u VS_bytes=%u FS_bytes=%u readback_called=%u readback_error=%u readback_bytes=%u GPU_SUBMISSIONS=0\n",
  error,quarter_report.fbo_status,quarter_report.program_draw_called,quarter_report.program_draw_indexed,
  quarter_report.program_draw_gl_error,quarter_report.program_draw_pipe_error,quarter_report.program_vs_bytes,
  quarter_report.program_fs_bytes,quarter_report.readback_called,quarter_report.readback_error,quarter_report.readback_bytes);fflush(stdout);
 assert(!error&&quarter_report.fbo_status==GL_FRAMEBUFFER_COMPLETE&&quarter_report.program_draw_called&&!quarter_report.program_draw_indexed);
 assert(!quarter_report.program_draw_gl_error&&quarter_report.program_draw_pipe_error==ENOTSUP);
 /* The recorder has no qualified numeric VS register mapping. Compile
  * status retains the semantic VS and reports no encoded body yet; the
  * actual Linux preparation/staging path must prove its later VS16 bytes. */
 assert(quarter_report.program_vs_bytes==0&&quarter_report.program_fs_bytes==8);
 assert(!quarter_report.readback_called&&quarter_report.readback_error==ENOTSUP&&!quarter_report.readback_bytes);
 assert(!memcmp(quarter_pixels,quarter_saved,sizeof(quarter_pixels)));
 struct remesa_opengl_lab_report invalid_report=quarter_report,invalid_saved=invalid_report;
 assert(remesa_opengl_lab_draw_arrays_quarter(screen,color,&invalid_report,quarter_pixels,4095)==EINVAL);
 assert(!memcmp(&invalid_report,&invalid_saved,sizeof(invalid_report))&&!memcmp(quarter_pixels,quarter_saved,sizeof(quarter_pixels)));
 assert(remesa_opengl_lab_draw_arrays_quarter(screen,color,&invalid_report,&invalid_report,4096)==EINVAL);
 assert(!memcmp(&invalid_report,&invalid_saved,sizeof(invalid_report)));
 assert(!rec.draws&&!rec.shader_prepares&&!rec.shader_submits);
 struct remesa_opengl_lab_report saved;memset(&report,0xa5,sizeof(report));saved=report;
 assert(remesa_opengl_lab_clear(NULL,color,&report)==EINVAL&&!memcmp(&report,&saved,sizeof(report)));
 assert(remesa_opengl_lab_clear(screen,NULL,&report)==EINVAL&&!memcmp(&report,&saved,sizeof(report)));
 remesa_resource_reference(&color,NULL);screen->destroy(screen);
 assert(rec.allocs==rec.frees);
 return 0;
}
