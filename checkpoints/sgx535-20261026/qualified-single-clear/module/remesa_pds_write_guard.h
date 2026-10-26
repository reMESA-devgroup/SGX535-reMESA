/* Kernel-only production write policy. Metadata/holes and bounded float rule
 * match remesa_stream_mask_sources exactly; full metadata is admitted once. */
#ifndef REMESA_PDS_WRITE_GUARD_H
#define REMESA_PDS_WRITE_GUARD_H
static int remesa_canonical_pds_page(const struct remesa_stream_batch *b,unsigned count,
 const unsigned char *canonical,__u64 offset,const void *data,unsigned bytes)
{
 const unsigned char *p=data;unsigned at,i;__u64 absolute,mvp,light;int dynamic;
 if(!b || !canonical || !data || !count || count>64 || bytes!=4096 ||
    (offset&4095) || offset>131072-4096)return -EINVAL;
 for(at=0;at<4096;at+=4){
  absolute=offset+at;dynamic=0;
  for(i=0;i<count;i++){
   mvp=b[i].source_offset+68;
   light=b[i].source_offset+4+(b[i].normal_variant?8:9)*16;
   if((absolute>=mvp&&absolute<mvp+64)||(absolute>=light&&absolute<light+12)){dynamic=1;break;}
  }
  if(dynamic){
   __u32 word=(__u32)p[at]|((__u32)p[at+1]<<8)|((__u32)p[at+2]<<16)|((__u32)p[at+3]<<24);
   if((word&0x7fffffffU)>0x44800000U)return -ERANGE;
  }else if(memcmp(p+at,canonical+absolute,4))return -EKEYREJECTED;
 }
 return 0;
}
#endif
