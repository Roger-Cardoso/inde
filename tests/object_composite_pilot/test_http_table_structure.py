"""Authenticated structural table operations and restored stable identities."""
import json
from pathlib import Path
import sys,unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()
class TableStructureHttp(base.HttpPilot):
 profile='p2d2'
 def op(self,op,**v):
  status,_,raw=self.request('/api/request',dict(op=op,expected=self.state()['revision'],**v));self.assertEqual(status,200);r=json.loads(raw);self.assertTrue(r['accepted'],r);return r['state']
 def new(self):return self.op('create_table_sized',before='',title='Inventário',header=True,rows=3,columns=4)['tables'][0]
 def test_insert_retire_reopen_and_restore(self):
  t=self.new();row=t['row_ids'][1];cell=t['cells'][4];self.op('set_context',context='text');self.op('edit_cell',id=t['id'],cell=cell['id'],text='Conservado 🙂');self.op('set_context',context='structure');s=self.op('insert_table_axis',id=t['id'],axis='column',before=t['column_ids'][1]);self.assertEqual(len(s['tables'][0]['column_ids']),5);self.op('remove_table_axis',id=t['id'],axis='row',axis_id=row);self.op('save');self.op('reopen',discard_unsaved=False);s=self.op('restore_table_axis',id=t['id'],axis='row',axis_id=row);self.assertEqual(next(c for c in s['tables'][0]['cells'] if c['id']==cell['id'])['text'],'Conservado 🙂')
 def test_invalid_structure_is_atomic(self):
  t=self.new();before=self.state();status,_,raw=self.request('/api/request',dict(op='remove_table_axis',expected=before['revision'],id=t['id'],axis='row',axis_id='missing'));self.assertEqual(status,200);self.assertFalse(json.loads(raw)['accepted']);self.assertEqual(self.state(),before)
 def test_caps_and_controls_available(self):
  self.assertTrue(self.state()['table_structure_supported']);_,_,js=self.request('/app.js',authenticated=False);self.assertIn(b'create_table_sized',js);self.assertIn(b'restore_table_axis',js);self.assertIn(b'Linhas da nova tabela',js);self.assertIn(b'table-viewport',js)
if __name__=='__main__':unittest.main(verbosity=2)
