/* Version 2 presentation: editable authored text and protected result spans. */
let richTextEditing = null;
function richProfile() { return typeof state !== 'undefined' && state?.profile === 'P2-D2'; }
function richModel(key) { return state?.formula_text?.[key]; }
function richDomRuns(node) {
  return [...node.children].map(n => n.dataset.kind === 'result' ? {kind:'result',formula:n.dataset.result} : {kind:'text',text:n.innerText.replace(/\r\n/g,'\n')});
}
function richSameRuns(a,b) { return Array.isArray(a)&&Array.isArray(b)&&a.length===b.length&&a.every((r,i)=>r.kind===b[i].kind&&(r.kind==='text'?r.text===b[i].text:r.formula===b[i].formula)); }
function richPending() {
  const pending=[];
  if(!richProfile())return pending;
  for(const n of document.querySelectorAll('.rich-flow')) {
    const runs=richDomRuns(n), key=n.dataset.target;
    if(!richSameRuns(runs,richModel(key)?.runs))pending.push({target:key,runs});
  }
  if(richTextEditing && !richSameRuns(richTextEditing.runs,richModel(richTextEditing.key)?.runs)) {
    const at=pending.findIndex(p=>p.target===richTextEditing.key);if(at>=0)pending.splice(at,1);
    pending.push({target:richTextEditing.key,runs:structuredClone(richTextEditing.runs)});
  }
  return pending;
}
function richDrafts() { return richPending().length>0; }
function richControls() {
  if(!richProfile())return;
  const uncertain=state.persistence_status==='uncertain';
  for(const n of document.querySelectorAll('.rich-literal'))n.contentEditable=String(state.context==='text'&&!uncertain&&(!busy||['edit_formula_text','select','select_cell'].includes(activeOperation)));
  for(const n of document.querySelectorAll('.rich-control'))n.disabled=busy||uncertain;
  for(const n of document.querySelectorAll('.rich-builder input,.rich-builder textarea,.rich-builder select'))n.disabled=busy||uncertain;
}
function richChoose(key,table,cell) {
  if(table)return action('select_cell',{id:table,cell});
  return action('select',{id:key});
}
async function richEditPhrase(id,key,table,cell) {
  if(state.context!=='text'&&!await action('set_context',{context:'text'}))return;
  if(!await richChoose(key,table,cell))return;
  richTextEditing={id,key,table,cell,runs:structuredClone(richModel(key).runs)};formulaEditing=null;render();
  document.querySelector('.rich-builder textarea')?.focus({preventScroll:true});
}
function richButton(label,fn) {
  const b=document.createElement('button');b.type='button';b.textContent=label;b.className='rich-control';b.addEventListener('click',fn);return b;
}
function richBuilder(wrapper,id,key,table,cell) {
  wrapper.classList.add('rich-builder');
  const heading=document.createElement('p');heading.textContent='Texto com resultados';wrapper.append(heading);
  const editor=richTextEditing;
  const rebuild=()=>{render();controls();};
  editor.runs.forEach((r,index)=>{
    const row=document.createElement('div');row.className='rich-run-row';
    const label=document.createElement('label');label.textContent=r.kind==='text'?'Texto':'Resultado de';
    let field;
    if(r.kind==='text') {
      field=document.createElement('textarea');field.value=r.text;field.rows=2;field.setAttribute('aria-label',`Texto da frase ${index+1}`);
      field.addEventListener('input',()=>{editor.runs[index]={kind:'text',text:field.value};controls();});
    } else {
      field=document.createElement('select');field.setAttribute('aria-label',`Resultado da frase ${index+1}`);
      for(const f of state.formulas.filter(f=>!f.command&&!f.consumed)) {const o=document.createElement('option');o.value=f.id;o.textContent=f.name;field.append(o);}
      field.value=r.formula;field.addEventListener('change',()=>{editor.runs[index]={kind:'result',formula:field.value};controls();});
    }
    label.append(field);row.append(label);
    const up=richButton('Subir trecho',()=>{if(index){[editor.runs[index-1],editor.runs[index]]=[editor.runs[index],editor.runs[index-1]];rebuild();}});up.disabled=index===0;
    const remove=richButton('Retirar trecho',()=>{editor.runs.splice(index,1);rebuild();});
    row.append(up,remove);wrapper.append(row);
  });
  wrapper.append(richButton('Acrescentar texto',()=>{editor.runs.push({kind:'text',text:''});rebuild();}),richButton('Acrescentar resultado',()=>{editor.runs.push({kind:'result',formula:id});rebuild();}));
  wrapper.append(richButton('Aplicar frase',async()=>{if(await action('edit_formula_text',{target:key,runs:structuredClone(editor.runs)},'Frase aplicada; resultados continuam ligados às fórmulas.')){richTextEditing=null;render();}}),richButton('Cancelar frase',()=>{richTextEditing=null;render();}));
}
async function richExecute(id) {
  if(state.context!=='text'&&!await action('set_context',{context:'text'}))return;
  return action('execute_formula',{id},'Valor fixado na fonte; comando consumido.');
}
function richFormulaItem(id,key,table='',cell='') {
  const f=state.formulas.find(f=>f.id===id),wrapper=document.createElement('div');wrapper.className='formula-box rich-formula';wrapper.dataset.formula=id;wrapper.dataset.target=key;
  if(f.command) {
    wrapper.classList.add('rich-command');
    const label=document.createElement('p');label.textContent=f.source;wrapper.append(label);
    wrapper.append(richButton('Executar alteração',()=>richExecute(id)),
      richButton('Editar comando',()=>openFormula(id,key,table,cell)));
    if(!f.valid){const e=document.createElement('p');e.className='formula-error';e.textContent=formulaError(f);wrapper.append(e);}
    return wrapper;
  }
  if(richTextEditing?.key===key){richBuilder(wrapper,id,key,table,cell);return wrapper;}
  const p=richModel(key);
  if(!p){wrapper.textContent='A apresentação precisa ser conferida.';return wrapper;}
  const flow=document.createElement('div');flow.className='paragraph rich-flow';flow.dataset.target=key;
  p.runs.forEach((r,index)=>{
    if(r.kind==='result') {
      const value=state.formulas.find(f=>f.id===r.formula),span=document.createElement('span');
      span.dataset.kind='result';span.dataset.result=r.formula;span.className='rich-result';span.tabIndex=0;span.contentEditable='false';
      span.textContent=value.valid?value.display:formulaError(value);span.classList.toggle('formula-error',!value.valid);
      span.setAttribute('role','button');span.setAttribute('aria-label',`Editar fórmula ${value.name}: ${value.valid?value.display:'conferir fonte'}`);span.title=`Editar fonte de ${value.name}`;
      if(cell)span.dataset.cell=cell;
      const open=()=>openFormula(value.id,key,table,cell);
      span.addEventListener('click',open);span.addEventListener('keydown',e=>{if(e.key==='Enter'||e.key===' '){e.preventDefault();open();}});flow.append(span);
    } else {
      const span=document.createElement('span');span.className='rich-literal';span.dataset.kind='text';span.dataset.index=String(index);span.textContent=r.text;
      span.setAttribute('role','textbox');span.setAttribute('aria-label',`Texto de ${f.name}, trecho ${index+1}`);span.setAttribute('aria-multiline','true');span.tabIndex=0;span.spellcheck=true;
      span.addEventListener('focus',()=>{if(state.context==='text'&&(state.selection!==(table||key)||(cell&&state.selected_cell!==cell)))richChoose(key,table,cell);});
      const protect=()=>{if(span.contentEditable==='true'&&!richSameRuns(richDomRuns(flow),richModel(key)?.runs))action('edit_formula_text',{target:key,runs:richDomRuns(flow)});};
      span.addEventListener('input',e=>{controls();if(!e.isComposing)protect();});span.addEventListener('compositionend',protect);
      const insertText=value=>{
        const selection=window.getSelection();if(!selection?.rangeCount)return;
        const range=selection.getRangeAt(0);if(!span.contains(range.commonAncestorContainer))return;
        range.deleteContents();const node=document.createTextNode(value);range.insertNode(node);range.setStartAfter(node);range.collapse(true);selection.removeAllRanges();selection.addRange(range);protect();
      };
      span.addEventListener('paste',e=>{e.preventDefault();insertText(e.clipboardData.getData('text/plain'));});
      span.addEventListener('beforeinput',e=>{if(e.inputType==='insertParagraph'||e.inputType==='insertLineBreak'){e.preventDefault();insertText('\n');}});
      flow.append(span);
    }
  });
  wrapper.append(flow);
  const tools=document.createElement('div');tools.className='rich-tools';
  tools.append(richButton(`Editar fonte de ${f.name}`,()=>openFormula(id,key,table,cell)),richButton(`Editar frase de ${f.name}`,()=>richEditPhrase(id,key,table,cell)));
  wrapper.append(tools);
  return wrapper;
}

