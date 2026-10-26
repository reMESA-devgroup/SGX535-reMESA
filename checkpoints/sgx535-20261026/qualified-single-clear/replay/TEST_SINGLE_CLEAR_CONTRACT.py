from pathlib import Path
import struct,json
from single_clear_output_contract import compare_single_clear_outputs as compare
n=20;c=bytes([0,255,0,255])*n+bytes(4);oldc=c;z=struct.pack('<21I',*([int(.9*0xffffff)]*n+[0xffffff]));oldz=struct.pack('<21I',*([int(.95*0xffffff)]*n+[12345]));rgb=bytes([0,255,0])*n+bytes(3);ref=struct.pack('<21f',*([.8]*n+[float('inf')]));mask=bytes([1])*n+bytes(1)
r=compare(c,oldc,z,oldz,rgb,ref,mask);assert r['old_depth_only_artifacts_removed_and_independently_empty']==[20]
cases=[]
def negative(name,cc=c,zz=z,oc=oldc,oz=oldz,rr=rgb,rd=ref,sm=mask):
 try:compare(cc,oc,zz,oz,rr,rd,sm)
 except AssertionError:cases.append(name)
 else:raise AssertionError(name)
negative('depth_without_COLOR',zz=z[:-4]+struct.pack('<I',12345))
negative('lost_existing_visible_pixel',cc=bytes(4)+c[4:],zz=struct.pack('<I',0xffffff)+z[4:])
negative('new_bogus_background_geometry',cc=c[:-4]+bytes([0,255,0,255]),zz=z[:-4]+struct.pack('<I',100))
negative('background_nonblack',cc=c[:-4]+bytes([1,0,0,0]))
negative('old_removed_depth_has_reference_geometry',rr=rgb[:-3]+bytes([0,255,0]),rd=ref[:-4]+struct.pack('<f',.8))
negative('visible_depth_farther',zz=oldz)
negative('wrong_visible_gear',cc=bytes([255,0,0,255])*n+bytes(4))
negative('wrong_nearest_depth',rd=struct.pack('<21f',*([.5]*n+[float('inf')])))
negative('no_nearer_depth_improvement',zz=struct.pack('<21I',*([int(.95*0xffffff)]*n+[0xffffff])))
negative('no_stable_reference',sm=bytes(n+1))
Path(__file__).with_name('SINGLE_CLEAR_CONTRACT_TEST_RESULTS.json').write_text(json.dumps({'status':'PASS','GPU_operations':0,'positive_strict_COLOR_Z_coverage_and_old_artifact_removal':True,'negative_cases':cases},indent=2)+'\n');print('Corrected-clear positive and10negative cases PASS; no GPU')
