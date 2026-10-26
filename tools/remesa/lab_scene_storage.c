/* SPDX-License-Identifier: MIT */
#include "lab_scene_storage.h"
#include <errno.h>
#include <stdint.h>
#include <string.h>
static int add(struct remesa_lab_scene_storage *s,unsigned role,size_t bytes,
 enum remesa_persistent_va_class kind)
{
 return remesa_persistent_allocate_placed(s->device,bytes,kind,&s->objects[role]);
}
void remesa_lab_scene_storage_destroy(struct remesa_lab_scene_storage *s)
{
 if(!s||!s->device)return;
 for(unsigned i=REMESA_LAB_SCENE_ROLES;i;i--)
  if(s->objects[i-1])s->device->ops->destroy_bo(s->device->priv,s->objects[i-1]);
 memset(s,0,sizeof(*s));
}
int remesa_lab_scene_storage_prepare(struct remesa_device *d,
 const struct remesa_depth_scene_image_request *r,struct remesa_lab_scene_storage *out)
{
 if(!d||!r||!out)return EINVAL;
 uintptr_t a=(uintptr_t)r,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*r)||b>UINTPTR_MAX-sizeof(*out)||
    (a<b+sizeof(*out)&&b<a+sizeof(*r)))return EINVAL;
 int e=remesa_persistent_shader_output_check(d,out,sizeof(*out));if(e)return e;
 if(out->device)return EBUSY;
 for(unsigned i=0;i<REMESA_LAB_SCENE_ROLES;i++)if(out->objects[i])return EBUSY;
 if(r->gpu_va||r->allocation_bytes)return EINVAL;
 if(!r->width||!r->height||r->width>2048||r->height>2048)return EINVAL;
 struct remesa_lab_scene_storage s={.device=d,.clear_depth=r->clear_depth};
 /* Sizes/alignment are the retained native LAB storage contract, not a
  * dimension-independent claim of parameter-buffer capacity or GPU admission. */
 e=add(&s,REMESA_LAB_AUX_PDS,0x20000,REMESA_VA_PROGRAM);if(e)goto fail;
 e=add(&s,REMESA_LAB_AUX_USE,0x8000,REMESA_VA_PROGRAM);if(e)goto fail;
 e=remesa_persistent_depth_scene_prepare(d,r,&s.objects[REMESA_LAB_GENERATED]);if(e)goto fail;
 e=add(&s,REMESA_LAB_BACKGROUND,4096,REMESA_VA_RASTER);if(e)goto fail;
 e=add(&s,REMESA_LAB_CONTROL,4096,REMESA_VA_DEFAULT);if(e)goto fail;
 uint32_t pitch=(r->width+31u)&~31u;
 e=add(&s,REMESA_LAB_COLOR,(size_t)pitch*r->height*4,REMESA_VA_SURFACE);if(e)goto fail;
 e=remesa_persistent_depth_allocate(d,r->width,r->height,4,&s.objects[REMESA_LAB_Z]);if(e)goto fail;
 e=remesa_persistent_scene_allocate(d,r->width,r->height,&s.objects[REMESA_LAB_REGION]);if(e)goto fail;
 e=add(&s,REMESA_LAB_TA_TABLE,0x2000000,REMESA_VA_SURFACE);if(e)goto fail;
 e=add(&s,REMESA_LAB_TA_PARAMETER,0x2000000,REMESA_VA_RASTER);if(e)goto fail;
 e=add(&s,REMESA_LAB_XHW,4096,REMESA_VA_DEFAULT);if(e)goto fail;
 for(unsigned i=0;i<REMESA_LAB_SCENE_ROLES;i++) {
  e=remesa_persistent_memory_snapshot(d,s.objects[i],&s.bindings[i]);if(e)goto fail;
  struct remesa_persistent_memory_backing *v=&s.bindings[i];
  uint64_t low=i<=REMESA_LAB_GENERATED?0x20000000:
   (i==REMESA_LAB_BACKGROUND||i==REMESA_LAB_TA_PARAMETER)?0x30000000:0x40000000;
  uint64_t high=low==0x40000000?UINT64_C(0x100000000):low+0x10000000;
  uint64_t align=i==REMESA_LAB_AUX_USE?32768:i==REMESA_LAB_TA_PARAMETER?1048576:4096;
  if(v->gpu_va<low||v->gpu_va>=high||v->allocation_bytes>high-v->gpu_va||
     v->gpu_va%align){e=ENOTSUP;goto fail;}
  for(unsigned j=0;j<i;j++) {
   const struct remesa_persistent_memory_backing *p=&s.bindings[j];
   if(v->owner!=p->owner||v->handle==p->handle||
      (v->gpu_va<p->gpu_va+p->allocation_bytes&&p->gpu_va<v->gpu_va+v->allocation_bytes)){
    e=EPROTO;goto fail;
   }
  }
 }
 e=remesa_persistent_render_target_snapshot(d,s.objects[REMESA_LAB_COLOR],r->width,r->height,pitch,&s.color);if(e)goto fail;
 e=remesa_persistent_depth_snapshot(d,s.objects[REMESA_LAB_Z],&s.depth);if(e)goto fail;
 e=remesa_persistent_scene_snapshot(d,s.objects[REMESA_LAB_REGION],&s.region);if(e)goto fail;
 e=remesa_persistent_depth_scene_snapshot(d,s.objects[REMESA_LAB_GENERATED],&s.generated);if(e)goto fail;
 for(unsigned i=0;i<3;i++)if(s.generated.use_entries[i].base_address!=
      (s.bindings[REMESA_LAB_AUX_USE].gpu_va&~UINT64_C(0x7ffff))){e=ENOTSUP;goto fail;}
 /* Xpsb_scene_switch_fire fresh TA setup consumes these exact cookie
  * indices (ELF460c..4642); normal raster consumes base+cookie7 at408.
  * Values derive from owned region metadata, never caller-supplied addresses.
  * The new native executor must still place them in its qualified sequence. */
 const struct remesa_scene_layout *layout=&s.region.layout;
 if(layout->cookie[7]>=s.region.allocation_bytes||
    layout->cookie[8]>=s.region.allocation_bytes){e=EPROTO;goto fail;}
 const uint32_t base=(uint32_t)s.region.gpu_va;
 const uint32_t ta[14]={0x220,base+layout->cookie[8],
  0x208,layout->cookie[2],0x20c,layout->cookie[3],
  0x214,layout->cookie[4],0x210,layout->cookie[5],
  0x248,layout->cookie[6],0x21c,base+layout->cookie[7]};
 memcpy(s.ta_scene,ta,sizeof(ta));
 s.raster_scene[0]=0x408;s.raster_scene[1]=base+layout->cookie[7];
 *out=s;return 0;
