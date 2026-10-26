"""Offline defined-format comparison only. Files are never normalized or changed."""
import hashlib,json
from pathlib import Path

def verify_format_sources(manifest_path):
 manifest=json.loads(Path(manifest_path).read_text())
 for row in manifest['sources']:
  raw=Path(row['path']).read_bytes();assert hashlib.sha256(raw).hexdigest()==row['sha256']
  for required in row['required_text']:assert required in raw.decode()
 return manifest

def compare_defined_outputs(color,expected_color,z,expected_z):
 assert len(color)==len(expected_color)==len(z)==len(expected_z)==4096000
 assert color==expected_color,'COLOR defined RGBA bytes differ'
 assert all(z[channel::4]==expected_z[channel::4]for channel in range(3)),'Defined Z24 bits differ'
 actual_x8=z[3::4];expected_x8=expected_z[3::4]
 mismatch=sum(a!=b for a,b in zip(actual_x8,expected_x8))
 tile=[actual_x8[y*1280:y*1280+16].hex()for y in range(16)]
 # This describes the observed ignored channel; it is not a required X8 value.
 periodic16=all(actual_x8[y*1280:(y+1)*1280]==bytes.fromhex(tile[y%16])*80 for y in range(800))
 return {'COLOR_full_byte_exact':True,'Z24_defined_bits_byte_exact':True,'raw_Z_byte_exact':z==expected_z,'X8_mismatch_count':mismatch,'X8_scope':'VOID channel of PIPE_FORMAT_Z24X8_UNORM; not stencil or GL-visible depth','X8_observed_period16x16':periodic16,'X8_observed_first16x16_hex_rows':tile,'offline_actual_raw_Z_sha256':hashlib.sha256(z).hexdigest(),'offline_expected_raw_Z_sha256':hashlib.sha256(expected_z).hexdigest()}
