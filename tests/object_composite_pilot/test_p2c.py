"""P2-C: durable complete working state, real process crashes and owner reorders."""
import hashlib
import json
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import uuid
import test_pilot as legacy

BINARY=str(Path(sys.argv.pop(1)).resolve())
FAULT=str(Path(sys.argv.pop(1)).resolve())
legacy.BINARY=BINARY
STAGES=['before_write','after_blobs','after_snapshot','before_commit','after_commit']

class Recovery(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='obj01-p2c-test-')
        self.folder=Path(self.tmp.name)/'p2c'
        self.c=legacy.Client(self.folder,profile='p2c')
        self.origin=(self.folder/'origem-documento.json').read_bytes()
    def tearDown(self):
        self.c.close()
        self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin)
        self.tmp.cleanup()
    def yes(self,r):
        self.assertTrue(r['accepted'],r);return r['state']
    def context(self,name):return self.yes(self.c.op('set_context',context=name))
    def table(self):
        return self.yes(self.c.op('create_table',before='',title='Notas recuperáveis',header=True,cells=['Lugar','Imagem','Nota','Margem','Verde','Memória 🙂']))['tables'][-1]
    def restart(self):
        self.c.close();self.c=legacy.Client(self.folder,profile='p2c');return self.c.state
    def db(self):return sqlite3.connect(self.folder/'obj01-piloto.sqlite3')
    def model(self,s):return {k:s[k] for k in ('paragraphs','tables','figures','rows','author_order','visual_order','selection','selected_cell','context','source_sha256','source_body_sha256')}
    def test_p13_unsaved_complete_work_survives_sigkill(self):
        s=self.yes(self.c.image());f=s['figures'][0]
        self.context('composition');self.yes(self.c.op('arrange',id=f['id'],paragraph=s['paragraphs'][1]['id'],first=.3,second=.7,gap=20))
        self.context('structure');t=self.table();self.context('text')
        before=self.yes(self.c.op('edit_cell',id=t['id'],cell=t['cells'][5]['id'],text='Trabalho ainda não salvo 漢🙂'))
        with self.db() as db:
            saved=json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0]);self.assertEqual(saved['tables'],[])
        self.c.process.kill();self.c.close(expected=-signal.SIGKILL)
        self.c=legacy.Client(self.folder,profile='p2c');after=self.c.state
        self.assertEqual(self.model(before),self.model(after));self.assertTrue(after['dirty']);self.assertTrue(after['recovered_on_open']);self.assertFalse(after['can_undo'])
        self.assertEqual(after['saved_revision'],0)
    def test_save_promotes_work_but_keeps_live_history(self):
        t=self.table();self.context('text');self.yes(self.c.op('edit_cell',id=t['id'],cell=t['cells'][0]['id'],text='Salvo'))
        s=self.yes(self.c.op('save'));self.assertFalse(s['dirty']);self.assertTrue(s['can_undo'])
        with self.db() as db:
            saved=db.execute('SELECT revision,sha256,snapshot FROM saved').fetchone();work=db.execute('SELECT base_revision,base_sha256,sha256,snapshot FROM working').fetchone()
            self.assertEqual((saved[0],saved[1],saved[1],saved[2]),work)
        after=self.restart();self.assertFalse(after['dirty']);self.assertFalse(after['recovered_on_open']);self.assertFalse(after['can_undo'])
    def test_explicit_saved_discard_and_verified_unclaimed_blob(self):
        self.yes(self.c.image())
        denied=self.c.op('reopen',discard_unsaved=False);self.assertEqual(denied['error'],'unsaved_confirmation_required')
        s=self.yes(self.c.op('reopen',discard_unsaved=True));self.assertEqual(s['figures'],[]);self.assertFalse(s['dirty'])
        with self.db() as db:self.assertEqual(db.execute('SELECT count(*) FROM blobs').fetchone()[0],1)
        self.assertEqual(self.restart()['figures'],[])
        self.yes(self.c.image());self.yes(self.c.op('save'))
    def test_checkpoint_failure_rolls_back_and_preserves_work_and_history(self):
        before=self.c.state;self.c.close();self.c=legacy.Client(self.folder,profile='p2c',binary=FAULT,fault='checkpoint:before_commit:throw')
        r=self.c.image();self.assertFalse(r['accepted']);self.assertEqual(r['error'],'injected_persistence_fault');self.assertEqual(self.model(r['state']),self.model(before));self.assertFalse(r['state']['can_undo']);self.assertEqual(r['state']['persistence_status'],'error')
        with self.db() as db:
            self.assertEqual(db.execute('SELECT generation FROM working').fetchone()[0],0);self.assertEqual(db.execute('SELECT count(*) FROM blobs').fetchone()[0],0)
    def test_lost_checkpoint_ack_requires_reopen_and_recovers_candidate(self):
        self.c.close();self.c=legacy.Client(self.folder,profile='p2c',binary=FAULT,fault='checkpoint:after_commit:throw')
        r=self.c.image();self.assertFalse(r['accepted']);self.assertEqual(r['error'],'persistence_outcome_uncertain');self.assertTrue(r['state']['uncertain_candidate_available']);self.assertEqual(r['state']['figures'],[])
        self.assertEqual(self.c.op('save')['error'],'persistence_requires_reopen')
        s=self.yes(self.c.op('reopen_working',discard_unsaved=True));self.assertEqual(len(s['figures']),1);self.assertEqual(s['persistence_status'],'protected');self.assertTrue(s['dirty'])
    def test_lost_save_ack_reopens_committed_saved_state(self):
        self.yes(self.c.image());self.c.close()
        self.c=legacy.Client(self.folder,profile='p2c',binary=FAULT,fault='save:after_commit:throw')
        r=self.c.op('save');self.assertEqual(r['error'],'persistence_outcome_uncertain')
        self.assertEqual(r['state']['saved_revision'],0)
        self.assertEqual(self.c.op('undo')['error'],'persistence_requires_reopen')
        s=self.yes(self.c.op('reopen_working',discard_unsaved=True))
        self.assertEqual(s['saved_revision'],1);self.assertFalse(s['dirty']);self.assertEqual(len(s['figures']),1)
    def test_undo_checkpoint_is_recovered_after_sigkill(self):
        t=self.table();self.context('text')
        self.yes(self.c.op('edit_cell',id=t['id'],cell=t['cells'][0]['id'],text='Depois'))
        s=self.yes(self.c.op('undo'));self.assertEqual(s['tables'][0]['cells'][0]['text'],'Lugar')
        self.c.process.kill();self.c.close(expected=-signal.SIGKILL)
        self.c=legacy.Client(self.folder,profile='p2c')
        self.assertEqual(self.model(self.c.state),self.model(s));self.assertFalse(self.c.state['can_redo'])
    def test_working_base_and_resource_corruption_fail_closed(self):
        self.yes(self.c.image());self.c.close()
        with self.db() as db:db.execute('UPDATE working SET base_revision=99')
        r=subprocess.run([BINARY,'--workspace',str(self.folder),'--profile','p2c'],input='',text=True,capture_output=True,timeout=15)
        self.assertEqual(r.returncode,1);self.assertIn('working_base_conflict',r.stderr)
        with self.db() as db:
            db.execute('UPDATE working SET base_revision=0')
            original=db.execute('SELECT data FROM blobs').fetchone()[0]
            db.execute("UPDATE blobs SET data=x'00'")
        r=subprocess.run([BINARY,'--workspace',str(self.folder),'--profile','p2c'],input='',text=True,capture_output=True,timeout=15)
        self.assertEqual(r.returncode,1)
        with self.db() as db:db.execute('UPDATE blobs SET data=?',(original,))
        self.c=legacy.Client(self.folder,profile='p2c')
    def test_real_checkpoint_crash_windows(self):
        for stage in STAGES:
            with self.subTest(stage=stage):
                folder=Path(self.tmp.name)/stage
                c=legacy.Client(folder,profile='p2c',binary=FAULT,fault='checkpoint:'+stage+':crash')
                with self.assertRaises(AssertionError):c.image()
                c.close(expected=86)
                c=legacy.Client(folder,profile='p2c')
                try:
                    self.assertEqual(len(c.state['figures']),1 if stage=='after_commit' else 0)
                    self.assertEqual(c.state['saved_revision'],0)
                    with sqlite3.connect(folder/'obj01-piloto.sqlite3') as db:self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0],'ok')
                finally:c.close()
    def test_real_save_crash_windows(self):
        for stage in STAGES:
            with self.subTest(stage=stage):
                folder=Path(self.tmp.name)/('save-'+stage)
                c=legacy.Client(folder,profile='p2c');c.image();c.close()
                c=legacy.Client(folder,profile='p2c',binary=FAULT,fault='save:'+stage+':crash')
                with self.assertRaises(AssertionError):c.op('save')
                c.close(expected=86);c=legacy.Client(folder,profile='p2c')
                try:
                    self.assertEqual(len(c.state['figures']),1);self.assertEqual(c.state['saved_revision'],1 if stage=='after_commit' else 0);self.assertEqual(c.state['dirty'],stage!='after_commit')
                finally:c.close()
    def test_work_generation_conflict_and_reload(self):
        with self.db() as db:db.execute('UPDATE working SET generation=generation+1')
        r=self.c.image();self.assertFalse(r['accepted']);self.assertEqual(r['error'],'working_generation_conflict');self.assertEqual(r['state']['figures'],[])
        self.yes(self.c.op('reopen_working',discard_unsaved=True));self.yes(self.c.image())
    def test_corrupt_work_fails_closed_without_saved_fallback(self):
        self.table();self.c.close()
        with self.db() as db:db.execute("UPDATE working SET sha256='false'")
        r=subprocess.run([BINARY,'--workspace',str(self.folder),'--profile','p2c'],input='',text=True,capture_output=True,timeout=15)
        self.assertEqual(r.returncode,1);self.assertIn('saved_hash_mismatch',r.stderr)
        with self.db() as db:db.execute('UPDATE working SET sha256=?',(hashlib.sha256(db.execute('SELECT snapshot FROM working').fetchone()[0].encode()).hexdigest(),))
        self.c=legacy.Client(self.folder,profile='p2c')
    def test_p10_owner_group_reorder_and_undo_conserve_original(self):
        s=self.c.state;initial=self.model(s);g=s['text_groups'][1]
        moved=self.yes(self.c.op('reorder_text',group=g['id'],before=s['paragraphs'][0]['id']))
        self.assertEqual([p['id'] for p in moved['paragraphs']],[s['paragraphs'][1]['id'],s['paragraphs'][2]['id'],s['paragraphs'][0]['id']])
        self.assertEqual(moved['source_sha256'],s['source_sha256']);self.assertNotEqual(moved['source_body_sha256'],s['source_body_sha256'])
        self.yes(self.c.op('undo'));self.assertEqual(self.model(self.c.state),initial)
        self.yes(self.c.op('redo'));self.assertEqual(self.model(self.restart()),self.model(moved))
    def test_partial_group_and_interval_owner_refusal_are_atomic(self):
        s=self.c.state;r=self.c.op('reorder_text',group=s['paragraphs'][2]['paragraph'],before=s['paragraphs'][0]['id'])
        self.assertFalse(r['accepted']);self.assertEqual(r['error'],'reading_group_not_found');self.assertEqual(self.model(r['state']),self.model(s))
        # A valid synthetic owner source with a range across the two groups.
        folder=Path(self.tmp.name)/'anchored';old=legacy.Client(folder,profile='p2b');old.close()
        origin=folder/'origem-documento.json';v=json.loads(origin.read_text())
        document=v['document']
        document['anchors'].append({'id':str(uuid.uuid4()),'label':'Trecho que deve permanecer unido','start':0,'end':30})
        raw=json.dumps(v,ensure_ascii=False,separators=(',',':'),sort_keys=True)+'\n'
        # Owner snapshots must be canonical; use the fixture only if canonical
        # formatting roundtrips, via the owner's existing encoding on seed.
        with sqlite3.connect(folder/'obj01-piloto.sqlite3') as db:
            state=json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0]);state['source']=raw;state['source_sha256']=hashlib.sha256(raw.encode()).hexdigest();payload=json.dumps(state,ensure_ascii=False,separators=(',',':'))
            db.execute('UPDATE saved SET snapshot=?,sha256=?',(payload,hashlib.sha256(payload.encode()).hexdigest()))
        origin.write_text(raw)
        guarded=legacy.Client(Path(self.tmp.name)/'anchored-copy',profile='p2c',seed=folder)
        try:
            before=guarded.state
            denied=guarded.op('reorder_text',group=before['text_groups'][1]['id'],before=before['paragraphs'][0]['id'])
            self.assertFalse(denied['accepted']);self.assertEqual(denied['error'],'owner_text_move_refused');self.assertEqual(self.model(denied['state']),self.model(before))
        finally:guarded.close()
    def test_p14_context_keeps_target_and_publication_is_refused(self):
        t=self.table();self.context('text');s=self.yes(self.c.op('select_cell',id=t['id'],cell=t['cells'][0]['id']))
        for context in ['structure','composition','text']:
            now=self.context(context);self.assertEqual((now['selection'],now['selected_cell']),(s['selection'],s['selected_cell']))
        self.context('composition');r=self.c.op('edit_cell',id=t['id'],cell=t['cells'][0]['id'],text='fora do instrumento');self.assertEqual(r['error'],'editing_context_required')
        r=self.c.op('export_legacy');self.assertEqual(r['error'],'composite_publication_not_supported');self.assertNotIn('FOLHA PRIVADA',json.dumps(r,ensure_ascii=False))
    def test_p2b_seed_readonly_and_profiles_preserved(self):
        folder=Path(self.tmp.name)/'previous';b=legacy.Client(folder,profile='p2b');b.image();b.op('save')
        hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'}
        c=legacy.Client(Path(self.tmp.name)/'copy',profile='p2c',seed=folder)
        try:self.assertEqual(c.state['figures'],b.state['figures'])
        finally:c.close();b.close()
        self.assertEqual(hashes,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'})

if __name__=='__main__':unittest.main(verbosity=2)