fail:remesa_lab_scene_storage_destroy(&s);return e;
}
static bool same_binding(const struct remesa_persistent_memory_backing *a,
 const struct remesa_persistent_memory_backing *b)
{
 return a->owner==b->owner&&a->generation==b->generation&&a->gpu_va==b->gpu_va&&
 a->handle==b->handle&&a->logical_bytes==b->logical_bytes&&a->allocation_bytes==b->allocation_bytes;
}
static int raster_input(const struct remesa_lab_scene_storage *s,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c,struct remesa_raster_control_request *out)
{
 const unsigned needed[]={REMESA_LAB_BACKGROUND,REMESA_LAB_AUX_PDS,REMESA_LAB_COLOR,REMESA_LAB_Z,REMESA_LAB_REGION};
 for(unsigned i=0;i<sizeof(needed)/sizeof(needed[0]);i++) {
  struct remesa_persistent_memory_backing now;
  int e=remesa_persistent_memory_snapshot(s->device,s->objects[needed[i]],&now);if(e)return e;
  if(!same_binding(&now,&s->bindings[needed[i]]))return ESTALE;
 }
 struct remesa_persistent_scene_backing region;
 struct remesa_persistent_depth_backing z;
 int e=remesa_persistent_scene_snapshot(s->device,s->objects[REMESA_LAB_REGION],&region);if(e)return e;
 e=remesa_persistent_depth_snapshot(s->device,s->objects[REMESA_LAB_Z],&z);if(e)return e;
 const struct remesa_persistent_memory_backing *color=&s->bindings[REMESA_LAB_COLOR],
  *bg=&s->bindings[REMESA_LAB_BACKGROUND],*event=&s->bindings[REMESA_LAB_AUX_PDS];
 const struct remesa_raster_control_request r={
  .color={region.width,region.height,(region.width+31u)&~31u,color->gpu_va,color->allocation_bytes},
  .has_depth=true,.depth={.depth={z.width,z.height,z.pitch_pixels,z.cpp,
   REMESA_DEPTH_SURFACE_UNDEFINED,z.gpu_va,z.allocation_bytes},
   .color_width=region.width,.color_height=region.height,.clear_depth=s->clear_depth},
  .background={bg->gpu_va,bg->allocation_bytes,background_offset,68},
  .event={event->gpu_va,event->allocation_bytes,event_offset,event_bytes},.event_field0c=event_field0c};
 *out=r;return 0;
}

