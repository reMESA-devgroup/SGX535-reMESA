/* SPDX-License-Identifier: MIT */
#ifndef REMESA_MATRIX_PRESERVE_TA_H
#define REMESA_MATRIX_PRESERVE_TA_H
#include "ta_references.h"
/* Candidate ordinary no-clear draw: INITIAL STATE8/INDEX20, existing
 * homogeneous STATE8/PARAM8/INDEX20, CLOSE12. The initialization primitive is
 * retained; the separate Z-clear primitive is omitted. Every consumed INDEX,
 * vertex image and state has an explicit reference. The closed LAB grammars
 * and content authority remain separate and unchanged. */
static inline int remesa_matrix_preserve_ta_walk(const void *input,size_t bytes,
 remesa_ta_reference_visitor visitor,void *context,struct remesa_ta_reference_counts *out)
{
 remesa_ta_ref_uptr a=(remesa_ta_ref_uptr)input,b=(remesa_ta_ref_uptr)out;
 remesa_ta_ref_uptr limit=~(remesa_ta_ref_uptr)0;
 const remesa_ta_ref_u8 *p=input;
 struct remesa_ta_reference refs[8];remesa_ta_ref_u8 group[28];
 struct remesa_ta_reference_counts value={1,8};
 remesa_scene_u32 initial,parameter,close;unsigned i;int error;
 if(!input||!out||bytes!=76||a>limit-bytes||b>limit-sizeof(*out)||
    b%_Alignof(struct remesa_ta_reference_counts))return EINVAL;
 if(a<=b?b-a<bytes:a-b<sizeof(*out))return EINVAL;
 initial=remesa_ta_ref_word(p);parameter=remesa_ta_ref_word(p+36);
 close=remesa_ta_ref_word(p+64);
 if((initial&0xf0000000u)!=0x40000000u||remesa_ta_ref_word(p+4)!=0x0c02a203u||
    (parameter&0xf0000000u)!=0x40000000u||remesa_ta_ref_word(p+40)!=0x0c01e10au||
    (close&0xf0000000u)!=0x60000000u||remesa_ta_ref_word(p+68)!=0x0c02a201u||
    remesa_ta_ref_word(p+72)!=0xc0000000u)return EINVAL;
 error=remesa_ta_ref_group(p,0,&refs[0]);if(error)return error;
 refs[0].kind=REMESA_TA_REF_INITIAL_STATE;
 memcpy(group,p+28,8);memcpy(group+8,p+44,20);
 error=remesa_ta_ref_group(group,1,&refs[5]);if(error)return error;
 refs[3]=refs[5];
 refs[4]=(struct remesa_ta_reference){(parameter&0x0fffffffu)<<4,60,REMESA_TA_REF_DRAW_PARAMETERS,0,1};
 refs[5]=refs[6];refs[6]=refs[7];
 refs[7]=(struct remesa_ta_reference){(close&0x0fffffffu)<<4,60,REMESA_TA_REF_CLOSE_STATE,0,2};
 for(i=0;i<8;i++)if(!remesa_ta_ref_span(refs[i].address,refs[i].minimum_bytes))return ERANGE;
 if(visitor)for(i=0;i<8;i++){error=visitor(context,&refs[i]);if(error)return error;}
 *out=value;return 0;
}
#endif
