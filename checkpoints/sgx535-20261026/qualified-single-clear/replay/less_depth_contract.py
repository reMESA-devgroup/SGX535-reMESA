"""Separate LESS qualification. No change to old ALWAYS/raw or LAB verifiers.
Analytical stable-interior depth/gear-class is a narrow diagnostic, not full GL
precision, shading or conformance. All raw readbacks stay unmodified."""
import struct,sys,math

def compare_less_outputs(color,oldcolor,depth,olddepth,ref_rgb,ref_depth,stable):
 assert sys.byteorder=='little';n=len(depth)//4
 assert n and len(depth)==len(olddepth)==len(color)==len(oldcolor)==n*4 and len(ref_rgb)==n*3 and len(ref_depth)==n*4 and len(stable)==n
 z=memoryview(depth).cast('I');before=memoryview(olddepth).cast('I');ref=memoryview(ref_depth).cast('f');closer=visible=stable_count=0;maxerr=0.
 for i in range(n):
  actual=z[i]&0xffffff;old=before[i]&0xffffff
  assert (actual==0xffffff)==(old==0xffffff),('Geometry coverage changed',i)
  assert actual<=old+2,('LESS left farther depth than ALWAYS',i,actual,old)
  if actual<old-2:closer+=1
  if actual==0xffffff:
   assert not any(color[i*4:i*4+3]),('Background not black',i)
   assert color[i*4+3]==oldcolor[i*4+3],('Background alpha changed',i)
   continue
  visible+=1
  assert color[i*4+3] in (0,255),('Invalid alpha',i)
  if color[i*4+3]==0:
   assert not stable[i] and not any(color[i*4:i*4+3]) and oldcolor[i*4+3]==0,('Unexpected uncovered COLOR at covered depth',i)
  if stable[i]:
   assert color[i*4+3]==255,('Stable-interior alpha differs',i)
   expected=ref[i]*.5+.5;assert math.isfinite(expected)
   e=abs(actual/0xffffff-expected);maxerr=max(maxerr,e)
   assert e<=1e-4,('Stable-interior Z differs from analytical nearest surface',i,e)
   b,g,r=color[i*4:i*4+3];got=max(range(3),key=lambda c:[r,g,b][c]);ref_channels=ref_rgb[i*3:i*3+3];want=max(range(3),key=lambda c:ref_channels[c]);assert max(r,g,b)>0 and got==want,('Wrong visible gear in stable interior',i,got,want)
   stable_count+=1
 assert visible and closer>=min(100,n//10) and stable_count>=min(100000,n//10)
 return {'COLOR_full_byte_exact_to_ALWAYS_baseline':color==oldcolor,'raw_Z_byte_exact':depth==olddepth,'geometry_coverage_identical_to_ALWAYS':True,'Z24_never_farther_than_ALWAYS':True,'Z24_strictly_closer_pixels':closer,'visible_pixels':visible,'analytical_stable_interior_pixels':stable_count,'analytical_max_abs_Z_error':maxerr,'analytical_visible_gear_class_exact_in_stable_interiors':True,'analytical_reference_scope':'Captured triangles/MVP, CCW back cull, nearest depth, stable interiors>1.5pixels from triangle edges. Not full GL shading/precision/conformance.'}
