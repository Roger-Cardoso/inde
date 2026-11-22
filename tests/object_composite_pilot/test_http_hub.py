"""Hub operations through the authenticated local HTTP service."""
import json
from pathlib import Path
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()
def condition():return {'kind':'condition','cards':{'version':2,'title':'Preço','variable':'Preco','type':'number','label':'Preço','entry':{'kind':'literal','value':'16'},'rules':[{'op':'eq','value':'15','text':'vou negociar'}],'otherwise':'preço caro'}}
class HubHttp(base.HttpPilot):
 profile='p2d2'
 def op(self,op,**data):
  status,_,body=self.request('/api/request',dict(op=op,expected=self.state()['revision'],**data));self.assertEqual(status,200);r=json.loads(body);self.assertTrue(r['accepted'],r);return r
 def new(self,name,m):self.op('set_context',context='structure');return self.op('create_formula_hub',name=name,model=m,before='',table='',cell='',prefix='',suffix='')['state']['formulas'][-1]
 def test_named_remote_free_change_and_undo(self):
  self.assertTrue(self.state()['formula_hub_supported']);f=self.new('Negociacao',condition());g=self.new('Negociar',{'kind':'free','source':'alterar Negociacao.Preco = 15'});self.op('set_context',context='text');s=self.op('execute_formula',id=g['id'])['state'];self.assertEqual(s['formulas'][0]['display'],'vou negociar');self.assertTrue(s['formulas'][1]['consumed']);s=self.op('undo')['state'];self.assertEqual(s['formulas'][0]['display'],'preço caro');self.assertFalse(s['formulas'][1]['consumed'])
 def test_preview_change_is_read_only_and_roundtrip_saved(self):
  f=self.new('Negociacao',condition());before=self.state();m={'kind':'change','title':'Negociar','type':'number','target':{'formula':'Negociacao','variable':'Preco'},'operation':'set','operand':{'kind':'literal','value':'15'}};r=self.op('preview_formula_hub',id='',model=m);self.assertEqual(r['preview']['changes'][0]['after'],'15');self.assertEqual(self.state(),before);g=self.new('Negociar',m);self.op('save');s=self.op('reopen',discard_unsaved=False)['state'];self.assertEqual(s['formulas'][1]['hub'],m)
 def test_reference_rename_refused_and_hub_assets_available(self):
  f=self.new('Negociacao',condition());self.op('set_context',context='text');m=condition();m['cards']['variable']='Outro';before=self.state();key=before['formula_uses'][0]['id'];status,_,body=self.request('/api/request',dict(op='edit_formula_hub',expected=before['revision'],id=f['id'],model=m,target=key,prefix='',suffix=''));self.assertEqual(status,200);self.assertEqual(json.loads(body)['error'],'formula_symbol_immutable');self.assertEqual(self.state(),before);_,_,js=self.request('/app.js',authenticated=False);self.assertIn(b'Guardar um valor',js);self.assertIn(b'preview_formula_hub',js)
if __name__=='__main__':unittest.main(verbosity=2)
