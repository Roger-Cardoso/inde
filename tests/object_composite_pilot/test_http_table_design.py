"""Authenticated transport for table design, atomic styling and readback."""
import copy
import json
from pathlib import Path
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()
class TableDesignHttp(base.HttpPilot):
 profile='p2d2'
 def op(self,op,**v):
  status,_,body=self.request('/api/request',dict(op=op,expected=self.state()['revision'],**v));self.assertEqual(status,200);r=json.loads(body);self.assertTrue(r['accepted'],r);return r['state']
 def table(self):return self.op('create_table',before='',title='Notas',header=True,cells=['a','b','c','d','e','f'])['tables'][0]
 def model(self,t):return dict(id=t['id'],title='Caderno 🙂',header=False,appearance=copy.deepcopy(self.state()['table_design_presets'][1]['appearance']),placement=dict(width=70,horizontal='center',text_align='right',vertical='center'))
 def test_apply_and_reopen_design_title_and_geometry(self):
  t=self.table();self.op('set_context',context='composition');m=self.model(t);m['appearance']['column_weights']=[40,35,25];self.op('edit_table_design',**m);self.op('save');s=self.op('reopen',discard_unsaved=False);self.assertEqual(s['tables'][0]['appearance'],m['appearance']);self.assertEqual(s['tables'][0]['title'],m['title']);self.assertEqual(s['composition_version'],3)
 def test_css_injection_is_refused_without_partial_changes(self):
  t=self.table();self.op('set_context',context='composition');m=self.model(t);m['appearance']['fill_color']='url(https://example.invalid)';before=self.state();status,_,raw=self.request('/api/request',dict(op='edit_table_design',expected=before['revision'],**m));self.assertEqual(status,200);r=json.loads(raw);self.assertFalse(r['accepted']);self.assertEqual(r['error'],'invalid_table_design');self.assertEqual(self.state(),before)
 def test_assets_offer_real_designer_and_cell_sizing(self):
  self.assertTrue(self.state()['table_design_supported']);_,_,js=self.request('/app.js',authenticated=False);_,_,css=self.request('/style.css',authenticated=False);self.assertIn(b'edit_table_design',js);self.assertIn(b'fitTableEditors',js);self.assertIn(b'setTableCellDetail',js);self.assertIn(b'Tamanho e posi',js);self.assertIn(b'design-layout',css)
if __name__=='__main__':unittest.main(verbosity=2)
