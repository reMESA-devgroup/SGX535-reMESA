#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded private-session LAB collector. Offline plan by default; never presents.

Does not install/load a module, change controls, reboot or promote FAST/LAB gates.
Every executed job needs its own validated receipt and saved physical readback.
"""
import argparse
import fcntl
import json
import os
from pathlib import Path
import struct
import time
import private_probe as p

SESSION = struct.Struct('<IIQQQ11I3IQQ4Ii3I')
assert SESSION.size == 136
OPEN, SUBMIT, CLOSE = 1, 2, 3
ACTIVE, POISONED, CLOSED = 1, 2, 3

def packet(action, context, session=0, sequence=0, ids=None, budget=0, maximum=0):
    return bytearray(SESSION.pack(1, action, context, session, sequence,
                                 *(ids or [0]*11), budget, maximum, 0, *([0]*10)))

def exchange(fd, request, output, stem):
    before = SESSION.unpack(request)
    (output / (stem+'-REQUEST.bin')).write_bytes(request)
    try:
        result = fcntl.ioctl(fd, p.ioctl_number(10, SESSION.size), request, True)
    finally:
        (output / (stem+'-RESPONSE.bin')).write_bytes(request)
    q = SESSION.unpack(request)
    mutable = {3} if before[1] == OPEN else set()
    if result or q[28] or any(q[i] != before[i] for i in range(19) if i not in mutable):
        raise RuntimeError('session ioctl changed immutable envelope or returned a nonzero success')
    r = dict(zip(('operation','cookie','core','revision','phase','ledger','error','state','stages'), q[19:28]))
    r.update(session=q[3], sequence=q[4], action=q[1])
    if r['error']:
        raise RuntimeError('session returned an error; retain ownership: '+json.dumps(r))
    if before[1] == OPEN:
        good = (r['session'] and r['state'] == ACTIVE and r['stages'] == 16 and
                r['core'] == 0x01130000 and r['revision'] == 0x00010201 and
                not any(r[k] for k in ('operation','cookie','phase','ledger')))
    elif before[1] == SUBMIT:
        good = (r['state'] == ACTIVE and r['stages'] == 15 and r['operation'] and
                r['operation'] == r['cookie'] and r['core'] == 0x01130000 and
                r['revision'] == 0x00010201 and r['phase'] == 9 and r['ledger'] == 7)
    else:
        good = (r['state'] == CLOSED and r['stages'] == 32 and
                not any(r[k] for k in ('operation','cookie','core','revision','phase','ledger')))
    if not good:
        raise RuntimeError('incomplete session response; no release: '+json.dumps(r))
    return r

def update_pds(memory, object_id, old, new):
    if old is new: return 0
    if len(old) != len(new) or len(new) % 4096:
        raise ValueError('PDS update must preserve its complete allocation')
    pages = [at for at in range(0, len(new), 4096) if old[at:at+4096] != new[at:at+4096]]
    if not pages: return 0
    memory.request(p.LEASE, object_id, flags=1)
    try:
        for at in pages:
            memory.request(p.WRITE, object_id, offset=at, length=4096, data=new[at:at+4096])
    finally:
        if not memory.poisoned: memory.request(p.UNLEASE, object_id)
    # The qualified cold executor publishes the CPU writes before issuing GPU
    # work. Do not add a redundant whole-PDS cache flush here.
    return len(pages)

def collect(device, output, plan, images, frames, budget, second=None):
    report = dict(status='PREPARING', identity=p.device_identity(device), frames=[],
                  hardware_driver_qualified=False, requires_independent_pixel_oracle=True,
                  scope='bounded private-session LAB, not general GL or sustained FPS')
    def save():
        (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
    save()
    fd = os.open(device, os.O_RDWR | os.O_CLOEXEC)
    memory = p.Memory(fd,0)
    objects=[];opened=False;session_attempted=False;session_closed=False
    try:
        memory.context=memory.request(p.OPEN)[4];opened=True
        if not memory.context: raise RuntimeError('missing server memory context')
        start=time.monotonic()
        for binding in plan['bindings']:
            rid=memory.request(p.CREATE,size=binding['bytes'])[2]
            state=dict(id=rid,pinned=False,mapped=False);objects.append(state)
            memory.request(p.PIN,rid);state['pinned']=True
            memory.request(p.MAP,rid,va=binding['va']);state['mapped']=True
            if binding['role']<5: memory.upload(rid,images[binding['role']])
        for va in plan['guard_va']:
            rid=memory.request(p.CREATE,size=4096)[2]
            state=dict(id=rid,pinned=False,mapped=False);objects.append(state)
            memory.request(p.PIN,rid);state['pinned']=True
            memory.request(p.MAP,rid,va=va);state['mapped']=True
            memory.upload(rid,b'\xa5'*4096)
        ids=[obj['id'] for obj in objects[:11]]
        report['prepare_upload_wall_seconds']=time.monotonic()-start
        session_attempted=True
        start=time.monotonic()
        reply=exchange(fd,packet(OPEN,memory.context,ids=ids,maximum=frames),output,'OPEN')
        session=reply['session'];report['open']=reply
        report['open_wall_seconds']=time.monotonic()-start;save()
        current=images[0];last_operation=0
        for i in range(1,frames+1):
            expected=second if second is not None and i%2==0 else images
            row=dict(sequence=i);report['frames'].append(row)
            row['PDS_pages_uploaded']=update_pds(memory,ids[0],current,expected[0]);current=expected[0]
            start=time.monotonic()
            reply=exchange(fd,packet(SUBMIT,memory.context,session,i,budget=budget),output,f'frame-{i:06}')
            row['submit_wall_seconds']=time.monotonic()-start;row['receipt']=reply;save()
            if reply['operation']<=last_operation: raise RuntimeError('non-monotonic GPU operation')
            last_operation=reply['operation']
            info=memory.info()
            if any(info[j] for j in (6,7,8)): raise RuntimeError('provider retains jobs/leases/quarantine after job receipt')
            for obj,binding in zip(objects,plan['bindings']+[dict(va=va,bytes=4096) for va in plan['guard_va']]):
                info=memory.info(obj['id'])
                if (info[1:3]!=(binding['bytes'],binding['va']) or any(info[j] for j in (6,7,8,11)) or
                    info[9]!=1 or info[10]!=binding['bytes']//4096 or info[12]!=info[10]):
                    raise RuntimeError('provider mapping changed after retirement')
            start=time.monotonic();readbacks={}
            for role in (0,1,2,3,4,5,10):
                data=memory.read(ids[role],plan['bindings'][role]['bytes'])
                (output/f'frame-{i:06}-role{role}.bin').write_bytes(data)
                readbacks[str(role)]=dict(bytes=len(data),sha256=p.sha(data))
                if role<5 and data!=expected[role]: raise RuntimeError('input integrity failure; stop session')
            for g,obj in enumerate(objects[11:]):
                data=memory.read(obj['id'],4096)
                (output/f'frame-{i:06}-guard{g}.bin').write_bytes(data)
                if data!=b'\xa5'*4096: raise RuntimeError('guard integrity failure; stop session')
            row['readback_wall_seconds']=time.monotonic()-start
            row.update(readbacks=readbacks,guards_intact=True);save()
        reply=exchange(fd,packet(CLOSE,memory.context,session,frames),output,'CLOSE')
        report['close']=reply;session_closed=True
        final_info=memory.info()
        if any(final_info[j] for j in (6,7,8)): raise RuntimeError('provider not idle after session close')
        report['status']='SESSION_RECEIPTS_AND_READBACK_COLLECTED_ORACLE_PENDING'
    except BaseException as error:
        report.update(status='FAILED',error=str(error));raise
    finally:
        if opened and not memory.poisoned and (not session_attempted or session_closed):
            try:
                for obj in reversed(objects):
                    if obj['mapped']:memory.request(p.UNMAP,obj['id'])
                    if obj['pinned']:memory.request(p.UNPIN,obj['id'])
                    memory.request(p.DESTROY,obj['id'])
                memory.request(p.CLOSE);report['memory_cleanup']='complete'
            except Exception as error:
                report.update(status='FAILED',memory_cleanup='failed',cleanup_error=str(error))
        else:report['memory_cleanup']='suppressed_after_uncertain_memory_session_or_readback'
        try:os.close(fd)
        finally:save()
    if report['status']=='FAILED':raise RuntimeError(report.get('cleanup_error','collection failed'))
    return report

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('--assets',type=Path,required=True);a.add_argument('--next-assets',type=Path)
    a.add_argument('--output',type=Path,required=True);a.add_argument('--frames',type=int,default=2)
    a.add_argument('--budget-ms',type=int,default=10000);a.add_argument('--device',type=Path)
    a.add_argument('--execute',action='store_true');args=a.parse_args()
    if not 1<=args.frames<=64 or not 1<=args.budget_ms<=10000:a.error('frames1..64 and budget1..10000ms required')
    if args.execute and not args.device:a.error('--execute requires --device')
    plan,images=p.load_assets(args.assets);second=None
    if args.next_assets:
        next_plan,second=p.load_assets(args.next_assets)
        first_raw=(args.assets/'stream-profile.bin').read_bytes();next_raw=(args.next_assets/'stream-profile.bin').read_bytes()
        if first_raw[:48]+first_raw[80:]!=next_raw[:48]+next_raw[80:] or images[1:]!=second[1:]:
            raise ValueError('second pose must preserve commands/bindings/normalized digest and all other inputs')
        if next_plan['bindings']!=plan['bindings'] or next_plan['guard_va']!=plan['guard_va']:raise ValueError('second pose changed layout')
    args.output.mkdir(parents=True,exist_ok=False)
    if args.execute:r=collect(args.device,args.output,plan,images,args.frames,args.budget_ms,second)
    else:
        r=dict(status='OFFLINE_PLAN_ONLY',GPU_operations=0,frames=args.frames,plan=plan,
               second_pose=second is not None,hardware_driver_qualified=False)
        (args.output/'RESULT.json').write_text(json.dumps(r,indent=2)+'\n')
    print(json.dumps(r,indent=2))
if __name__=='__main__':main()