'use strict';
const $ = id => document.getElementById(id);
const token = location.hash.slice(1) || sessionStorage.getItem('obj01-p2a-token') || '';
if (token) sessionStorage.setItem('obj01-p2a-token', token);
if (location.hash) history.replaceState(null, '', location.pathname);
let state, upload, uploadName = '', mode = 'visual', busy = false, generation = 0, activeOperation = '';
const imageCache = new Map();
const errors = {
  formula_command_must_be_alone: 'O comando alterar deve ficar sozinho: uma instrução, sem declarações, resultado, frase ou usos compartilhados.',
  formula_command_consumed: 'Esse comando já foi executado e consumido. Desfazer pode restaurá-lo.',
  formula_command_has_no_value: 'Um comando alterar não oferece resultado para outra fórmula. Consulte a variável alterada.',
  formula_cards_invalid: 'Preencha a entrada e os valores dos cartões. Confira também o limite de 12 condições.',
  formula_cards_advanced_source: 'Esta fonte foi alterada fora do formato dos cartões. Abra a edição avançada para conservá-la integralmente.',
  formula_cards_complex_phrase: 'A frase reúne outros resultados. Use Editar frase para conservar todos os trechos.',
  formula_type: 'A operação recebeu um tipo incompatível. Use números nas contas e & para juntar texto.',
  formula_text_limit: 'O resultado textual excede 4.096 bytes neste experimento.',
  formula_output_limit: 'O conjunto de resultados excede o limite de texto deste experimento.',
  formula_text_owner_result_missing: 'Mantenha na frase pelo menos um resultado da fórmula principal.',
  formula_text_reference_invalid: 'Um trecho da frase aponta para um objeto que não fornece resultado.',
  formula_literal_limit: 'Um trecho de texto excede 4.096 caracteres. A frase anterior foi conservada.',
  formula_text_run_limit: 'A frase precisa ter entre 1 e 32 trechos de texto ou resultado.',
  formula_current_undeclared: 'Declare a variável antes de registrar seu valor atual.',
  formula_duplicate_current: 'Uma variável tem mais de uma atribuição atual na fonte. Mantenha uma só.',
  formula_string_syntax: 'Confira as aspas e os escapes do texto na fórmula.',

  formula_identity: 'Use um nome único, sem espaços, com letras de A a Z, números ou sublinhado; comece por uma letra ou sublinhado.',
  formula_source_limit: 'A fonte da fórmula excede o limite de 16 KiB ou contém texto inválido.',
  formula_expression: 'Há uma expressão incompleta. Confira os números e operadores.',
  formula_syntax: 'Confira os parênteses, chaves e separadores da fórmula.',
  formula_statement: 'Separe as declarações e o resultado por linhas.',
  formula_result_required: 'Declare um resultado para esta fórmula.',
  formula_reference_missing: 'Uma fórmula referenciada ainda não foi encontrada.',
  formula_variable_missing: 'Uma variável referenciada não existe na fórmula de origem.',
  formula_cycle: 'Há uma dependência circular entre variáveis ou resultados. Abra a fonte para corrigir.',
  formula_division_zero: 'A fórmula tenta dividir por zero.',
  formula_number: 'Confira a escrita e o tamanho desse número.',
  formula_number_limit: 'O cálculo excede o limite numérico deste experimento.',
  formula_function: 'Esta função ainda não é suportada. Use SOMA, MEDIA, MIN, MAX, ABS ou SE.',
  formula_arguments: 'Confira a quantidade de argumentos da função.',
  formula_execution_would_break_dependency: 'A alteração deixaria um resultado dependente inválido. Nenhuma variável foi alterada.',
  formula_duplicate_assignment: 'A mesma execução tenta alterar uma variável mais de uma vez. Nenhuma alteração foi aplicada.',
  formula_cell_already_bound: 'Esta célula já contém uma fórmula. Edite-a ou retire o uso primeiro.',
  formula_depth: 'A expressão tem níveis demais para este experimento.',
  formula_evaluation_limit: 'O cálculo excede o limite de dependências ou operações deste experimento.',
  formula_identifier: 'Confira o nome usado para a variável ou referência.',
  formula_duplicate_variable: 'Uma variável foi declarada mais de uma vez.',
  formula_no_actions: 'Esta fórmula calcula um resultado, mas não declara alterações para executar.',
  formula_execution_invalid: 'Corrija a fórmula antes de executar suas alterações.',
  reading_group_split: 'Esse ponto divide um grupo textual protegido. Escolha antes do grupo ou o fim do documento.',
  authorial_requires_alignment: 'Restaure a apresentação pela ordem de leitura antes de alterar a sequência autoral.',
  row_requires_adjacent_items: 'A figura e o parágrafo precisam estar vizinhos na apresentação. Mova a figura primeiro.',
  destination_inside_row: 'Esse destino está dentro de uma composição lado a lado. Volte essa composição à coluna única primeiro.',
  invalid_row_profile: 'As duas larguras precisam ser positivas e somar 100%.',
  stale_session_revision: 'A revisão mudou. A tela foi atualizada; confira a seleção e tente novamente.',
  persisted_revision_conflict: 'O documento salvo foi alterado fora desta sessão. Seu trabalho continua em memória. Confira antes de reabrir.',
  persisted_snapshot_conflict: 'O estado salvo não corresponde à base desta sessão. O trabalho em memória foi conservado.',
  alt_text_required: 'Informe uma descrição ou marque a figura como decorativa.',
  png_dimensions_limit: 'A imagem ultrapassa o perfil do experimento: lados de até 8.192 pixels e até 16 milhões de pixels.',
  png_byte_limit: 'O PNG precisa ter até 8 MiB.',
  blob_total_limit: 'Os recursos do experimento precisam caber em 32 MiB.',
  editing_context_required: 'Escolha o instrumento adequado: Texto para células, Estrutura para inserir/mover e Composição para arranjos.',
  persistence_outcome_uncertain: 'O resultado da gravação ficou incerto. O candidato foi conservado; reabra o trabalho para conferir o estado confirmado.',
  persistence_requires_reopen: 'Reabra o trabalho antes de editar novamente: o resultado anterior precisa ser conferido.',
  working_generation_conflict: 'O registro de trabalho mudou fora desta sessão. Reabra e confira antes de continuar.',
  working_base_conflict: 'O trabalho recuperável não corresponde ao estado salvo atual. Nenhuma substituição foi realizada.',
  owner_text_move_refused: 'Esse movimento partiria ou inverteria um trecho protegido. O motor textual recusou a operação inteira.',
  reading_group_not_found: 'Escolha um grupo textual completo. Um parágrafo interno de resma não pode ser movido isoladamente.',
  reading_group_target_selected: 'Escolha um destino fora do próprio grupo textual.',
  composite_publication_not_supported: 'Este experimento ainda não possui tradução para o publicador antigo. Nenhuma publicação foi feita.',
  cell_text_limit: 'Cada célula pode conter até 4.096 caracteres Unicode.',
  invalid_text_or_limit: 'O texto contém caracteres inválidos ou excede o limite deste campo.',
  document_string_limit: 'O texto do experimento ultrapassa seu limite de armazenamento.',
  table_not_found: 'A tabela não está mais disponível nesta revisão.',
  cell_not_found: 'A célula não está mais disponível nesta revisão.',
  source_removal_out_of_scope: 'Os parágrafos de origem são preservados neste corte. Selecione uma figura ou tabela.',
  undo_empty: 'Não há outra operação disponível para desfazer nesta sessão.',
  redo_empty: 'Não há outra operação disponível para refazer nesta sessão.',
  animated_png_not_supported: 'Este experimento aceita PNG estático. A imagem escolhida é animada.',
  invalid_png: 'O arquivo não é um PNG válido.',
  png_crc_mismatch: 'A verificação de integridade do PNG falhou. O arquivo não foi inserido.',
  png_decode_failed: 'O PNG não pôde ser decodificado. Nenhuma figura foi inserida.',
  invalid_png_end: 'O PNG está incompleto ou contém dados incompatíveis.',
  authorization_required: 'Abra o endereço completo fornecido pelo iniciador da demonstração.',
  engine_unavailable: 'O núcleo local não está respondendo. Confira a janela em que a demonstração foi iniciada.',
  blob_hash_mismatch: 'O recurso salvo falhou na verificação de integridade. A reabertura foi recusada.',
  source_hash_mismatch: 'A origem falhou na verificação de integridade.',
  frozen_source_changed: 'O arquivo de origem foi alterado. A operação foi recusada para preservação.',
  unsupported_database_version: 'A versão desse banco experimental não é compatível.',
};
function message(text, error = false) {
  $('message').textContent = text;
  $('message').className = error ? 'error' : '';
  $('message').hidden = false;
  if(cardsEditing&&$('cards-error')){ $('cards-error').textContent=text; $('cards-error').hidden=!error; }
}
async function fetchApi(path, options = {}) {
  const response = await fetch(path, { ...options, headers: { 'X-Obj01-Token': token, ...(options.headers || {}) } });
  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: 'engine_unavailable' }));
    throw new Error(error.error || 'engine_unavailable');
  }
  return response;
}
function cellData(tableId, cellId) { return state?.tables?.find(t => t.id === tableId)?.cells.find(c => c.id === cellId); }
let formulaEditing = null;
function formulaDraft() { return formulaEditing && formulaEditing.draft !== state?.formulas?.find(f => f.id === formulaEditing.id)?.source; }
function hasDrafts() { return cardsDraft() || formulaDraft() || richDrafts() || [...document.querySelectorAll('.cell-editor')].some(n => n.value !== cellData(n.dataset.table, n.dataset.cell)?.text); }
function updateSelection() {
  for (const node of $('document').querySelectorAll('.item')) {
    const selected = state.selection === node.dataset.occurrence;
    node.classList.toggle('selected', selected);
    if (node.getAttribute('role') === 'button') node.setAttribute('aria-pressed', String(selected));
  }
  for (const n of $('document').querySelectorAll('.cell-editor')) n.classList.toggle('selected-cell', state.selection === n.dataset.table && state.selected_cell === n.dataset.cell);
  controls();
}
let requests = Promise.resolve();
async function sendOperation(op, values, success) {
  const response = await fetchApi('/api/request', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ op, expected: state.revision, ...values }),
  });
  const result = await response.json();
  if (result.state) {
    state = result.state;
    if (op === 'edit_cell' || op === 'select_cell' || op === 'edit_formula_text' || (op === 'select' && richProfile() && document.activeElement?.classList.contains('rich-literal'))) updateSelection();
    else {
      render();
      if (op === 'select') [...$('document').querySelectorAll('.item')].find(n => n.dataset.occurrence === state.selection)?.focus({ preventScroll: true });
    }
  }
  if (!result.accepted) { message(errors[result.error] || 'Não foi possível concluir a operação. Seu trabalho foi conservado; confira os campos e tente novamente.', true); return false; }
  if (success) message(success);
  return true;
}
function action(op, values = {}, success = '') {
  const run = async () => {
    if (!state) return false;
    busy = true; activeOperation = op; controls();
    try {
      if (!['edit_formula_text','reopen','reopen_working'].includes(op) && richDrafts()) {
        for (const p of richPending()) if (!await sendOperation('edit_formula_text',p)) return false;
      }
      if (!['edit_formula','reopen','reopen_working'].includes(op) && formulaDraft()) {
        if (!await sendOperation('edit_formula',{id:formulaEditing.id,source:formulaEditing.draft})) return false;
      }
      // A structural change/save must never silently discard a typed cell.
      if (!['edit_cell', 'select_cell', 'reopen', 'reopen_working'].includes(op)) {
        for (const n of [...document.querySelectorAll('.cell-editor')]) {
          if (n.value !== cellData(n.dataset.table, n.dataset.cell)?.text) {
            if (!await sendOperation('edit_cell', { id: n.dataset.table, cell: n.dataset.cell, text: n.value })) return false;
          }
        }
      }
      return await sendOperation(op, values, success);
    } catch (error) {
      message(errors[error.message] || 'Não foi possível concluir a operação. Confira se o núcleo local continua aberto.', true);
      return false;
    } finally { busy = false; activeOperation = ''; controls(); }
  };
  const next = requests.then(run);
  requests = next.then(() => undefined, () => undefined);
  return next;
}
function selectedTable() { return state?.tables?.find(t => t.id === state.selection); }
function selectedFormulaUse() { return state?.formula_uses?.find(u => u.id === state.selection); }
function libraryFormula() { return state?.formulas?.find(f => f.id === $('formula-library').value); }
function figure() { return state?.figures.find(f => f.id === state.selection); }
function shortText(id) {
  const p = state.paragraphs.find(p => p.id === id);
  if (p) return p.text.length > 36 ? p.text.slice(0, 36) + '…' : p.text;
  const table = state.tables?.find(t => t.id === id);
  if (table) return 'Tabela: ' + table.title;
  const use = state.formula_uses?.find(u => u.id === id);
  if (use) return 'Fórmula: ' + state.formulas.find(f => f.id === use.object).name;
  const f = state.figures.find(f => f.id === id);
  return f ? 'Figura: ' + (f.caption || 'sem legenda') : id;
}
function options(id, values, fallback = '') {
  const selected = $(id).options.length ? $(id).value : fallback;
  $(id).replaceChildren();
  for (const [value, label] of values) {
    const option = document.createElement('option'); option.value = value; option.textContent = label;
    $(id).append(option);
  }
  $(id).value = values.some(v => v[0] === selected) ? selected : values.some(v => v[0] === fallback) ? fallback : values.some(v => v[0] === '') ? '' : (values[0]?.[0] || '');
}
function controls() {
  if (!state) return;
  richControls();
  cardsControls();
  for (const n of document.querySelectorAll('.cell-editor')) n.readOnly = busy && !['edit_cell', 'select_cell'].includes(activeOperation);
  for (const id of ['save', 'reopen', 'sample', 'file', 'caption', 'alt', 'decorative', 'intent', 'insert-before', 'move-before', 'paragraph', 'ratio', 'restore', 'table-title', 'table-header', 'table-before', 'create-table']) $(id).disabled = busy;
  $('import').disabled = busy || !upload;
  for (const id of ['move', 'duplicate', 'arrange', 'stack']) $(id).disabled = busy || !figure();
  $('save').disabled = false; $('reopen').disabled = false;
  $('undo').disabled = !state.can_undo; $('redo').disabled = !state.can_redo;
  $('move').disabled = busy || !(figure() || selectedTable() || selectedFormulaUse());
  $('remove').disabled = busy || !(figure() || selectedTable() || selectedFormulaUse());
  $('move').textContent = selectedFormulaUse() ? 'Mover fórmula selecionada' : selectedTable() ? 'Mover tabela selecionada' : 'Mover figura selecionada';
  $('selected-label').textContent = selectedTable() ? 'Tabela selecionada: ' + selectedTable().title : figure() ? 'Figura selecionada: ' + (figure().caption || 'sem legenda') : 'Selecione uma figura ou tabela no documento.';
  if (selectedFormulaUse()) $('selected-label').textContent = shortText(state.selection);
  $('alt').disabled = busy || $('decorative').checked;
  $('save-status').textContent = hasDrafts() ? 'Célula em edição' : state.dirty ? 'Alterações em memória' : 'Salvo localmente';
  $('save-status').className = state.dirty ? 'dirty' : '';
  if (['P2-C','P2-D','P2-D2'].includes(state.profile)) {
    for (const id of ['reorder-text', 'text-group', 'text-before', 'publication-check', 'reopen-working']) $(id).disabled = busy;
    $('reopen-working').disabled = false;
    const context = state.context;
    $('context').value = context;
    for (const n of document.querySelectorAll('.cell-editor')) n.readOnly = context !== 'text' || state.persistence_status === 'uncertain' || (busy && !['edit_cell', 'select_cell'].includes(activeOperation));
    for (const id of ['import', 'create-table', 'move', 'duplicate', 'remove', 'reorder-text']) $(id).disabled ||= context !== 'structure';
    for (const id of ['arrange', 'stack', 'restore']) $(id).disabled ||= context !== 'composition';
    $('context').disabled = busy || state.persistence_status === 'uncertain';
    $('reorder-text').disabled ||= busy || state.persistence_status === 'uncertain';
    if (state.persistence_status === 'uncertain') for (const id of ['save','import','create-table','move','duplicate','remove','arrange','stack','restore','undo','redo']) $(id).disabled = true;
    $('save-status').textContent = state.persistence_status === 'uncertain' ? 'Resultado incerto' : state.persistence_status === 'error' ? 'Registro não confirmado' : formulaDraft() ? 'Fonte aguardando aplicação' : hasDrafts() ? 'Texto ainda sendo aplicado' : state.dirty ? 'Trabalho protegido · não salvo' : 'Salvo e conferido';
    $('recovery-note').textContent = state.persistence_status === 'uncertain' ? 'Reabra o trabalho para conferir a gravação. O candidato foi retido; novas alterações estão bloqueadas.' : state.recovered_on_open ? 'Trabalho não salvo recuperado. Conteúdo e seleção foram reconstruídos; o histórico desta sessão começa vazio.' : 'Cada operação confirmada é registrada para recuperação. Salvar confirma o estado autoral; o histórico não sobrevive ao reinício.';
  }

  if (['P2-D','P2-D2'].includes(state.profile)) {
    const uncertain = state.persistence_status === 'uncertain';
    for (const id of ['formula-name','formula-source','formula-target','formula-before','formula-library']) $(id).disabled = busy || uncertain;
    $('create-formula').disabled = busy || uncertain || state.context !== 'structure';
    $('edit-formula').disabled = busy || uncertain || !libraryFormula();
    $('execute-formula').disabled = busy || uncertain || state.context !== 'text' || !libraryFormula()?.valid || !libraryFormula()?.actions;
    $('attach-formula').disabled = busy || uncertain || state.context !== 'structure' || !state.selected_cell || !libraryFormula();
    $('detach-formula').disabled = busy || uncertain || state.context !== 'structure' || !state.formula_cells?.[state.selected_cell];
    for (const n of document.querySelectorAll('.formula-open')) n.disabled = busy || uncertain;
    for (const n of document.querySelectorAll('.formula-editor')) n.readOnly = busy || uncertain || state.context !== 'text';
    for (const n of document.querySelectorAll('.formula-reset')) n.disabled = busy || uncertain || state.context !== 'text';
    for (const n of document.querySelectorAll('.formula-apply')) n.disabled = busy || uncertain || state.context !== 'text';
  }
}
async function imageUrl(resource, hash) {
  const key = resource + '/' + hash;
  if (!imageCache.has(key)) imageCache.set(key, fetchApi('/api/image/' + resource).then(r => r.blob()).then(b => URL.createObjectURL(b)).catch(e => { imageCache.delete(key); throw e; }));
  return imageCache.get(key);
}
function tableItem(t) {
  const wrapper = document.createElement('section');
  wrapper.className = 'item table-item' + (state.selection === t.id ? ' selected' : '');
  wrapper.tabIndex = 0; wrapper.setAttribute('role', 'group');
  wrapper.setAttribute('aria-label', 'Tabela: ' + t.title); wrapper.dataset.occurrence = t.id;
  const kind = document.createElement('span'); kind.className = 'kind'; kind.textContent = `TABELA · ${t.row_ids.length} × ${t.column_ids.length}`; wrapper.append(kind);
  const table = document.createElement('table'), caption = document.createElement('caption');
  caption.textContent = t.title; table.append(caption);
  const body = document.createElement('tbody');
  const ordered = t.row_ids.flatMap(row => t.column_ids.map(column => t.cells.find(c => c.row === row && c.column === column)));
  for (const [ri, row] of t.row_ids.entries()) {
    const tr = document.createElement('tr');
    for (const [ci, column] of t.column_ids.entries()) {
      const cell = t.cells.find(c => c.row === row && c.column === column);
      const holder = document.createElement(t.header && ri === 0 ? 'th' : 'td');
      if (t.header && ri === 0) holder.scope = 'col';
      if (state.formula_cells?.[cell.id]) {
        holder.append(formulaItem(state.formula_cells[cell.id],cell.id,t.id,cell.id)); tr.append(holder); continue;
      }
      const input = document.createElement('textarea');
      input.className = 'cell-editor' + (state.selection === t.id && state.selected_cell === cell.id ? ' selected-cell' : ''); input.value = cell.text; input.rows = 2;
      input.dataset.table = t.id; input.dataset.cell = cell.id;
      input.setAttribute('aria-label', `${t.title}, linha ${ri + 1}, coluna ${ci + 1}`);
      input.addEventListener('focus', () => {
        if (state.selection !== t.id || state.selected_cell !== cell.id) action('select_cell', { id: t.id, cell: cell.id });
      });
      const commit = () => {
        if (input.value !== cellData(t.id, cell.id)?.text) return action('edit_cell', { id: t.id, cell: cell.id, text: input.value });
        return Promise.resolve(true);
      };
      input.addEventListener('change', commit);
      const protect = () => { if (['P2-C','P2-D','P2-D2'].includes(state.profile) && input.value !== cellData(t.id,cell.id)?.text) action('edit_cell',{id:t.id,cell:cell.id,text:input.value}); };
      input.addEventListener('input', event => { controls(); if (!event.isComposing) protect(); });
      input.addEventListener('compositionend', protect);
      input.addEventListener('keydown', async event => {
        if (event.key !== 'Tab' && !(event.key === 'Enter' && (event.ctrlKey || event.metaKey))) return;
        event.preventDefault();
        if (!await commit()) { input.focus(); return; }
        if (event.key === 'Enter') { message('Célula aplicada ao documento.'); return; }
        const next = ordered.findIndex(c => c.id === cell.id) + (event.shiftKey ? -1 : 1);
        if (next < 0) { wrapper.focus(); return; }
        if (next >= ordered.length) { exit.focus(); return; }
        [...wrapper.querySelectorAll('.cell-editor, .formula-result, .rich-result')].find(n => n.dataset.cell === ordered[next].id)?.focus();
      });
      holder.append(input); tr.append(holder);
    }
    body.append(tr);
  }
  table.append(body); wrapper.append(table);
  const hint = document.createElement('p'); hint.className = 'cell-hint';
  hint.textContent = 'Tab / Shift+Tab: trocar de célula e aplicar o texto. Ctrl+Enter: aplicar sem sair. Salvar também aplica as células em edição.';
  const exit = document.createElement('button'); exit.className = 'table-exit'; exit.textContent = 'Selecionar a tabela inteira';
  exit.addEventListener('click', e => { e.stopPropagation(); action('select', { id: t.id }); });
  wrapper.append(hint, exit);
  wrapper.addEventListener('click', e => { if (e.target === wrapper || e.target === kind || e.target === caption) action('select', { id: t.id }); });
  wrapper.addEventListener('keydown', e => { if (e.target === wrapper && (e.key === 'Enter' || e.key === ' ')) { e.preventDefault(); action('select', { id: t.id }); } });
  return wrapper;
}

