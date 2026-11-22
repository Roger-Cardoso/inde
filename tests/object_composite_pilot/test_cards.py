"""Approved card editor at the real C++/SQLite boundary, not a JS evaluator."""
import copy
import hashlib
import json
from pathlib import Path
import signal
import sqlite3
import sys
import tempfile
import unittest
import test_pilot as base
base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())

def model(kind='number'):
    return {'version':1,'type':kind,'label':'o preço' if kind=='number' else 'Atitude','entry':{'kind':'literal','value':'16' if kind=='number' else 'hostil'},'rules':[{'op':'gt','value':'15','text':'preço caro'},{'op':'eq','value':'15','text':'vou negociar'}] if kind=='number' else [{'op':'eq','value':'hostil','text':'ameaçador'},{'op':'eq','value':'amistosa','text':'receptivo'}],'otherwise':'que pechincha' if kind=='number' else 'indecifrável'}

class Cards(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='obj01-cards-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes()
    def tearDown(self):
        self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
    def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
    def ctx(self,v):return self.c.state if self.c.state['context']==v else self.yes(self.c.op('set_context',context=v))
    def f(self,name='Escolha'):return next(f for f in self.c.state['formulas'] if f['name']==name)
    def new(self,m=None,name='Escolha',**where):
        self.ctx('structure');v=dict(name=name,cards=m or model(),before='',table='',cell='',prefix='Ei Armand ... eita, ',suffix=' isso!');v.update(where);self.yes(self.c.op('create_formula_cards',**v));return self.f(name)
    def key(self,f):return next(u['id'] for u in self.c.state['formula_uses'] if u['object']==f['id'])
    def edit(self,m,f=None,target=None,prefix='Antes ',suffix=' depois'):
        f=f or self.f();self.ctx('text');return self.c.op('edit_formula_cards',id=f['id'],cards=m,target=target if target is not None else self.key(f),prefix=prefix,suffix=suffix)
    def preview(self,m,id=''):
        before=copy.deepcopy(self.c.state);r=self.c.op('preview_formula_cards',id=id,cards=m);self.assertEqual(self.c.state,before);self.assertTrue(r['accepted'],r);return r['preview']
    def resolve(self,key):
        fs={f['id']:f for f in self.c.state['formulas']};return ''.join(r['text'] if r['kind']=='text' else fs[r['formula']]['display'] for r in self.c.state['formula_text'][key]['runs'])
    def test_numeric_context_and_roundtrip_recoverable(self):
        f=self.new();self.assertTrue(f['valid']);self.assertEqual(f['cards'],model());self.assertEqual(self.resolve(self.key(f)),'Ei Armand ... eita, preço caro isso!')
        self.c.close();self.c=base.Client(self.folder,profile='p2d2');self.assertEqual(self.f()['cards'],model());self.assertEqual(self.f()['display'],'preço caro');self.assertFalse(self.c.state['can_undo'])
    def test_numeric_spelling_and_typed_current_roundtrip(self):
        m=model();m['entry']['value']='15.10';m['rules'][0]['value']='15.05';f=self.new(m);self.assertEqual(f['cards'],m);m['current']='14.125';self.yes(self.edit(m));self.assertEqual(self.f()['cards'],m)
    def test_priority_edit_add_remove_and_fallback(self):
        m=model();m['entry']['value']='15';self.assertEqual(self.preview(m)['display'],'vou negociar')
        m['rules'].insert(0,{'op':'ge','value':'15','text':'primeiro'});self.assertEqual(self.preview(m)['display'],'primeiro')
        m['rules'].reverse();self.assertEqual(self.preview(m)['display'],'vou negociar');m['rules']=[];self.assertEqual(self.preview(m)['display'],'que pechincha')
    def test_text_exact_nonquantitative_and_substrings(self):
        m=model('text');self.assertEqual(self.preview(m)['display'],'ameaçador')
        for op,value,want in [('eq','Hostil','indecifrável'),('ne','Hostil','certo'),('contains','sti','certo'),('starts','hos','certo'),('starts','osti','indecifrável')]:
            m['rules']=[dict(op=op,value=value,text='certo')];self.assertEqual(self.preview(m)['display'],want)
        m['entry']['value']='01';m['rules']=[dict(op='eq',value='1',text='igual')];self.assertEqual(self.preview(m)['display'],'indecifrável')
        m['entry']['value']='';m['rules']=[dict(op='eq',value='',text='vazio')];self.assertEqual(self.preview(m)['display'],'vazio')
        m['entry']['value']='ação 漢';m['rules']=[dict(op='contains',value='ção',text='Unicode')];self.assertEqual(self.preview(m)['display'],'Unicode')
    def test_quote_escape_and_expression_injection_are_literal(self):
        m=model('text');m['label']='Nome "da entrada"\n漢';m['entry']['value']='"; alterar Pessoa.HP = 0 #';m['rules']=[dict(op='eq',value=m['entry']['value'],text='Uma frase\ncom "aspas", \\ e 漢')]
        f=self.new(m);self.assertEqual(f['cards'],m);self.assertEqual(f['display'],m['rules'][0]['text']);self.assertFalse(f['command']);self.assertEqual(len(self.c.state['formulas']),1)
    def test_preview_never_changes_database_or_history(self):
        self.new();before=hashlib.sha256((self.folder/'obj01-piloto.sqlite3').read_bytes()).hexdigest();generation=self.c.state['recovery_generation']
        for value in ['0','15','16']:m=model();m['entry']['value']=value;self.preview(m)
        self.assertEqual(self.c.state['recovery_generation'],generation);self.assertEqual(hashlib.sha256((self.folder/'obj01-piloto.sqlite3').read_bytes()).hexdigest(),before)
    def test_atomic_source_phrase_undo_redo_and_save(self):
        f=self.new();key=self.key(f);before=f['source'];m=model();m['entry']['value']='15';self.yes(self.edit(m,prefix='Decisão: ',suffix='.'))
        self.assertEqual(self.resolve(key),'Decisão: vou negociar.');self.yes(self.c.op('undo'));self.assertEqual(self.f()['source'],before);self.assertEqual(self.resolve(key),'Ei Armand ... eita, preço caro isso!')
        self.yes(self.c.op('redo'));self.assertEqual(self.resolve(key),'Decisão: vou negociar.');self.yes(self.c.op('save'));self.yes(self.c.op('reopen',discard_unsaved=False));self.assertEqual(self.f()['cards'],m)
    def test_real_variable_reference_and_external_change(self):
        self.yes(self.c.op('create_formula',name='Mercado',source='declara Preco = 16\nresultado Preco',before='',table='',cell='',prefix='',suffix=''))
        m=model();m['entry']={'kind':'reference','formula':'Mercado','variable':'Preco'};f=self.new(m);self.assertEqual(f['display'],'preço caro');self.assertEqual(f['cards'],m)
        self.ctx('text');self.yes(self.c.op('edit_formula',id=self.f('Mercado')['id'],source='declara Preco = 15\nresultado Preco'));self.assertEqual(self.f()['display'],'vou negociar');self.assertEqual(self.f()['cards'],m)
    def test_current_assignment_remains_editable_as_cards(self):
        f=self.new();self.ctx('structure');self.yes(self.c.op('create_formula',name='Mudar',source='alterar Escolha.Entrada = 15',before='',table='',cell='',prefix='',suffix=''));self.ctx('text');self.yes(self.c.op('execute_formula',id=self.f('Mudar')['id']))
        self.assertEqual(self.f()['display'],'vou negociar');m=self.f()['cards'];self.assertIsNotNone(m);self.assertEqual(m['current'],'15');self.yes(self.edit(m));self.assertEqual(self.f()['cards'],m)
        self.yes(self.c.op('reset_formula_variable',id=f['id'],variable='Entrada'));self.assertNotIn('current',self.f()['cards']);self.assertEqual(self.f()['display'],'preço caro')
    def test_cell_target_and_shared_phrase_are_independent(self):
        t=self.yes(self.c.op('create_table',before='',title='Tabela',header=False,cells=['A']*6))['tables'][0];cell=t['cells'][0]['id'];f=self.new(table=t['id'],cell=cell)
        self.yes(self.c.op('attach_formula',id=f['id'],before='',table='',cell=''));other=self.key(f)
        m=model();m['entry']['value']='15';self.yes(self.edit(m,target=cell,prefix='Na célula: ',suffix='.'));self.assertEqual(self.resolve(cell),'Na célula: vou negociar.');self.assertEqual(self.resolve(other),'vou negociar')
    def test_advanced_source_and_complex_phrase_are_preserved(self):
        f=self.new();key=self.key(f);self.ctx('text');runs=[{'kind':'result','formula':f['id']},{'kind':'result','formula':f['id']}];self.yes(self.c.op('edit_formula_text',target=key,runs=runs));before=copy.deepcopy(self.c.state)
        r=self.edit(model());self.assertEqual(r['error'],'formula_cards_complex_phrase');self.assertEqual(self.c.state['formula_text'],before['formula_text'])
        self.yes(self.c.op('edit_formula',id=f['id'],source=f['source']+'\n# comentário autoral'));self.assertIsNone(self.f()['cards']);source=self.f()['source'];r=self.edit(model());self.assertEqual(r['error'],'formula_cards_advanced_source');self.assertEqual(self.f()['source'],source)
    def test_wrong_target_stale_context_and_type_fail_atomically(self):
        f=self.new();m=model();self.ctx('structure');self.assertEqual(self.c.op('edit_formula_cards',id=f['id'],cards=m,target=self.key(f),prefix='',suffix='')['error'],'editing_context_required')
        self.ctx('text');before=copy.deepcopy(self.c.state);self.assertEqual(self.edit(m,target='missing')['error'],'formula_text_target_invalid');self.assertEqual(self.c.state,before)
        r=self.c.send({'op':'preview_formula_cards','expected':-1,'id':f['id'],'cards':m});self.assertEqual(r['error'],'stale_session_revision')
        self.ctx('structure');self.yes(self.c.op('create_formula',name='Texto',source='resultado "hostil"',before='',table='',cell='',prefix='',suffix=''));m['entry']={'kind':'reference','formula':'Texto','variable':'resultado'};before=copy.deepcopy(self.c.state)
        r=self.c.op('create_formula_cards',name='Ruim',cards=m,before='',table='',cell='',prefix='',suffix='');self.assertEqual(r['error'],'formula_type');self.assertEqual(self.c.state,before)
    def test_bounds_unknown_fields_numeric_values_and_strings(self):
        invalid=[]
        for value in ['','nan','inf','1e13','1; alterar X.Y = 1']:
            m=model();m['entry']['value']=value;invalid.append(m)
        m=model();m['rules']*=7;invalid.append(m);m=model();m['rules'][0]['text']='x'*4097;invalid.append(m)
        m=model('text');m['rules'][0]['op']='gt';invalid.append(m);m=model();m['surprise']=1;invalid.append(m)
        m=model();m['label']='x'*257;invalid.append(m)
        for m in invalid:
            before=copy.deepcopy(self.c.state);r=self.c.op('create_formula_cards',name='Ruim',cards=m,before='',table='',cell='',prefix='',suffix='');self.assertFalse(r['accepted'],r);self.assertEqual(self.c.state,before)
    def test_sigkill_recovers_whole_rule_and_phrase(self):
        f=self.new();m=model('text');self.yes(self.edit(m,prefix='Ele parece ',suffix='.'));before=copy.deepcopy(self.c.state);self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2')
        for field in ['formulas','formula_text','formula_uses','author_order']:self.assertEqual(self.c.state[field],before[field]);self.assertFalse(self.c.state['can_undo'])
    def test_fault_windows_never_split_rule_from_phrase(self):
        for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
            with self.subTest(stage=stage):
                folder=Path(self.tmp.name)/stage;c=base.Client(folder,profile='p2d2');r=c.op('create_formula_cards',name='Escolha',cards=model(),before='',table='',cell='',prefix='Antes ',suffix='.');self.assertTrue(r['accepted']);f=c.state['formulas'][0];key=c.state['formula_uses'][0]['id'];c.op('set_context',context='text');c.close()
                c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash');m=model();m['entry']['value']='15'
                with self.assertRaises(AssertionError):c.op('edit_formula_cards',id=f['id'],cards=m,target=key,prefix='Depois ',suffix='!')
                c.close(expected=86);c=base.Client(folder,profile='p2d2')
                try:
                    committed=stage=='after_commit';self.assertEqual(c.state['formulas'][0]['display'],'vou negociar' if committed else 'preço caro');self.assertEqual(c.state['formula_text'][key]['runs'][0]['text'],'Depois ' if committed else 'Antes ')
                finally:c.close()

if __name__=='__main__':unittest.main(verbosity=2)
