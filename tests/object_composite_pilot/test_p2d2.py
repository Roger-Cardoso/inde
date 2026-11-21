"""Text formulas, materialized state and consumed commands at real process boundary."""
import hashlib
import json
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import test_pilot as base

base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
SOURCE='declara HP = 100\ndeclara DanoEspada = 10\nresultado HP - 2 × DanoEspada'
def text(s):return {'kind':'text','text':s}
def result(f):return {'kind':'result','formula':f['id']}

class RichFormulas(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='obj01-d2-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes()
    def tearDown(self):self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
    def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
    def ctx(self,v):return self.yes(self.c.op('set_context',context=v))
    def new(self,name='Combate',source=SOURCE,prefix='',suffix='',**where):
        self.ctx('structure');v={'name':name,'source':source,'before':'','table':'','cell':'','prefix':prefix,'suffix':suffix};v.update(where)
        self.yes(self.c.op('create_formula',**v));return self.f(name)
    def f(self,name='Combate'):return next(f for f in self.c.state['formulas'] if f['name']==name)
    def edit(self,f,source):self.ctx('text');self.yes(self.c.op('edit_formula',id=f['id'],source=source));return self.f(f['name'])
    def use(self,f):return next(u for u in self.c.state['formula_uses'] if u['object']==f['id'])['id']
    def runs(self,key):return self.c.state['formula_text'][key]['runs']
    def resolve(self,key):
        fs={f['id']:f for f in self.c.state['formulas']};return ''.join(r['text'] if r['kind']=='text' else fs[r['formula']]['display'] for r in self.runs(key))
    def restart(self):self.c.close();self.c=base.Client(self.folder,profile='p2d2');return self.c.state
    def table(self):self.ctx('structure');return self.yes(self.c.op('create_table',before='',title='Células',header=False,cells=['A','B','C','1','2','3']))['tables'][-1]
    def test_local_current_assignment_and_reset_are_real_source(self):
        f=self.new(source='declara HP = 100\nHP = 80\nresultado HP');self.assertEqual(f['display'],'80');self.assertEqual(f['overrides'],{})
        hp=f['variables'][0];self.assertEqual((hp['initial_display'],hp['current_display']),('100','80'))
        self.ctx('text');self.yes(self.c.op('reset_formula_variable',id=f['id'],variable='HP'));self.assertEqual(self.f()['display'],'100');self.assertNotIn('HP = 80',self.f()['source'])
        self.yes(self.c.op('undo'));self.assertEqual(self.f()['source'],f['source'])
    def test_command_writes_fixed_value_and_removes_own_occurrence(self):
        f=self.new();g=self.new('Golpe','alterar Combate.HP = Combate.HP - 20');key=self.use(g);self.ctx('text')
        s=self.yes(self.c.op('execute_formula',id=g['id']));self.assertEqual(self.f()['display'],'60')
        self.assertIn('declara HP = 100\nHP = 80\n',self.f()['source']);self.assertEqual(self.f()['overrides'],{})
        self.assertNotIn(key,s['author_order']);self.assertNotIn(key,s['visual_order']);self.assertFalse(any(u['id']==key for u in s['formula_uses']));self.assertTrue(self.f('Golpe')['consumed'])
        self.assertEqual(self.c.op('execute_formula',id=g['id'])['error'],'formula_command_consumed')
        after=self.restart();self.assertEqual(self.f()['display'],'60');self.assertTrue(self.f('Golpe')['consumed']);self.assertNotIn(key,after['author_order'])
    def test_undo_redo_restores_command_source_state_and_phrase_together(self):
        f=self.new(prefix='Restam ',suffix=' pontos.');target=self.use(f);g=self.new('Golpe','alterar Combate.HP = 80');command=self.use(g);self.ctx('text')
        self.yes(self.c.op('execute_formula',id=g['id']));self.assertEqual(self.resolve(target),'Restam 60 pontos.')
        self.yes(self.c.op('undo'));self.assertEqual(self.f()['source'],SOURCE);self.assertIn(command,self.c.state['author_order']);self.assertFalse(self.f('Golpe')['consumed']);self.assertEqual(self.resolve(target),'Restam 80 pontos.')
        self.yes(self.c.op('redo'));self.assertEqual(self.resolve(target),'Restam 60 pontos.');self.assertNotIn(command,self.c.state['author_order'])
    def test_repeated_commands_replace_current_line_and_keep_comments(self):
        f=self.new(source='{\ndeclara HP = 100 # inicial autoral 漢\nHP = 90 # atual autoral\nresultado HP\n}')
        for name in ['Um','Dois']:
            g=self.new(name,'alterar Combate.HP = Combate.HP - 10');self.ctx('text');self.yes(self.c.op('execute_formula',id=g['id']))
        self.assertEqual(self.f()['display'],'70');self.assertEqual(self.f()['source'].count('HP = 70'),1);self.assertNotIn('HP = 90',self.f()['source']);self.assertIn('# atual autoral',self.f()['source']);self.assertIn('# inicial autoral 漢',self.f()['source'])
    def test_mixed_or_multiple_commands_are_refused_atomically(self):
        self.new();self.ctx('structure');before=self.c.state
        for source in ['alterar Combate.HP = 80\nresultado 80','declara X = 1\nalterar Combate.HP = 80','alterar Combate.HP = 80\nalterar Combate.HP = 60']:
            r=self.c.op('create_formula',name='Ruim',source=source,before='',table='',cell='',prefix='',suffix='');self.assertFalse(r['accepted']);self.assertEqual(r['error'],'formula_command_must_be_alone');self.assertEqual(self.c.state,before)
    def test_command_with_authored_phrase_is_refused(self):
        self.new();self.ctx('structure');r=self.c.op('create_formula',name='Ruim',source='alterar Combate.HP = 80',before='',table='',cell='',prefix='Execute ',suffix=' agora')
        self.assertEqual(r['error'],'formula_command_must_be_alone')
    def test_failed_command_keeps_itself_and_all_values(self):
        self.new('Estado','declara X = 2\nresultado X');self.new('Inverso','resultado 1 / Estado.X');g=self.new('Zerar','alterar Estado.X = 0');key=self.use(g);self.ctx('text');before=self.c.state
        r=self.c.op('execute_formula',id=g['id']);self.assertEqual(r['error'],'formula_execution_would_break_dependency');self.assertEqual(self.c.state,before);self.assertIn(key,self.c.state['author_order'])
    def test_conditional_text_and_aliases_are_lazy_and_typed(self):
        f=self.new(source='declara HP = 80\nresultado SE(HP <= 0; "caído"; SE(HP < 30; "ferido"; "de pé"))');self.assertEqual(f['display'],'de pé');self.assertEqual(f['type'],'text')
        for expr,want in [('IF(AND(1 < 2; NOT(false)); "vivo"; 1/0)','vivo'),('SE(OU(verdadeiro; 1/0); "sim"; "não")','sim'),('Se(E(0; 1/0); "sim"; "não")','não'),('SE(NÃO(falso); "verdadeiro"; "falso")','verdadeiro'),('SE(não(0); "ok"; "x")','ok'),('"HP: " & 80','HP: 80'),('""',''),('"declara"','declara'),('"}"','}'),('"a\\n\\\"b\\\""','a\n"b"')]:
            with self.subTest(expr=expr):self.assertEqual(self.edit(f,'resultado '+expr)['display'],want)
        self.assertEqual(self.edit(f,'resultado "x" + 1')['error'],'formula_type')
    def test_text_current_assignment_is_fixed_and_consumed(self):
        f=self.new('Pessoa','declara Estado = "vivo"\nresultado Estado');g=self.new('Mudar','alterar Pessoa.Estado = "ferido"');self.ctx('text');self.yes(self.c.op('execute_formula',id=g['id']))
        self.assertIn('Estado = "ferido"',self.f('Pessoa')['source']);self.assertEqual(self.f('Pessoa')['display'],'ferido');self.assertEqual(self.f('Pessoa')['variables'][0]['current'],'ferido');self.restart();self.assertEqual(self.f('Pessoa')['display'],'ferido')
    def test_authored_text_survives_recalculation_and_multiple_values(self):
        f=self.new(prefix='Combate resultou em ',suffix=' pontos de Dano.');key=self.use(f);g=self.new('Metade','resultado Combate.resultado / 2')
        self.ctx('text');runs=[text('Após dois ataques: '),result(f),text(' pontos; metade: '),result(g),text('.')];self.yes(self.c.op('edit_formula_text',target=key,runs=runs))
        self.assertEqual(self.resolve(key),'Após dois ataques: 80 pontos; metade: 40.')
        self.edit(f,SOURCE.replace('DanoEspada = 10','DanoEspada = 15'));self.assertEqual(self.resolve(key),'Após dois ataques: 70 pontos; metade: 35.');self.assertEqual(self.runs(key),runs)
        self.yes(self.c.op('save'));self.restart();self.assertEqual(self.resolve(key),'Após dois ataques: 70 pontos; metade: 35.')
    def test_each_shared_use_keeps_its_own_literal_text(self):
        f=self.new(prefix='No texto: ');key=self.use(f);t=self.table();cell=t['cells'][0]['id'];self.ctx('structure');self.yes(self.c.op('attach_formula',id=f['id'],before='',table=t['id'],cell=cell))
        self.ctx('text');self.yes(self.c.op('edit_formula_text',target=cell,runs=[text('Na célula: '),result(f)]));self.edit(f,'resultado 42')
        self.assertEqual(self.resolve(key),'No texto: 42');self.assertEqual(self.resolve(cell),'Na célula: 42')
    def test_cell_command_is_consumed_restoring_plain_cell_text(self):
        self.new();t=self.table();cell=t['cells'][0]['id'];g=self.new('Golpe','alterar Combate.HP = 80',table=t['id'],cell=cell)
        self.ctx('text');self.yes(self.c.op('execute_formula',id=g['id']));self.assertNotIn(cell,self.c.state['formula_cells']);self.assertEqual(self.c.state['tables'][0]['cells'][0]['text'],'A')
        self.yes(self.c.op('undo'));self.assertEqual(self.c.state['formula_cells'][cell],g['id'])
    def test_literal_admission_identity_and_context_guards(self):
        f=self.new();key=self.use(f);self.ctx('text');before=self.c.state
        for runs,code in [([text('Sem resultado')],'formula_text_owner_result_missing'),([result(f),text('a'*4097)],'formula_literal_limit'),([result(f),{'kind':'result','formula':'inexistente'}],'formula_text_reference_invalid')]:
            r=self.c.op('edit_formula_text',target=key,runs=runs);self.assertEqual(r['error'],code);self.assertEqual(self.c.state,before)
        self.ctx('structure');self.assertEqual(self.c.op('edit_formula_text',target=key,runs=[result(f)])['error'],'editing_context_required')
    def test_commands_cannot_be_shared_or_interpolated(self):
        f=self.new();g=self.new('Golpe','alterar Combate.HP = 80');t=self.table();cell=t['cells'][0]['id'];self.ctx('structure');before=self.c.state
        r=self.c.op('attach_formula',id=g['id'],before='',table=t['id'],cell=cell);self.assertEqual(r['error'],'formula_command_must_be_alone');self.assertEqual(self.c.state,before)
        self.ctx('text');r=self.c.op('edit_formula_text',target=self.use(f),runs=[result(f),result(g)]);self.assertEqual(r['error'],'formula_text_reference_invalid')
    def test_sigkill_recovers_fixed_source_text_and_consumption(self):
        f=self.new(prefix='Restam ',suffix=' pontos.');key=self.use(f);g=self.new('Golpe','alterar Combate.HP = 80');self.ctx('text');before=self.yes(self.c.op('execute_formula',id=g['id']))
        self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2');after=self.c.state
        for field in ['formulas','formula_text','formula_uses','formula_cells','author_order','visual_order']:self.assertEqual(after[field],before[field])
        self.assertTrue(after['recovered_on_open']);self.assertFalse(after['can_undo']);self.assertEqual(self.resolve(key),'Restam 60 pontos.')
    def test_checkpoint_failure_and_lost_ack_do_not_half_consume(self):
        self.new();g=self.new('Golpe','alterar Combate.HP = 80');key=self.use(g);self.ctx('text');self.c.close()
        self.c=base.Client(self.folder,profile='p2d2',binary=FAULT,fault='checkpoint:before_commit:throw');before=self.c.state
        r=self.c.op('execute_formula',id=g['id']);self.assertFalse(r['accepted']);self.assertEqual(r['state']['formulas'],before['formulas']);self.assertIn(key,r['state']['author_order'])
        self.c.close();self.c=base.Client(self.folder,profile='p2d2',binary=FAULT,fault='checkpoint:after_commit:throw');r=self.c.op('execute_formula',id=g['id']);self.assertEqual(r['error'],'persistence_outcome_uncertain')
        self.yes(self.c.op('reopen_working',discard_unsaved=True));self.assertTrue(self.f('Golpe')['consumed']);self.assertNotIn(key,self.c.state['author_order']);self.assertIn('HP = 80',self.f()['source'])
    def test_real_crash_windows_keep_source_and_consumption_atomic(self):
        for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
            with self.subTest(stage=stage):
                folder=Path(self.tmp.name)/stage;c=base.Client(folder,profile='p2d2')
                for name,source in [('Combate',SOURCE),('Golpe','alterar Combate.HP = 80')]:c.op('create_formula',name=name,source=source,before='',table='',cell='',prefix='',suffix='')
                command=c.state['formulas'][1]['id'];key=c.state['formula_uses'][1]['id'];c.op('set_context',context='text');c.close()
                c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash')
                with self.assertRaises(AssertionError):c.op('execute_formula',id=command)
                c.close(expected=86);c=base.Client(folder,profile='p2d2')
                try:
                    committed=stage=='after_commit';self.assertEqual(c.state['formulas'][1]['consumed'],committed);self.assertEqual(key not in c.state['author_order'],committed);self.assertEqual('HP = 80' in c.state['formulas'][0]['source'],committed)
                finally:c.close()
    def test_unknown_language_and_hidden_override_refuse_startup(self):
        self.new();self.c.close();dbpath=self.folder/'obj01-piloto.sqlite3'
        with sqlite3.connect(dbpath) as db:
            raw,digest=db.execute('SELECT snapshot,sha256 FROM working').fetchone();v=json.loads(raw);v['formulas'][0]['overrides']={'HP':80};changed=json.dumps(v,ensure_ascii=False,separators=(',',':'));db.execute('UPDATE working SET snapshot=?,sha256=?',(changed,hashlib.sha256(changed.encode()).hexdigest()))
        r=subprocess.run([base.BINARY,'--workspace',str(self.folder),'--profile','p2d2'],input='',text=True,capture_output=True,timeout=15);self.assertEqual(r.returncode,1);self.assertIn('formula_current_must_be_in_source',r.stderr)
        with sqlite3.connect(dbpath) as db:db.execute('UPDATE working SET snapshot=?,sha256=?',(raw,digest))
        self.c=base.Client(self.folder,profile='p2d2')
    def test_numeric_string_depth_and_cycle_diagnostics_are_bounded(self):
        f=self.new(source='resultado 0')
        for source,code in [('resultado 1/0','formula_division_zero'),('declara X = 1\nX = X - 1\nresultado X','formula_cycle'),('resultado "'+'x'*4097+'"','formula_text_limit'),('resultado '+'('*60+'1'+')'*60,'formula_depth'),('HP = 80\nresultado HP','formula_current_undeclared')]:self.assertEqual(self.edit(f,source)['error'],code)
    def test_boolean_current_is_typed_fixed_and_not_reexecuted(self):
        f=self.new('Porta','declara Aberta = falso\nresultado SE(Aberta; "aberta"; "fechada")');g=self.new('Abrir','alterar Porta.Aberta = NÃO(Porta.Aberta)');self.ctx('text')
        self.yes(self.c.op('execute_formula',id=g['id']));self.assertEqual(self.f('Porta')['display'],'aberta');self.assertIn('Aberta = verdadeiro',self.f('Porta')['source']);self.assertIs(self.f('Porta')['variables'][0]['current'],True)
        self.restart();self.assertEqual(self.f('Porta')['display'],'aberta');self.assertTrue(self.f('Abrir')['consumed'])
    def test_unknown_language_and_forged_consumption_fail_closed(self):
        self.new();self.new('Golpe','alterar Combate.HP = 80');self.c.close();path=self.folder/'obj01-piloto.sqlite3'
        with sqlite3.connect(path) as db:raw,digest=db.execute('SELECT snapshot,sha256 FROM working').fetchone()
        for index,key,value,error in [(0,'language',99,'formula_language_mismatch'),(0,'consumed',True,'formula_consumed_not_command'),(1,'consumed',True,'formula_consumed_still_active')]:
            with self.subTest(error=error):
                v=json.loads(raw);v['formulas'][index][key]=value;changed=json.dumps(v,ensure_ascii=False,separators=(',',':'))
                with sqlite3.connect(path) as db:db.execute('UPDATE working SET snapshot=?,sha256=?',(changed,hashlib.sha256(changed.encode()).hexdigest()))
                r=subprocess.run([base.BINARY,'--workspace',str(self.folder),'--profile','p2d2'],input='',capture_output=True,text=True,timeout=15);self.assertEqual(r.returncode,1);self.assertIn(error,r.stderr)
        with sqlite3.connect(path) as db:db.execute('UPDATE working SET snapshot=?,sha256=?',(raw,digest))
        self.c=base.Client(self.folder,profile='p2d2')
    def test_explicit_seed_materializes_old_current_without_writing_original(self):
        folder=Path(self.tmp.name)/'old';old=base.Client(folder,profile='p2d');old.op('create_formula',name='Combate',source=SOURCE,before='',table='',cell='');old.op('save');old.close()
        # A controlled, valid v1 snapshot with a current value and no commands.
        with sqlite3.connect(folder/'obj01-piloto.sqlite3') as db:
            snap=json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0]);snap['formulas'][0]['overrides']={'HP':80};raw=json.dumps(snap,ensure_ascii=False,separators=(',',':'));digest=hashlib.sha256(raw.encode()).hexdigest()
            db.execute('UPDATE saved SET snapshot=?,sha256=?',(raw,digest));db.execute('UPDATE working SET snapshot=?,sha256=?,base_sha256=?',(raw,digest,digest))
        old=base.Client(folder,profile='p2d');self.assertEqual(old.state['formulas'][0]['display'],'60');old.close()
        hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'}
        copy=base.Client(Path(self.tmp.name)/'copy',profile='p2d2',seed=folder)
        try:self.assertIn('HP = 80',copy.state['formulas'][0]['source']);self.assertEqual(copy.state['formulas'][0]['overrides'],{})
        finally:copy.close()
        self.assertEqual(hashes,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'})
    def test_seed_with_legacy_mixed_command_is_refused_without_source_write(self):
        folder=Path(self.tmp.name)/'old-command';old=base.Client(folder,profile='p2d')
        old.op('create_formula',name='Combate',source=SOURCE,before='',table='',cell='');old.op('create_formula',name='Golpe',source='alterar Combate.HP = 80\nresultado 0',before='',table='',cell='');old.op('save');old.close()
        before={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'}
        target=Path(self.tmp.name)/'not-converted'
        r=subprocess.run([base.BINARY,'--workspace',str(target),'--profile','p2d2','--seed-p2d',str(folder)],capture_output=True,text=True,timeout=15)
        self.assertEqual(r.returncode,1);self.assertIn('formula_command_must_be_alone',r.stderr);self.assertFalse((target/'obj01-piloto.sqlite3').exists())
        self.assertEqual(before,{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.name!='.pilot.lock'})

if __name__=='__main__':unittest.main(verbosity=2)
