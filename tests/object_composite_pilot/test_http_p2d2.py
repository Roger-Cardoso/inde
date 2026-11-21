"""Actual HTTP controller for authored text, typed results and consumed commands."""
import json
from pathlib import Path
import sqlite3
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()

class RichHttp(base.HttpPilot):
    profile='p2d2'
    def op(self,op,**values):
        status,_,raw=self.request('/api/request',{'op':op,'expected':self.state()['revision'],**values});self.assertEqual(status,200);r=json.loads(raw);self.assertTrue(r['accepted'],r);return r['state']
    def new(self,name,source,**target):
        self.op('set_context',context='structure');v={'name':name,'source':source,'before':'','table':'','cell':'','prefix':'','suffix':''};v.update(target);s=self.op('create_formula',**v);return next(f for f in s['formulas'] if f['name']==name)
    def test_fixed_source_and_consumption_are_one_durable_change(self):
        f=self.new('Combate','declara HP = 100\nresultado HP');g=self.new('Golpe','alterar Combate.HP = 80');command=next(u['id'] for u in self.state()['formula_uses'] if u['object']==g['id'])
        self.op('set_context',context='text');s=self.op('execute_formula',id=g['id']);self.assertNotIn(command,s['author_order']);self.assertIn('HP = 80',s['formulas'][0]['source']);self.assertTrue(s['formulas'][1]['consumed'])
        with sqlite3.connect(Path(self.tmp.name)/'fixture'/'obj01-piloto.sqlite3') as db:
            w=json.loads(db.execute('SELECT snapshot FROM working').fetchone()[0]);self.assertTrue(w['formulas'][1]['consumed']);self.assertIn('HP = 80',w['formulas'][0]['source']);self.assertFalse(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['formulas'])
        self.op('save');s=self.op('reopen',discard_unsaved=False);self.assertTrue(s['formulas'][1]['consumed']);self.assertEqual(s['formulas'][0]['display'],'80')
    def test_typed_word_and_literal_phrase_survive_restart_record(self):
        f=self.new('Estado','declara HP = 80\nresultado IF(AND(HP > 0; HP < 90); "ferido"; "de pé")',prefix='Ele está ',suffix='.')
        s=self.state();key=next(u['id'] for u in s['formula_uses'] if u['object']==f['id']);self.assertEqual(f['display'],'ferido')
        self.op('set_context',context='text');runs=[{'kind':'text','text':'O personagem está '},{'kind':'result','formula':f['id']},{'kind':'text','text':'.'}];self.op('edit_formula_text',target=key,runs=runs)
        s=self.op('reopen_working',discard_unsaved=True);self.assertEqual(s['formula_text'][key]['runs'],runs);self.assertEqual(s['formulas'][0]['display'],'ferido')
        status,_,body=self.request('/app.js',authenticated=False);self.assertEqual(status,200);self.assertIn(b'contentEditable',body)
    def test_cell_uses_same_model_and_mixed_command_is_refused(self):
        t=self.op('create_table',before='',title='Texto',header=False,cells=['']*6)['tables'][0]
        f=self.new('Celula','resultado SE(NÃO(falso); "sim"; "não")',table=t['id'],cell=t['cells'][0]['id'],prefix='Resposta: ')
        self.assertEqual(f['display'],'sim');before=self.state()
        status,_,raw=self.request('/api/request',{'op':'create_formula','expected':before['revision'],'name':'Ruim','source':'alterar Celula.X = 1\nresultado 0','before':'','table':'','cell':'','prefix':'','suffix':''})
        self.assertEqual(status,200);self.assertFalse(json.loads(raw)['accepted']);self.assertEqual(self.state(),before)

if __name__=='__main__':unittest.main(verbosity=2)
