"""P2-B acceptance against the live C++ process and real auxiliary SQLite."""
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import test_pilot as legacy

BINARY=str(Path(sys.argv.pop(1)).resolve())
legacy.BINARY=BINARY


class Tables(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='obj01-p2b-test-')
        self.folder=Path(self.tmp.name)/'p2b'
        self.c=legacy.Client(self.folder,profile='p2b')
        self.origin=(self.folder/'origem-documento.json').read_bytes()

    def tearDown(self):
        self.c.close()
        self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin)
        self.tmp.cleanup()

    def yes(self,r):
        self.assertTrue(r['accepted'],r)
        return r['state']

    def no(self,fn,error):
        before=self.c.state
        r=fn()
        self.assertFalse(r['accepted'],r)
        self.assertEqual(r['error'],error)
        self.assertEqual(r['state'],before)

    def table(self,before=None):
        s=self.yes(self.c.op('create_table',before=before if before is not None else self.c.state['paragraphs'][1]['id'],title='Notas da travessia',header=True,cells=['Lugar','Imagem','Nota','Margem','Verde','Memória 🙂']))
        return s['tables'][-1]

    def edit(self,t,cell,value):
        return self.c.op('edit_cell',id=t['id'],cell=cell,text=value)

    def test_p06_table_and_cell_identities(self):
        t=self.table()
        ids=[t['id'],t['object'],*t['row_ids'],*t['column_ids'],*[c['id'] for c in t['cells']]]
        self.assertEqual(len(ids),len(set(ids)))
        self.assertEqual((len(t['row_ids']),len(t['column_ids']),len(t['cells'])),(2,3,6))
        self.assertEqual(len({(c['row'],c['column']) for c in t['cells']}),6)
        old=t['cells']
        s=self.yes(self.edit(t,old[4]['id'],'Verde & água 漢🙂'))
        self.assertEqual(s['tables'][0]['payload_revision'],1)
        for a,b in zip(old,s['tables'][0]['cells']):
            self.assertEqual((a['id'],a['row'],a['column']),(b['id'],b['row'],b['column']))
            self.assertEqual(b['text'],'Verde & água 漢🙂' if a['id']==old[4]['id'] else a['text'])

    def test_p07_cell_selection_is_typed_and_does_not_invalidate_redo(self):
        t=self.table();cell=t['cells'][2]['id']
        s=self.yes(self.c.op('select_cell',id=t['id'],cell=cell))
        self.assertEqual((s['selection'],s['selected_cell']),(t['id'],cell))
        s=self.yes(self.edit(t,cell,'Nova nota'))
        self.yes(self.c.op('undo'))
        s=self.yes(self.c.op('select_cell',id=t['id'],cell=t['cells'][0]['id']))
        self.assertTrue(s['can_redo'])
        s=self.yes(self.c.op('redo'))
        self.assertEqual(s['selected_cell'],cell)
        self.assertEqual(s['tables'][0]['cells'][2]['text'],'Nova nota')
        self.no(lambda:self.c.op('select_cell',id=t['id'],cell='not-a-cell'),'cell_not_found')

    def test_p08_move_remove_undo_redo_conserves_table_and_selection(self):
        t=self.table();cell=t['cells'][3]['id']
        self.yes(self.edit(t,cell,'Margem revisada'))
        before=self.c.state
        moved=self.yes(self.c.op('move',id=t['id'],before='',intent='authorial'))
        self.assertEqual(moved['author_order'][-1],t['id'])
        self.assertEqual(moved['tables'],before['tables'])
        self.assertEqual(moved['selected_cell'],cell)
        removed=self.yes(self.c.op('remove',id=t['id']))
        self.assertEqual(removed['tables'],[])
        self.assertEqual(removed['retained_tables'],1)
        self.assertEqual(removed['selection'],'')
        restored=self.yes(self.c.op('undo'))
        for key in ('tables','selection','selected_cell','author_order','visual_order'):
            self.assertEqual(restored[key],moved[key])
        self.yes(self.c.op('redo'))
        self.assertEqual(self.c.state['tables'],[])
        self.yes(self.c.op('undo'))
        self.yes(self.c.op('undo'))
        self.assertEqual(self.c.state['author_order'],before['author_order'])

    def test_p08_last_figure_resource_survives_remove_save_restart(self):
        s=self.yes(self.c.image());f=s['figures'][0]
        self.yes(self.c.op('arrange',id=f['id'],paragraph=s['paragraphs'][1]['id'],first=.4,second=.6,gap=20))
        row=self.c.state['rows']
        self.yes(self.c.op('remove',id=f['id']))
        self.assertEqual(self.c.state['rows'],[])
        self.yes(self.c.op('save'))
        self.yes(self.c.op('undo'))
        self.assertEqual(self.c.state['rows'],row)
        self.assertEqual(self.c.state['figures'][0],f)
        self.yes(self.c.op('redo'))
        self.yes(self.c.op('save'))
        self.c.close();self.c=legacy.Client(self.folder,profile='p2b')
        self.assertEqual(self.c.state['retained_figures'],1)
        self.assertFalse(self.c.state['can_undo'])
        with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:
            self.assertEqual(hashlib.sha256(db.execute('SELECT data FROM blobs').fetchone()[0]).hexdigest(),f['sha256'])

    def test_undo_import_after_save_keeps_resource_and_can_save_again(self):
        s=self.yes(self.c.image());image=s['figures'][0]
        self.yes(self.c.op('save'))
        self.yes(self.c.op('undo'))
        self.assertEqual(self.c.state['figures'],[])
        self.assertEqual(self.c.state['retained_figures'],1)
        self.yes(self.c.op('save'))
        restored=self.yes(self.c.op('redo'))
        self.assertEqual(restored['figures'][0],image)
        self.yes(self.c.op('save'))

    def test_p09_tables_images_profiles_new_process_roundtrip(self):
        s=self.yes(self.c.image());f=s['figures'][0]
        self.yes(self.c.op('arrange',id=f['id'],paragraph=s['paragraphs'][1]['id'],first=.3,second=.7,gap=18))
        t=self.table(before=f['id'])
        self.yes(self.edit(t,t['cells'][5]['id'],'Texto\ncom Unicode 漢🙂'))
        saved=self.yes(self.c.op('save'))
        self.assertTrue(saved['can_undo'])
        self.c.close();self.c=legacy.Client(self.folder,profile='p2b')
        for key in ('tables','figures','rows','author_order','visual_order','selection','selected_cell','source_sha256'):
            self.assertEqual(self.c.state[key],saved[key],key)
        self.assertFalse(self.c.state['can_undo'])
        self.assertFalse(self.c.state['can_redo'])
        with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:
            self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0],2)
            self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0],'ok')

    def test_new_edit_after_undo_clears_redo(self):
        t=self.table();cell=t['cells'][0]['id']
        self.yes(self.edit(t,cell,'Primeira'))
        self.yes(self.c.op('undo'))
        self.assertTrue(self.c.state['can_redo'])
        self.yes(self.edit(t,cell,'Outra escolha'))
        self.assertFalse(self.c.state['can_redo'])
        self.no(lambda:self.c.op('redo'),'redo_empty')

    def test_history_is_bounded_and_revision_monotonic(self):
        t=self.table();cell=t['cells'][0]['id']
        for i in range(40):self.yes(self.edit(t,cell,'Valor '+str(i)))
        self.assertEqual(self.c.state['history_entries'],32)
        revision=self.c.state['revision']
        for _ in range(32):
            s=self.yes(self.c.op('undo'))
            self.assertGreater(s['revision'],revision);revision=s['revision']
        self.no(lambda:self.c.op('undo'),'undo_empty')
        for _ in range(32):self.yes(self.c.op('redo'))
        self.assertEqual(self.c.state['tables'][0]['cells'][0]['text'],'Valor 39')

    def test_limits_and_group_refusals_are_atomic(self):
        t=self.table();cell=t['cells'][0]['id']
        self.yes(self.edit(t,cell,'🙂'*4096))
        self.no(lambda:self.edit(t,cell,'a'*4097),'cell_text_limit')
        self.no(lambda:self.edit(t,cell,'\x00'),'invalid_text_or_limit')
        self.no(lambda:self.c.op('move',id=t['id'],before=self.c.state['paragraphs'][2]['id'],intent='authorial'),'reading_group_split')
        self.no(lambda:self.c.op('remove',id=self.c.state['paragraphs'][0]['id']),'source_removal_out_of_scope')
        self.no(lambda:self.c.send({'op':'edit_cell','expected':self.c.state['revision']-1,'id':t['id'],'cell':cell,'text':'stale'}),'stale_session_revision')

    def test_p2a_seed_is_readonly_and_keeps_ids_and_bytes(self):
        source=Path(self.tmp.name)/'p2a'
        p2a=legacy.Client(source)
        try:
            original=self.yes(p2a.image())
            self.yes(p2a.op('save'))
            before={p.name:p.read_bytes() for p in source.iterdir() if p.name!='.pilot.lock'}
            target=Path(self.tmp.name)/'copy'
            copy=legacy.Client(target,profile='p2b',seed=source)
            try:
                for key in ('paragraphs','figures','author_order','source_sha256'):
                    self.assertEqual(copy.state[key],original[key])
            finally:copy.close()
            self.assertEqual(before,{p.name:p.read_bytes() for p in source.iterdir() if p.name!='.pilot.lock'})
            self.assertEqual(p2a.send({'op':'view'})['state']['profile'],'P2-A')
        finally:p2a.close()

    def test_profiles_do_not_implicitly_upgrade_each_other(self):
        result=subprocess.run([BINARY,'--workspace',str(self.folder),'--profile','p2a'],input='',text=True,capture_output=True,timeout=15)
        self.assertIn('workspace_already_open',result.stderr)
        self.c.close()
        result=subprocess.run([BINARY,'--workspace',str(self.folder),'--profile','p2a'],input='',text=True,capture_output=True,timeout=15)
        self.assertEqual(result.returncode,1)
        self.assertIn('unsupported_database_version',result.stderr)
        self.c=legacy.Client(self.folder,profile='p2b')

    def test_invalid_persisted_table_rejected_without_replacing_work(self):
        self.table();self.yes(self.c.op('save'))
        with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:
            obj=json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])
            obj['tables'][0]['cells'][1]['id']=obj['tables'][0]['cells'][0]['id']
            raw=json.dumps(obj,ensure_ascii=False)
            db.execute('UPDATE saved SET snapshot=?,sha256=?',(raw,hashlib.sha256(raw.encode()).hexdigest()))
        self.no(lambda:self.c.op('reopen',discard_unsaved=False),'table_identity_collision')


if __name__=='__main__':unittest.main(verbosity=2)
