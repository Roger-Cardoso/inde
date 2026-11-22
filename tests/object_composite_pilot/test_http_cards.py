"""Card authoring through the actual authenticated HTTP controller."""
import json
from pathlib import Path
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()

def cards(kind='number'):
    return {'version':1,'type':kind,'label':'Preço' if kind=='number' else 'Atitude','entry':{'kind':'literal','value':'16' if kind=='number' else 'hostil'},'rules':[{'op':'gt' if kind=='number' else 'eq','value':'15' if kind=='number' else 'hostil','text':'caro' if kind=='number' else 'ameaçador'}],'otherwise':'barato' if kind=='number' else 'tranquilo'}

class CardsHttp(base.HttpPilot):
    profile='p2d2'
    def op(self,op,**values):
        status,_,raw=self.request('/api/request',{'op':op,'expected':self.state()['revision'],**values});self.assertEqual(status,200);r=json.loads(raw);self.assertTrue(r['accepted'],r);return r
    def new(self,m,**where):
        self.op('set_context',context='structure');v=dict(name='Escolha',cards=m,before='',table='',cell='',prefix='Está ',suffix='.');v.update(where);return self.op('create_formula_cards',**v)['state']
    def test_preview_is_read_only_and_same_evaluator(self):
        before=self.state();r=self.op('preview_formula_cards',id='',cards=cards());self.assertEqual(r['preview']['display'],'caro');self.assertEqual(self.state(),before)
        _,_,js=self.request('/app.js',authenticated=False);self.assertIn(b'preview_formula_cards',js);self.assertIn(b'Adicionar condi',js)
    def test_rule_phrase_undo_and_reopen_are_atomic(self):
        s=self.new(cards());f=s['formulas'][0];key=s['formula_uses'][0]['id'];self.op('set_context',context='text');m=cards('text')
        s=self.op('edit_formula_cards',id=f['id'],cards=m,target=key,prefix='Ele parece ',suffix='!')['state'];self.assertEqual(s['formulas'][0]['display'],'ameaçador');self.assertEqual(s['formula_text'][key]['runs'][0]['text'],'Ele parece ')
        s=self.op('undo')['state'];self.assertEqual(s['formulas'][0]['display'],'caro');self.assertEqual(s['formula_text'][key]['runs'][0]['text'],'Está ');self.op('redo');self.op('save');s=self.op('reopen',discard_unsaved=False)['state'];self.assertEqual(s['formulas'][0]['cards'],m)
    def test_cell_context_and_refused_invalid_values(self):
        t=self.op('create_table',before='',title='Condições',header=False,cells=['']*6)['state']['tables'][0];cell=t['cells'][0]['id'];s=self.new(cards(),table=t['id'],cell=cell);self.assertEqual(s['formula_cells'][cell],s['formulas'][0]['id']);before=self.state();bad=cards();bad['entry']['value']='nan'
        _,_,raw=self.request('/api/request',{'op':'create_formula_cards','expected':before['revision'],'name':'Ruim','cards':bad,'before':'','table':'','cell':'','prefix':'','suffix':''});self.assertFalse(json.loads(raw)['accepted']);self.assertEqual(self.state(),before)

if __name__=='__main__':unittest.main(verbosity=2)
