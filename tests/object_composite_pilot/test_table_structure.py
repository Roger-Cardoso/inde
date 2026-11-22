"""Stable table identities, reversible axes and hidden intersection retention."""
import copy,json,math,signal,sqlite3,sys,tempfile,unittest
from pathlib import Path
import test_pilot as base
base.BINARY=str(Path(sys.argv.pop(1)).resolve());FAULT=str(Path(sys.argv.pop(1)).resolve())
class TableStructure(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory(prefix='inde-table-grid-');self.folder=Path(self.tmp.name)/'work';self.c=base.Client(self.folder,profile='p2d2');self.origin=(self.folder/'origem-documento.json').read_bytes();self.new(3,3)
 def tearDown(self):self.c.close();self.assertEqual((self.folder/'origem-documento.json').read_bytes(),self.origin);self.tmp.cleanup()
 def yes(self,r):self.assertTrue(r['accepted'],r);return r['state']
 def ctx(self,n):
  if self.c.state['context']!=n:self.yes(self.c.op('set_context',context=n))
 def new(self,rows,columns):
  self.ctx('structure');s=self.yes(self.c.op('create_table_sized',before='',title='Inventário',header=True,rows=rows,columns=columns));self.id=s['tables'][-1]['id'];return self.table()
 def table(self):return next(t for t in self.c.state['tables'] if t['id']==self.id)
 def cell(self,r,c):t=self.table();return next(x for x in t['cells'] if x['row']==t['row_ids'][r] and x['column']==t['column_ids'][c])
 def edit(self,r,c,text):cell=self.cell(r,c);self.ctx('text');self.yes(self.c.op('edit_cell',id=self.id,cell=cell['id'],text=text));return cell['id']
 def axis(self,op,axis,chosen='',before=''):
  self.ctx('structure');return self.yes(self.c.op(op,id=self.id,axis=axis,**({'before':before} if op=='insert_table_axis' else {'axis_id':chosen})))
 def style(self,**patch):
  self.ctx('composition');t=self.table();a=copy.deepcopy(t['appearance']);a.update(patch);return self.yes(self.c.op('edit_table_design',id=self.id,title=t['title'],header=t['header'],appearance=a,placement=dict(width=80,horizontal='center',text_align='left',vertical='start')))
 def no(self,op,**v):
  before=copy.deepcopy(self.c.state);r=self.c.op(op,**v);self.assertFalse(r['accepted'],r);self.assertEqual(self.c.state,before);return r
 def restart(self):self.c.close();self.c=base.Client(self.folder,profile='p2d2');return self.c.state
 def test_create_dimensions_and_old_creation_api_stays_2_by_3(self):
  t=self.table();self.assertEqual((len(t['row_ids']),len(t['column_ids']),len(t['cells'])),(3,3,9));self.assertTrue(all(c['text']=='' for c in t['cells']));self.assertEqual(self.c.state['composition_version'],4)
  self.yes(self.c.op('create_table',before='',title='Antiga',header=True,cells=['a','b','c','d','e','f']));self.assertEqual(len(self.c.state['tables'][-1]['cells']),6)
 def test_invalid_counts_types_and_fields_are_atomic(self):
  for r,c in [(0,3),(65,3),(2,0),(2,33),(-1,3),(2.5,3),(True,3)]:self.no('create_table_sized',before='',title='Ruim',header=True,rows=r,columns=c)
  self.no('create_table_sized',before='',title='Ruim',header=True,rows=2,columns=3,extra=1)
 def test_insert_before_after_end_preserves_existing_ids_text_and_styles(self):
  id=self.edit(1,1,'Mantido 漢🙂');old=copy.deepcopy(self.table());self.style(cells={id:{'fill':'#ffeecc'}},column_weights=[40,35,25]);self.axis('insert_table_axis','row',before=old['row_ids'][1]);self.assertEqual(self.cell(2,1)['id'],id);self.assertEqual(self.cell(2,1)['text'],'Mantido 漢🙂');self.axis('insert_table_axis','column',before=old['column_ids'][1]);t=self.table();self.assertEqual(self.cell(2,2)['id'],id);self.assertEqual(t['appearance']['cells'][id]['fill'],'#ffeecc');self.assertEqual((len(t['row_ids']),len(t['column_ids'])),(4,4));self.assertTrue(all(c['text']=='' for c in t['cells'] if c['id'] not in [c['id'] for c in old['cells']]))
 def test_remove_restore_row_preserves_ids_styles_height_and_payload(self):
  id=self.edit(1,1,'Espada');row=self.table()['row_ids'][1];self.style(row_heights={row:120},cells={id:{'bold':True}});old=copy.deepcopy(self.table());self.axis('remove_table_axis','row',row);self.assertNotIn(id,[c['id'] for c in self.table()['cells']]);self.assertNotIn(row,self.table()['appearance']['row_heights']);self.assertEqual(self.table()['structure']['removed_rows'][0]['filled_cells'],1);self.axis('restore_table_axis','row',row);self.assertEqual(self.table()['cells'],old['cells']);self.assertEqual(self.table()['appearance'],old['appearance'])
 def test_crossed_removed_axes_and_new_intersections_do_not_lose_content(self):
  id=self.edit(1,1,'Interseção');row=self.table()['row_ids'][1];col=self.table()['column_ids'][1];self.axis('remove_table_axis','row',row);self.axis('remove_table_axis','column',col);self.axis('insert_table_axis','row');freshrow=self.table()['row_ids'][-1];self.axis('insert_table_axis','column');freshcol=self.table()['column_ids'][-1];self.restart();self.axis('restore_table_axis','column',col);self.axis('restore_table_axis','row',row);self.assertEqual(self.cell(1,1)['id'],id);self.assertEqual(self.cell(1,1)['text'],'Interseção');t=self.table();self.assertEqual(len(t['cells']),16);self.assertEqual(next(c for c in t['cells'] if c['row']==freshrow and c['column']==freshcol)['text'],'')
 def test_visible_style_edits_preserve_retired_cell_and_row_overrides(self):
  id=self.edit(1,1,'Retida');row=self.table()['row_ids'][1];self.style(cells={id:{'color':'#123456','italic':True}},row_heights={row:123},column_weights=[40,35,25]);self.axis('remove_table_axis','row',row);self.style(font='mono',cells={},row_heights={});self.axis('restore_table_axis','row',row);a=self.table()['appearance'];self.assertEqual(a['cells'][id],{'color':'#123456','italic':True});self.assertEqual(a['row_heights'][row],123);self.assertEqual(a['font'],'mono')
 def test_visible_designer_cannot_modify_hidden_cell_or_row_directly(self):
  id=self.cell(1,1)['id'];row=self.table()['row_ids'][1];self.axis('remove_table_axis','row',row);self.ctx('composition');t=self.table();a=copy.deepcopy(t['appearance']);a['cells'][id]={'bold':True};args=dict(id=self.id,title=t['title'],header=t['header'],placement=dict(width=80,horizontal='center',text_align='left',vertical='start'));self.no('edit_table_design',appearance=a,**args);a=copy.deepcopy(t['appearance']);a['row_heights'][row]=100;self.no('edit_table_design',appearance=a,**args)
 def test_column_weights_project_merge_and_restore_by_identity(self):
  self.style(column_weights=[20,30,50]);col=self.table()['column_ids'][1];self.axis('remove_table_axis','column',col);w=self.table()['appearance']['column_weights'];self.assertAlmostEqual(w[0],100*20/70);self.assertAlmostEqual(w[1],100*50/70);self.style(column_weights=[50,50]);self.axis('restore_table_axis','column',col);w=self.table()['appearance']['column_weights'];self.assertAlmostEqual(w[0],35);self.assertAlmostEqual(w[1],30);self.assertAlmostEqual(w[2],35)
 def test_formula_binding_phrase_and_result_survive_retirement(self):
  cell=self.cell(1,1);self.ctx('structure');self.yes(self.c.op('create_formula_hub',name='Vida',model=dict(kind='free',source='declara HP = 100\nresultado HP'),before='',table=self.id,cell=cell['id'],prefix='Vida ',suffix='.'));before=copy.deepcopy(self.c.state);row=self.table()['row_ids'][1];self.axis('remove_table_axis','row',row);self.assertEqual(self.c.state['formula_cells'],before['formula_cells']);self.assertEqual(self.c.state['formulas'],before['formulas']);self.restart();self.axis('restore_table_axis','row',row);self.assertEqual(self.c.state['formula_text'],before['formula_text']);self.assertEqual(self.c.state['formula_cells'],before['formula_cells'])
 def test_retired_cells_reject_direct_edit_select_attach_and_rich_edit(self):
  cell=self.cell(1,1);row=self.table()['row_ids'][1];self.axis('remove_table_axis','row',row);self.ctx('text');self.no('edit_cell',id=self.id,cell=cell['id'],text='Não');self.no('select_cell',id=self.id,cell=cell['id']);self.no('edit_formula_text',target=cell['id'],runs=[{'kind':'text','text':'Não'}]);self.ctx('structure');self.no('create_formula_hub',name='Nao',model=dict(kind='free',source='resultado 1'),before='',table=self.id,cell=cell['id'],prefix='',suffix='')
 def test_last_axis_protected_and_single_column_designer_valid(self):
  self.new(1,1);t=self.table();self.no('remove_table_axis',id=self.id,axis='row',axis_id=t['row_ids'][0]);self.no('remove_table_axis',id=self.id,axis='column',axis_id=t['column_ids'][0]);self.style(column_weights=[100]);self.assertEqual(self.table()['appearance']['column_weights'],[100])
 def test_retention_limit_includes_retired_axes_and_max_shape_valid(self):
  t=self.new(64,32);self.assertEqual(len(t['cells']),2048);row=t['row_ids'][5];self.axis('remove_table_axis','row',row);self.no('insert_table_axis',id=self.id,axis='row',before='');self.no('insert_table_axis',id=self.id,axis='column',before='');self.axis('restore_table_axis','row',row);self.assertEqual(len(self.table()['cells']),2048)
 def test_invalid_axis_target_foreign_inactive_and_stale_are_atomic(self):
  t=self.table();row=t['row_ids'][1];self.axis('remove_table_axis','row',row)
  for op,extra in [('insert_table_axis',dict(before=row)),('remove_table_axis',dict(axis_id=row)),('restore_table_axis',dict(axis_id=t['row_ids'][0]))]:self.no(op,id=self.id,axis='row',**extra)
  self.no('remove_table_axis',id=self.id,axis='diagonal',axis_id=row);self.no('remove_table_axis',id=self.id,axis='row',axis_id='missing');self.no('insert_table_axis',id=self.id,axis='row',before='',extra=1);before=copy.deepcopy(self.c.state);r=self.c.send(dict(op='insert_table_axis',expected=before['revision']-1,id=self.id,axis='row',before=''));self.assertFalse(r['accepted']);self.assertEqual(self.c.state,before)
 def test_hidden_command_is_not_consumed_or_executed_by_removal(self):
  self.ctx('structure');self.yes(self.c.op('create_formula_hub',name='Vida',model=dict(kind='free',source='declara HP = 100\nresultado HP'),before='',table='',cell='',prefix='',suffix=''));cell=self.cell(1,1);self.yes(self.c.op('create_formula_hub',name='Golpe',model=dict(kind='free',source='alterar Vida.HP = 80'),before='',table=self.id,cell=cell['id'],prefix='',suffix=''));row=self.table()['row_ids'][1];self.axis('remove_table_axis','row',row);self.restart();self.assertEqual(self.c.state['formulas'][0]['display'],'100');self.assertFalse(self.c.state['formulas'][1]['consumed']);self.axis('restore_table_axis','row',row);self.assertFalse(self.c.state['formulas'][1]['consumed'])
 def test_insert_remove_restore_each_one_undo_and_outer_arrangement_kept(self):
  self.ctx('composition');self.yes(self.c.op('arrange_objects',id=self.id,other=self.c.state['paragraphs'][2]['id'],side='right',first=.4,second=.6,gap=18));rows=copy.deepcopy(self.c.state['rows']);old=copy.deepcopy(self.table());self.axis('insert_table_axis','row');self.yes(self.c.op('undo'));self.assertEqual(self.table(),old);self.yes(self.c.op('redo'));self.assertEqual(len(self.table()['row_ids']),4);id=self.table()['row_ids'][1];self.axis('remove_table_axis','row',id);self.yes(self.c.op('undo'));self.assertIn(id,self.table()['row_ids']);self.yes(self.c.op('redo'));self.assertNotIn(id,self.table()['row_ids']);self.axis('restore_table_axis','row',id);self.assertEqual(self.c.state['rows'],rows)
 def test_undo_formula_created_in_new_axis_keeps_earlier_insert_history(self):
  before=copy.deepcopy(self.table());self.axis('insert_table_axis','row');cell=self.cell(3,0);self.yes(self.c.op('create_formula_hub',name='Nova',model=dict(kind='free',source='resultado 42'),before='',table=self.id,cell=cell['id'],prefix='Valor ',suffix='.'));self.yes(self.c.op('undo'));self.assertTrue(self.c.state['can_undo']);self.yes(self.c.op('undo'));self.assertEqual(self.table(),before);self.yes(self.c.op('redo'));self.yes(self.c.op('redo'));self.assertEqual(self.c.state['formula_cells'][cell['id']],self.c.state['formulas'][0]['id'])
 def test_sigkill_keeps_retired_axes_full_grid_and_saved_snapshot_five(self):
  row=self.table()['row_ids'][1];self.edit(1,0,'Conservado');self.axis('remove_table_axis','row',row);before=copy.deepcopy(self.c.state);self.c.process.kill();self.c.close(expected=-signal.SIGKILL);self.c=base.Client(self.folder,profile='p2d2')
  for k in ['tables','formula_cells','formula_text','presentation','rows','source_body_sha256']:self.assertEqual(self.c.state[k],before[k])
  self.assertFalse(self.c.state['can_undo']);self.yes(self.c.op('save'));self.restart();self.axis('restore_table_axis','row',row);self.assertEqual(self.cell(1,0)['text'],'Conservado')
  with sqlite3.connect(self.folder/'obj01-piloto.sqlite3') as db:self.assertEqual(db.execute('PRAGMA user_version').fetchone()[0],5);self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0],'ok')
 def test_four_crash_windows_insert_remove_restore_are_atomic(self):
  for op in ['insert_table_axis','remove_table_axis','restore_table_axis']:
   for stage in ['after_blobs','after_snapshot','before_commit','after_commit']:
    with self.subTest(op=op,stage=stage):
     folder=Path(self.tmp.name)/(op+stage);c=base.Client(folder,profile='p2d2');self.assertTrue(c.op('create_table_sized',before='',title='T',header=True,rows=3,columns=2)['accepted']);t=c.state['tables'][0];row=t['row_ids'][1]
     if op=='restore_table_axis':self.assertTrue(c.op('remove_table_axis',id=t['id'],axis='row',axis_id=row)['accepted'])
     c.close();c=base.Client(folder,profile='p2d2',binary=FAULT,fault='checkpoint:'+stage+':crash')
     with self.assertRaises(AssertionError):c.op(op,id=t['id'],axis='row',**({'before':''} if op=='insert_table_axis' else {'axis_id':row}))
     c.close(expected=86);c=base.Client(folder,profile='p2d2')
     try:self.assertEqual(len(c.state['tables'][0]['row_ids']),4 if op=='insert_table_axis' and stage=='after_commit' else 2 if (op=='remove_table_axis' and stage=='after_commit') or (op=='restore_table_axis' and stage!='after_commit') else 3)
     finally:c.close()
if __name__=='__main__':unittest.main(verbosity=2)
