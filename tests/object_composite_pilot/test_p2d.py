"""Real formula objects: common document/cell graph, state, recovery and failures."""
import hashlib
import json
from pathlib import Path
import random
import signal
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import test_pilot as base

base.BINARY=str(Path(sys.argv.pop(1)).resolve())
FAULT=str(Path(sys.argv.pop(1)).resolve())
SOURCE='declara HP = 100\ndeclara DanoEspada = 10\nresultado HP - 2 × DanoEspada'

class Formulas(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='obj01-p2d-')
        self.folder=Path(self.tmp.name)/'work'
        self.c=base.Client(self.folder,profile='p2d')
        self.origin=(self.folder/'origem-documento.json').read_bytes()
    def tearDown(self):
        self.c.close();self.assertEqual(self.origin,(self.folder/'origem-documento.json').read_bytes());self.tmp.cleanup()
    def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
    def ctx(self,context):return self.yes(self.c.op('set_context',context=context))
    def new(self,name='Combate',source=SOURCE,**target):
        self.ctx('structure');s=self.yes(self.c.op('create_formula',name=name,source=source,before='',table='',cell='',**target))
        return next(f for f in s['formulas'] if f['name']==name)
    def formula(self,name='Combate'):return next(f for f in self.c.state['formulas'] if f['name']==name)
    def edit(self,f,source):
        self.ctx('text');self.yes(self.c.op('edit_formula',id=f['id'],source=source));return self.formula(f['name'])
    def restart(self):
        self.c.close();self.c=base.Client(self.folder,profile='p2d');return self.c.state
    def table(self):
        self.ctx('structure');return self.yes(self.c.op('create_table',before='',title='Planilha',header=True,cells=['A','B','C','1','2','3']))['tables'][-1]
    def test_multiline_user_example_and_excel_style_expression(self):
        f=self.new();self.assertTrue(f['valid']);self.assertEqual(f['display'],'80')
        f=self.edit(f,'{\ndeclara HP = 100\ndeclara DanoEspada = 10\nresultado final calcula HP - 2 × DanoEspada\n}')
        self.assertEqual(f['display'],'80')
        self.assertEqual(self.edit(f,'= SOMA(10; 20; 30) / 2')['display'],'30')
    def test_cross_formula_variables_results_and_recalculation(self):
        a=self.new();self.new('Metade','resultado Combate.resultado / 2');self.new('Reserva','resultado Combate.HP + 50')
        self.assertEqual(self.formula('Metade')['display'],'40');self.assertEqual(self.formula('Reserva')['display'],'150')
        self.edit(a,SOURCE.replace('DanoEspada = 10','DanoEspada = 15'))
        self.assertEqual(self.formula()['display'],'70');self.assertEqual(self.formula('Metade')['display'],'35')
    def test_formula_born_in_cell_drives_document_and_reverse(self):
        t=self.table();cell=t['cells'][0]['id']
        self.ctx('structure');s=self.yes(self.c.op('create_formula',name='Celula',source='= 12',before='',table=t['id'],cell=cell))
        f=next(f for f in s['formulas'] if f['name']=='Celula');self.assertEqual(s['formula_cells'][cell],f['id']);self.assertEqual(s['formula_uses'],[])
        doc=self.new('Texto','resultado Celula.resultado * 3');self.assertEqual(doc['display'],'36')
        self.ctx('structure');other=t['cells'][1]['id'];self.yes(self.c.op('create_formula',name='OutraCelula',source='resultado Texto.resultado + 4',before='',table=t['id'],cell=other))
        self.assertEqual(self.formula('OutraCelula')['display'],'40')
        self.edit(f,'= 20');self.assertEqual(self.formula('Texto')['display'],'60');self.assertEqual(self.formula('OutraCelula')['display'],'64')
    def test_explicit_assignment_updates_chain_once_and_keeps_initial(self):
        self.new();self.new('Metade','resultado Combate.resultado / 2')
        action=self.new('Golpe','alterar Combate.HP = Combate.HP - 20\nresultado Combate.HP')
        self.assertEqual(self.formula()['display'],'80');self.assertEqual(action['display'],'100')
        self.ctx('text');s=self.yes(self.c.op('execute_formula',id=action['id']))
        self.assertEqual(self.formula()['display'],'60');self.assertEqual(self.formula('Metade')['display'],'30')
        hp=next(v for v in self.formula()['variables'] if v['name']=='HP');self.assertEqual((hp['initial'],hp['current'],hp['overridden']),(100,80,True))
        self.c.send({'op':'view'});self.restart();self.assertEqual(self.formula()['display'],'60');self.assertEqual(self.formula('Golpe')['display'],'80')
        self.assertEqual(self.c.state['formulas'],s['formulas'])
    def test_simultaneous_assignments_undo_redo_and_reset(self):
        f=self.new();action=self.new('Troca','alterar Combate.HP = Combate.DanoEspada\nalterar Combate.DanoEspada = Combate.HP\nresultado Combate.HP')
        self.ctx('text');self.yes(self.c.op('execute_formula',id=action['id']));self.assertEqual(self.formula()['display'],'-190')
        self.yes(self.c.op('undo'));self.assertEqual(self.formula()['display'],'80')
        self.yes(self.c.op('redo'));self.assertEqual(self.formula()['display'],'-190')
        self.yes(self.c.op('reset_formula_variable',id=f['id'],variable='HP'));self.assertEqual(self.formula()['display'],'-100')
    def test_command_cannot_break_dependency_or_partially_write(self):
        f=self.new('Estado','declara X = 2\nresultado X');self.new('Inverso','resultado 1 / Estado.X')
        action=self.new('Zerar','alterar Estado.X = 0\nresultado Estado.X');self.ctx('text');before=self.c.state
        r=self.c.op('execute_formula',id=action['id']);self.assertFalse(r['accepted']);self.assertEqual(r['error'],'formula_execution_would_break_dependency');self.assertEqual(r['state'],before)
        self.assertEqual(self.formula('Estado')['overrides'],{})
    def test_duplicate_assignment_and_missing_targets_are_atomic(self):
        self.new();a=self.new('Duplo','alterar Combate.HP = 80\nalterar Combate.HP = 60\nresultado 0');self.ctx('text')
        self.assertEqual(self.c.op('execute_formula',id=a['id'])['error'],'formula_duplicate_assignment')
        a=self.edit(a,'alterar Combate.Inexistente = 1\nresultado 0');before=self.c.state
        self.assertEqual(self.c.op('execute_formula',id=a['id'])['error'],'formula_variable_missing');self.assertEqual(self.c.state,before)
    def test_cycles_and_forward_references_report_and_repair(self):
        a=self.new('A','resultado B.resultado + 1');self.assertEqual(a['error'],'formula_reference_missing')
        b=self.new('B','resultado A.resultado + 1');self.assertEqual(self.formula('A')['error'],'formula_cycle');self.assertEqual(b['error'],'formula_cycle')
        self.edit(b,'resultado 10');self.assertEqual(self.formula('A')['display'],'11')
        self.assertEqual(self.restart()['formulas'],self.c.state['formulas'])
    def test_invalid_draft_is_saved_recovered_and_repairable(self):
        f=self.new();f=self.edit(f,'declara HP =\nresultado HP');self.assertFalse(f['valid']);self.assertEqual(f['source'],'declara HP =\nresultado HP')
        self.yes(self.c.op('save'));self.restart();self.assertFalse(self.formula()['valid'])
        self.assertEqual(self.edit(self.formula(),SOURCE)['display'],'80')
    def test_syntax_functions_number_and_depth_diagnostics(self):
        f=self.new()
        for source,error in [('= 1/0','formula_division_zero'),('= 1e999','formula_number'),('= 1e12*2','formula_number_limit'),('= INEXISTENTE(1)','formula_function'),('= SE(1; 7; 1/0)',None),('= SE(0; 1/0; 8)',None),('= '+'('*60+'1'+')'*60,'formula_depth')]:
            with self.subTest(source=source):
                out=self.edit(f,source);self.assertEqual(out['error'],error or '');self.assertEqual(out['valid'],error is None)
        self.assertEqual(self.edit(f,'= MEDIA(10;20;30) + MAX(1;2) - MIN(4;5) + ABS(-2)')['display'],'20')
    def test_generated_arithmetic_matches_independent_values(self):
        f=self.new();rng=random.Random(713)
        for _ in range(40):
            a,b,c=[rng.randint(-50,50) for _ in range(3)];out=self.edit(f,f'= ({a} + {b}) * {c}')
            self.assertEqual(float(out['display']),(a+b)*c)
    def test_bound_cell_text_protected_and_detach_undo_keeps_plain_text(self):
        t=self.table();cell=t['cells'][0]['id'];self.ctx('structure')
        s=self.yes(self.c.op('create_formula',name='NaCelula',source='= 42',before='',table=t['id'],cell=cell))
        self.ctx('text');before=self.c.state;r=self.c.op('edit_cell',id=t['id'],cell=cell,text='Apagar?');self.assertEqual(r['error'],'formula_cell_requires_editor');self.assertEqual(r['state'],before)
        self.ctx('structure');s=self.yes(self.c.op('detach_formula',table=t['id'],cell=cell));self.assertNotIn(cell,s['formula_cells']);self.assertEqual(s['tables'][0]['cells'][0]['text'],'A')
        self.yes(self.c.op('undo'));self.assertIn(cell,self.c.state['formula_cells'])
    def test_move_remove_undo_keeps_formula_identity_and_dependents(self):
        f=self.new();self.new('Uso','resultado Combate.resultado');self.ctx('structure');use=self.c.state['formula_uses'][0]
        s=self.yes(self.c.op('move',id=use['id'],before=safe_first(self.c.state),intent='authorial'));self.assertEqual(self.formula()['id'],f['id'])
        self.yes(self.c.op('remove',id=use['id']));self.assertEqual(self.formula('Uso')['display'],'80')
        self.yes(self.c.op('undo'));self.assertIn(use,self.c.state['formula_uses'])
    def test_shared_formula_document_and_cell_use(self):
        f=self.new();t=self.table();cell=t['cells'][1]['id'];self.ctx('structure')
        self.yes(self.c.op('attach_formula',id=f['id'],before='',table=t['id'],cell=cell));self.edit(f,'resultado 123')
        self.assertEqual(self.formula()['display'],'123');self.assertEqual(self.c.state['formula_cells'][cell],f['id'])
    def test_sigkill_recovers_formula_state_bindings_cells_and_results(self):
        self.new();action=self.new('Golpe','alterar Combate.HP = Combate.HP - 20\nresultado Combate.HP');t=self.table();self.ctx('structure')
        self.yes(self.c.op('create_formula',name='Planilha',source='resultado Combate.resultado',before='',table=t['id'],cell=t['cells'][0]['id']))
        self.ctx('text');before=self.yes(self.c.op('execute_formula',id=action['id']))
        self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d')
        self.assertEqual(self.c.state['formulas'],before['formulas']);self.assertEqual(self.c.state['formula_cells'],before['formula_cells']);self.assertTrue(self.c.state['recovered_on_open']);self.assertFalse(self.c.state['can_undo']);self.assertEqual(self.c.state['saved_revision'],0)
    def test_checkpoint_faults_preserve_atomic_formula_cascade(self):
        self.new();a=self.new('Golpe','alterar Combate.HP = Combate.HP - 20\nresultado Combate.HP');self.ctx('text');self.c.close()
        self.c=base.Client(self.folder,profile='p2d',binary=FAULT,fault='checkpoint:before_commit:throw');before=self.c.state['formulas'];r=self.c.op('execute_formula',id=a['id']);self.assertFalse(r['accepted']);self.assertEqual(r['state']['formulas'],before)
        self.c.close();self.c=base.Client(self.folder,profile='p2d',binary=FAULT,fault='checkpoint:after_commit:throw');r=self.c.op('execute_formula',id=a['id']);self.assertEqual(r['error'],'persistence_outcome_uncertain')
        self.yes(self.c.op('reopen_working',discard_unsaved=True));self.assertEqual(self.formula()['display'],'60')
    def test_names_limits_context_revision_and_publication_guards(self):
        f=self.new();self.ctx('text');before=self.c.state
        self.assertEqual(self.c.op('create_formula',name='Outro',source='= 1',before='',table='',cell='')['error'],'editing_context_required')
        self.assertEqual(self.c.send({'op':'edit_formula','id':f['id'],'source':'= 1','expected':0})['error'],'stale_session_revision')
        self.assertEqual(self.c.op('export_legacy')['error'],'composite_publication_not_supported');self.assertEqual(self.c.state,before)
        self.ctx('structure');before=self.c.state
        for name,source,error in [('Combate','= 1','formula_identity'),('Com espaço','= 1','formula_identity'),('Longa','x'*16385,'formula_source_limit')]:
            r=self.c.op('create_formula',name=name,source=source,before='',table='',cell='');self.assertEqual(r['error'],error);self.assertEqual(self.c.state,before)
    def test_corrupt_binding_identity_fails_closed_without_rebinding(self):
        self.new('A','= 10');self.new('B','= 20');self.new('C','resultado A.resultado');self.c.close()
        path=self.folder/'obj01-piloto.sqlite3'
        with sqlite3.connect(path) as db:
            raw,digest=db.execute('SELECT snapshot,sha256 FROM working').fetchone();snap=json.loads(raw)
            snap['formulas'][2]['bindings']['A']=snap['formulas'][1]['id']
            changed=json.dumps(snap,ensure_ascii=False,separators=(',',':'))
            db.execute('UPDATE working SET snapshot=?,sha256=?',(changed,hashlib.sha256(changed.encode()).hexdigest()))
        r=subprocess.run([base.BINARY,'--workspace',str(self.folder),'--profile','p2d'],input='',capture_output=True,text=True,timeout=15)
        self.assertEqual(r.returncode,1);self.assertIn('formula_binding_identity',r.stderr)
        with sqlite3.connect(path) as db:db.execute('UPDATE working SET snapshot=?,sha256=?',(raw,digest))
        self.c=base.Client(self.folder,profile='p2d');self.assertEqual(self.formula('C')['display'],'10')
    def test_p2c_seed_preserves_reordered_owner_and_prior_files(self):
        folder=Path(self.tmp.name)/'c';c=base.Client(folder,profile='p2c')
        s=c.state;c.op('reorder_text',group=s['text_groups'][1]['id'],before=s['paragraphs'][0]['id']);c.op('save');expected=c.state['paragraphs'];c.close()
        hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'}
        d=base.Client(Path(self.tmp.name)/'copy',profile='p2d',seed=folder)
        try:self.assertEqual(d.state['paragraphs'],expected);self.assertEqual(d.state['formulas'],[])
        finally:d.close()
        self.assertEqual(hashes,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'})

def safe_first(state):return state['author_order'][0]
if __name__=='__main__':unittest.main(verbosity=2)
