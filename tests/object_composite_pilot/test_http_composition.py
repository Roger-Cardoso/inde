"""Authenticated transport for shared composition and reversible removal."""
import json
from pathlib import Path
import sys
import unittest
import test_http as base
base.BINARY=Path(sys.argv.pop(1)).resolve()
class CompositionHttp(base.HttpPilot):
 profile='p2d2'
 def op(self,op,**data):
  status,_,body=self.request('/api/request',dict(op=op,expected=self.state()['revision'],**data));self.assertEqual(status,200);r=json.loads(body);self.assertTrue(r['accepted'],r);return r['state']
 def test_text_table_row_style_remove_and_restore(self):
  s=self.state();p=s['paragraphs'][0]['id'];t=self.op('create_table',before='',title='Notas',header=True,cells=['a','b','c','d','e','f'])['tables'][0];self.op('set_context',context='composition');s=self.op('arrange_objects',id=p,other=t['id'],side='right',first=.35,second=.65,gap=18);self.assertEqual(s['rows'][0]['children'],[t['id'],p]);s=self.op('set_presentation',id=p,width=80,horizontal='center',text_align='justify',vertical='end');self.assertEqual(s['presentation'][p]['width'],80);self.op('set_context',context='structure');s=self.op('remove',id=p);self.assertEqual(len(s['paragraphs']),2);self.assertEqual(s['rows'],[]);s=self.op('restore_removed',id=s['removed_items'][0]['id'],before=t['id']);self.assertEqual(len(s['paragraphs']),3);self.assertEqual(s['presentation'][p]['text_align'],'justify')
 def test_invalid_geometry_is_atomic_and_save_reopen_preserves_v2(self):
  p=self.state()['paragraphs'][0]['id'];self.op('set_context',context='composition');before=self.state();status,_,body=self.request('/api/request',dict(op='set_presentation',expected=before['revision'],id=p,width=150,horizontal='start',text_align='left',vertical='start'));self.assertEqual(status,200);self.assertEqual(json.loads(body)['error'],'invalid_presentation');self.assertEqual(self.state(),before);self.op('set_presentation',id=p,width=55,horizontal='center',text_align='center',vertical='center');self.op('save');s=self.op('reopen',discard_unsaved=False);self.assertEqual(s['composition_version'],2);self.assertEqual(s['presentation'][p]['width'],55)
 def test_assets_expose_removal_restore_and_shared_alignment(self):
  self.assertTrue(self.state()['object_composition_supported']);_,_,html=self.request('/',authenticated=False);_,_,js=self.request('/app.js',authenticated=False);self.assertIn(b'Itens removidos',html);self.assertIn(b'object-text-align',html);self.assertIn(b'arrange_objects',js);self.assertIn(b'restore_removed',js);self.assertIn(b'decorateItem',js)
if __name__=='__main__':unittest.main(verbosity=2)
