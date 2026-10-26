/* SPDX-License-Identifier: MIT
 * Offline diagnostic link wrapper. Not installed in the driver or hardware frontend. */
#include "compiler/nir/nir.h"
#include "remesa_shader_ir.h"
#include <stdio.h>
int __real_remesa_shader_ir_lower(const struct nir_shader *,struct remesa_shader_ir *);
int __wrap_remesa_shader_ir_lower(const struct nir_shader *,struct remesa_shader_ir *);
int __wrap_remesa_shader_ir_lower(const struct nir_shader *s,struct remesa_shader_ir *o)
{
 fprintf(stderr,"REMESA_NIR_CAPTURE_BEGIN stage=%u scope=CPU_ONLY\n",s->info.stage);
 nir_print_shader((nir_shader *)s,stderr);
 int e=__real_remesa_shader_ir_lower(s,o);
 fprintf(stderr,"REMESA_NIR_CAPTURE_END stage=%u error=%d\n",s->info.stage,e);
 return e;
}
