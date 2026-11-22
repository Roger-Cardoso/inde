"""Real object composition, reversible removal, owner groups and private v2."""
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
base.BINARY=str(Path(sys.argv.pop(1)).resolve())
FAULT=str(Path(sys.argv.pop(1)).resolve())

class Composition(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='obj01-composition-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes()
 def tearDown(self):
  self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
 def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
 def ctx(self,n):
  if self.c.state['context']!=n:self.yes(self.c.op('set_context',context=n))
 def no(self,op,**v):
  before=copy.deepcopy(self.c.state);r=self.c.op(op,**v);self.assertFalse(r['accepted'],r);self.assertEqual(self.c.state,before);return r
 def objects(self):
  self.ctx('structure');self.yes(self.c.image(before=''));image=self.c.state['figures'][-1]['id']
  t=self.yes(self.c.op('create_table',before='',title='Notas',header=True,cells=['a','b','c','d','e','f']))['tables'][-1]
  self.yes(self.c.op('create_formula_hub',name='Vida',model={'kind':'free','source':'declara HP = 100\nresultado HP'},before='',table='',cell='',prefix='Vida: ',suffix='.'))
  return [self.c.state['paragraphs'][0]['id'],image,t['id'],self.c.state['formula_uses'][-1]['id']]
 def pair(self,a,b,side='left',**extra):
  self.ctx('composition');return self.yes(self.c.op('arrange_objects',id=a,other=b,side=side,first=.4,second=.6,gap=24,**extra))
 def style(self,id,**extra):
  self.ctx('composition');v=dict(id=id,width=65,horizontal='end',text_align='right',vertical='center');v.update(extra);return self.yes(self.c.op('set_presentation',**v))
 def restore(self,id,before=''):
  self.ctx('structure');return self.yes(self.c.op('restore_removed',id=id,before=before))
 def restart(self):self.c.close();self.c=base.Client(self.folder,profile='p2d2');return self.c.state
 def test_every_distinct_object_kind_pair_and_order_independence(self):
  ids=self.objects();author=list(self.c.state['author_order'])
  for i,a in enumerate(ids):
   for j,b in enumerate(ids):
    if i==j:continue
    with self.subTest(i=i,j=j):
     s=self.pair(a,b);self.assertEqual(s['rows'][0]['children'],[a,b]);self.assertEqual(s['author_order'],author);self.assertEqual(s['composition_version'],2);self.yes(self.c.op('stack',id=a));self.assertEqual(self.c.state['rows'],[])
 def test_text_text_including_same_protected_group_and_swap(self):
  p=[p['id'] for p in self.c.state['paragraphs']];s=self.pair(p[1],p[2]);self.assertEqual(s['author_order'],p);self.assertEqual(s['rows'][0]['children'],p[1:]);s=self.pair(p[1],p[2],'right');self.assertEqual(s['rows'][0]['children'],[p[2],p[1]]);self.assertEqual(s['author_order'],p);self.assertFalse(s['aligned'])
 def test_width_alignment_and_vertical_roundtrip_undo_and_reset(self):
  ids=self.objects();s=self.style(ids[0]);p=copy.deepcopy(s['presentation']);self.yes(self.c.op('save'));s=self.restart();self.assertEqual(s['presentation'],p);self.ctx('composition');self.yes(self.c.op('reset_presentation',id=ids[0]));self.assertEqual(self.c.state['presentation'],{});self.yes(self.c.op('undo'));self.assertEqual(self.c.state['presentation'],p);self.yes(self.c.op('redo'));self.assertEqual(self.c.state['presentation'],{})
 def test_invalid_geometry_types_fields_self_and_inactive_are_atomic(self):
  a,b=[p['id'] for p in self.c.state['paragraphs'][:2]];self.ctx('composition')
  for patch in [{'width':19},{'width':101},{'horizontal':'diagonal'},{'text_align':'html'},{'vertical':'bottom'},{'width':'50'}]:
   v=dict(id=a,width=50,horizontal='center',text_align='justify',vertical='end');v.update(patch);self.no('set_presentation',**v)
  for patch in [{'other':a},{'side':'above'},{'first':0},{'gap':257},{'id':'missing'},{'first':.8}]:
   v=dict(id=a,other=b,side='left',first=.5,second=.5,gap=20);v.update(patch);self.no('arrange_objects',**v)
  self.no('set_presentation',id=a,width=50,horizontal='center',text_align='left',vertical='start',extra=True);self.assertEqual(self.c.state['composition_version'],1)
 def test_remove_whole_text_group_preserves_source_and_restores(self):
  original=copy.deepcopy(self.c.state);ids=[p['id'] for p in original['paragraphs']];group=original['text_groups'][1]['id'];self.pair(ids[1],ids[2]);self.ctx('structure');s=self.yes(self.c.op('remove',id=ids[2]));self.assertEqual(s['author_order'],ids[:1]);self.assertEqual(s['rows'],[]);self.assertEqual(s['source_body_sha256'],original['source_body_sha256']);self.assertEqual(s['private_alternatives'],1);self.assertEqual(s['visibility_profiles'],1);self.assertEqual(s['removed_items'][0]['count'],2)
  s=self.restore(group);self.assertEqual(s['author_order'],ids);self.assertEqual(s['removed_items'],[]);self.yes(self.c.op('undo'));self.assertEqual(len(self.c.state['paragraphs']),1);self.yes(self.c.op('redo'));self.assertEqual(len(self.c.state['paragraphs']),3)
 def test_remove_all_text_and_restore_after_reopen_without_physical_deletion(self):
  original=copy.deepcopy(self.c.state);group=original['text_groups'];self.ctx('structure')
  for g in group:self.yes(self.c.op('remove',id=g['occurrences'][0]))
  self.assertEqual(self.c.state['paragraphs'],[]);self.assertEqual(self.c.state['author_order'],[]);self.yes(self.c.op('save'));s=self.restart();self.assertEqual(s['paragraphs'],[]);self.assertEqual(len(s['removed_items']),2)
  self.restore(group[1]['id']);s=self.restore(group[0]['id'],group[1]['occurrences'][0]);self.assertEqual([p['text'] for p in s['paragraphs']],[p['text'] for p in original['paragraphs']])
 def test_restore_partial_group_destination_is_refused(self):
  original=copy.deepcopy(self.c.state);self.yes(self.c.op('remove',id=original['paragraphs'][0]['id']));self.no('restore_removed',id=original['text_groups'][0]['id'],before=original['paragraphs'][2]['id']);self.assertEqual(len(self.c.state['paragraphs']),2)
 def test_remove_from_divergent_visual_preserves_other_order_and_styles(self):
  ids=self.objects();s=self.pair(ids[0],ids[2],'right');self.assertFalse(s['aligned']);self.style(ids[2]);self.ctx('structure');s=self.yes(self.c.op('remove',id=ids[2]));self.assertNotIn(ids[2],s['author_order']);self.assertNotIn(ids[2],s['visual_order']);self.assertEqual(s['rows'],[]);self.assertIn(ids[2],s['presentation']);self.yes(self.c.op('undo'));self.assertEqual(self.c.state['rows'][0]['children'],[ids[2],ids[0]])
 def test_remove_restore_image_table_and_formula_keep_content_and_bindings(self):
  ids=self.objects();source=copy.deepcopy(self.c.state);table=source['tables'][0];formula=source['formulas'][0]
  self.ctx('structure');self.yes(self.c.op('attach_formula',id=formula['id'],before='',table=table['id'],cell=table['cells'][0]['id']))
  for id in ids[1:]:
   self.style(id);self.ctx('structure');self.yes(self.c.op('remove',id=id))
  self.assertEqual(len(self.c.state['removed_items']),3);self.assertEqual(len(self.c.state['formulas']),1)
  for id in ids[1:]:self.restore(id)
  s=self.c.state;self.assertEqual(s['figures'][0]['sha256'],source['figures'][0]['sha256']);self.assertEqual(s['tables'][0]['cells'],source['tables'][0]['cells']);self.assertEqual(s['formula_cells'][table['cells'][0]['id']],formula['id']);self.assertEqual(s['formula_text'][ids[-1]],source['formula_text'][ids[-1]]);self.assertEqual(len(s['presentation']),3)
 def test_consumed_command_is_not_a_restorable_removed_item(self):
  self.objects();self.yes(self.c.op('create_formula_hub',name='Dano',model={'kind':'free','source':'alterar Vida.HP = 80'},before='',table='',cell='',prefix='',suffix=''));f=self.c.state['formulas'][-1];key=self.c.state['formula_uses'][-1]['id'];self.ctx('text');self.yes(self.c.op('execute_formula',id=f['id']));self.assertNotIn(key,[r['id'] for r in self.c.state['removed_items']]);self.ctx('structure');self.no('restore_removed',id=key,before='');self.assertEqual(self.c.state['formulas'][0]['display'],'80')
 def test_generic_move_text_authorial_uses_whole_owner_group(self):
  s=copy.deepcopy(self.c.state);p=s['paragraphs'];self.yes(self.c.op('move',id=p[2]['id'],before=p[0]['id'],intent='authorial'));self.assertEqual(self.c.state['author_order'],[p[1]['id'],p[2]['id'],p[0]['id']]);self.assertNotEqual(self.c.state['source_body_sha256'],s['source_body_sha256']);self.assertEqual(self.c.state['source_sha256'],s['source_sha256']);self.assertEqual(self.c.state['private_alternatives'],1);self.yes(self.c.op('undo'));self.assertEqual(self.c.state['paragraphs'],p)
 def test_visual_text_move_does_not_change_owner_and_remove_then_reorder(self):
  p=copy.deepcopy(self.c.state['paragraphs']);body=self.c.state['source_body_sha256'];self.yes(self.c.op('move',id=p[2]['id'],before=p[0]['id'],intent='visual'));self.assertEqual(self.c.state['source_body_sha256'],body);self.assertEqual(self.c.state['author_order'],[x['id'] for x in p]);self.ctx('composition');self.yes(self.c.op('restore'));self.ctx('structure');self.yes(self.c.op('remove',id=p[0]['id']));g=self.c.state['text_groups'][0];self.yes(self.c.op('reorder_text',group=g['id'],before=''));self.assertEqual(len(self.c.state['paragraphs']),2)
 def test_old_profile_and_snapshot_shape_conserved_until_extension(self):
  self.yes(self.c.op('save'))
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:
   s=json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0]);self.assertEqual(s['format'],'INDE-OBJ01-COMPOSITE-P2D2-1');self.assertNotIn('composition',s);self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0],5)
  with tempfile.TemporaryDirectory() as tmp:
   c=base.Client(Path(tmp)/'old',profile='p2b')
   try:r=c.op('arrange_objects',id=c.state['paragraphs'][0]['id'],other=c.state['paragraphs'][1]['id'],side='left',first=.5,second=.5,gap=20);self.assertFalse(r['accepted']);self.assertEqual(r['error'],'p2d2_profile_required')
   finally:c.close()
 def test_context_and_stale_revision_do_not_change_composition(self):
  a,b=[p['id'] for p in self.c.state['paragraphs'][:2]];self.no('arrange_objects',id=a,other=b,side='left',first=.5,second=.5,gap=20);self.ctx('composition');before=copy.deepcopy(self.c.state);r=self.c.send({'op':'set_presentation','expected':before['revision']-1,'id':a,'width':50,'horizontal':'center','text_align':'center','vertical':'start'});self.assertFalse(r['accepted']);self.assertEqual(self.c.state,before)
 def test_sigkill_keeps_removal_presentation_and_general_rows(self):
  ids=self.objects();self.style(ids[2]);self.pair(ids[1],ids[2]);self.ctx('structure');self.yes(self.c.op('remove',id=ids[0]));before=copy.deepcopy(self.c.state);self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2')
  for k in ['paragraphs','rows','presentation','removed_items','author_order','visual_order','formula_cells']:self.assertEqual(self.c.state[k],before[k])
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0],'ok');self.assertEqual(db.execute('PRAGMA foreign_key_check').fetchall(),[])
 def test_crash_windows_removal_and_restore_are_atomic(self):
  for operation in ['remove','restore_removed']:
   for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
    with self.subTest(operation=operation,stage=stage):
     folder=Path(self.tmp.name)/(operation+stage);c=base.Client(folder,profile='p2d2');p=c.state['paragraphs'][0]['id'];g=c.state['text_groups'][0]['id']
     if operation=='restore_removed':self.assertTrue(c.op('remove',id=p)['accepted'])
     c.close();c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash')
     with self.assertRaises(AssertionError):c.op(operation,**({'id':p} if operation=='remove' else {'id':g,'before':''}))
     c.close(expected=86);c=base.Client(folder,profile='p2d2')
     try:self.assertEqual(p in c.state['author_order'],(stage!='after_commit') if operation=='remove' else (stage=='after_commit'))
     finally:c.close()

if __name__=='__main__':unittest.main(verbosity=2)
