/* Same per-visible-pixel gate as the qualified userspace LAB. */
static int remesa_gears_visible_gate(const unsigned char*c,const unsigned char*z){
 unsigned dominant[3]={0},visible=0,i;
 for(i=0;i<1024000;i++){unsigned b=c[i*4],g=c[i*4+1],r=c[i*4+2],depth;if(!(b|g|r))continue;visible++;
  depth=z[i*4]|((unsigned)z[i*4+1]<<8)|((unsigned)z[i*4+2]<<16);if(depth==0xffffff)return EIO;
  dominant[0]+=r>g&&r>b;dominant[1]+=g>r&&g>b;dominant[2]+=b>r&&b>g;
 }
 return visible>=12&&dominant[0]>=4&&dominant[1]>=4&&dominant[2]>=4?0:ERANGE;
}
