#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage a private Mesa25.2 Xlib frontend with explicit SGX-only admission.

No installation, system Mesa edit, DRI admission change or backend opening.
Build inputs for the focused linker runner; upstream licenses are preserved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil


def prepare(mesa, output):
    here = Path(__file__).resolve().parent
    pins = json.loads((here / 'glx_lab_mesa_pin.json').read_text())
    texts = {}
    for name, digest in pins.items():
        data = (mesa / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Mesa input mismatch: ' + name)
        texts[Path(name).name] = data.decode()
    if output.exists():
        raise ValueError('Output must be new')

    def replace(name, old, new):
        if texts[name].count(old) != 1:
            raise ValueError('Nonunique integration anchor: ' + name + ':' + old[:60])
        texts[name] = texts[name].replace(old, new)

    # The upstream Xlib frontend advertises pixmaps, pbuffers and context
    # extensions that this private SGX resource/presentation path cannot defend.
    start = texts['glx_api.c'].index('#define EXTENSIONS \\\n')
    end = texts['glx_api.c'].index('#define DEFAULT_DIRECT', start)
    texts['glx_api.c'] = (texts['glx_api.c'][:start] +
        '#define EXTENSIONS "GLX_ARB_get_proc_address "\n\n' + texts['glx_api.c'][end:])
    replace('glx_api.c', '#define VENDOR "Brian Paul"',
            '#define VENDOR "SGX535-reMESA experimental LAB"')

    replace('glx_api.c', '   mask = VisualScreenMask | VisualDepthMask | VisualClassMask;',
            '   /* Private publisher supports only 24-bit TrueColor windows. */\n'
            '   if (depth != 24 || xclass != TrueColor) return NULL;\n'
            '   mask = VisualScreenMask | VisualDepthMask | VisualClassMask;')
    replace('glx_api.c', '   glxCtx->currentDpy = dpy;',
            '   glxCtx->currentDpy = NULL; /* created, not bound */')
    replace('glx_api.c', '         SetCurrentContext(ctx);\n         return True;',
            '         if (current && current != ctx) {\n'
            '            current->currentDpy = NULL;\n'
            '            current->currentDrawable = current->currentReadable = None;\n'
            '            if (current->xid == None) {\n'
            '               XMesaDestroyContext(current->xmesaContext); free(current);\n'
            '            }\n'
            '         }\n'
            '         SetCurrentContext(ctx);\n         return True;')
    replace('glx_api.c', '      SetCurrentContext(NULL);\n      return True;',
            '      SetCurrentContext(NULL);\n'
            '      if (current) {\n'
            '         current->currentDpy = NULL;\n'
            '         current->currentDrawable = current->currentReadable = None;\n'
            '         if (current->xid == None) {\n'
            '            XMesaDestroyContext(current->xmesaContext); free(current);\n'
            '         }\n'
            '      }\n      return True;')
    no_raster = ('   static bool firsttime = 1, no_rast = 0;\n\n'
                 '   if (firsttime) {\n'
                 '      no_rast = getenv("SP_NO_RAST") != NULL;\n'
                 '      firsttime = 0;\n   }')
    if texts['glx_api.c'].count(no_raster) != 2:
        raise ValueError('Expected exactly two software no-raster switches')
    texts['glx_api.c'] = texts['glx_api.c'].replace(no_raster,
        '   /* No software-winsys no-raster bypass in the SGX LAB frontend. */')
    replace('glx_api.c', '      if (no_rast && current == ctx)\n         return True;\n\n', '')
    replace('glx_api.c', '   if (no_rast)\n      return;\n\n', '')
    texts['xm_api.c'] = '#include \"glx_lab_frontend.h\"\n' + texts['xm_api.c']
    replace('xm_api.c', '   if (!rgb_flag)\n      return NULL;',
            '   if (!rgb_flag || !visinfo || visinfo->depth != 24 ||\n'
            '       visinfo->class != TrueColor || visinfo->red_mask != 0xff0000 ||\n'
            '       visinfo->green_mask != 0xff00 || visinfo->blue_mask != 0xff ||\n'
            '       stereo_flag || (depth_size && (depth_size != 24 || !remesa_glx_lab_depth24_enabled(display))) || stencil_size || num_samples ||\n'
            '       accum_red_size || accum_green_size || accum_blue_size || accum_alpha_size)\n'
            '      return NULL;')

    replace('xm_st.c', '#include "state_tracker/st_context.h"',
            '#include "state_tracker/st_context.h"\n#include "glx_lab_frontend.h"')
    replace('xm_st.c', '   if (!ptex)\n      return true;',
            '   if (!ptex) { remesa_glx_lab_fail(ENODATA); return false; }\n'
            '   if (!pctx) {\n'
            '      XMesaContext current = XMesaGetCurrentContext();\n'
            '      if (!current || current->xm_buffer != xstfb->buffer) {\n'
            '         remesa_glx_lab_fail(EINVAL); return false;\n'
            '      }\n'
            '      pctx = current->st->pipe;\n'
            '   }')
    replace('xm_st.c', '   xstfb->screen->flush_frontbuffer(xstfb->screen, pctx, pres, 0, 0, &xstfb->buffer->ws, nboxes, box);\n   return true;',
            '   return remesa_glx_lab_present(xstfb->buffer->xm_visual->display,\n'
            '      xstfb->buffer->ws.drawable, pctx, pres, nboxes) == 0;')
    replace('xm_st.c', 'bind = PIPE_BIND_DISPLAY_TARGET |\n                     PIPE_BIND_RENDER_TARGET;',
            '/* SGX-owned linear RT; no software winsys/display-target import. */\n'
            '         bind = PIPE_BIND_RENDER_TARGET;')
    for name in ('xm_st.c', 'xm_st.h'):
        replace(name, 'void\nxmesa_swap_st_framebuffer(', 'bool\nxmesa_swap_st_framebuffer(')
    replace('xm_st.c', '      if (xmesa_strict_invalidate())\n         xmesa_check_buffer_size(xstfb->buffer);\n   }\n}',
            '      if (xmesa_strict_invalidate())\n         xmesa_check_buffer_size(xstfb->buffer);\n   }\n   return ret;\n}')
    replace('xm_api.c', '#include "state_tracker/st_context.h"',
            '#include "state_tracker/st_context.h"\n#include "state_tracker/st_atom.h"\n#include "glx_lab_frontend.h"')
    replace('xm_api.c', '   st_screen_destroy(xmdpy->fscreen);\n   free(xmdpy->fscreen);',
            '   /* Release the frontend copy context before the owned screen.\n'
            '    * SGX resources/contexts retain screen lifetime themselves. */\n'
            '   if (xmdpy->pipe) xmdpy->pipe->destroy(xmdpy->pipe);\n'
            '   st_screen_destroy(xmdpy->fscreen);\n'
            '   free(xmdpy->fscreen);\n'
            '   remesa_glx_lab_close_display(display);\n'
            '   xmdpy->screen->destroy(xmdpy->screen);')
    replace('xm_api.c',
            '      st_api_make_current(c->st,\n'
            '                          drawBuffer ? drawBuffer->drawable : NULL,\n'
            '                          readBuffer ? readBuffer->drawable : NULL);',
            '      if (!st_api_make_current(c->st,\n'
            '                          drawBuffer ? drawBuffer->drawable : NULL,\n'
            '                          readBuffer ? readBuffer->drawable : NULL)) {\n'
            '         remesa_glx_lab_fail(EINVAL); return GL_FALSE;\n'
            '      }\n'
            '      /* Private SGX-owned storage uses bottom-left rows, as in\n'
            '       * the qualified FBO profile. XCB publication flips rows.\n'
            '       * This is a frontend storage choice, not a new GPU rule. */\n'
            '      if (c->st->ctx->WinSysDrawBuffer) c->st->ctx->WinSysDrawBuffer->FlipY = false;\n'
            '      if (c->st->ctx->WinSysReadBuffer) c->st->ctx->WinSysReadBuffer->FlipY = false;\n'
            '      c->st->ctx->NewState |= _NEW_BUFFERS;\n'
            '      c->st->ctx->NewDriverState |= ST_NEW_FB_STATE | ST_NEW_VIEWPORT |\n'
            '         ST_NEW_RASTERIZER | ST_NEW_SCISSOR;')
    start = texts['xm_api.c'].index('void XMesaSwapBuffers( XMesaBuffer b )')
    end = texts['xm_api.c'].index('\n\n\n/*\n * Copy sub-region', start)
    texts['xm_api.c'] = texts['xm_api.c'][:start] + '''void XMesaSwapBuffers( XMesaBuffer b )
{
   XMesaContext ctx = XMesaGetCurrentContext();
   if (!b || !ctx || ctx->xm_buffer != b) {
      remesa_glx_lab_fail(EINVAL); return;
   }
   struct pipe_fence_handle *fence = NULL;
   st_context_flush(ctx->st, ST_FLUSH_FRONT, &fence, NULL, NULL);
   bool ready = remesa_glx_lab_finish(ctx->st->pipe, fence);
   if (fence)
      ctx->st->pipe->screen->fence_reference(ctx->st->pipe->screen, &fence, NULL);
   if (ready && xmesa_swap_st_framebuffer(b->drawable))
      st_context_invalidate_state(ctx->st, ST_INVALIDATE_FB_STATE);
}''' + texts['xm_api.c'][end:]
    start = texts['xm_api.c'].index('void XMesaCopySubBuffer(')
    end = texts['xm_api.c'].index('\n\n\nvoid XMesaFlush(', start)
    texts['xm_api.c'] = texts['xm_api.c'][:start] + '''void XMesaCopySubBuffer( XMesaBuffer b, int x, int y, int width, int height )
{
   /* Copy/preservation is not physically qualified; do not silently emulate. */
   remesa_glx_lab_fail(ENOTSUP);
}''' + texts['xm_api.c'][end:]
    start = texts['xm_api.c'].index('void XMesaFlush(')
    end = texts['xm_api.c'].index('\n\n\n\n\nXMesaBuffer XMesaFindBuffer', start)
    texts['xm_api.c'] = texts['xm_api.c'][:start] + '''void XMesaFlush( XMesaContext c )
{
   if (!c || !c->st) { remesa_glx_lab_fail(EINVAL); return; }
   struct pipe_fence_handle *fence = NULL;
   st_context_flush(c->st, ST_FLUSH_FRONT, &fence, NULL, NULL);
   bool ready = remesa_glx_lab_finish(c->st->pipe, fence);
   if (fence)
      c->st->pipe->screen->fence_reference(c->st->pipe->screen, &fence, NULL);
   if (ready) XFlush(c->xm_visual->display);
}''' + texts['xm_api.c'][end:]
    output.mkdir(parents=True)
    for name, text in texts.items():
        (output / name).write_text(text)
    for name in ('glx_lab_frontend.c', 'glx_lab_frontend.h'):
        shutil.copyfile(here / name, output / name)
    (output / 'INPUT_SHA256.json').write_text(json.dumps(pins, indent=2) + '\n')


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mesa', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    prepare(a.mesa, a.output)
