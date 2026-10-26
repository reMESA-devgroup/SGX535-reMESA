/* SPDX-License-Identifier: MIT
 * Small discriminating structural tests. Captured CPU oracle values below
 * are data fixtures, not executable shader programs. Independent comparison
 * with original x86 replay is performed by the investigation runner.
 */
#include "../remesa_pds_structure.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while(0)

static struct remesa_pds_structure_entry r(uint32_t id)
{
 struct remesa_pds_structure_entry e={0};
 e.kind=REMESA_PDS_STRUCTURE_R;e.id=id;e.components=4;e.mode=3;
 return e;
}
static struct remesa_pds_structure_entry t(uint32_t id)
{
 struct remesa_pds_structure_entry e={0};
 e.kind=REMESA_PDS_STRUCTURE_T;e.id=id;
 e.state_words[0]=UINT32_C(0x001e0002);
 e.state_words[1]=UINT32_C(0x60000000);
 e.reloc_arg3=UINT32_C(0x100);e.reloc_arg4=UINT32_C(0x200);
 return e;
}
static void original_owner_differential(void)
{
 struct remesa_pds_structure_request q={0};
 struct remesa_pds_structure a,b;
 q.entry_count=1;q.entries[0]=t(4);
 CHECK(!remesa_pds_structure_encode(&q,&a));
 q.extra_r=1;
 CHECK(!remesa_pds_structure_encode(&q,&b));
 CHECK(a.data_dwords==12&&b.data_dwords==12);
 CHECK(a.a2_count==1&&b.a2_count==2);
 CHECK(a.code_slot_count==4&&b.code_slot_count==5);
 CHECK(a.data[9]==UINT32_C(0x0c00f904)&&b.data[9]==a.data[9]);
 CHECK(b.data[11]==UINT32_C(0x0f80e00f));
 CHECK(a.use_background==UINT32_C(0x100000)&&b.use_background==UINT32_C(0x180000));
 CHECK(a.last_t_cell==9&&b.last_t_cell==9&&b.last_r_cell==11);
 CHECK(a.code[1].word==UINT32_C(0x070418a2));
 CHECK(a.code[2].word==UINT32_C(0x07042364));
 CHECK(b.code[3].word==UINT32_C(0x070438a2)&&b.code[3].ds1_index==3);
 CHECK(a.code[0].kind==REMESA_PDS_STRUCTURE_INITIAL_HOLE&&!a.code[0].known_mask);
 CHECK(b.code[4].kind==REMESA_PDS_STRUCTURE_FINAL_HOLE&&!b.code[4].known_mask);
 CHECK(!a.data_known_mask[0]&&!a.data_known_mask[10]&&!a.data_known_mask[11]);
 CHECK(a.relocations[1].data_cell==10&&a.relocations[1].arg3==0x100&&a.relocations[1].arg4==0x200);
}
static void adjacency_and_identity(void)
{
 struct remesa_pds_structure_request q={0};
 struct remesa_pds_structure rt,tr,changed;
 q.entry_count=2;q.entries[0]=r(2);q.entries[1]=t(4);
 CHECK(!remesa_pds_structure_encode(&q,&rt));
 CHECK(rt.descriptor_count==1&&rt.a2_count==1&&rt.class64_count==1);
 CHECK(rt.data[9]==UINT32_C(0x0fc02904));
 q.entries[0]=t(4);q.entries[1]=r(2);
 CHECK(!remesa_pds_structure_encode(&q,&tr));
 CHECK(tr.descriptor_count==2&&tr.a2_count==2&&tr.class64_count==1);
 CHECK(tr.data[9]==UINT32_C(0x0c00f904)&&tr.data[11]==UINT32_C(0x0fc0200f));
 /* Identity changes DATA, while index allocation and A2 stay unchanged. */
 q.entries[0]=r(3);q.entries[1]=t(5);
 CHECK(!remesa_pds_structure_encode(&q,&changed));
 CHECK((rt.data[9]^changed.data[9])==UINT32_C(0x1001));
 CHECK(rt.code[1].word==changed.code[1].word);
 /* Consecutive R cannot merge; the second R may merge with following T. */
 q.entry_count=3;q.entries[0]=r(2);q.entries[1]=r(3);q.entries[2]=t(5);
 CHECK(!remesa_pds_structure_encode(&q,&changed));
 CHECK(changed.descriptor_count==2&&changed.a2_count==2);
 CHECK(changed.descriptors[0].ds1_index==1&&changed.descriptors[1].ds1_index==2);
 CHECK(changed.code[2].word==UINT32_C(0x070430a2));
}
static void flags_formats_and_bounds(void)
{
 struct remesa_pds_structure_request q={0};
 struct remesa_pds_structure o;
 CHECK(!remesa_pds_structure_encode(&q,&o));
 CHECK(o.data_dwords==12&&o.code_slot_count==2&&o.a2_count==0&&!o.use_background);
 q.entry_count=1;q.entries[0]=r(10);q.entries[0].components=1;
 q.entries[0].mode=0;q.entries[0].flag_r=1;q.entries[0].format_present=1;
 q.entries[0].explicit_format=UINT32_C(0x04000000);
 CHECK(!remesa_pds_structure_encode(&q,&o));
 CHECK(o.data[9]==UINT32_C(0x0711a00f));
 q.entry_count=2;q.entries[0]=r(2);q.entries[1]=t(4);
 q.entries[1].flag_a=1;q.entries[1].flag_b=1;
 CHECK(!remesa_pds_structure_encode(&q,&o));
 CHECK(o.data[9]==UINT32_C(0x0fc02a14));
 /* The next-index extent and CPU interleave cross an eight-cell boundary. */
 q.entry_count=4;
 for(uint32_t i=0;i<4;i++)q.entries[i]=t(i+1);
 CHECK(!remesa_pds_structure_encode(&q,&o));
 CHECK(o.data_dwords==28&&o.descriptors[3].data_cell==15);
 CHECK(o.data_known_mask[16]==UINT32_MAX&&o.relocations[4].data_cell==24);
 q.entry_count=REMESA_PDS_STRUCTURE_INPUTS;q.extra_r=1;
 for(uint32_t i=0;i<q.entry_count;i++)q.entries[i]=t(i+1);
 CHECK(!remesa_pds_structure_encode(&q,&o));
 CHECK(o.data_dwords==76&&o.descriptor_count==17&&o.code_slot_count==35&&o.relocation_count==17);
 CHECK(o.last_t_cell==63&&o.last_r_cell==73);
}
static void invalid_preserves_output(void)
{
 struct remesa_pds_structure_request q={0};
 struct remesa_pds_structure o,saved;
 memset(&o,0xa5,sizeof(o));saved=o;
 q.entry_count=17;CHECK(remesa_pds_structure_encode(&q,&o)==E2BIG);
 CHECK(!memcmp(&o,&saved,sizeof(o)));
 q.entry_count=1;q.entries[0]=r(2);q.entries[0].components=0;
 CHECK(remesa_pds_structure_encode(&q,&o)==ERANGE);
 q.entries[0]=r(2);q.entries[0].id=256;
 CHECK(remesa_pds_structure_encode(&q,&o)==ERANGE);
 q.entries[0]=r(10);
 CHECK(remesa_pds_structure_encode(&q,&o)==EINVAL);
 q.entries[0]=t(2);q.entries[0].flag_b=2;
 CHECK(remesa_pds_structure_encode(&q,&o)==ERANGE);
 q.entries[0]=t(2);q.entries[0].components=1;
 CHECK(remesa_pds_structure_encode(&q,&o)==EINVAL);
 CHECK(remesa_pds_structure_encode(NULL,&o)==EINVAL);
 CHECK(!memcmp(&o,&saved,sizeof(o)));
 union { struct remesa_pds_structure_request request; struct remesa_pds_structure output; } alias;
 memset(&alias,0,sizeof(alias));
 CHECK(remesa_pds_structure_encode(&alias.request,&alias.output)==EINVAL);
 uint32_t word=UINT32_C(0x12345678);
 CHECK(remesa_pds_structure_a2(64,&word)==ERANGE&&word==UINT32_C(0x12345678));
 CHECK(remesa_pds_structure_class64(64,0,&word)==ERANGE&&word==UINT32_C(0x12345678));
 CHECK(remesa_pds_structure_class64(0,64,&word)==ERANGE&&word==UINT32_C(0x12345678));
 CHECK(!remesa_pds_structure_a2(63,&word)&&word==UINT32_C(0x0707f8a2));
}
static void shared_cpu_inputs(void)
{
 struct remesa_pds_structure_request q={0};
 struct remesa_pds_structure o,saved;
 q.entry_count=2;q.entries[0]=r(10);q.entries[1]=r(11);
 q.entries[0].format_present=q.entries[1].format_present=1;
 q.entries[0].explicit_format=q.entries[1].explicit_format=UINT32_C(0x04000000);
 CHECK(!remesa_pds_structure_encode(&q,&o));saved=o;
 q.entries[1].explicit_format=UINT32_C(0x08000000);
 CHECK(remesa_pds_structure_encode(&q,&o)==EINVAL&&!memcmp(&o,&saved,sizeof(o)));
 q.entry_count=0;q.temp_count=1;
 CHECK(remesa_pds_structure_encode(&q,&o)==EINVAL&&!memcmp(&o,&saved,sizeof(o)));
 q.task_word1=UINT32_C(0x08000000);
 CHECK(!remesa_pds_structure_encode(&q,&o)&&o.data[1]==q.task_word1);
 q.temp_count=32;q.task_word1=0;
 CHECK(!remesa_pds_structure_encode(&q,&o)&&o.data[8]==UINT32_C(0x21));
}
int main(void)
{
 original_owner_differential();adjacency_and_identity();
 flags_formats_and_bounds();invalid_preserves_output();shared_cpu_inputs();
 puts("PDS structure: owner differential, adjacency, fields, bounds and transactional rejection passed");
 return 0;
}
