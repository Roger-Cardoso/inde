"""Math source is authoring data; rendering never mutates calculation state."""
import copy,json,signal,sqlite3,sys,tempfile,unittest
from pathlib import Path
import test_pilot as base
base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
class Equations(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes()
 def tearDown(self):self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
 def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
 def create(self,source=r'\frac{-b\pm\sqrt{b^2-4ac}}{2a}',mode='block'):
  if self.c.state['context']!='structure':self.yes(self.c.op('set_context',context='structure'))
  s=self.yes(self.c.op('create_equation',source=source,mode=mode,label='Equação',before=''));return next(e for e in s['equations'] if e['id']==s['selection'])
 def no(self,op,**args):
  before=copy.deepcopy(self.c.state);self.assertFalse(self.c.op(op,**args)['accepted']);self.assertEqual(self.c.state,before)
 def test_source_identity_mode_revision_and_old_format(self):
  self.assertEqual(self.c.state['composition_version'],1);e=self.create();self.assertEqual(self.c.state['composition_version'],5);self.assertEqual(e['source'],r'\frac{-b\pm\sqrt{b^2-4ac}}{2a}');self.yes(self.c.op('set_context',context='text'));s=self.yes(self.c.op('edit_equation',id=e['id'],source=r'\int_0^1 x^2\,dx',mode='inline',label='Integral'));self.assertEqual(s['equations'][0]['object'],e['object']);self.assertEqual(s['equations'][0]['revision'],1)
 def test_invalid_input_stale_context_and_extra_fields_are_atomic(self):
  for patch in [dict(source=''),dict(source='x'*16385),dict(mode='document'),dict(label='x'*257),dict(extra=1)]:
   values=dict(source='x',mode='block',label='L',before='');values.update(patch);self.no('create_equation',**values)
  e=self.create();self.no('edit_equation',id=e['id'],source='y',mode='block',label='L');self.yes(self.c.op('set_context',context='text'));self.no('edit_equation',id='missing',source='x',mode='block',label='L');before=copy.deepcopy(self.c.state);r=self.c.send(dict(op='edit_equation',expected=before['revision']-1,id=e['id'],source='y',mode='block',label='L'));self.assertFalse(r['accepted']);self.assertEqual(self.c.state,before)
 def test_incomplete_unsupported_and_markup_sources_are_preserved_as_data(self):
  for source in [r'\frac{',r'\unknown{x}',r'\href{https://example.invalid}{x}',r'\includegraphics{/etc/passwd}',r'<img src=x onerror=alert(1)>']:
   e=self.create(source);self.assertEqual(next(x for x in self.c.state['equations'] if x['id']==e['id'])['source'],source)
 def test_move_composition_style_remove_restore_and_undo(self):
  e=self.create();self.yes(self.c.op('set_context',context='composition'));self.yes(self.c.op('set_presentation',id=e['id'],width=80,horizontal='center',text_align='center',vertical='center'));self.yes(self.c.op('arrange_objects',id=e['id'],other=self.c.state['paragraphs'][0]['id'],side='right',first=.5,second=.5,gap=12));self.yes(self.c.op('set_context',context='structure'));self.yes(self.c.op('remove',id=e['id']));self.assertFalse(self.c.state['equations']);self.assertEqual(self.c.state['removed_items'][-1]['kind'],'equation');self.yes(self.c.op('undo'));self.assertEqual(self.c.state['equations'][0],e);self.yes(self.c.op('redo'));self.yes(self.c.op('restore_removed',id=e['id'],before=''));self.assertEqual(self.c.state['equations'][0],e);self.yes(self.c.op('move',id=e['id'],before=self.c.state['paragraphs'][0]['id'],intent='authorial'));self.assertEqual(self.c.state['author_order'][0],e['id'])
 def test_save_reopen_sigkill_and_recovery_keep_source_and_saved_separate(self):
  e=self.create();self.yes(self.c.op('save'));self.yes(self.c.op('set_context',context='text'));self.yes(self.c.op('edit_equation',id=e['id'],source=r'\sum_{i=1}^n i',mode='block',label='Soma'));self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2');self.assertEqual(self.c.state['equations'][0]['source'],r'\sum_{i=1}^n i');self.assertFalse(self.c.state['can_undo']);self.assertTrue(self.c.state['dirty']);self.yes(self.c.op('reopen',discard_unsaved=True));self.assertEqual(self.c.state['equations'][0]['source'],e['source'])
 def test_all_four_crash_windows_create_edit_remove_are_atomic(self):
  for op in ['create_equation','edit_equation','remove']:
   for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
    with self.subTest(op=op,stage=stage):
     folder=Path(self.tmp.name)/(op+stage);c=base.Client(folder,profile='p2d2');r=c.op('create_equation',source='x',mode='block',label='L',before='');self.assertTrue(r['accepted']);e=c.state['equations'][0]
     if op=='edit_equation':self.assertTrue(c.op('set_context',context='text')['accepted'])
     c.close();c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash');args=dict(id=e['id']) if op=='remove' else dict(source='y',mode='inline',label='M',**(dict(id=e['id']) if op=='edit_equation' else dict(before='')))
     with self.assertRaises(AssertionError):c.op(op,**args)
     c.close(expected=86);c=base.Client(folder,profile='p2d2')
     try:
      if op=='create_equation':self.assertEqual(len(c.state['equations']),2 if stage=='after_commit' else 1)
      elif op=='edit_equation':self.assertEqual(c.state['equations'][0]['source'],'y' if stage=='after_commit' else 'x')
      else:self.assertEqual(len(c.state['equations']),0 if stage=='after_commit' else 1)
     finally:c.close()
if __name__=='__main__':unittest.main(verbosity=2)