int remesa_lab_scene_raster_encode(const struct remesa_lab_scene_storage *s,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c,uint8_t out[REMESA_RASTER_CONTROL_BYTES])
{
 if(!s||!s->device||!out)return EINVAL;
 uintptr_t a=(uintptr_t)s,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*s)||b>UINTPTR_MAX-REMESA_RASTER_CONTROL_BYTES||
    (a<b+REMESA_RASTER_CONTROL_BYTES&&b<a+sizeof(*s)))return EINVAL;
 int e=remesa_persistent_shader_output_check(s->device,out,REMESA_RASTER_CONTROL_BYTES);if(e)return e;
 struct remesa_raster_control_request r;
 e=raster_input(s,background_offset,event_offset,event_bytes,event_field0c,&r);if(e)return e;
 return remesa_raster_control_encode(&r,out);
}

static int ta_input(const struct remesa_lab_scene_storage *s,struct remesa_raster_object_reference *out)
{
 struct remesa_persistent_memory_backing binding;
 int e=remesa_persistent_memory_snapshot(s->device,s->objects[REMESA_LAB_GENERATED],&binding);if(e)return e;
 if(!same_binding(&binding,&s->bindings[REMESA_LAB_GENERATED]))return ESTALE;
 struct remesa_persistent_depth_scene_backing generated;
 e=remesa_persistent_depth_scene_snapshot(s->device,s->objects[REMESA_LAB_GENERATED],&generated);if(e)return e;
 e=remesa_persistent_depth_scene_verify(s->device,s->objects[REMESA_LAB_GENERATED]);if(e)return e;
 *out=(struct remesa_raster_object_reference){generated.gpu_va,generated.allocation_bytes,
  generated.ta_offset,generated.ta_bytes};
 return 0;
}


int remesa_lab_scene_control_encode(const struct remesa_lab_scene_storage *s,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c,uint8_t out[REMESA_LAB_SCENE_CONTROL_BYTES])
{
 if(!s||!s->device||!out)return EINVAL;
 uintptr_t a=(uintptr_t)s,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*s)||b>UINTPTR_MAX-REMESA_LAB_SCENE_CONTROL_BYTES||
    (a<b+REMESA_LAB_SCENE_CONTROL_BYTES&&b<a+sizeof(*s)))return EINVAL;
 int e=remesa_persistent_shader_output_check(s->device,out,REMESA_LAB_SCENE_CONTROL_BYTES);if(e)return e;
 uint8_t bytes[REMESA_LAB_SCENE_CONTROL_BYTES];
 e=remesa_lab_scene_raster_encode(s,background_offset,event_offset,event_bytes,event_field0c,bytes);if(e)return e;
 struct remesa_raster_object_reference stream;
 e=ta_input(s,&stream);if(e)return e;
 e=remesa_ta_control_encode(&stream,bytes+REMESA_RASTER_CONTROL_BYTES);if(e)return e;
 memcpy(out,bytes,sizeof(bytes));return 0;
}