function renderVariables() {
  const target = $('formula-variables'); target.replaceChildren();
  const f = libraryFormula();
  if (!f) return;
  const summary = document.createElement('p'); summary.textContent = f.command ? 'Comando de uso único: será retirado depois de uma execução bem-sucedida.' : f.valid ? `Resultado de ${f.name}: ${f.display}` : formulaError(f); target.append(summary);
  for (const v of f.variables) {
    const row = document.createElement('p');
    row.textContent = `${v.name}: inicial ${richProfile()?v.initial_display:(v.initial ?? 'indisponível')} · atual ${richProfile()?v.current_display:v.current}${v.overridden ? ' · alterado por execução' : ''}`;
    if (v.overridden) {
      const reset = document.createElement('button'); reset.textContent = `Restaurar inicial de ${v.name}`; reset.className='formula-reset';
      reset.disabled = busy || state.context !== 'text' || state.persistence_status === 'uncertain';
      reset.addEventListener('click',()=>action('reset_formula_variable',{id:f.id,variable:v.name},'Valor inicial restaurado.'));
      row.append(reset);
    }
    target.append(row);
  }
  if (formulaEditing?.key === 'library') target.append(formulaItem(f.id,'library'));
}
function formulaError(f) {
  return errors[f.error] || 'A fórmula precisa ser conferida. Abra sua fonte para corrigir.';
}
async function openFormula(id,key,table='',cell='',advanced=false) {
  if(!advanced&&richProfile()&&state.formulas.find(f=>f.id===id)?.cards)return openCards(id,key,table,cell);
  if (state.context !== 'text' && !await action('set_context',{context:'text'})) return;
  if (table) { if (!await action('select_cell',{id:table,cell})) return; }
  else if (key !== 'library') { if (!await action('select',{id:key})) return; }
  else if (formulaDraft() && !await action('edit_formula',{id:formulaEditing.id,source:formulaEditing.draft})) return;
  const f=state.formulas.find(f=>f.id===id);
  formulaEditing={id,key,draft:f.source}; $('formula-library').value=id; render();
  document.querySelector('.formula-editor')?.focus({preventScroll:true});
}
function formulaItem(id,key,table='',cell='') {
  if(richProfile() && formulaEditing?.key !== key) return richFormulaItem(id,key,table,cell);
  if(richProfile() && formulaEditing?.key === key) id=formulaEditing.id;
  const f=state.formulas.find(f=>f.id===id), wrapper=document.createElement('div');
  wrapper.className='formula-box'; wrapper.dataset.formula=id;
  if (formulaEditing?.id === id && formulaEditing.key === key) {
    const label=document.createElement('label'); label.textContent=`Fonte de ${f.name}`;
    const input=document.createElement('textarea'); input.className='formula-editor'; input.rows=Math.min(12,Math.max(3,formulaEditing.draft.split('\n').length));
    input.maxLength=16384;input.value=formulaEditing.draft;input.setAttribute('aria-label',`Fonte de ${f.name}`);label.append(input);wrapper.append(label);
    input.addEventListener('input',()=>{formulaEditing.draft=input.value;controls();});
    const done=async()=>{if(await action('edit_formula',{id,source:input.value},'Fórmula aplicada e trabalho protegido.')) {formulaEditing=null;render();}};
    const apply=document.createElement('button');apply.textContent='Aplicar fórmula';apply.className='formula-apply';apply.addEventListener('click',done);
    const close=document.createElement('button');close.textContent='Cancelar edição';close.addEventListener('click',()=>{formulaEditing=null;render();});
    input.addEventListener('keydown',e=>{if(e.key==='Enter'&&(e.ctrlKey||e.metaKey)){e.preventDefault();done();}else if(e.key==='Escape'){e.preventDefault();formulaEditing=null;render();}});
    wrapper.append(apply,close);
    if(!f.valid){const error=document.createElement('p');error.className='formula-error';error.textContent=formulaError(f);wrapper.append(error);}
  } else {
    const output=document.createElement('button');output.type='button';output.className='formula-result formula-open';
    output.textContent=f.valid?f.display:formulaError(f);output.classList.toggle('formula-error',!f.valid);
    output.setAttribute('aria-label',`Editar fórmula ${f.name}: ${f.valid?f.display:'conferir fonte'}`);
    output.title=`${f.name} · editar fórmula`;if(cell)output.dataset.cell=cell;
    output.addEventListener('click',()=>openFormula(id,key,table,cell));wrapper.append(output);
  }
  return wrapper;
}

