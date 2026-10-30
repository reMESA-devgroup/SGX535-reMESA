#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Collector protocol/cleanup tests; interposed transport only, GPU0."""
import errno
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import private_session_lab as lab
import private_probe as p
from test_private_probe import Transport

class SessionTransport(Transport):
    def __init__(self,fault=None):
        super().__init__(fault)
        self.actions=[];self.session=91;self.sequence=0;self.alive=False
    def ioctl(self,fd,number,packet,mutate):
        if number!=p.ioctl_number(10,lab.SESSION.size):return super().ioctl(fd,number,packet,mutate)
        assert fd==123 and mutate
        q=list(lab.SESSION.unpack(packet));action=q[1];self.actions.append(action)
        assert q[2]==self.context and q[18]==0 and not any(q[19:])
        if action==lab.OPEN:
            assert not self.alive and not q[3] and not q[4] and 1<=q[17]<=64
            assert len(self.objects)==13 and q[5:16]==list(range(32,43))
            self.alive=True;q[3]=self.session;q[21:23]=[0x01130000,0x00010201];q[26:28]=[1,16]
            if self.fault=='open_receipt':q[19]=1
        elif action==lab.SUBMIT:
            assert self.alive and q[3]==self.session and q[4]==self.sequence+1
            assert not any(q[5:16]) and not q[17]
            self.sequence+=1;q[19:28]=[100+self.sequence,100+self.sequence,0x01130000,0x00010201,9,7,0,1,15]
            if self.fault=='cookie':q[20]+=1
            if self.fault=='ledger':q[24]=3
            if self.fault=='structured_error':q[25]=-5;q[26]=2
            if self.fault=='submit_ioctl':raise OSError(errno.EIO,'modeled uncertain submission')
        else:
            assert action==lab.CLOSE and self.alive and q[3]==self.session and q[4]==self.sequence
            self.alive=False;q[26:28]=[3,32]
            if self.fault=='close_receipt':q[26]=1
            if self.fault=='close_ioctl':raise OSError(errno.EIO,'modeled uncertain close')
        if self.fault=='immutable':q[2]^=1
        if self.fault=='reserved':q[28]=1
        packet[:]=lab.SESSION.pack(*q);return 0

class Client(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.output=Path(self.tmp.name)
        self.plan=dict(bindings=[dict(role=i,va=0x40001000+i*8192,bytes=4096) for i in range(11)],guard_va=[0x40000000,0x40020000])
        self.images=[bytes([i])*4096 for i in range(5)]
    def collect(self,fault=None,second=None,frames=2):
        self.t=SessionTransport(fault)
        with patch.object(p,'device_identity',return_value={'fixture':'CPU_ONLY_NO_GPU'}), \
             patch.object(p.os,'open',return_value=123) as opened,patch.object(p.os,'close') as closed, \
             patch.object(p.fcntl,'ioctl',side_effect=self.t.ioctl):
            try:return lab.collect(Path('/never-real-device'),self.output,self.plan,self.images,frames,1000,second)
            finally:opened.assert_called_once();closed.assert_called_once_with(123)
    def test_two_jobs_keep_objects_and_one_session_then_release(self):
        r=self.collect()
        self.assertEqual(self.t.actions,[1,2,2,3]);self.assertFalse(self.t.objects)
        self.assertEqual(r['memory_cleanup'],'complete');self.assertFalse(r['hardware_driver_qualified'])
        self.assertEqual(r['status'],'SESSION_RECEIPTS_AND_READBACK_COLLECTED_ORACLE_PENDING')
        self.assertEqual([x['receipt']['operation'] for x in r['frames']],[101,102])
        self.assertEqual([x['PDS_pages_uploaded'] for x in r['frames']],[0,0])
        self.assertEqual([rid for action,rid in self.t.calls if action==p.DESTROY],list(reversed(range(32,45))))
    def test_uncertain_reply_never_sends_close_or_free(self):
        for fault in ('open_receipt','cookie','ledger','structured_error','submit_ioctl','immutable','reserved','close_receipt','close_ioctl'):
            with self.subTest(fault=fault):
                with self.assertRaises((RuntimeError,OSError)):self.collect(fault)
                self.assertFalse(any(action in (p.UNMAP,p.UNPIN,p.DESTROY,p.CLOSE) for action,rid in self.t.calls))
                report=json.loads((self.output/'RESULT.json').read_text())
                self.assertEqual(report['status'],'FAILED')
                self.assertEqual(report['memory_cleanup'],'suppressed_after_uncertain_memory_session_or_readback')
                self.assertLessEqual(self.t.actions.count(lab.CLOSE),1)
    def test_integrity_or_mapping_failure_stops_next_job(self):
        for fault in ('input_changed','guard_changed','mapping','provider_job','provider_quarantine'):
            with self.subTest(fault=fault):
                with self.assertRaises(RuntimeError):self.collect(fault)
                self.assertEqual(self.t.actions,[1,2])
                self.assertFalse(any(a in (p.UNMAP,p.UNPIN,p.DESTROY,p.CLOSE) for a,rid in self.t.calls))
    def test_update_sends_only_changed_pds_pages(self):
        second=list(self.images);data=bytearray(second[0]);data[70]=0x80;data[71]=0x3f;second[0]=bytes(data)
        r=self.collect(second=second,frames=3)
        self.assertEqual([x['PDS_pages_uploaded'] for x in r['frames']],[0,1,1])
        writes=[rid for action,rid in self.t.calls if action==p.WRITE]
        self.assertEqual(writes.count(32),3)
        self.assertTrue(all(writes.count(rid)==1 for rid in range(33,37)))
    def test_cleanup_failure_cannot_report_success(self):
        with self.assertRaises(RuntimeError):self.collect('cleanup')
        r=json.loads((self.output/'RESULT.json').read_text())
        self.assertEqual(r['status'],'FAILED');self.assertEqual(r['memory_cleanup'],'failed')
    def test_offline_default_opens_no_device(self):
        out=self.output/'offline'
        with patch('sys.argv',['private_session_lab.py','--assets','/fixture','--output',str(out)]), \
             patch.object(p,'load_assets',return_value=(self.plan,self.images)), \
             patch.object(p.os,'open',side_effect=AssertionError('device opened')),patch('sys.stdout',new=io.StringIO()):
            lab.main()
        r=json.loads((out/'RESULT.json').read_text());self.assertEqual(r['GPU_operations'],0)

if __name__=='__main__':unittest.main()
