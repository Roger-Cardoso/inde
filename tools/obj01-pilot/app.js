'use strict';
const $ = id => document.getElementById(id);
const token = location.hash.slice(1) || sessionStorage.getItem('obj01-p2a-token') || '';
if (token) sessionStorage.setItem('obj01-p2a-token', token);
if (location.hash) history.replaceState(null, '', location.pathname);
let state, upload, uploadName = '', mode = 'visual', busy = false, generation = 0, activeOperation = '';
const imageCache = new Map();
const errors = {
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
function hasDrafts() { return [...document.querySelectorAll('.cell-editor')].some(n => n.value !== cellData(n.dataset.table, n.dataset.cell)?.text); }
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
    if (op === 'edit_cell' || op === 'select_cell') updateSelection();
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
      // A structural change/save must never silently discard a typed cell.
      if (!['edit_cell', 'select_cell', 'reopen'].includes(op)) {
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
function figure() { return state?.figures.find(f => f.id === state.selection); }
function shortText(id) {
  const p = state.paragraphs.find(p => p.id === id);
  if (p) return p.text.length > 36 ? p.text.slice(0, 36) + '…' : p.text;
  const table = state.tables?.find(t => t.id === id);
  if (table) return 'Tabela: ' + table.title;
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
  $(id).value = values.some(v => v[0] === selected) ? selected : fallback;
}
function controls() {
  if (!state) return;
  for (const n of document.querySelectorAll('.cell-editor')) n.readOnly = busy && !['edit_cell', 'select_cell'].includes(activeOperation);
  for (const id of ['save', 'reopen', 'sample', 'file', 'caption', 'alt', 'decorative', 'intent', 'insert-before', 'move-before', 'paragraph', 'ratio', 'restore', 'table-title', 'table-header', 'table-before', 'create-table']) $(id).disabled = busy;
  $('import').disabled = busy || !upload;
  for (const id of ['move', 'duplicate', 'arrange', 'stack']) $(id).disabled = busy || !figure();
  $('save').disabled = false; $('reopen').disabled = false;
  $('undo').disabled = !state.can_undo; $('redo').disabled = !state.can_redo;
  $('move').disabled = busy || !(figure() || selectedTable());
  $('remove').disabled = busy || !(figure() || selectedTable());
  $('move').textContent = selectedTable() ? 'Mover tabela selecionada' : 'Mover figura selecionada';
  $('selected-label').textContent = selectedTable() ? 'Tabela selecionada: ' + selectedTable().title : figure() ? 'Figura selecionada: ' + (figure().caption || 'sem legenda') : 'Selecione uma figura ou tabela no documento.';
  $('alt').disabled = busy || $('decorative').checked;
  $('save-status').textContent = hasDrafts() ? 'Célula em edição' : state.dirty ? 'Alterações em memória' : 'Salvo localmente';
  $('save-status').className = state.dirty ? 'dirty' : '';
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
      input.addEventListener('input', controls);
      input.addEventListener('keydown', async event => {
        if (event.key !== 'Tab' && !(event.key === 'Enter' && (event.ctrlKey || event.metaKey))) return;
        event.preventDefault();
        if (!await commit()) { input.focus(); return; }
        if (event.key === 'Enter') { message('Célula aplicada ao documento.'); return; }
        const next = ordered.findIndex(c => c.id === cell.id) + (event.shiftKey ? -1 : 1);
        if (next < 0) { wrapper.focus(); return; }
        if (next >= ordered.length) { exit.focus(); return; }
        [...wrapper.querySelectorAll('.cell-editor')].find(n => n.dataset.cell === ordered[next].id)?.focus();
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

function item(id, currentGeneration) {
  const table = state.tables?.find(t => t.id === id);
  if (table) return tableItem(table);
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
  const p2b = state.profile === 'P2-B';
  document.querySelectorAll('.p2b-only').forEach(n => { n.hidden = !p2b; });
  $('phase').textContent = 'OBJ-01 / ' + state.profile;
  if (p2b) { document.title = 'INDE · Texto, figura e tabela'; document.querySelector('.eyebrow').textContent = 'DOCUMENTO COMPOSTO · TEXTO + FIGURA + TABELA'; document.querySelector('.intro p:not(.eyebrow)').textContent = 'Edite notas e experimente a composição sem perder a ordem de leitura.'; document.querySelector('.paper-footer').textContent = 'INDE · EXPERIMENTO AUTORAL P2-B'; document.querySelector('h1').textContent = 'Palavras, figuras e notas.'; $('limits').textContent = 'Texto de origem em leitura. Desfazer/refazer vale nesta sessão; alterações não salvas e o histórico não sobrevivem ao encerramento.'; }
  const destination = state.author_order.filter(id => !state.rows.some(r => r.children[1] === id) && (state.figures.some(f => f.id === id) || state.tables?.some(t => t.id === id) || state.reading_groups.some(g => g[0] === id))).map(id => [id, shortText(id)]);
  destination.push(['', 'Fim do documento']);
  options('insert-before', destination, state.paragraphs[1]?.id || '');
  options('table-before', destination, '');
  options('move-before', destination.filter(([id]) => id !== state.selection), '');
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

$('create-table').addEventListener('click', () => action('create_table', { before: $('table-before').value, title: $('table-title').value, header: $('table-header').checked, cells: ['Lugar', 'Imagem', 'Nota', 'Margem', 'Verde', 'Memória 🙂'] }, 'Tabela inserida. Clique em uma célula para editar.'));
$('remove').addEventListener('click', () => action('remove', { id: state.selection }, 'Uso removido. Os recursos foram conservados; Desfazer restaura o uso.'));
$('undo').addEventListener('click', () => action('undo', {}, 'Operação desfeita.'));
$('redo').addEventListener('click', () => action('redo', {}, 'Operação refeita.'));

$('save').addEventListener('click', () => action('save', {}, 'Documento salvo e conferido no armazenamento local.'));
$('reopen').addEventListener('click', () => {
  if ((state.dirty || hasDrafts()) && !confirm('Reabrir o estado salvo e descartar as alterações que estão apenas em memória?')) return;
  action('reopen', { discard_unsaved: state.dirty || hasDrafts() }, 'Estado salvo reaberto e conferido.');
});
$('visual-tab').addEventListener('click', () => { mode = 'visual'; render(); });
$('reading-tab').addEventListener('click', () => { mode = 'reading'; render(); });
window.addEventListener('beforeunload', event => { if (state?.dirty || hasDrafts()) { event.preventDefault(); event.returnValue = ''; } });
(async () => {
  try {
    const response = await fetchApi('/api/state'), result = await response.json();
    if (!result.accepted) throw new Error(result.error);
    state = result.state; render();
  } catch (error) { message(errors[error.message] || 'Não foi possível abrir o documento experimental.', true); }
})();
