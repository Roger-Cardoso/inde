"""Formula operations traverse HTTP -> C++ -> recoverable SQLite state."""
import json
from pathlib import Path
import sqlite3
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()

class FormulaHttp(base.HttpPilot):
    profile='p2d'
    def op(self,op,**values):
        status,_,raw=self.request('/api/request',{'op':op,'expected':self.state()['revision'],**values})
        self.assertEqual(status,200);r=json.loads(raw);self.assertTrue(r['accepted'],r);return r['state']
    def formula(self,name,source,**target):
        self.op('set_context',context='structure')
        values={'name':name,'source':source,'before':'','table':'','cell':''};values.update(target)
        s=self.op('create_formula',**values);return next(f for f in s['formulas'] if f['name']==name)
    def test_mutation_cascade_is_durable_and_save_promotes(self):
        a=self.formula('Estado','declara HP = 100\nresultado HP')
        b=self.formula('Golpe','alterar Estado.HP = Estado.HP - 20\nresultado Estado.HP')
        self.op('set_context',context='text');s=self.op('execute_formula',id=b['id'])
        self.assertEqual(next(f for f in s['formulas'] if f['id']==a['id'])['display'],'80')
        dbpath=Path(self.tmp.name)/'fixture'/'obj01-piloto.sqlite3'
        with sqlite3.connect(dbpath) as db:
            self.assertEqual(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['formulas'],[])
            self.assertEqual(json.loads(db.execute('SELECT snapshot FROM working').fetchone()[0])['formulas'][0]['overrides'],{'HP':80})
        self.op('save');s=self.op('reopen',discard_unsaved=False)
        self.assertEqual(s['formulas'][0]['display'],'80');self.assertFalse(s['can_undo'])
    def test_formula_created_in_cell_drives_document_result(self):
        t=self.op('create_table',before='',title='Células',header=False,cells=['']*6)['tables'][0]
        f=self.formula('Celula','= 7',table=t['id'],cell=t['cells'][0]['id'])
        out=self.formula('Documento','resultado Celula.resultado * 6')
        self.assertEqual(out['display'],'42');self.op('set_context',context='text')
        s=self.op('edit_formula',id=f['id'],source='= 8');self.assertEqual(s['formulas'][-1]['display'],'48')
        self.assertEqual(self.op('reopen_working',discard_unsaved=True)['formulas'],s['formulas'])
    def test_invalid_formula_is_an_editable_diagnostic_and_actions_guarded(self):
        f=self.formula('Ruim','= 1/0');self.assertFalse(f['valid']);self.assertEqual(f['error'],'formula_division_zero')
        self.op('set_context',context='text');s=self.op('edit_formula',id=f['id'],source='= SE(0; 1/0; 12)');self.assertEqual(s['formulas'][0]['display'],'12')
        before=self.state();status,_,raw=self.request('/api/request',{'op':'export_legacy','expected':before['revision']})
        self.assertEqual(status,200);self.assertEqual(json.loads(raw)['error'],'composite_publication_not_supported');self.assertEqual(self.state(),before)

if __name__=='__main__':unittest.main(verbosity=2)
