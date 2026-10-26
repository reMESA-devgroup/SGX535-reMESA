"""Pure filesystem bundle validation/copy helpers. No network, GLX or GPU calls."""
from pathlib import Path
import hashlib,json,shutil
CLIENT_SHA256='159aa90de37785038490ff51bb5d7de86f37d81c129fc8090e13f6687713dc37'
REPLACEMENTS={'assets/stream-profile.bin',*[f'assets/role{i}.bin'for i in range(5)]}
def file_hash(path):
 h=hashlib.sha256()
 with Path(path).open('rb')as f:
  for block in iter(lambda:f.read(1048576),b''):h.update(block)
 return h.hexdigest()
def verify_bundle(root,expected_manifest=None):
 root=Path(root);raw=(root/'MANIFEST.json').read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if expected_manifest is not None:assert digest==expected_manifest,'Manifest SHA differs'
 m=json.loads(raw);assert m['client_sha256']==CLIENT_SHA256 and m['files']['glxgears-original']==CLIENT_SHA256
 actual=set()
 for p in root.rglob('*'):
  assert not p.is_symlink(),str(p)
  if p.is_file():actual.add(str(p.relative_to(root)))
 assert actual==set(m['files'])|{'MANIFEST.json'},'Unlisted or missing bundle files'
 for name,digest in m['files'].items():
  rel=Path(name);assert not rel.is_absolute() and '..'not in rel.parts and name!='MANIFEST.json'
  path=root/rel;assert path.is_file()and not path.is_symlink()and path.resolve().is_relative_to(root.resolve())
  assert file_hash(path)==digest,name
 return m,hashlib.sha256(raw).hexdigest()
def replace_bundle(old,new,payload,expected,old_manifest_sha,scope):
 old=Path(old);new=Path(new);m,_=verify_bundle(old,old_manifest_sha)
 assert set(payload)==set(expected)==REPLACEMENTS
 for name,data in payload.items():
  assert name in m['files'] and isinstance(data,bytes)
  assert len(data)==expected[name]['bytes'] and hashlib.sha256(data).hexdigest()==expected[name]['sha256']
 assert not new.exists()and not new.parent.exists(),'Unique private target already exists'
 new.parent.mkdir(mode=0o700);shutil.copytree(old,new)
 files=dict(m['files'])
 for name,data in payload.items():(new/name).write_bytes(data);files[name]=expected[name]['sha256']
 updated={'client_sha256':CLIENT_SHA256,'files':dict(sorted(files.items())),'source_bundle':str(old),'source_manifest_sha256':old_manifest_sha,'source_metadata':{k:v for k,v in m.items()if k not in('files','client_sha256')},'scope':scope,'GPU_submissions_at_stage':0}
 (new/'MANIFEST.json').write_text(json.dumps(updated,indent=2)+'\n')
 checked,digest=verify_bundle(new)
 assert file_hash(new/'glxgears-original')==CLIENT_SHA256 and checked['files']['glxgears-original']==CLIENT_SHA256
 return {'manifest_sha256':digest,'whole_manifest_files_verified':len(checked['files']),'client_sha256':CLIENT_SHA256,'replaced_files':expected,'GPU_submissions':0}