int remesa_lab_scene_control_stage(struct remesa_lab_scene_storage *s,
 uint64_t background_offset,uint64_t event_offset,uint64_t event_bytes,
 uint32_t event_field0c)
{
 if(!s||!s->device)return EINVAL;
 int e=remesa_persistent_shader_output_check(s->device,s,sizeof(*s));if(e)return e;
 struct remesa_persistent_memory_backing old;
 e=remesa_persistent_memory_snapshot(s->device,s->objects[REMESA_LAB_CONTROL],&old);if(e)return e;
 if(!same_binding(&old,&s->bindings[REMESA_LAB_CONTROL]))return ESTALE;
 for(unsigned i=0;i<REMESA_LAB_SCENE_ROLES;i++)if(i!=REMESA_LAB_CONTROL&&
    (s->objects[i]==s->objects[REMESA_LAB_CONTROL]||s->bindings[i].handle==old.handle))return EINVAL;
 struct remesa_raster_control_request raster;
 struct remesa_raster_object_reference ta;
 e=raster_input(s,background_offset,event_offset,event_bytes,event_field0c,&raster);if(e)return e;
 e=ta_input(s,&ta);if(e)return e;
 void *candidate=NULL;
 e=remesa_persistent_control_prepare(s->device,&raster,&ta,&candidate);if(e)return e;
 struct remesa_persistent_memory_backing binding;
 e=remesa_persistent_control_snapshot(s->device,candidate,&binding);
 if(!e)e=remesa_persistent_control_verify(s->device,candidate);
 if(e){s->device->ops->destroy_bo(s->device->priv,candidate);return e;}
 void *previous=s->objects[REMESA_LAB_CONTROL];
 s->objects[REMESA_LAB_CONTROL]=candidate;s->bindings[REMESA_LAB_CONTROL]=binding;
 s->device->ops->destroy_bo(s->device->priv,previous);return 0;
}

int remesa_lab_scene_dpm_encode(const struct remesa_lab_scene_storage *s,
 struct remesa_lab_dpm_control *out)
{
 if(!s||!s->device||!out)return EINVAL;
 uintptr_t a=(uintptr_t)s,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*s)||b>UINTPTR_MAX-sizeof(*out)||
    (a<b+sizeof(*out)&&b<a+sizeof(*s)))return EINVAL;
 int e=remesa_persistent_shader_output_check(s->device,out,sizeof(*out));if(e)return e;
 const unsigned roles[]={REMESA_LAB_TA_TABLE,REMESA_LAB_TA_PARAMETER};
 struct remesa_persistent_memory_backing v[2];
 for(unsigned i=0;i<2;i++) {
  e=remesa_persistent_memory_snapshot(s->device,s->objects[roles[i]],&v[i]);if(e)return e;
  if(!same_binding(&v[i],&s->bindings[roles[i]]))return ESTALE;
  /* Preserve the demonstrated native LAB capacity; do not generalize the
   * original info producer's smaller allocation threshold into admission. */
  if(v[i].logical_bytes!=0x2000000||v[i].allocation_bytes!=0x2000000)return ENOTSUP;
 }
 if(v[0].owner!=v[1].owner||v[0].handle==v[1].handle)return EINVAL;
 if(v[0].gpu_va<0x40000000||v[0].gpu_va>UINT64_C(0x100000000)-v[0].allocation_bytes||
    (v[0].gpu_va&4095)||v[1].gpu_va<0x30000000||
    v[1].gpu_va+v[1].allocation_bytes>0x40000000||(v[1].gpu_va&0xfffff))return ENOTSUP;
 return remesa_scene_dpm_build(v[0].gpu_va,v[0].allocation_bytes,
  v[1].gpu_va,v[1].allocation_bytes,out);
}
