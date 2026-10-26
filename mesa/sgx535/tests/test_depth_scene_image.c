/* SPDX-License-Identifier: MIT */
#include "remesa_depth_scene_image.h"
#include "remesa_shader_program.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
static uint32_t le(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static struct remesa_shader_image *program(uint32_t argb)
{
 struct remesa_shader_ir ir={.schema=1,.stage=REMESA_IR_VERTEX,.count=2};
 ir.instructions[0]=(struct remesa_ir_instruction){.op=REMESA_IR_INPUT,.components=4,.type=REMESA_IR_FLOAT32};
 ir.instructions[1]=(struct remesa_ir_instruction){.op=REMESA_IR_POSITION,.components=4,.type=REMESA_IR_FLOAT32,.dst=REMESA_SHADER_IR_NO_REGISTER};
 struct remesa_shader_program vs,fs;struct remesa_shader_draw_programs pair;
 assert(!remesa_shader_program_prepare(&ir,&vs));assert(!remesa_shader_program_prepare_color(argb,&fs));
 assert(!remesa_shader_draw_programs_prepare(&vs,&fs,&pair));
 struct remesa_usse_vertex_mapping mapping={0};
 for(unsigned c=0;c<4;c++){mapping.input[c]=(struct remesa_usse_cpu_register){2,c};mapping.output[c]=(struct remesa_usse_cpu_register){1,c};}
 struct remesa_shader_image *out=NULL;assert(!remesa_shader_image_prepare_vertex_candidate(&pair,&mapping,&out));return out;
}
static void reject(const struct remesa_depth_scene_image_request *r,int error)
{
 struct remesa_depth_scene_image image,saved;memset(&image,0xa5,sizeof(image));saved=image;
 assert(remesa_depth_scene_image_encode(r,&image)==error);assert(!memcmp(&image,&saved,sizeof(image)));
}
int main(void)
{
 struct remesa_shader_image *images[]={program(0xff00ff00),program(0xffff00ff),program(0xffffff00)};
 struct remesa_depth_scene_image_request r={.gpu_va=0x40000000,.allocation_bytes=4096,
  .programs={images[0],images[1],images[2]},.width=1280,.height=800,.vertex_bank=2,.pixel_bank=3,.state_bank=4,
  .triangle={{160,100},{1120,100},{160,675}},.draw_depth={.75f,.25f,.75f},.clear_depth=1,
  .compare=REMESA_DEPTH_LESS,.depth_write=true};
 struct remesa_depth_scene_image a,b;
 assert(!remesa_depth_scene_image_encode(&r,&a));assert(a.used_bytes<4096&&a.ta_bytes==124);
 /* Original CPU programmable-VS task contract must change PDS and TA
  * descriptor together. Geometry, code, state and references stay identical.
  * This does not qualify a native packet or clip-space vertex semantics. */
 struct remesa_depth_scene_image compiled,expected=a;
 expected.vertex_profile=REMESA_DEPTH_VERTEX_COMPILED_POSITION4;
 r.vertex_profile=REMESA_DEPTH_VERTEX_COMPILED_POSITION4;
 assert(!remesa_depth_scene_image_encode(&r,&compiled));
 for(unsigned j=0;j<4;j++) {
  size_t p=a.vertex_pds[j];
  const size_t cells[]={p+20,p+36};const uint32_t values[]={0xe0000000u,12};
  for(unsigned c=0;c<2;c++)for(unsigned k=0;k<4;k++) {
   expected.bytes[cells[c]+k]=(uint8_t)(values[c]>>(8*k));
   expected.known[cells[c]+k]=255;
  }
  /* Existing CPU packer: output byte count16 rounds to4, in bits25+. */
  size_t descriptor=a.ta_offset+28*j+24;
  uint32_t word=le(a.bytes+descriptor);
  assert(((word>>25)&31)==1);
  word=(word&~(31u<<25))|(4u<<25);
  for(unsigned k=0;k<4;k++)expected.bytes[descriptor+k]=(uint8_t)(word>>(8*k));
 }
 assert(!memcmp(&expected,&compiled,sizeof(compiled)));
 r.vertex_profile=(enum remesa_depth_vertex_profile)2;reject(&r,ENOTSUP);
 r.vertex_profile=REMESA_DEPTH_VERTEX_COPY;
 assert(!remesa_depth_scene_image_encode(&r,&b));assert(!memcmp(&a,&b,sizeof(a)));
 assert(a.use_entries[0].register_offset==0xa14&&a.use_entries[0].register_word==0x00800000);
 assert(a.use_entries[1].register_offset==0xa18&&a.use_entries[1].register_word==0x02800000);
 assert(a.use_entries[2].register_offset==0xa1c&&a.use_entries[2].register_word==0x00800000);
 const unsigned heads[]={0x54c5,0xb41,0x40,0x40,0x2000},lengths[]={44,52,16,16,8};
 for(unsigned i=0;i<5;i++) {
  assert(le(a.bytes+a.state_data[i])==heads[i]&&a.state_bytes[i]==lengths[i]);
  assert(le(a.bytes+a.state_pds[i])==r.gpu_va+a.state_data[i]);
  assert(le(a.bytes+a.ta_offset+28*i)==(((r.gpu_va+a.state_pds[i])>>4)|(i==4?0x60000000u:0x40000000u)));
  if(i<4){assert(le(a.bytes+a.vertex_pds[i])==r.gpu_va+a.vertices[i]);
   assert(le(a.bytes+a.ta_offset+28*i+8)==(0x81400000u|(i?3:6)));
   assert(le(a.bytes+a.ta_offset+28*i+12)==r.gpu_va+a.indices[i]);}
 }
 assert(le(a.bytes+a.ta_offset+120)==0xc0000000);
 for(unsigned i=0;i<3;i++) {
  struct remesa_shader_image_view v;assert(!remesa_shader_image_inspect(images[i],&v));
  assert(!memcmp(a.bytes+a.fragment_code[i],v.fragment.bytes,8));
  assert(!a.known[a.fragment_pds[i]]&&a.known[a.fragment_pds[i]+48]==255);
 }
 for(size_t i=a.used_bytes;i<4096;i++)assert(!a.bytes[i]&&!a.known[i]);
 r.gpu_va+=0x80000;assert(!remesa_depth_scene_image_encode(&r,&b));
 assert(le(b.bytes+b.vertex_pds[0])-le(a.bytes+a.vertex_pds[0])==0x80000);
 for(unsigned i=0;i<3;i++) {
  assert(b.use_entries[i].register_offset==a.use_entries[i].register_offset);
  assert(b.use_entries[i].register_word-a.use_entries[i].register_word==0x1000);
  assert(b.use_entries[i].entry_address-a.use_entries[i].entry_address==0x80000);
 }
 /* Change only bank selection: entry addresses/base words stay constant,
  * while lower19 bank operands and register destinations change together. */
 r.vertex_bank=5;r.pixel_bank=6;r.state_bank=7;
 struct remesa_depth_scene_image other;assert(!remesa_depth_scene_image_encode(&r,&other));
 for(unsigned i=0;i<3;i++) {
  assert(other.use_entries[i].register_offset==b.use_entries[i].register_offset+12);
  assert(other.use_entries[i].register_word==b.use_entries[i].register_word);
  assert(other.use_entries[i].entry_address==b.use_entries[i].entry_address);
 }
 assert((le(other.bytes+other.vertex_pds[0]+16)&15)==5);
 assert((le(other.bytes+other.fragment_pds[0])&15)==6);
 assert((le(other.bytes+other.state_pds[0]+8)&15)==7);
 r.vertex_bank=2;r.pixel_bank=3;r.state_bank=2;
 assert(!remesa_depth_scene_image_encode(&r,&other));
 assert(other.use_entries[0].register_offset==other.use_entries[2].register_offset);
 assert(other.use_entries[0].register_word==other.use_entries[2].register_word);
 assert((le(other.bytes+other.state_pds[0]+8)&15)==2);
 assert(!memcmp(other.bytes+other.vertex_pds[0],b.bytes+b.vertex_pds[0],64));
 assert(!memcmp(other.bytes+other.fragment_pds[0],b.bytes+b.fragment_pds[0],56));
 r.state_bank=4;
 assert(!memcmp(a.bytes+a.vertex_code,b.bytes+b.vertex_code,16));
 /* Primary Mini12 target: full1280x800 extent, not a smaller drawable.
  * This is CPU encoding coverage, not physical resolution qualification. */
 assert(!remesa_depth_scene_image_encode(&r,&b));
 assert(le(b.bytes+b.state_data[4]+4)==0x00500032);
 r.allocation_bytes=4095;reject(&r,ENOSPC);r.allocation_bytes=4096;
 r.gpu_va++;reject(&r,EINVAL);r.gpu_va--;
 r.state_bank=3;reject(&r,ENOTSUP);r.state_bank=16;reject(&r,ERANGE);r.state_bank=4;
 r.width=2049;reject(&r,ENOTSUP);r.width=1280;r.height=800;
 r.programs[1]=images[0];reject(&r,ENOTSUP);r.programs[1]=images[1];
 r.compare=REMESA_DEPTH_EQUAL;reject(&r,ENOTSUP);r.compare=REMESA_DEPTH_ALWAYS;
 r.depth_write=false;assert(!remesa_depth_scene_image_encode(&r,&b));
 assert(memcmp(a.bytes+a.state_data[1],b.bytes+b.state_data[1],52));
 assert(remesa_depth_scene_image_encode(&r,(void *)&r)==EINVAL);
 reject(NULL,EINVAL);
 for(unsigned i=0;i<3;i++)remesa_shader_image_destroy(images[i]);
 puts("PASS: explicit compiled-VS/copy PDS+TA profile coupling and transactional refusal; compact depth scene: one page, generated VS/3FS/PDS/state/TA; address relocation,1280x800 extent, unknown masks, transactional negatives; CPU ONLY");
 return 0;
}