function item(id, currentGeneration) {
  const table = state.tables?.find(t => t.id === id);
  if (table) return tableItem(table);
  const use = state.formula_uses?.find(u => u.id === id);
  if (use) { const n = formulaItem(use.object,id); n.classList.add('item'); n.dataset.occurrence = id; n.classList.toggle('selected',state.selection === id); return n; }
  const node = document.createElement('div');
  node.className = 'item' + (state.selection === id ? ' selected' : '');
  node.tabIndex = 0; node.setAttribute('role', 'button'); node.setAttribute('aria-pressed', String(state.selection === id));
  node.dataset.occurrence = id;
  const p = state.paragraphs.find(p => p.id === id);
  node.setAttribute('aria-label', p ? 'Selecionar parágrafo: ' + p.text : 'Selecionar figura: ' + (state.figures.find(f => f.id === id)?.caption || 'sem legenda'));
  const label = document.createElement('span'); label.className = 'kind'; label.textContent = p ? 'TEXTO DE ORIGEM' : 'FIGURA'; node.append(label);
  if (p) {
    const text = document.createElement('div'); text.className = 'paragraph'; text.textContent = p.text; node.append(text);
  } else {
    const f = state.figures.find(f => f.id === id);
    const wrapper = document.createElement('figure'), img = document.createElement('img');
    img.alt = f.decorative ? '' : f.alt; img.width = f.width; img.height = f.height;
    wrapper.append(img);
    if (f.caption) { const caption = document.createElement('figcaption'); caption.textContent = f.caption; wrapper.append(caption); }
    node.append(wrapper);
    imageUrl(f.resource, f.sha256).then(url => { if (generation === currentGeneration) img.src = url; }).catch(() => {
      if (generation !== currentGeneration) return;
      const error = document.createElement('div'); error.className = 'image-error'; error.textContent = 'Não foi possível carregar esta figura. Confira o núcleo local.'; img.replaceWith(error);
    });
  }
  const select = () => action('select', { id });
  node.addEventListener('click', select);
  node.addEventListener('keydown', e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); select(); } });
  return node;
}
function render() {
  const currentGeneration = ++generation;
  const p2c = ['P2-C','P2-D','P2-D2'].includes(state.profile);
  const p2d = ['P2-D','P2-D2'].includes(state.profile);
  const p2d2=richProfile();document.body.classList.toggle('has-rich-formulas',p2d2);
  document.querySelectorAll('.p2d2-only').forEach(n=>{n.hidden=!p2d2;});
  document.body.classList.toggle('has-formulas',p2d);
  if (state.context !== 'text') { formulaEditing = null; richTextEditing = null; }
  if (formulaEditing && state.formulas?.find(f=>f.id===formulaEditing.id)?.consumed) formulaEditing=null;
  if (richTextEditing && !state.formula_text?.[richTextEditing.key]) richTextEditing=null;
  const p2b = state.profile !== 'P2-A';
  document.querySelectorAll('.p2d-only').forEach(n => { n.hidden = !p2d; });
  document.querySelectorAll('.p2c-only').forEach(n => { n.hidden = !p2c; });
  document.querySelectorAll('.p2b-only').forEach(n => { n.hidden = !p2b; });
  $('phase').textContent = 'OBJ-01 / ' + state.profile;
  if (p2b) { document.title = 'INDE · Texto, figura e tabela'; document.querySelector('.eyebrow').textContent = 'DOCUMENTO COMPOSTO · TEXTO + FIGURA + TABELA'; document.querySelector('.intro p:not(.eyebrow)').textContent = 'Edite notas e experimente a composição sem perder a ordem de leitura.'; document.querySelector('.paper-footer').textContent = 'INDE · EXPERIMENTO AUTORAL P2-B'; document.querySelector('h1').textContent = 'Palavras, figuras e notas.'; $('limits').textContent = 'Texto de origem em leitura. Desfazer/refazer vale nesta sessão; alterações não salvas e o histórico não sobrevivem ao encerramento.'; }
  document.querySelector('.bottom span:last-child').textContent = p2c ? 'Operações confirmadas ficam protegidas para recuperação. Salvar confirma o estado autoral. Nenhuma publicação é realizada.' : 'Salve antes de encerrar o servidor. Nenhuma publicação é realizada.';
  if (p2c) {document.title = 'INDE · Trabalho recuperável'; document.querySelector('h1').textContent = 'Seu trabalho continua aqui.'; document.querySelector('.paper-footer').textContent = 'INDE · EXPERIMENTO AUTORAL P2-C'; $('limits').textContent = 'Células em Texto, objetos em Estrutura, arranjos em Composição. Operações confirmadas são recuperáveis; o histórico reinicia.';}
  if (p2d) { document.title = 'INDE · Fórmulas vivas'; document.querySelector('h1').textContent = 'Resultados que acompanham sua história.'; document.querySelector('.paper-footer').textContent = 'INDE · EXPERIMENTO AUTORAL P2-D'; $('limits').textContent = 'Fórmulas numéricas com variáveis e dependências. Alterações de variáveis exigem execução; resultados recalculam automaticamente.'; }
  if(p2d2) { document.title='INDE · Texto e fórmulas'; document.querySelector('h1').textContent='A conta faz parte da escrita.'; document.querySelector('.intro p:not(.eyebrow)').textContent='Escreva a frase, deixe os resultados acompanharem as mudanças.'; $('limits').textContent='Texto livre e resultados protegidos no mesmo parágrafo. O comando alterar age uma vez e é consumido.'; document.querySelector('.paper-footer').textContent='INDE · EXPERIMENTO AUTORAL P2-D2'; }
  if(state.formula_cards_supported){document.title='INDE · Texto por condições';document.querySelector('h1').textContent='Escolha as palavras, sem programar.';document.querySelector('.intro p:not(.eyebrow)').textContent='Preencha as frases e deixe os cartões acompanharem a situação.';}
  const destination = state.author_order.filter(id => !state.rows.some(r => r.children[1] === id) && (state.figures.some(f => f.id === id) || state.tables?.some(t => t.id === id) || state.formula_uses?.some(u => u.id === id) || state.reading_groups.some(g => g[0] === id))).map(id => [id, shortText(id)]);
  destination.push(['', 'Fim do documento']);
  options('insert-before', destination, state.paragraphs[1]?.id || '');
  options('table-before', destination, '');
  options('formula-before',destination,'');
  options('formula-library',(state.formulas || []).filter(f=>!f.consumed).map(f => [f.id,f.name + ': ' + (f.valid?f.display:'conferir fórmula')]),state.formulas?.[0]?.id || '');
  renderVariables();
  cardsSetup();
  options('move-before', destination.filter(([id]) => id !== state.selection), '');
  options('text-before', destination, '');
  options('text-group', (state.text_groups || []).map(g => [g.id, g.occurrences.map(shortText).join(' / ')]), state.text_groups?.[0]?.id || '');
  options('paragraph', state.paragraphs.map(p => [p.id, shortText(p.id)]), state.paragraphs[1]?.id || '');
  const f = figure();
  $('selected-label').textContent = f ? 'Figura selecionada: ' + (f.caption || 'sem legenda') : 'Selecione uma figura no documento.';
  $('alignment').textContent = state.aligned ? 'Ordem alinhada' : 'Apresentação independente';
  $('divergence').hidden = state.aligned;
  $('visual-tab').classList.toggle('active', mode === 'visual');
  $('reading-tab').classList.toggle('active', mode === 'reading');
  const target = $('document'); target.replaceChildren();
  const order = mode === 'visual' ? state.visual_order : state.author_order;
  const skipped = new Set();
  for (const id of order) {
    if (skipped.has(id)) continue;
    const row = mode === 'visual' ? state.rows.find(r => r.children[0] === id) : undefined;
    if (row) {
      const container = document.createElement('div'); container.className = 'composite-row'; container.dataset.layout = row.id;
      container.style.gridTemplateColumns = `minmax(0, ${row.first}fr) minmax(0, ${row.second}fr)`;
      container.style.gap = row.gap + 'px';
      for (const child of row.children) { container.append(item(child, currentGeneration)); skipped.add(child); }
      target.append(container);
    } else target.append(item(id, currentGeneration));
  }
  controls();
}
async function choose(file, name) {
  if (file.size > 8 * 1024 * 1024) { message(errors.png_byte_limit, true); return; }
  upload = file; uploadName = name; $('file-name').textContent = name; controls();
}
$('file').addEventListener('change', () => { if ($('file').files[0]) choose($('file').files[0], $('file').files[0].name); });
$('sample').addEventListener('click', async () => {
  try {
    const response = await fetch('/sample.png'); if (!response.ok) throw new Error('engine_unavailable');
    await choose(await response.blob(), 'Figura sintética do experimento');
    $('caption').value = 'Uma passagem na paisagem';
    $('alt').value = 'Paisagem geométrica sintética em tons de verde, com um círculo dourado no céu.';
    $('decorative').checked = false; controls();
    message('Exemplo escolhido. Confira a legenda, a descrição e o ponto de inserção.');
  } catch { message(errors.engine_unavailable, true); }
});
$('decorative').addEventListener('change', controls);
$('import').addEventListener('click', async () => {
  if (!upload) return;
  const bytes = new Uint8Array(await upload.arrayBuffer()); let binary = '';
  for (let i = 0; i < bytes.length; i += 16384) binary += String.fromCharCode(...bytes.subarray(i, i + 16384));
  await action('import', { data: btoa(binary), before: $('insert-before').value, caption: $('caption').value, alt: $('alt').value, decorative: $('decorative').checked }, `Figura inserida: ${uploadName}.`);
});
$('move').addEventListener('click', () => action('move', { id: state.selection, before: $('move-before').value, intent: $('intent').value }, 'Uso selecionado movido.'));
$('duplicate').addEventListener('click', () => action('duplicate', { id: state.selection, before: $('move-before').value }, 'Outro uso criado; o recurso é compartilhado.'));
$('arrange').addEventListener('click', () => { const first = Number($('ratio').value) / 100; action('arrange', { id: state.selection, paragraph: $('paragraph').value, first, second: 1 - first, gap: 20 }, 'Figura e parágrafo lado a lado.'); });
$('stack').addEventListener('click', () => action('stack', { id: state.selection }, 'Apresentação em coluna única.'));
$('restore').addEventListener('click', () => action('restore', {}, 'Apresentação restaurada pela sequência autoral.'));
$('ratio').addEventListener('input', () => { $('ratio-label').textContent = $('ratio').value + '%'; });

