"""Corrected clear oracle, separate from preserved failed legacy Z-footprint gate.
Opaque GLXGears only: historical COLOR silhouette is unchanged. Every newly
visible pixel must have depth; every invisible pixel must have clear depth.
Old depth-only artifacts may disappear ONLY with independent empty geometry.
No tolerance/coordinate exclusion is added for any new missing gear pixels.
"""
import math,sys

def compare_single_clear_outputs(color,oldcolor,depth,olddepth,ref_rgb,ref_depth,stable):
 assert sys.byteorder=='little';n=len(depth)//4
 assert n and len(depth)==len(olddepth)==len(color)==len(oldcolor)==n*4 and len(ref_rgb)==n*3 and len(ref_depth)==n*4 and len(stable)==n
 z=memoryview(depth).cast('I');before=memoryview(olddepth).cast('I');ref=memoryview(ref_depth).cast('f');closer=visible=stable_count=0;removed=[];maxerr=0.
 for i in range(n):
  actual=z[i]&0xffffff;old=before[i]&0xffffff
  old_visible=oldcolor[i*4+3]==255;new_visible=color[i*4+3]==255
  assert color[i*4+3]in(0,255)and oldcolor[i*4+3]in(0,255),('Unexpected alpha',i)
  assert new_visible==old_visible,('Historical COLOR silhouette changed',i)
  assert (actual!=0xffffff)==new_visible,('COLOR and Z coverage disagree',i)
  if not new_visible:
   assert color[i*4:i*4+4]==bytes(4)and oldcolor[i*4:i*4+4]==bytes(4),('Background is not exact black/alpha0',i)
   if old!=0xffffff:
    assert ref_rgb[i*3:i*3+3]==bytes(3)and math.isinf(ref[i])and ref[i]>0 and not stable[i],('Removed old depth is not independently empty geometry',i)
    removed.append(i)
   continue
  assert old!=0xffffff and actual<=old+2,('Visible depth farther than old ALWAYS',i,actual,old)
  closer+=actual<old-2;visible+=1
  if stable[i]:
   expected=ref[i]*.5+.5;assert math.isfinite(expected)
   e=abs(actual/0xffffff-expected);maxerr=max(maxerr,e);assert e<=1e-4,('Stable nearest-surface Z differs',i,e)
   b,g,r=color[i*4:i*4+3];channels=[r,g,b];target=ref_rgb[i*3:i*3+3]
   assert max(channels)>0 and max(range(3),key=lambda c:channels[c])==max(range(3),key=lambda c:target[c]),('Wrong visible gear in stable interior',i)
   stable_count+=1
 assert visible and closer>=min(100,n//10)and stable_count>=min(100000,n//10)
 return {'COLOR_full_byte_exact_to_ALWAYS_baseline':color==oldcolor,'raw_Z_byte_exact':depth==olddepth,'COLOR_silhouette_identical_to_ALWAYS':True,'COLOR_Z_co_coverage_exact_all_pixels':True,'background_COLOR_RGBA_zero_and_Z24_clear_all_pixels':True,'Z24_never_farther_on_historical_visible_geometry':True,'Z24_strictly_closer_pixels':closer,'old_depth_only_artifacts_removed_and_independently_empty':removed,'visible_pixels':visible,'analytical_stable_interior_pixels':stable_count,'analytical_max_abs_Z_error':maxerr,'analytical_visible_gear_class_exact_in_stable_interiors':True,'analytical_reference_scope':'Captured original triangles/MVP, nearest depth and gear class in stable interiors. Removed old depth-only pixels additionally require independent no-triangle reference. Not full GL shading/precision conformance.'}
