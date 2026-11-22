"""Real table appearance, dimensions, typed overrides, history and recovery."""
import copy
import hashlib
import json
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import test_pilot as base
base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
class TableDesign(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='inde-table-design-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes();self.yes(self.c.op('create_table',before='',title='Inventário',header=True,cells=['Item','Estado','Valor','Espada','Íntegra','10']));self.id=self.c.state['tables'][0]['id']
 def tearDown(self):self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
 def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
 def ctx(self,n):
  if self.c.state['context']!=n:self.yes(self.c.op('set_context',context=n))
 def table(self):return next(t for t in self.c.state['tables'] if t['id']==self.id)
 def model(self):return dict(id=self.id,title=self.table()['title'],header=self.table()['header'],appearance=copy.deepcopy(self.table()['appearance']),placement=copy.deepcopy(self.c.state['presentation'].get(self.id,dict(width=100,horizontal='start',text_align='left',vertical='start'))))
 def edit(self,**patch):self.ctx('composition');m=self.model();m.update(patch);return self.c.op('edit_table_design',**m)
 def no(self,model):
  self.ctx('composition');before=copy.deepcopy(self.c.state);r=self.c.op('edit_table_design',**model);self.assertFalse(r['accepted'],r);self.assertEqual(self.c.state,before);return r
 def restart(self):self.c.close();self.c=base.Client(self.folder,profile='p2d2');return self.c.state
 def test_defaults_and_four_presets_are_real_valid_models(self):
  self.assertTrue(self.c.state['table_design_supported']);self.assertEqual(len(self.c.state['table_design_presets']),4);self.assertEqual(self.table()['appearance'],self.c.state['table_design_defaults']);cells=copy.deepcopy(self.table()['cells'])
  for p in self.c.state['table_design_presets']:
   with self.subTest(p=p['id']):self.yes(self.edit(appearance=p['appearance']));self.assertEqual(self.table()['appearance'],p['appearance']);self.assertEqual(self.table()['cells'],cells)
 def test_title_header_style_and_placement_share_one_undo(self):
  before=copy.deepcopy(self.table());self.ctx('text');self.yes(self.c.op('select_cell',id=self.id,cell=before['cells'][0]['id']));self.ctx('composition');s=self.yes(self.edit(title='Armas · 漢🙂',header=False,appearance=self.c.state['table_design_presets'][1]['appearance'],placement=dict(width=70,horizontal='center',text_align='right',vertical='end')));self.assertEqual(s['selected_cell'],'');self.assertEqual(self.table()['payload_revision'],1);self.assertEqual(s['composition_version'],3);self.yes(self.c.op('undo'));self.assertEqual(self.table(),before);self.assertNotIn(self.id,self.c.state['presentation']);self.yes(self.c.op('redo'));self.assertEqual(self.table()['title'],'Armas · 漢🙂');self.assertFalse(self.table()['header']);self.assertEqual(self.c.state['presentation'][self.id]['width'],70)
 def test_colors_fonts_borders_and_sizes_are_closed_and_atomic(self):
  for patch in [{'fill_color':'red'},{'text_color':'#000000;display:none'},{'header_fill':'#fff'},{'font':'url(https://x)'},{'font_size':9},{'font_size':33},{'line_height':0},{'padding_x':33},{'padding_y':-1},{'min_row_height':241},{'border_width':5},{'border_kind':'free-css'},{'border_style':'double'},{'cell_vertical':'center'},{'show_title':'false'},{'extra':0}]:
   with self.subTest(patch=patch):m=self.model();m['appearance'].update(patch);self.no(m)
  self.assertEqual(self.c.state['composition_version'],1)
 def test_column_weights_and_per_row_heights_roundtrip_without_rounding(self):
  a=self.model()['appearance'];a.update(column_weights=[17.125,32.875,50],row_heights={self.table()['row_ids'][1]:123.5},min_row_height=37.5,font_size=15.25);self.yes(self.edit(appearance=a));self.yes(self.c.op('save'));self.restart();self.assertEqual(self.table()['appearance'],a)
 def test_wrong_column_count_weights_and_row_identity_are_rejected(self):
  for values in [dict(column_weights=[50,50]),dict(column_weights=[20,30,40]),dict(column_weights=[-10,50,60]),dict(column_weights=[0,50,50]),dict(row_heights={'missing':50}),dict(row_heights={self.table()['row_ids'][0]:481})]:
   m=self.model();m['appearance'].update(values);self.no(m)
 def test_partial_cell_overrides_and_bold_false_are_preserved(self):
  a=self.model()['appearance'];cell=self.table()['cells'][0]['id'];a['cells']={cell:dict(align='right',vertical='middle',fill='#012345',color='#abcdef',bold=False,italic=True)};self.yes(self.edit(appearance=a));self.assertEqual(self.table()['appearance']['cells'][cell]['bold'],False);self.yes(self.c.op('save'));self.restart();self.assertEqual(self.table()['appearance'],a)
 def test_cell_overrides_cannot_point_outside_table_or_inject_styles(self):
  own=self.table()['cells'][0]['id']
  for cells in [{'missing':{'bold':True}},{own:{}},{own:{'align':'url(x)'}},{own:{'fill':'transparent'}},{own:{'bold':1}},{own:{'font_size':20}}]:
   m=self.model();m['appearance']['cells']=cells;self.no(m)
 def test_scoped_row_column_overrides_keep_formula_and_authored_text(self):
  t=copy.deepcopy(self.table());self.yes(self.c.op('create_formula_hub',name='Vida',model=dict(kind='free',source='declara HP = 100\nresultado HP'),before='',table=self.id,cell=t['cells'][5]['id'],prefix='Vida: ',suffix='.'));before=copy.deepcopy(self.c.state);a=self.model()['appearance']
  a['cells']={c['id']:{'align':'right'} for c in t['cells'] if c['column']==t['column_ids'][2]};a['cells'][t['cells'][5]['id']]['fill']='#fff0cc';self.yes(self.edit(appearance=a));s=self.c.state
  for key in ['formulas','formula_cells','formula_text','source_sha256','source_body_sha256']:self.assertEqual(s[key],before[key])
  self.assertEqual(self.table()['cells'],t['cells']);self.assertEqual(len(self.table()['appearance']['cells']),2)
 def test_remove_restore_preserves_style_and_column_sizes(self):
  a=self.model()['appearance'];a.update(column_weights=[40,30,30],font='serif',caption_side='bottom',show_title=False);self.yes(self.edit(appearance=a));self.ctx('structure');self.yes(self.c.op('remove',id=self.id));self.restart();self.yes(self.c.op('restore_removed',id=self.id,before=''));self.assertEqual(self.table()['appearance'],a);self.assertEqual(self.table()['title'],'Inventário')
 def test_style_edit_keeps_existing_side_by_side_arrangement_and_reading(self):
  self.ctx('composition');p=self.c.state['paragraphs'][2]['id'];self.yes(self.c.op('arrange_objects',id=self.id,other=p,side='right',first=.4,second=.6,gap=18));before=copy.deepcopy(self.c.state);self.yes(self.edit(appearance=self.c.state['table_design_presets'][3]['appearance']));self.assertEqual(self.c.state['rows'],before['rows']);self.assertEqual(self.c.state['author_order'],before['author_order']);self.assertEqual(self.c.state['visual_order'],before['visual_order'])
 def test_reset_to_defaults_is_reversible_and_does_not_reset_placement(self):
  self.yes(self.edit(appearance=self.c.state['table_design_presets'][3]['appearance'],placement=dict(width=65,horizontal='end',text_align='center',vertical='center')));self.yes(self.edit(appearance=self.c.state['table_design_defaults']));self.assertEqual(self.table()['appearance'],self.c.state['table_design_defaults']);self.assertEqual(self.c.state['presentation'][self.id]['width'],65);self.yes(self.c.op('undo'));self.assertEqual(self.table()['appearance']['fill_color'],'#192a36')
 def test_legacy_format_one_and_two_conserved_until_design_and_schema_is_five(self):
  self.yes(self.c.op('save'))
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:self.assertEqual(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['format'],'INDE-OBJ01-COMPOSITE-P2D2-1')
  self.ctx('composition');self.yes(self.c.op('set_presentation',id=self.id,width=80,horizontal='start',text_align='left',vertical='start'));self.yes(self.c.op('save'))
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:self.assertEqual(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['format'],'INDE-OBJ01-COMPOSITE-P2D2-2')
  self.yes(self.edit());self.yes(self.c.op('save'))
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0],5);self.assertEqual(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['format'],'INDE-OBJ01-COMPOSITE-P2D2-3')
 def test_context_stale_fields_and_inactive_targets_refuse_atomically(self):
  m=self.model();before=copy.deepcopy(self.c.state);r=self.c.op('edit_table_design',**m);self.assertEqual(r['error'],'editing_context_required');self.assertEqual(self.c.state,before);self.ctx('composition');before=copy.deepcopy(self.c.state);r=self.c.send(dict(op='edit_table_design',expected=before['revision']-1,**m));self.assertFalse(r['accepted']);self.assertEqual(self.c.state,before);self.no(dict(m,extra=1));self.no(dict(m,id=self.c.state['paragraphs'][0]['id']));self.ctx('structure');self.yes(self.c.op('remove',id=self.id));self.no(m)
 def test_identical_apply_does_not_add_history_or_payload_revisions(self):
  self.yes(self.edit());before=copy.deepcopy(self.c.state);self.yes(self.edit());self.assertEqual(self.c.state['history_entries'],before['history_entries']);self.assertEqual(self.table()['payload_revision'],before['tables'][0]['payload_revision'])
 def test_sigkill_restores_unsaved_design_and_keeps_saved_revision_zero(self):
  a=self.model()['appearance'];a.update(font='mono',column_weights=[20,40,40],row_heights={self.table()['row_ids'][0]:90});self.yes(self.edit(title='Arquivo íntegro 🙂',appearance=a));before=copy.deepcopy(self.c.state);self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2')
  for key in ['tables','rows','presentation','author_order','visual_order','formula_cells','source_body_sha256']:self.assertEqual(self.c.state[key],before[key])
  self.assertEqual(self.c.state['saved_revision'],0);self.assertFalse(self.c.state['can_undo'])
 def test_four_crash_windows_keep_style_title_header_and_placement_atomic(self):
  for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
   with self.subTest(stage=stage):
    folder=Path(self.tmp.name)/stage;c=base.Client(folder,profile='p2d2');self.assertTrue(c.op('create_table',before='',title='Antes',header=True,cells=['a','b','c','d','e','f'])['accepted']);t=c.state['tables'][0];m=dict(id=t['id'],title='Depois',header=False,appearance=copy.deepcopy(c.state['table_design_presets'][3]['appearance']),placement=dict(width=60,horizontal='center',text_align='right',vertical='end'));self.assertTrue(c.op('set_context',context='composition')['accepted']);c.close();c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash')
    with self.assertRaises(AssertionError):c.op('edit_table_design',**m)
    c.close(expected=86);c=base.Client(folder,profile='p2d2')
    try:self.assertEqual(c.state['tables'][0]['title'],'Depois' if stage=='after_commit' else 'Antes');self.assertEqual(c.state['composition_version'],3 if stage=='after_commit' else 1);self.assertEqual(t['id'] in c.state['presentation'],stage=='after_commit')
    finally:c.close()
if __name__=='__main__':unittest.main(verbosity=2)
