void (async()=>{
  const wait=async test=>{for(let i=0;i<200;i++){try{if(test())return;}catch{}await new Promise(r=>setTimeout(r,50));}throw new Error('native UI timeout');};
  const check=(v,m)=>{if(!v)throw new Error(m);};
  const run=async(op,args={})=>check(await action(op,args),'operation '+op);
  await wait(()=>typeof state!=='undefined'&&state?.native_document&&!busy);
  check(!window.location.href.startsWith('http'),'native editor used HTTP');
  if(document.body.dataset.smokeReopen==='yes'){
    check(state.equations.length===3,'reopen equations');check(!state.dirty,'main project save did not confirm object document');check(state.figures.length===1,'reopen image');check(state.tables.length===1,'reopen table');check(state.formulas.filter(f=>!f.consumed).length===1&&state.formulas.find(f=>!f.command).display==='80','reopen formula');check(document.querySelectorAll('.equation-item .katex').length===3&&document.querySelectorAll('.equation-item math').length===3,'reopen rendering');
    document.body.dataset.nativeSmoke='reopened';return;
  }
  openEquation(null);check(hasDrafts()&&document.querySelector('#equation-dialog[open]'),'math draft did not open');document.body.dataset.nativeSmoke='draft';
  await wait(()=>document.body.dataset.nativeDraftChecked==='yes');
  Array.from(document.querySelectorAll('#equation-dialog button')).find(b=>b.textContent==='Cancelar').click();
  const bytes=new Uint8Array(await (await fetch('inde://ui/sample.png')).arrayBuffer());let binary='';for(const byte of bytes)binary+=String.fromCharCode(byte);
  await run('import',{data:btoa(binary),before:'',caption:'Travessia',alt:'Figura sintética em verde',decorative:false});
  await wait(()=>document.querySelector('figure img')?.naturalWidth>0);
  const samples=[['Segundo grau',String.raw`x=\frac{-b\pm\sqrt{b^2-4ac}}{2a}`],['Soma',String.raw`\sum_{i=1}^{n}i=\frac{n(n+1)}2`],['Matriz',String.raw`\begin{pmatrix}a&b\\c&d\end{pmatrix}`]];
  for(const [label,source] of samples)await run('create_equation',{source,label,mode:'block',before:''});
  const equation=state.selection;await run('remove',{id:equation});await run('undo');check(state.equations.length===3,'undo equation');await run('redo');await run('restore_removed',{id:equation,before:''});
  await run('create_table_sized',{before:'',title:'Personagens',header:true,rows:2,columns:3});const table=state.tables[0],cell=table.cells.find(c=>c.row===table.row_ids[1]&&c.column===table.column_ids[2]);
  await run('set_context',{context:'text'});
  for(const [index,text] of [[0,'Nome'],[1,'Papel'],[2,'Vida'],[3,'Armand'],[4,'Guia']])await run('edit_cell',{id:table.id,cell:table.cells[index].id,text});
  await run('set_context',{context:'structure'});await run('create_formula_hub',{name:'Vida',model:{kind:'free',source:'declara HP = 100\nresultado HP'},before:'',table:table.id,cell:cell.id,prefix:'',suffix:' pontos'});
  await run('create_formula_hub',{name:'Golpe',model:{kind:'free',source:'alterar Vida.HP = 80'},before:'',table:'',cell:'',prefix:'',suffix:''});
  const command=state.formulas.find(f=>f.command&&!f.consumed).id;await run('set_context',{context:'text'});await run('execute_formula',{id:command});check(state.formulas.find(f=>!f.command).display==='80','remote value change');await run('undo');check(state.formulas.find(f=>!f.command).display==='100','undo command');await run('redo');check(state.formulas.find(f=>f.id===command).consumed,'command consumption');await run('set_context',{context:'structure'});
  await run('insert_table_axis',{id:table.id,axis:'row',before:''});const inserted=state.tables[0].row_ids[2];await run('remove_table_axis',{id:table.id,axis:'row',axis_id:inserted});await run('restore_table_axis',{id:table.id,axis:'row',axis_id:inserted});
  await run('set_context',{context:'composition'});const appearance=structuredClone(state.table_design_presets.find(p=>p.id==='notebook')?.appearance||state.table_design_defaults);check(appearance.font,'style model');
  await run('edit_table_design',{id:table.id,title:'Personagens',header:true,appearance,placement:{width:90,horizontal:'center',text_align:'left',vertical:'start'}});
  await run('set_presentation',{id:equation,width:80,horizontal:'center',text_align:'center',vertical:'center'});
  await run('arrange_objects',{id:equation,other:state.paragraphs[0].id,side:'right',first:.5,second:.5,gap:16});
  await run('save');await run('reopen',{discard_unsaved:false});
  check(state.equations.length===3&&document.querySelectorAll('.equation-item .katex').length===3,'math rendering');check(state.formulas.find(f=>!f.command).display==='80','formula result');check(state.tables[0].row_ids.length===3,'table structure');
  const probe=document.createElement('div');equationRender(probe,String.raw`\href{https://example.invalid}{x}`,'block');check(!probe.querySelector('a,img'),'trusted math command');
  await run('set_context',{context:'text'});const matrix=state.equations.find(e=>e.id===equation);await run('edit_equation',{id:matrix.id,source:matrix.source,mode:matrix.mode,label:'Matriz revisada'});check(state.dirty,'unsaved composition expected');
  document.body.dataset.nativeSmoke='saved';
})().catch(e=>{document.body.dataset.nativeSmoke='error:'+e.message;console.error(e);});
