/* SPDX-License-Identifier: MIT; independently authored host backend schema. */
#include "remesa_usse.h"
#include "remesa_nir.h"
#include "remesa_fragment_body.h"
#include <errno.h>
#include <string.h>
static int overlaps(const void *a,size_t na,const void *b,size_t nb)
{
 if(!na||!nb)return 0;
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return x<=y?y-x<na:x-y<nb;
}
struct scalar { uint64_t used;uint32_t input,channel,bits; };
static int zero_storage(const void *p,size_t n)
{const unsigned char *s=p;for(size_t k=0;k<n;k++)if(s[k])return 0;return 1;}
int remesa_usse_validate(const struct remesa_usse_program *p)
{
 struct scalar values[REMESA_USSE_VALUES]={0};uint32_t defined=0,exports=0;
 uint64_t used=0;uint32_t colors[4]={0};
 if(!p)return EINVAL;
 if(p->schema!=REMESA_USSE_SCHEMA||(p->stage!=REMESA_IR_VERTEX&&p->stage!=REMESA_IR_FRAGMENT)||
    !p->count||p->count>REMESA_USSE_INSTRUCTIONS)return ENOTSUP;
 for(uint32_t n=0;n<p->count;n++) {
  const struct remesa_usse_instruction *i=&p->instructions[n];struct scalar v={0};
  if(i->reserved)return ENOTSUP;
  if(i->op==REMESA_USSE_EXPORT_POSITION||i->op==REMESA_USSE_EXPORT_COLOR) {
   if(i->dst!=REMESA_USSE_NO_VALUE||i->src>=defined||i->bits||i->channel!=exports||exports>=4)return ENOTSUP;
   v=values[i->src];used|=v.used;
   if(i->op==REMESA_USSE_EXPORT_POSITION) {
    if(p->stage!=REMESA_IR_VERTEX||!v.input||v.channel!=exports)return ENOTSUP;
   } else {
    if(p->stage!=REMESA_IR_FRAGMENT||v.input)return ENOTSUP;
    colors[exports]=v.bits;
   }
   exports++;continue;
  }
  if(exports||defined>=REMESA_USSE_VALUES||i->dst!=defined)return ENOTSUP;
  v.used=UINT64_C(1)<<defined;
  switch(i->op) {
  case REMESA_USSE_INPUT:
   if(p->stage!=REMESA_IR_VERTEX||defined>=4||i->channel!=defined||i->src!=REMESA_USSE_NO_VALUE||i->bits)return ENOTSUP;
   v.input=1;v.channel=i->channel;break;
  case REMESA_USSE_CONSTANT:
   if(p->stage!=REMESA_IR_FRAGMENT||i->channel||i->src!=REMESA_USSE_NO_VALUE||
      (i->bits&&i->bits!=UINT32_C(0x3f800000)))return ENOTSUP;
   v.bits=i->bits;break;
  case REMESA_USSE_MOVE:
   if(i->src>=defined||i->channel||i->bits)return ENOTSUP;
   v=values[i->src];v.used|=UINT64_C(1)<<defined;break;
  default:return ENOTSUP;
  }
  values[defined++]=v;
 }
 if(exports!=4||!defined||used!=(defined==64?UINT64_MAX:((UINT64_C(1)<<defined)-1)))return ENOTSUP;
 if(p->stage==REMESA_IR_FRAGMENT) {
  uint32_t color;int ret=remesa_constant_rgba32(colors,&color);if(ret)return ret;
 }
 if(!zero_storage(&p->instructions[p->count],
                  (REMESA_USSE_INSTRUCTIONS-p->count)*sizeof(p->instructions[0])))return ENOTSUP;
 return 0;
}
int remesa_usse_lower(const struct remesa_shader_ir *ir,struct remesa_usse_program *out)
{
 struct remesa_usse_program p={.schema=REMESA_USSE_SCHEMA};
 uint32_t map[REMESA_SHADER_IR_VALUES][4]={{0}},defined=0;
 uint32_t constants[REMESA_USSE_VALUES]={0},constant_values[REMESA_USSE_VALUES]={0};
 uint32_t constant_count=0;
 if(!out||!ir||overlaps(out,sizeof(*out),ir,sizeof(*ir)))return EINVAL;
 int ret=remesa_shader_ir_validate(ir);if(ret)return ret;
 /* Complete host profiles have canonical non-scalar destinations (including
  * NO_REGISTER). Reject them before indexing scalar value maps. Their typed
  * offline encoders never imply admission through this older scalar path. */
 for(uint32_t n=0;n<ir->count;n++) {
  enum remesa_ir_op op=ir->instructions[n].op;
  if(op!=REMESA_IR_INPUT&&op!=REMESA_IR_CONSTANT&&op!=REMESA_IR_COPY&&
     op!=REMESA_IR_COMPOSE4&&op!=REMESA_IR_POSITION&&op!=REMESA_IR_COLOR)return ENOTSUP;
 }
 p.stage=ir->stage;
 for(uint32_t n=0;n<ir->count;n++) {
  const struct remesa_ir_instruction *i=&ir->instructions[n];
  /* COPY/COMPOSE4 are pure float32 channel aliases in the admitted IR.
   * Propagate them before allocating scalar values. This is an original
   * semantic optimization, not a claim about hardware registers or MOV bits.
   * Validation above rejects side effects, cycles and forward references. */
  if(i->op==REMESA_IR_COPY||i->op==REMESA_IR_COMPOSE4) {
   for(uint32_t c=0;c<i->components;c++)
    map[i->dst][c]=map[i->src[c].reg][i->src[c].channel];
   continue;
  }
  for(uint32_t c=0;c<i->components;c++) {
   /* Value-number only literal float32 bit patterns after semantic admission.
    * No arithmetic conversion or hardware immediate/register assumption.
    * Reusing a constant must not merge position inputs or export channels. */
   if(i->op==REMESA_IR_CONSTANT) {
    uint32_t k=0;
    for(;k<constant_count;k++)if(constants[k]==i->bits[c])break;
    if(k<constant_count) {map[i->dst][c]=constant_values[k];continue;}
   }
   if(p.count>=REMESA_USSE_INSTRUCTIONS)return EOVERFLOW;
   struct remesa_usse_instruction *u=&p.instructions[p.count++];
   u->dst=REMESA_USSE_NO_VALUE;u->src=REMESA_USSE_NO_VALUE;
   if(i->op==REMESA_IR_POSITION||i->op==REMESA_IR_COLOR) {
    u->op=i->op==REMESA_IR_POSITION?REMESA_USSE_EXPORT_POSITION:REMESA_USSE_EXPORT_COLOR;
    u->src=map[i->src[0].reg][c];u->channel=c;
   } else {
    if(defined>=REMESA_USSE_VALUES)return EOVERFLOW;
    u->dst=defined;map[i->dst][c]=defined++;
    if(i->op==REMESA_IR_INPUT) {u->op=REMESA_USSE_INPUT;u->channel=c;}
    else if(i->op==REMESA_IR_CONSTANT) {
     u->op=REMESA_USSE_CONSTANT;u->bits=i->bits[c];
     constants[constant_count]=i->bits[c];constant_values[constant_count++]=u->dst;
    }
    else return ENOTSUP;
   }
  }
 }
 ret=remesa_usse_validate(&p);if(ret)return ret;
 *out=p;return 0;
}
static void put32(unsigned char *p,uint32_t n)
{for(unsigned k=0;k<4;k++)p[k]=(unsigned char)(n>>(k*8));}
static uint32_t get32(const unsigned char *p)
{uint32_t n=0;for(unsigned k=0;k<4;k++)n|=(uint32_t)p[k]<<(k*8);return n;}
int remesa_usse_serialize_host(const struct remesa_usse_program *p,void *out,size_t cap,size_t *written)
{
 if(!written||(!out&&cap))return EINVAL;
 int ret=remesa_usse_validate(p);if(ret)return ret;
 size_t bytes=REMESA_USSE_HOST_HEADER_BYTES+p->count*REMESA_USSE_HOST_INSTRUCTION_BYTES;
 if(cap<bytes)return ENOSPC;
 if(overlaps(out,bytes,p,sizeof(*p))||overlaps(written,sizeof(*written),p,sizeof(*p))||
    overlaps(written,sizeof(*written),out,bytes))return EINVAL;
 unsigned char *s=out;
 /* RUSI is original host identification, NOT hardware program magic. */
 put32(s,UINT32_C(0x49535552));put32(s+4,p->schema);put32(s+8,p->stage);put32(s+12,p->count);
 for(uint32_t n=0;n<p->count;n++) {
  const struct remesa_usse_instruction *i=&p->instructions[n];unsigned char *d=s+16+n*24;
  put32(d,i->op);put32(d+4,i->dst);put32(d+8,i->src);put32(d+12,i->channel);put32(d+16,i->bits);put32(d+20,i->reserved);
 }
 *written=bytes;return 0;
}
int remesa_usse_deserialize_host(const void *data,size_t bytes,struct remesa_usse_program *out)
{
 if(!data||!out||overlaps(data,bytes,out,sizeof(*out)))return EINVAL;
 if(bytes<REMESA_USSE_HOST_HEADER_BYTES)return EINVAL;
 const unsigned char *s=data;struct remesa_usse_program p={0};
 if(get32(s)!=UINT32_C(0x49535552))return ENOTSUP;
 p.schema=get32(s+4);p.stage=(enum remesa_ir_stage)get32(s+8);p.count=get32(s+12);
 if(!p.count||p.count>REMESA_USSE_INSTRUCTIONS||bytes!=16+p.count*24u)return EINVAL;
 for(uint32_t n=0;n<p.count;n++) {
  struct remesa_usse_instruction *i=&p.instructions[n];const unsigned char *d=s+16+n*24;
  i->op=get32(d);i->dst=get32(d+4);i->src=get32(d+8);i->channel=get32(d+12);i->bits=get32(d+16);i->reserved=get32(d+20);
 }
 int ret=remesa_usse_validate(&p);if(ret)return ret;
 *out=p;return 0;
}
int remesa_usse_identity(const struct remesa_usse_program *p,uint64_t *identity)
{
 unsigned char bytes[16+REMESA_USSE_INSTRUCTIONS*24];size_t written=0;
 if(!identity||!p||overlaps(identity,sizeof(*identity),p,sizeof(*p)))return EINVAL;
 int ret=remesa_usse_serialize_host(p,bytes,sizeof(bytes),&written);if(ret)return ret;
 /* Original host checksum only. Not hardware evidence or cryptography. */
 uint64_t h=0;
 for(size_t n=0;n<written;n++){h=(h<<7)|(h>>57);h^=(uint64_t)bytes[n]+n;}
 *identity=h;return 0;
}
void remesa_usse_encoding_status(struct remesa_usse_encoding_status *status)
{if(status)memset(status,0,sizeof(*status));}
int remesa_usse_encoded_size(const struct remesa_usse_program *p,size_t *bytes)
{
 if(!p||!bytes||sizeof(*bytes)>UINTPTR_MAX-(uintptr_t)bytes||
    sizeof(*p)>UINTPTR_MAX-(uintptr_t)p||
    overlaps(p,sizeof(*p),bytes,sizeof(*bytes)))return EINVAL;
 struct remesa_fragment_constant constant;
 int error=remesa_fragment_constant_prepare(p,&constant);if(error)return error;
 *bytes=sizeof(constant.record);return 0;
}
int remesa_usse_encode(const struct remesa_usse_program *p,void *out,size_t cap,size_t *written)
{
 if(!p||!written||(!out&&cap)||sizeof(*p)>UINTPTR_MAX-(uintptr_t)p||
    sizeof(*written)>UINTPTR_MAX-(uintptr_t)written||
    cap>UINTPTR_MAX-(uintptr_t)out||
    overlaps(p,sizeof(*p),out,cap)||overlaps(p,sizeof(*p),written,sizeof(*written))||
    overlaps(out,cap,written,sizeof(*written)))return EINVAL;
 struct remesa_fragment_constant constant;
 int error=remesa_fragment_constant_prepare(p,&constant);if(error)return error;
 if(cap<sizeof(constant.record))return ENOSPC;
 memcpy(out,constant.record,sizeof(constant.record));
 *written=sizeof(constant.record);return 0;
}
static int cpu_register_valid(struct remesa_usse_cpu_register r)
{return r.class_id<=2 && r.number<=127;}
static int gears_parameter_move_valid(const struct remesa_usse_cpu_move *m)
{
 /* Parameterized original250aeb writer oracles, exact captured-gears uses.
  * Keep all other unqualified class3 MOVE bindings/controls rejected. */
 if(m->source.class_id!=3||m->control11||m->control18||m->control23!=1)
  return 0;
 return (m->destination.class_id==0&&m->destination.number==4&&
         (m->source.number==5||m->source.number==33)&&
         m->control12==2&&m->control21==1)||
        (m->destination.class_id==1&&m->destination.number==7&&
         m->source.number==4&&m->control12==1&&!m->control21);
}
static int cpu_register_equal(struct remesa_usse_cpu_register a,
                              struct remesa_usse_cpu_register b)
{return a.class_id==b.class_id && a.number==b.number;}
static int candidate_output_valid(const void *out,size_t cap,size_t bytes,
                                 const size_t *written)
{
 if(!out||!written)return EINVAL;
 if(cap<bytes)return ENOSPC;
 if(cap>UINTPTR_MAX-(uintptr_t)out ||
    sizeof(*written)>UINTPTR_MAX-(uintptr_t)written ||
    overlaps(out,cap,written,sizeof(*written)))return EINVAL;
 return 0;
}
int remesa_usse_cpu_control_ee_encode(
 const struct remesa_usse_cpu_control_ee *request,void *out,size_t cap,
 size_t *written)
{
 if(!request||(uintptr_t)request%_Alignof(struct remesa_usse_cpu_control_ee)||
    sizeof(*request)>UINTPTR_MAX-(uintptr_t)request||
    (uintptr_t)written%_Alignof(size_t))return EINVAL;
 int error=candidate_output_valid(out,cap,8,written);if(error)return error;
 if(overlaps(out,cap,request,sizeof(*request))||
    overlaps(written,sizeof(*written),request,sizeof(*request)))return EINVAL;
 struct remesa_usse_cpu_control_ee r=*request;
 if(r.reserved)return EINVAL;
 if(r.mode8>1u||r.record_last>1u||(r.mode8&&r.record_last))return ENOTSUP;
 /*252d32..252d8a: selected flags bit2 clear produces major31; fixed
  * minor bits8/6, flags bit3->word1 bit11, bit16->word1 bit18.
  * Word0's flag26 and feature-specific bit1 are absent in this profile.
  * MVP records EE/200 and EE/208 independently exercise the mode bit;
  * PARAM303ff+30fb7 independently exercises the finalizer bit. */
 unsigned char bytes[8];put32(bytes,0);
 put32(bytes+4,(31u<<27)|(1u<<8)|(1u<<6)|
  (r.mode8<<11)|(r.record_last<<18));
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
int remesa_usse_cpu_move_encode(const struct remesa_usse_cpu_move *move,
 void *out,size_t cap,size_t *written)
{
 unsigned char bytes[REMESA_USSE_CPU_MOVE_BYTES];
 if(!move)return EINVAL;
 int error=candidate_output_valid(out,cap,sizeof(bytes),written);if(error)return error;
 if(overlaps(out,cap,move,sizeof(*move)) ||
    overlaps(written,sizeof(*written),move,sizeof(*move)))return EINVAL;
 if((!cpu_register_valid(move->source)&&!gears_parameter_move_valid(move))||!cpu_register_valid(move->destination)||
    move->control11>1||move->control18>1||move->control12>15||
    move->control21>1||move->control23>1)return ENOTSUP;
 /* Original disjoint CPU fields from the selected ordinary writer. No opaque
  * shader template, physical bank name or hardware END interpretation. */
 uint32_t first=(move->source.number<<7)|(move->destination.number<<21)|
  (move->source.class_id<<30)|(UINT32_C(1)<<29);
 uint32_t second=move->destination.class_id|(UINT32_C(5)<<27)|
  (move->control11<<11)|(move->control12<<12)|(move->control18<<18)|
  (move->control21<<21)|(move->control23<<23)|(UINT32_C(1)<<16);
 put32(bytes,first);put32(bytes+4,second);
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
int remesa_usse_cpu_move_decode(const void *bytes,size_t length,
 struct remesa_usse_cpu_move *out)
{
 if(!bytes||!out)return EINVAL;
 if(length!=REMESA_USSE_CPU_MOVE_BYTES)return ENOTSUP;
 if(length>UINTPTR_MAX-(uintptr_t)bytes ||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out ||
    overlaps(bytes,length,out,sizeof(*out)))return EINVAL;
 uint32_t first=get32(bytes),second=get32((const unsigned char *)bytes+4);
 const uint32_t first_mask=(UINT32_C(127)<<7)|(UINT32_C(127)<<21)|
  (UINT32_C(3)<<30)|(UINT32_C(1)<<29);
 const uint32_t second_mask=3u|(UINT32_C(31)<<27)|(UINT32_C(1)<<11)|
  (UINT32_C(15)<<12)|(UINT32_C(1)<<16)|(UINT32_C(1)<<18)|
  (UINT32_C(1)<<21)|(UINT32_C(1)<<23);
 if((first&~first_mask)||(second&~second_mask)||!(first&(UINT32_C(1)<<29))||
    !(second&(UINT32_C(1)<<16))||(second>>27)!=5)return ENOTSUP;
 struct remesa_usse_cpu_move move={
  .source={first>>30,(first>>7)&127},.destination={second&3,(first>>21)&127},
  .control11=(second>>11)&1,.control18=(second>>18)&1,
  .control12=(second>>12)&15,.control21=(second>>21)&1,.control23=(second>>23)&1
 };
 if((!cpu_register_valid(move.source)&&!gears_parameter_move_valid(&move))||
    !cpu_register_valid(move.destination))return ENOTSUP;
 *out=move;return 0;
}
int remesa_usse_cpu_three_source_decode(const void *bytes,size_t length,
 struct remesa_usse_cpu_three_source *out)
{
 if(!bytes||!out)return EINVAL;
 if(length!=REMESA_USSE_CPU_THREE_SOURCE_BYTES)return ENOTSUP;
 if(length>UINTPTR_MAX-(uintptr_t)bytes ||
    sizeof(*out)>UINTPTR_MAX-(uintptr_t)out ||
    overlaps(bytes,length,out,sizeof(*out)))return EINVAL;
 uint32_t first=get32(bytes),second=get32((const unsigned char *)bytes+4);
 const uint32_t variable=3u|(UINT32_C(1)<<2)|(UINT32_C(3)<<12)|
  (UINT32_C(15)<<16);
 const uint32_t fixed=(UINT32_C(31)<<27)|(UINT32_C(3)<<24)|
  (UINT32_C(1)<<21)|(UINT32_C(1)<<14);
 /* Numerical writer form only. In particular no flag below is named END. */
 if((second&~variable)!=fixed)return ENOTSUP;
 struct remesa_usse_cpu_three_source record={
  .source={{(second>>2)&1,(first>>14)&127},
           {first>>30,(first>>7)&127},{(first>>28)&3,first&127}},
  .source_extension={(second>>19)&1,(second>>17)&1,(second>>16)&1},
  .word0_control21=(first>>21)&1,.word0_control22_27=(first>>22)&63,
  .word1_control0_1=second&3,.word1_control12_13=(second>>12)&3,
  .word1_control18=(second>>18)&1
 };
 *out=record;return 0;
}
int remesa_usse_cpu_three_source_encode(const struct remesa_usse_cpu_three_source *record,
 void *out,size_t capacity,size_t *written)
{
 if(!record||sizeof(*record)>UINTPTR_MAX-(uintptr_t)record)return EINVAL;
 unsigned char bytes[REMESA_USSE_CPU_THREE_SOURCE_BYTES];
 int error=candidate_output_valid(out,capacity,sizeof(bytes),written);if(error)return error;
 if(overlaps(record,sizeof(*record),out,capacity)||
    overlaps(record,sizeof(*record),written,sizeof(*written)))return EINVAL;
 for(unsigned n=0;n<3;n++) {
  if(record->source[n].number>127||record->source_extension[n]>1||
     record->source[n].class_id>(n?3u:1u))return ENOTSUP;
 }
 if(record->word0_control21>1||record->word0_control22_27>63||
    record->word1_control0_1>3||record->word1_control12_13>3||
    record->word1_control18>1)return ENOTSUP;
 /* Independently traced CPU writer psb ELF30a09, zeroed record from2fabd.
  * These disjoint numeric fields are not named as GPU bank/opcode/END bits. */
 uint32_t first=(record->source[0].number<<14)|(record->source[1].number<<7)|
  record->source[2].number|(record->source[1].class_id<<30)|
  (record->source[2].class_id<<28)|(record->word0_control21<<21)|
  (record->word0_control22_27<<22);
 uint32_t second=(UINT32_C(31)<<27)|(UINT32_C(3)<<24)|(UINT32_C(1)<<21)|
  (UINT32_C(1)<<14)|(record->source[0].class_id<<2)|
  (record->source_extension[0]<<19)|(record->source_extension[1]<<17)|
  (record->source_extension[2]<<16)|record->word1_control0_1|
  (record->word1_control12_13<<12)|(record->word1_control18<<18);
 put32(bytes,first);put32(bytes+4,second);
 memcpy(out,bytes,sizeof(bytes));*written=sizeof(bytes);return 0;
}
int remesa_usse_vertex_mapping_validate(const struct remesa_usse_vertex_mapping *mapping)
{
 if(!mapping)return EINVAL;
 for(unsigned c=0;c<4;c++) {
  if(!cpu_register_valid(mapping->input[c])||!cpu_register_valid(mapping->output[c]))return ENOTSUP;
  for(unsigned k=0;k<4;k++) {
   if(cpu_register_equal(mapping->output[c],mapping->input[k]))return EINVAL;
   if(k!=c && (cpu_register_equal(mapping->input[c],mapping->input[k])||
               cpu_register_equal(mapping->output[c],mapping->output[k])))return EINVAL;
  }
 }
 return 0;
}
/* Valid original VS compilation qualifies this exact numeric body profile.
 * It does not qualify physical banks or the separately appended export tail. */
static int vertex_compiled_body_mapping(const struct remesa_usse_vertex_mapping *mapping)
{
 for(unsigned c=0;c<4;c++)
  if(mapping->input[c].class_id!=2||mapping->input[c].number!=c||
     mapping->output[c].class_id!=1||mapping->output[c].number!=c)return 0;
 return 1;
}
static int readable_range(const void *p,size_t bytes)
{return p&&bytes<=UINTPTR_MAX-(uintptr_t)p;}
static void export_resource_index(uint32_t mask[REMESA_USSE_CPU_REGISTER_MASK_WORDS],
 uint32_t *maximum,uint32_t number)
{
 mask[number/32]|=UINT32_C(1)<<(number%32);
 if(*maximum<number+1)*maximum=number+1;
}
int remesa_usse_vertex_export_plan(const struct remesa_usse_program *program,
 const struct remesa_usse_vertex_mapping *mapping,
 struct remesa_usse_vertex_export_plan *out)
{
 struct remesa_usse_vertex_export_plan plan={0};
 struct {uint32_t input_value,input_instruction,input_channel;uint64_t dependencies;}
  value[REMESA_USSE_VALUES]={{0}};
 if(!readable_range(program,sizeof(*program))||
    !readable_range(mapping,sizeof(*mapping))||!readable_range(out,sizeof(*out))||
    overlaps(program,sizeof(*program),out,sizeof(*out))||
    overlaps(mapping,sizeof(*mapping),out,sizeof(*out)))return EINVAL;
 int error=remesa_usse_validate(program);if(error)return error;
 if(program->stage!=REMESA_IR_VERTEX)return ENOTSUP;
 error=remesa_usse_vertex_mapping_validate(mapping);if(error)return error;
 plan.schema=REMESA_USSE_VERTEX_EXPORT_SCHEMA;plan.stage=program->stage;
 plan.candidate_bytes=vertex_compiled_body_mapping(mapping)?
  (REMESA_USSE_CPU_MOVE_BYTES+REMESA_USSE_CPU_THREE_SOURCE_BYTES):
  REMESA_USSE_VERTEX_CANDIDATE_BYTES;
 plan.input_bank_error=plan.export_bank_error=plan.termination_error=ENOTSUP;
 error=remesa_usse_identity(program,&plan.semantic_identity);if(error)return error;
 for(uint32_t n=0;n<program->count;n++) {
  const struct remesa_usse_instruction *i=&program->instructions[n];
  if(i->op==REMESA_USSE_INPUT) {
   value[i->dst].input_value=i->dst;value[i->dst].input_instruction=n;
   value[i->dst].input_channel=i->channel;
   value[i->dst].dependencies=UINT64_C(1)<<i->dst;
  } else if(i->op==REMESA_USSE_MOVE) {
   value[i->dst]=value[i->src];
   value[i->dst].dependencies|=UINT64_C(1)<<i->dst;
  } else if(i->op==REMESA_USSE_EXPORT_POSITION) {
   struct remesa_usse_vertex_export_route *r=&plan.route[plan.route_count++];
   r->channel=i->channel;r->bit_size=32;r->export_instruction=n;
   r->source_value=i->src;r->input_value=value[i->src].input_value;
   r->input_instruction=value[i->src].input_instruction;
   r->input_channel=value[i->src].input_channel;
   r->candidate_offset=plan.candidate_bytes!=REMESA_USSE_VERTEX_CANDIDATE_BYTES?0:
    i->channel*REMESA_USSE_CPU_MOVE_BYTES;
   r->dependencies=value[i->src].dependencies;
   r->source=mapping->input[r->input_channel];
   r->destination=mapping->output[r->channel];
   plan.used_values|=r->dependencies;
   struct remesa_usse_vertex_export_resource *read=&plan.resource[r->source.class_id];
   struct remesa_usse_vertex_export_resource *write=&plan.resource[r->destination.class_id];
   export_resource_index(read->read_mask,&read->read_max_index_plus_one,r->source.number);
   export_resource_index(write->write_mask,&write->write_max_index_plus_one,r->destination.number);
  } else return ENOTSUP;
 }
 if(plan.route_count!=4)return EPROTO;
 *out=plan;return 0;
}
int remesa_usse_vertex_export_validate(const struct remesa_usse_program *program,
 const struct remesa_usse_vertex_mapping *mapping,
 const struct remesa_usse_vertex_export_plan *plan)
{
 if(!readable_range(plan,sizeof(*plan))||
    !readable_range(program,sizeof(*program))||!readable_range(mapping,sizeof(*mapping))||
    overlaps(program,sizeof(*program),plan,sizeof(*plan))||
    overlaps(mapping,sizeof(*mapping),plan,sizeof(*plan)))return EINVAL;
 struct remesa_usse_vertex_export_plan expected;
 int error=remesa_usse_vertex_export_plan(program,mapping,&expected);if(error)return error;
 /* Exact canonical comparison also rejects altered error/status fields and
  * stale numeric mappings. No supplied success flag can certify GPU export. */
 return memcmp(plan,&expected,sizeof(expected))?EPROTO:0;
}
int remesa_usse_vertex_candidate(const struct remesa_usse_program *program,
 const struct remesa_usse_vertex_mapping *mapping,void *out,size_t cap,size_t *written)
{
 unsigned char bytes[REMESA_USSE_VERTEX_CANDIDATE_BYTES];
 struct remesa_usse_vertex_export_plan plan;
 if(!program||!mapping)return EINVAL;
 int error=candidate_output_valid(out,cap,0,written);if(error)return error;
 if(overlaps(out,cap,program,sizeof(*program))||overlaps(out,cap,mapping,sizeof(*mapping))||
    overlaps(written,sizeof(*written),program,sizeof(*program))||
    overlaps(written,sizeof(*written),mapping,sizeof(*mapping)))return EINVAL;
 error=remesa_usse_vertex_export_plan(program,mapping,&plan);if(error)return error;
 if(cap<plan.candidate_bytes)return ENOSPC;
 if(plan.candidate_bytes!=REMESA_USSE_VERTEX_CANDIDATE_BYTES) {
  /* Original 250aeb flags0x41: four scalar positions encoded by control12=3
   * plus control21, with control23. Independent valid VS compiler oracles
   * match this existing numeric codec; no historical shader template. */
  struct remesa_usse_cpu_move move={.source=mapping->input[0],
   .destination=mapping->output[0],.control12=3,.control21=1,.control23=1};
  size_t length;
  error=remesa_usse_cpu_move_encode(&move,bytes,sizeof(bytes),&length);
  if(error)return error;
  /* Original valid VS setter/linker has no auxiliary records in this profile.
   * Its builder4252d passes (0,8,8,0,1,0,0,1) to30a09; helper tag8 maps
   * to numeric class2/extension1. Finalizer30fb7 contributes control18.
   * These are CPU construction rules, not hardware export/END semantics. */
  struct remesa_usse_cpu_three_source tail={.source={{0,0},{2,0},{2,0}},
   .source_extension={0,1,1},.word0_control21=1,
   .word1_control12_13=1,.word1_control18=1};
  error=remesa_usse_cpu_three_source_encode(&tail,bytes+REMESA_USSE_CPU_MOVE_BYTES,
   REMESA_USSE_CPU_THREE_SOURCE_BYTES,&length);if(error)return error;
 } else for(unsigned n=0;n<plan.route_count;n++) {
  const struct remesa_usse_vertex_export_route *r=&plan.route[n];
  struct remesa_usse_cpu_move move={.source=r->source,.destination=r->destination};
  size_t length;
  error=remesa_usse_cpu_move_encode(&move,bytes+r->candidate_offset,
    REMESA_USSE_CPU_MOVE_BYTES,&length);if(error)return error;
 }
 memcpy(out,bytes,plan.candidate_bytes);*written=plan.candidate_bytes;return 0;
}
