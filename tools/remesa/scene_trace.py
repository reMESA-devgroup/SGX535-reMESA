#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Decode one closed LAB metadata snapshot. No MMIO or rendering functions."""
import struct
HEADER=struct.Struct('<6Ii9I8Q')
QUERY=struct.Struct('<IIQ4256s')
EVENT=struct.Struct('<6IQ')
BINDING=struct.Struct('<IIQQ')
assert HEADER.size==128 and QUERY.size==4272 and EVENT.size==32

def decode(blob,context,receipt,plan):
 if len(blob)!=4256:raise ValueError('trace size')
 h=HEADER.unpack_from(blob)
 names=('version','bytes','profile','stages','phase','ledger','error','retained','event_count','zero_samples','binding_count','load_count','ta_count','raster_count','use_mask','reserved','context','operation','cookie','cpu_flush_ns','ta_begin_ns','ta_end_ns','raster_begin_ns','raster_end_ns')
 q=dict(zip(names,h))
 if (q['version']!=1 or q['bytes']!=4256 or q['reserved'] or q['context']!=context or q['profile']!=plan['profile'] or q['binding_count']!=17 or q['event_count']>64 or q['load_count']!=29 or q['ta_count']!=31 or q['raster_count']!=20 or q['use_mask']&~0xfff8):raise ValueError('trace header/owner/plan')
 for k in ('operation','cookie','stages','phase','ledger','error','retained'):
  if q[k]!=receipt[k]:raise ValueError('trace differs from lifecycle receipt: '+k)
 at=128
 for name,count in [('ta',14),('raster',52),('use_banks',32),('load_actions',116),('ta_actions',124),('raster_actions',80)]:
  q[name]=list(struct.unpack_from('<'+str(count)+'I',blob,at));at+=count*4
 assert at==1800
 q['events']=[]
 for i in range(64):
  v=EVENT.unpack_from(blob,at);at+=32
  if i<q['event_count']:q['events'].append(dict(zip(('kind','sequence','status1','status2','ledger','phase','ns'),v)))
  elif any(v):raise ValueError('trace unused event tail')
 q['bindings']=[]
 for i in range(17):
  ident,access,address,n=BINDING.unpack_from(blob,at);at+=24;b=plan['bindings'][i]
  if not ident or access!=b['access'] or address!=b['va'] or n!=b['bytes']:raise ValueError('trace binding mismatch')
  q['bindings'].append({'id':ident,'access':access,'address':address,'bytes':n})
 assert at==len(blob)
 q['timing_scope']='CPU flush duration and host MMIO/source-event timestamps; GPU-only execution time is not measured'
 return q