$('create-formula').addEventListener('click',()=> {
  const inCell=$('formula-target').value==='cell';
  if(inCell && (!selectedTable() || !state.selected_cell)) {message('Selecione primeiro uma célula da tabela.',true);return;}
  const source=$('formula-source').value;const command=/^\s*(?:\{\s*)?alterar\b/.test(source);
  const presentation=richProfile()?{prefix:command?'':$('formula-prefix').value,suffix:command?'':$('formula-suffix').value}:{};
  action('create_formula',{name:$('formula-name').value,source,before:inCell?'':$('formula-before').value,table:inCell?state.selection:'',cell:inCell?state.selected_cell:'',...presentation},'Fórmula criada. Clique no resultado para editar.');
});
$('formula-library').addEventListener('change',()=>{renderVariables();controls();});
$('edit-formula').addEventListener('click',()=> {
  const f=libraryFormula();if(!f)return;
  const use=state.formula_uses.find(u=>u.object===f.id);
  if(use){openFormula(f.id,use.id);return;}
  for(const t of state.tables) {const c=t.cells.find(c=>state.formula_cells[c.id]===f.id);if(c){openFormula(f.id,c.id,t.id,c.id);return;}}
  openFormula(f.id,'library');
});
$('execute-formula').addEventListener('click',()=>{const f=libraryFormula();if(f)action('execute_formula',{id:f.id},'Alterações executadas uma vez; resultados dependentes atualizados.');});
$('attach-formula').addEventListener('click',()=>{const f=libraryFormula();if(f)action('attach_formula',{id:f.id,before:'',table:state.selection,cell:state.selected_cell},'A célula usa o mesmo objeto Fórmula.');});
$('detach-formula').addEventListener('click',()=>action('detach_formula',{table:state.selection,cell:state.selected_cell},'Fórmula retirada da célula; o texto anterior foi conservado.'));

$('create-table').addEventListener('click', () => action('create_table', { before: $('table-before').value, title: $('table-title').value, header: $('table-header').checked, cells: ['Lugar', 'Imagem', 'Nota', 'Margem', 'Verde', 'Memória 🙂'] }, 'Tabela inserida. Clique em uma célula para editar.'));
$('remove').addEventListener('click', () => action('remove', { id: state.selection }, 'Uso removido. Os recursos foram conservados; Desfazer restaura o uso.'));
$('undo').addEventListener('click', () => action('undo', {}, 'Operação desfeita.'));
$('redo').addEventListener('click', () => action('redo', {}, 'Operação refeita.'));

$('save').addEventListener('click', () => action('save', {}, 'Documento salvo e conferido no armazenamento local.'));
$('reopen').addEventListener('click', () => {
  if ((state.dirty || hasDrafts() || state.persistence_status === 'uncertain') && !confirm('Reabrir o estado salvo e descartar as alterações posteriores a ele?')) return;
  action('reopen', { discard_unsaved: state.dirty || hasDrafts() || state.persistence_status === 'uncertain' }, 'Estado salvo reaberto e conferido.');
});
$('context').addEventListener('change', () => action('set_context',{context:$('context').value},'Instrumento alterado; seleção conservada.'));
$('reorder-text').addEventListener('click', () => action('reorder_text',{group:$('text-group').value,before:$('text-before').value},'Grupo textual movido pelo motor proprietário.'));
$('publication-check').addEventListener('click', () => action('export_legacy'));
$('reopen-working').addEventListener('click', () => {
  if (hasDrafts() && !confirm('Reabrir o trabalho registrado e descartar o texto que ainda não foi confirmado?')) return;
  action('reopen_working',{discard_unsaved:true},'Trabalho registrado reaberto e conferido.');
});
$('visual-tab').addEventListener('click', () => { mode = 'visual'; render(); });
$('reading-tab').addEventListener('click', () => { mode = 'reading'; render(); });
window.addEventListener('beforeunload', event => { if (hasDrafts() || (state?.dirty && !['P2-C','P2-D','P2-D2'].includes(state.profile)) || state?.persistence_status === 'uncertain' || state?.persistence_status === 'error') { event.preventDefault(); event.returnValue = ''; } });
(async () => {
  try {
    const response = await fetchApi('/api/state'), result = await response.json();
    if (!result.accepted) throw new Error(result.error);
    state = result.state; render();
  } catch (error) { message(errors[error.message] || 'Não foi possível abrir o documento experimental.', true); }
})();

// Approved card authoring: the C++ compiler/evaluator is the only authority.
let cardsEditing = null, cardsPreviewTimer = null, cardsPreviewSequence = 0;
const cardsOperators = {
  number: [['lt','for menor que'],['le','for igual ou menor que'],['eq','for igual a'],['ne','for diferente de'],['ge','for igual ou maior que'],['gt','for maior que']],
  text: [['eq','for igual a'],['ne','for diferente de'],['contains','contiver'],['starts','começar com']],
};
function cardsNode(tag, text, props={}) {
  const n=document.createElement(tag); if(text!==null)n.textContent=text;
  for(const [k,v] of Object.entries(props))n.setAttribute(k,v);return n;
}
function cardsField(label,node) {const l=cardsNode('label',label);l.append(node);return l;}
function cardsButton(label,fn) {const b=cardsNode('button',label,{type:'button'});b.addEventListener('click',fn);return b;}
function cardsFingerprint() {return cardsEditing?JSON.stringify({model:cardsEditing.model,prefix:cardsEditing.prefix,suffix:cardsEditing.suffix,where:cardsEditing.where}):'';}
function cardsDraft() {return cardsEditing&&cardsFingerprint()!==cardsEditing.initial;}
function cardsNewModel(type='number') {return {version:1,type,label:type==='number'?'o preço':'Atitude',entry:{kind:'literal',value:type==='number'?'16':'hostil'},rules:type==='number'?[{op:'gt',value:'15',text:'preço caro'},{op:'eq',value:'15',text:'vou negociar'}]:[{op:'eq',value:'hostil',text:'ameaçador'},{op:'eq',value:'amistosa',text:'receptivo'}],otherwise:type==='number'?'que pechincha':'indecifrável'};}
function cardsSetup() {
  if(!$('cards-tools'))return;
  $('cards-tools').hidden=!state?.formula_cards_supported;
  if(!state?.formula_cards_supported)return;
  if(richProfile()&&!$('formula-advanced')) {
    const advanced=cardsNode('details',null,{id:'formula-advanced'}), summary=cardsNode('summary','Contas e comandos avançados');
    advanced.append(summary);const host=$('formula-tools');while(host.firstChild)advanced.append(host.firstChild);host.append(advanced);
    $('create-cards').addEventListener('click',()=>openCards());
    $('edit-cards').addEventListener('click',()=>{
      const f=state.formulas.find(f=>f.id===$('cards-library').value);if(!f)return;
      const u=state.formula_uses.find(u=>u.object===f.id);
      if(u){openCards(f.id,u.id);return;}
      for(const t of state.tables)for(const c of t.cells)if(state.formula_cells[c.id]===f.id){openCards(f.id,c.id,t.id,c.id);return;}
      openCards(f.id,'library');
    });
    $('cards-library').addEventListener('change',cardsControls);
  }
  if(richProfile()) options('cards-library',state.formulas.filter(f=>f.cards&&!f.consumed).map(f=>[f.id,`${f.cards.label||'Entrada'} · ${f.valid?f.display:'conferir'}`]),'');
}
function cardsControls() {
  if(!$('create-cards'))return;const blocked=busy||state?.persistence_status==='uncertain';
  $('create-cards').disabled=blocked||!state?.formula_cards_supported;$('edit-cards').disabled=blocked||!$('cards-library').value;$('cards-library').disabled=blocked;
  const dialog=$('cards-dialog');if(dialog)for(const n of dialog.querySelectorAll('input,select,textarea,button'))n.disabled=blocked||n.dataset.fixed==='true';
}
async function openCards(id='',key='',table='',cell='') {
  if(busy)return;
  if(id) {
    const f=state.formulas.find(f=>f.id===id);if(!f?.cards){message('Esta conta usa uma expressão avançada. Sua fonte foi conservada.',true);return;}
    if(state.context!=='text'&&!await action('set_context',{context:'text'}))return;
    if(key!=='library'&&!await richChoose(key,table,cell))return;
  } else if(!await action('set_context',{context:'structure'}))return;
  const f=state.formulas.find(f=>f.id===id);let prefix='',suffix='',target=key==='library'?'':key,complex=false;
  if(target) {
    const runs=richModel(target)?.runs||[];let seen=false,count=0;
    for(const r of runs)if(r.kind==='result'){if(r.formula!==id||++count!==1)complex=true;seen=true;}else if(seen)suffix+=r.text;else prefix+=r.text;
    if(complex){target='';prefix='';suffix='';}
  }
  cardsEditing={id,key,table,cell,target,complex,model:f?structuredClone(f.cards):cardsNewModel(),prefix:f?prefix:'Ei Armand ... eita, ',suffix:f?suffix:' isso!',models:{}};
  cardsEditing.models[cardsEditing.model.type]=cardsEditing.model;
  cardsEditing.where={target:cell?'cell':'document',before:$('formula-before').value};
  cardsEditing.initial=cardsFingerprint();formulaEditing=null;richTextEditing=null;
  let dialog=$('cards-dialog');if(!dialog){dialog=cardsNode('dialog',null,{id:'cards-dialog','aria-labelledby':'cards-title'});document.body.append(dialog);dialog.addEventListener('cancel',()=>closeCards());}
  buildCardsDialog();dialog.showModal();dialog.querySelector('input,select')?.focus();queueCardsPreview();
}
function closeCards() {clearTimeout(cardsPreviewTimer);++cardsPreviewSequence;cardsEditing=null;$('cards-dialog')?.close();controls();}
function cardsChanged() {controls();queueCardsPreview();}
function cardsInput(value,onInput,{number=false,multiline=false,limit=4096}={}) {
  const n=cardsNode(multiline?'textarea':'input',null,{maxlength:String(limit)});if(number){n.type='number';n.step='any';}else if(!multiline)n.type='text';n.value=value;
  n.addEventListener('input',()=>{onInput(n.value);cardsChanged();});return n;
}
function cardsReferenceSelect(editor) {
  const select=cardsNode('select',null,{'aria-label':'Resultado ou variável de entrada'});
  select.append(cardsNode('option','Escolha uma informação',{value:''}));
  for(const f of state.formulas.filter(f=>f.id!==editor.id&&!f.command&&!f.consumed)) {
    for(const symbol of ['resultado',...f.variables.map(v=>v.name)])select.append(cardsNode('option',`${f.name} · ${symbol==='resultado'?'resultado':symbol}`,{value:JSON.stringify([f.name,symbol])}));
  }
  select.value=editor.model.entry.kind==='reference'?JSON.stringify([editor.model.entry.formula,editor.model.entry.variable]):'';
  select.addEventListener('change',()=>{if(!select.value)return;const [formula,variable]=JSON.parse(select.value);editor.model.entry={kind:'reference',formula,variable};cardsChanged();});return select;
}
function buildCardsDialog() {
  const e=cardsEditing;if(!e)return;const model=e.model,dialog=$('cards-dialog');dialog.replaceChildren();
  dialog.append(cardsNode('h2',e.id?'Editar texto por condições':'Criar texto por condições',{id:'cards-title'}));
  const intro=cardsNode('p','A primeira condição atendida escolhe o texto. As palavras ao redor continuam suas.',{class:'hint'});dialog.append(intro);
  const error=cardsNode('p','',{id:'cards-error',role:'alert',class:'formula-error'});error.hidden=true;dialog.append(error);
  if(!e.id) {
    const destination=cardsNode('section',null,{class:'cards-destination'}),target=cardsNode('select',null,{'aria-label':'Inserir o trecho em'});
    target.append(cardsNode('option','Documento',{value:'document'}));if(selectedTable()&&state.selected_cell)target.append(cardsNode('option','Célula selecionada',{value:'cell'}));target.value=e.where.target;
    target.addEventListener('change',()=>{e.where.target=target.value;buildCardsDialog();cardsChanged();});destination.append(cardsField('Inserir o trecho em',target));
    if(e.where.target==='document'){const before=cardsNode('select',null,{'aria-label':'Inserir antes de'});for(const option of $('formula-before').options)before.append(option.cloneNode(true));before.value=e.where.before;before.addEventListener('change',()=>{e.where.before=before.value;cardsChanged();});destination.append(cardsField('Inserir antes de',before));}dialog.append(destination);
  }
  const entry=cardsNode('section',null,{class:'cards-entry'});entry.append(cardsNode('h3','Entrada para comparar'));
  const type=cardsNode('select',null,{'aria-label':'Tipo de entrada'});type.append(cardsNode('option','Número',{value:'number'}),cardsNode('option','Texto',{value:'text'}));type.value=model.type;
  type.addEventListener('change',()=>{e.models[model.type]=model;e.model=e.models[type.value]||cardsNewModel(type.value);e.models[type.value]=e.model;buildCardsDialog();cardsChanged();});
  entry.append(cardsField('Tipo de entrada',type),cardsField('Nome da entrada',cardsInput(model.label,v=>{model.label=v;for(const n of dialog.querySelectorAll('.cards-entry-name'))n.textContent=v||'Entrada';},{limit:256})));
  const origin=cardsNode('select',null,{'aria-label':'De onde vem a entrada'});origin.append(cardsNode('option','Valor preenchido aqui',{value:'literal'}),cardsNode('option','Resultado ou variável existente',{value:'reference'}));origin.value=model.entry.kind;
  origin.addEventListener('change',()=>{model.entry=origin.value==='literal'?{kind:'literal',value:model.type==='number'?'0':''}:{kind:'reference',formula:'',variable:''};delete model.current;buildCardsDialog();cardsChanged();});entry.append(cardsField('De onde vem a entrada',origin));
  if(model.entry.kind==='literal')entry.append(cardsField('Valor da entrada',cardsInput(model.entry.value,v=>model.entry.value=v,{number:model.type==='number'})));
  else entry.append(cardsField('Informação escolhida',cardsReferenceSelect(e)));
  if(Object.hasOwn(model,'current')){entry.append(cardsField('Valor atual alterado',cardsInput(model.current,v=>model.current=v,{number:model.type==='number'})),cardsButton('Voltar à entrada original',()=>{delete model.current;buildCardsDialog();cardsChanged();}));}
  dialog.append(entry);
  const context=cardsNode('section',null,{class:'cards-context'});context.append(cardsNode('h3','Frase de contexto'));
  const prefix=cardsInput(e.prefix,v=>e.prefix=v,{multiline:true}),suffix=cardsInput(e.suffix,v=>e.suffix=v,{multiline:true});
  if(e.complex||e.key==='library'){prefix.disabled=suffix.disabled=true;prefix.dataset.fixed=suffix.dataset.fixed='true';context.append(cardsNode('p',e.complex?'Esta frase reúne outros resultados. Edite seus trechos em Editar frase; os cartões conservam a montagem existente.':'Este objeto está sem uso escolhido. A frase pertence a cada lugar onde ele aparece.',{class:'hint'}));}
  context.append(cardsField('Texto antes do resultado',prefix),cardsField('Texto depois do resultado',suffix));dialog.append(context);
  const list=cardsNode('section',null,{class:'cards-rules'});list.append(cardsNode('h3','Condições · vale a primeira que servir'));
  model.rules.forEach((rule,index)=>{
    const card=cardsNode('div',null,{class:'cards-rule'}),head=cardsNode('div',null,{class:'cards-rule-head'});head.append(cardsNode('strong',`Condição ${index+1}`));
    const up=cardsButton('Subir',()=>{[model.rules[index-1],model.rules[index]]=[model.rules[index],model.rules[index-1]];buildCardsDialog();cardsChanged();});up.disabled=index===0;up.dataset.fixed=String(index===0);
    const down=cardsButton('Descer',()=>{[model.rules[index+1],model.rules[index]]=[model.rules[index],model.rules[index+1]];buildCardsDialog();cardsChanged();});down.disabled=index===model.rules.length-1;down.dataset.fixed=String(down.disabled);
    head.append(up,down,cardsButton('Remover',()=>{model.rules.splice(index,1);buildCardsDialog();cardsChanged();}));card.append(head);
    const when=cardsNode('div',null,{class:'cards-when'}),comparison=cardsNode('select',null,{'aria-label':`Comparação da condição ${index+1}`});
    for(const [value,label] of cardsOperators[model.type])comparison.append(cardsNode('option',label,{value}));comparison.value=rule.op;comparison.addEventListener('change',()=>{rule.op=comparison.value;cardsChanged();});
    const value=cardsInput(rule.value,v=>rule.value=v,{number:model.type==='number'});value.setAttribute('aria-label',`Valor da condição ${index+1}`);
    when.append(cardsNode('span','Se'),cardsNode('span',model.label||'Entrada',{class:'cards-entry-name'}),comparison,value);card.append(when,cardsField('Mostrar esta palavra ou frase',cardsInput(rule.text,v=>rule.text=v,{multiline:true})));list.append(card);
  });
  const add=cardsButton('Adicionar condição',()=>{model.rules.push({op:'eq',value:'',text:''});buildCardsDialog();cardsChanged();});add.disabled=model.rules.length>=12;add.dataset.fixed=String(add.disabled);list.append(add,cardsField('Se nenhuma condição servir, mostrar',cardsInput(model.otherwise,v=>model.otherwise=v,{multiline:true})));dialog.append(list);
  const preview=cardsNode('section',null,{class:'cards-preview'});preview.append(cardsNode('h3','Prévia no documento'),cardsNode('p','Conferindo…',{id:'cards-preview-result',class:'paragraph'}),cardsNode('p','Rascunho: aplicar confirma o trecho para recuperação.',{id:'cards-preview-note',class:'hint'}));dialog.append(preview);
  const footer=cardsNode('div',null,{class:'cards-footer'});footer.append(cardsButton(e.id?'Aplicar cartões e frase':'Inserir texto por condições',applyCards),cardsButton('Cancelar',closeCards));
  if(e.id)footer.append(cardsButton('Abrir fonte avançada',()=>{if(cardsDraft()){message('Aplique ou cancele as mudanças dos cartões antes de abrir a fonte.',true);return;}const {id,key,table,cell}=e;closeCards();openFormula(id,key,table,cell,true);}));dialog.append(footer);cardsControls();
}
function queueCardsPreview() {
  clearTimeout(cardsPreviewTimer);const sequence=++cardsPreviewSequence;
  if(!cardsEditing)return;
  $('cards-preview-note').textContent='Conferindo a regra…';
  cardsPreviewTimer=setTimeout(async()=>{
    const e=cardsEditing;if(!e)return;const fingerprint=cardsFingerprint();
    try {
      const response=await fetchApi('/api/request',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({op:'preview_formula_cards',expected:state.revision,id:e.id,cards:e.model})});const r=await response.json();
      if(sequence!==cardsPreviewSequence||cardsEditing!==e||fingerprint!==cardsFingerprint())return;
      const p=r.preview;
      $('cards-preview-result').textContent=r.accepted&&p?.valid?e.prefix+p.display+e.suffix:'Confira a entrada e as condições.';
      $('cards-preview-note').textContent=r.accepted&&p?.valid?'Prévia calculada pelo núcleo. Aplicar protege o trabalho; Cancelar conserva o estado anterior.':errors[p?.error||r.error]||'Preencha os valores das condições e escolha uma entrada compatível.';
    } catch {if(sequence===cardsPreviewSequence&&cardsEditing===e)$('cards-preview-note').textContent='A prévia não pôde ser calculada. Seu rascunho foi conservado.';}
  },250);
}
async function applyCards() {
  const e=cardsEditing;if(!e||busy)return;clearTimeout(cardsPreviewTimer);++cardsPreviewSequence;
  let values={cards:structuredClone(e.model),prefix:e.prefix,suffix:e.suffix},op;
  if(e.id){op='edit_formula_cards';values={...values,id:e.id,target:e.target};}
  else {
    let index=1;while(state.formulas.some(f=>f.name===`Escolha${index}`))index++;
    const inCell=e.where.target==='cell';if(inCell&&(!selectedTable()||!state.selected_cell)){message('Selecione uma célula antes de criar o trecho.',true);return;}
    op='create_formula_cards';values={...values,name:`Escolha${index}`,before:inCell?'':e.where.before,table:inCell?state.selection:'',cell:inCell?state.selected_cell:''};
  }
  if(await action(op,values,'Cartões e frase confirmados no documento.'))closeCards();else if(cardsEditing===e)queueCardsPreview();
}
