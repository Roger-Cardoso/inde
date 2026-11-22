"""Unified formula hub, stable symbols and consumed remote changes in real C++."""
import copy
from pathlib import Path
import signal
import sys
import tempfile
import unittest
import test_pilot as base
base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
def literal(value):return {'kind':'literal','value':str(value)}
def ref(name,symbol='resultado'):return {'kind':'reference','formula':name,'variable':symbol}
def condition():return {'kind':'condition','cards':{'version':2,'title':'Preço da negociação','variable':'Preco','type':'number','label':'Preço','entry':literal(16),'rules':[{'op':'gt','value':'15','text':'preço caro'},{'op':'eq','value':'15','text':'vou negociar'}],'otherwise':'que pechincha'}}
def value(kind='number',v='100'):return {'kind':'value','title':'Vida do personagem','type':kind,'variable':'HP' if kind=='number' else 'Atitude','label':'Vida' if kind=='number' else 'Atitude','entry':literal(v)}
def calculation():return {'kind':'calculate','title':'Dano de dois ataques','left':literal(2),'right':literal(10),'operation':'multiply'}
def change(name,symbol,v,op='set',kind='number'):return {'kind':'change','title':'Alterar um valor','type':kind,'target':{'formula':name,'variable':symbol},'operation':op,'operand':literal(v)}
class Hub(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory(prefix='inde-hub-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes()
 def tearDown(self):self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
 def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
 def ctx(self,v):return self.c.state if self.c.state['context']==v else self.yes(self.c.op('set_context',context=v))
 def f(self,name):return next(f for f in self.c.state['formulas'] if f['name']==name)
 def new(self,name,m,prefix='',suffix='',**where):
  self.ctx('structure');v=dict(name=name,model=m,before='',table='',cell='',prefix=prefix,suffix=suffix);v.update(where);self.yes(self.c.op('create_formula_hub',**v));return self.f(name)
 def key(self,f):return next(u['id'] for u in self.c.state['formula_uses'] if u['object']==f['id'])
 def execute(self,f):self.ctx('text');return self.c.op('execute_formula',id=f['id'])
 def edit(self,f,m,prefix='',suffix='',target=None):self.ctx('text');return self.c.op('edit_formula_hub',id=f['id'],model=m,target=self.key(f) if target is None else target,prefix=prefix,suffix=suffix)
 def preview(self,m,id=''):
  before=copy.deepcopy(self.c.state);r=self.c.op('preview_formula_hub',id=id,model=m);self.assertEqual(self.c.state,before);self.assertTrue(r['accepted'],r);return r['preview']
 def test_named_condition_remote_free_command_consumption_and_reopen(self):
  f=self.new('Negociacao',condition(),prefix='Armand: ',suffix='.');self.assertEqual(f['hub'],condition());self.assertEqual(f['variables'][0]['name'],'Preco');self.assertEqual(f['display'],'preço caro')
  g=self.new('Negociar',{'kind':'free','source':'alterar Negociacao.Preco = 15'});key=self.key(g);self.yes(self.execute(g));self.assertEqual(self.f('Negociacao')['display'],'vou negociar');self.assertEqual(self.f('Negociacao')['cards']['current'],'15');self.assertTrue(self.f('Negociar')['consumed']);self.assertNotIn(key,self.c.state['author_order'])
  self.yes(self.c.op('undo'));self.assertEqual(self.f('Negociacao')['display'],'preço caro');self.assertIn(key,self.c.state['author_order']);self.yes(self.c.op('redo'));self.c.close();self.c=base.Client(self.folder,profile='p2d2');self.assertEqual(self.f('Negociacao')['display'],'vou negociar');self.assertTrue(self.f('Negociar')['consumed'])
 def test_preview_uses_chosen_name_and_free_body_is_preserved(self):
  m={'kind':'free','title':'Minha fonte','source':'# escrita autoral\ndeclara HP = 3\nresultado Minha.HP'};r=self.c.op('preview_formula_hub',id='',name='Minha',model=m);self.assertTrue(r['accepted'],r);self.assertEqual(r['preview']['display'],'3');f=self.new('Minha',m);self.assertEqual(f['hub'],m);self.assertIn('# escrita autoral',f['source'])
 def test_free_source_preserves_crlf_and_blank_lines(self):
  m={'kind':'free','title':'Texto autoral','source':'# minha fonte\r\n\r\ndeclara HP = 3\r\nresultado HP\r\n'};f=self.new('Minha',m);self.assertTrue(f['valid']);self.assertEqual(f['hub'],m);self.yes(self.c.op('save'));self.yes(self.c.op('reopen',discard_unsaved=False));self.assertEqual(self.f('Minha')['hub'],m)
 def test_value_and_calculation_cards_share_real_variables(self):
  hp=self.new('Personagem',value());m=calculation();m['left']=ref('Personagem','HP');f=self.new('Combate',m);self.assertEqual(f['display'],'1000');self.assertEqual(f['hub'],m)
  g=self.new('Dano',change('Personagem','HP',20,'subtract'));self.yes(self.execute(g));self.assertEqual(self.f('Personagem')['display'],'80');self.assertEqual(self.f('Combate')['display'],'800');self.assertEqual(self.f('Personagem')['hub']['current'],{'HP':'80'})
 def test_text_change_drives_text_condition(self):
  self.new('Pessoa',value('text','hostil'));m=condition();m['cards'].update(type='text',variable='Estado',entry=ref('Pessoa','Atitude'),rules=[{'op':'eq','value':'hostil','text':'ameaçador'}],otherwise='tranquilo');self.new('Descricao',m)
  g=self.new('Acalmar',change('Pessoa','Atitude','amistosa',kind='text'));self.yes(self.execute(g));self.assertEqual(self.f('Descricao')['display'],'tranquilo');self.assertEqual(self.f('Pessoa')['hub']['current'],{'Atitude':'amistosa'})
 def test_preview_remote_change_has_before_after_and_no_effect(self):
  self.new('Vida',value());p=self.preview(change('Vida','HP',20,'subtract'));self.assertTrue(p['command']);self.assertEqual(p['changes'],[{'formula':'Vida','variable':'HP','before':'100','after':'80'}]);self.assertEqual(self.f('Vida')['display'],'100')
  p=self.preview({'kind':'free','source':'alterar Vida.HP = 90'});self.assertEqual(p['changes'][0]['after'],'90');self.assertEqual(self.f('Vida')['display'],'100')
 def test_symbol_rename_is_refused_but_title_and_label_editable(self):
  f=self.new('Negociacao',condition());m=condition();m['cards']['variable']='Outro';before=copy.deepcopy(self.c.state['formulas']);r=self.edit(f,m);self.assertEqual(r['error'],'formula_symbol_immutable');self.assertEqual(self.c.state['formulas'],before)
  m=condition();m['cards']['title']='Novo título';m['cards']['label']='Preço revisto';self.yes(self.edit(f,m));self.assertEqual(self.f('Negociacao')['hub'],m)
  f=self.new('Vida',value());m=value();m['variable']='Outro';self.assertEqual(self.edit(f,m)['error'],'formula_symbol_immutable')
 def test_original_cards_and_free_source_are_not_implicitly_converted(self):
  m=condition()['cards'];m['version']=1;del m['title'];del m['variable'];f=self.new('Antiga',{'kind':'condition','cards':m});self.assertEqual(f['cards'],m);m['label']='Outro rótulo';self.yes(self.edit(f,{'kind':'condition','cards':m}));self.assertIn('declara Entrada',self.f('Antiga')['source'])
  f=self.new('Livre',{'kind':'free','source':'# Minha fonte\nresultado 42'});raw=f['source'];self.assertEqual(self.edit(f,value())['error'],'formula_hub_advanced_source');self.assertEqual(self.f('Livre')['source'],raw)
 def test_calculation_operations_and_current_values_roundtrip(self):
  m=calculation();f=self.new('Conta',m)
  for op,want in [('add','12'),('subtract','-8'),('multiply','20'),('divide','0.2')]:
   m['operation']=op;self.yes(self.edit(f,m));self.assertEqual(self.f('Conta')['display'],want);self.assertEqual(self.f('Conta')['hub'],m)
  g=self.new('Mudar',change('Conta','Primeiro',4));self.yes(self.execute(g));self.assertEqual(self.f('Conta')['hub']['current'],{'Primeiro':'4'});self.assertEqual(self.f('Conta')['display'],'0.4');self.yes(self.c.op('reset_formula_variable',id=f['id'],variable='Primeiro'));self.assertNotIn('current',self.f('Conta')['hub'])
 def test_pending_change_card_can_be_edited_then_consumed(self):
  self.new('Vida',value());g=self.new('Golpe',change('Vida','HP',10,'subtract'));self.yes(self.edit(g,change('Vida','HP',25,'subtract')));self.assertEqual(self.f('Golpe')['hub']['operand'],literal(25));self.yes(self.execute(g));self.assertEqual(self.f('Vida')['display'],'75');self.assertEqual(self.c.op('execute_formula',id=g['id'])['error'],'formula_command_consumed')
 def test_failed_change_keeps_command_and_all_values(self):
  self.new('Vida',value());self.new('Inverso',{'kind':'free','source':'resultado 1 / Vida.HP'});g=self.new('Zerar',change('Vida','HP',0));self.ctx('text');before=copy.deepcopy(self.c.state);r=self.c.op('execute_formula',id=g['id']);self.assertEqual(r['error'],'formula_execution_would_break_dependency');self.assertEqual(self.c.state,before)
 def test_structured_invalid_and_unknown_fields_are_atomic(self):
  bad=[value('number','nan'),dict(calculation(),operation='power'),dict(value(),extra=1),dict(condition(),cards=dict(condition()['cards'],variable='resultado')),change('NaoExiste','HP',1)]
  for m in bad:
   before=copy.deepcopy(self.c.state);r=self.c.op('create_formula_hub',name='Ruim',model=m,before='',table='',cell='',prefix='',suffix='');self.assertFalse(r['accepted'],r);self.assertEqual(self.c.state,before)
  f=self.new('Rascunho',{'kind':'free','source':'resultado ('});self.assertFalse(f['valid']);self.assertEqual(f['hub']['source'],'resultado (')
 def test_cell_change_restores_plain_text_and_undo_restores_command(self):
  self.new('Vida',value());self.ctx('structure');t=self.yes(self.c.op('create_table',before='',title='Tabela',header=False,cells=['Anterior']*6))['tables'][0];cell=t['cells'][0]['id'];g=self.new('Golpe',change('Vida','HP',10,'subtract'),table=t['id'],cell=cell);self.yes(self.execute(g));self.assertNotIn(cell,self.c.state['formula_cells']);self.assertEqual(self.c.state['tables'][0]['cells'][0]['text'],'Anterior');self.yes(self.c.op('undo'));self.assertEqual(self.c.state['formula_cells'][cell],g['id']);self.assertEqual(self.f('Vida')['display'],'100')
 def test_source_and_phrase_one_undo_and_mixed_command_rejected(self):
  f=self.new('Vida',value(),prefix='Vida: ',suffix='.');m=value(v='80');self.yes(self.edit(f,m,prefix='Restam ',suffix=' pontos.'));self.yes(self.c.op('undo'));self.assertEqual(self.f('Vida')['hub'],value());self.assertEqual(self.c.state['formula_text'][self.key(f)]['runs'][0]['text'],'Vida: ')
  self.ctx('structure');before=copy.deepcopy(self.c.state);r=self.c.op('create_formula_hub',name='Ruim',model={'kind':'free','source':'alterar Vida.HP = 0\nresultado 0'},before='',table='',cell='',prefix='',suffix='');self.assertFalse(r['accepted']);self.assertEqual(self.c.state,before)
 def test_sigkill_keeps_named_symbols_and_consumed_change(self):
  self.new('Negociacao',condition());g=self.new('Negociar',change('Negociacao','Preco',15));self.yes(self.execute(g));before=copy.deepcopy(self.c.state);self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2')
  for key in ['formulas','formula_text','formula_uses','author_order']:self.assertEqual(self.c.state[key],before[key])
 def test_crash_windows_remote_change_is_atomic(self):
  for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
   with self.subTest(stage=stage):
    folder=Path(self.tmp.name)/stage;c=base.Client(folder,profile='p2d2');self.assertTrue(c.op('create_formula_hub',name='Vida',model=value(),before='',table='',cell='',prefix='',suffix='')['accepted']);self.assertTrue(c.op('create_formula_hub',name='Golpe',model=change('Vida','HP',20,'subtract'),before='',table='',cell='',prefix='',suffix='')['accepted']);g=c.state['formulas'][1];c.op('set_context',context='text');c.close();c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash')
    with self.assertRaises(AssertionError):c.op('execute_formula',id=g['id'])
    c.close(expected=86);c=base.Client(folder,profile='p2d2')
    try:self.assertEqual(c.state['formulas'][0]['display'],'80' if stage=='after_commit' else '100');self.assertEqual(c.state['formulas'][1]['consumed'],stage=='after_commit')
    finally:c.close()
if __name__=='__main__':unittest.main(verbosity=2)
