from pathlib import Path
import importlib.util,struct,gzip,hashlib,json
D=Path(__file__).resolve().parent;B=D.parent/'gl-triangle-recovery-20261021T101936Z/quarter-v2-private-initrd.img';raw=B.read_bytes();sha=lambda b:hashlib.sha256(b).hexdigest()
sp=importlib.util.spec_from_file_location('newc',D.parent/'boot-provenance-readonly-20261001T025947Z/read_initramfs.py');m=importlib.util.module_from_spec(sp);sp.loader.exec_module(m)
rows,cs=m.read_image(raw);assert len(cs)==2 and cs[1]['format']=='gzip';at=cs[1]['offset'];expanded=gzip.decompress(raw[at:]);mapping={x['name']:x for x in rows}
def note(ko):
 assert ko[:6]==b'\x7fELF\x01\x01';shoff=struct.unpack_from('<I',ko,32)[0];ents,num,st=struct.unpack_from('<3H',ko,46);hs=[struct.unpack_from('<10I',ko,shoff+i*ents)for i in range(num)];h=hs[st];names=ko[h[4]:h[4]+h[5]]
 return next(ko[h[4]:h[4]+h[5]]for h in hs if names[h[0]:].split(b'\0',1)[0]==b'.note.gnu.build-id')
script=mapping['scripts/sgx535-first-load']['data'];updates={};records={}
for name,local in [('gma500_gfx',D/'module/gma500_gfx.ko'),('sgx535_provenance',D/'observer/sgx535_provenance.ko')]:
 target='usr/lib/sgx535-first-load/'+name+'.ko';old=mapping[target]['data'];new=local.read_bytes();assert script.count(sha(old).encode())==1;assert script.count(sha(note(old)).encode())==1
 script=script.replace(sha(old).encode(),sha(new).encode()).replace(sha(note(old)).encode(),sha(note(new)).encode());updates[target]=new;records[name]={'sha256':sha(new),'note_sha256':sha(note(new))}
init=mapping['init']['data'];assert init.count(sha(mapping['scripts/sgx535-first-load']['data']).encode())==1;init=init.replace(sha(mapping['scripts/sgx535-first-load']['data']).encode(),sha(script).encode());updates.update({'scripts/sgx535-first-load':script,'init':init})
out=[];pos=0;changed=[]
while True:
 assert expanded[pos:pos+6]==b'070701';h=expanded[pos:pos+110];size=int(h[54:62],16);ns=int(h[94:102],16);name=expanded[pos+110:pos+110+ns-1].decode();ds=(pos+110+ns+3)&~3;end=(ds+size+3)&~3
 if name in updates:
  b=updates[name];part=h[:54]+f'{len(b):08x}'.encode()+h[62:]+expanded[pos+110:ds]+b;part+=b'\0'*((-len(part))%4);out.append(part);changed.append(name)
 else:out.append(expanded[pos:end])
 pos=end
 if name=='TRAILER!!!':break
out.append(expanded[pos:]);assert set(changed)==set(updates);img=raw[:at]+gzip.compress(b''.join(out),compresslevel=9,mtime=0);r,c=m.read_image(img);newmap={x['name']:x for x in r};assert set(newmap)==set(mapping)
for n,x in mapping.items():
 y=newmap[n];assert y['data']==updates.get(n,x['data'])
 for f in ['mode','uid','gid','nlink','inode','device','mtime']:assert x[f]==y[f]
assert img[:at]==raw[:at];p=D/'depth1280-private-initrd.img';p.write_bytes(img);(D/'depth1280-first-load.sh').write_bytes(script)
r={'base_path':str(B),'base_sha256':sha(raw),'image_sha256':sha(img),'image_bytes':len(img),'modules':records,'changed_members':changed,'other_members_unchanged':True,'boot_script_logic_unchanged_identity_bindings_updated':True,'GPU_submissions':0,'installed':False}
(D/'INITRD_BUILD.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
