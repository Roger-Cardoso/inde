'use strict';
const $ = id => document.getElementById(id);
const token = location.hash.slice(1) || sessionStorage.getItem('obj01-p2a-token') || '';
if (token) sessionStorage.setItem('obj01-p2a-token', token);
if (location.hash) history.replaceState(null, '', location.pathname);
let state, upload, uploadName = '', mode = 'visual', busy = false, generation = 0;
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
async function action(op, values = {}, success = '') {
  if (busy || !state) return;
  busy = true; controls();
  try {
    const response = await fetchApi('/api/request', {
      method: 'POST', headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ op, expected: state.revision, ...values }),
    });
    const result = await response.json();
    if (result.state) {
      state = result.state; render();
      if (op === 'select') {
        const selected = [...$('document').querySelectorAll('.item')].find(n => n.dataset.occurrence === state.selection);
        selected?.focus({ preventScroll: true });
      }
    }
    if (!result.accepted) throw new Error(result.error);
    if (success) message(success);
  } catch (error) {
    message(errors[error.message] || `Operação recusada: ${error.message}`, true);
  } finally { busy = false; controls(); }
}
function figure() { return state?.figures.find(f => f.id === state.selection); }
function shortText(id) {
  const p = state.paragraphs.find(p => p.id === id);
  if (p) return p.text.length > 36 ? p.text.slice(0, 36) + '…' : p.text;
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
  for (const id of ['save', 'reopen', 'sample', 'file', 'caption', 'alt', 'decorative', 'intent', 'insert-before', 'move-before', 'paragraph', 'ratio', 'restore']) $(id).disabled = busy;
  $('import').disabled = busy || !upload;
  for (const id of ['move', 'duplicate', 'arrange', 'stack']) $(id).disabled = busy || !figure();
  $('alt').disabled = busy || $('decorative').checked;
  $('save-status').textContent = state.dirty ? 'Alterações em memória' : 'Salvo localmente';
  $('save-status').className = state.dirty ? 'dirty' : '';
}
async function imageUrl(resource, hash) {
  const key = resource + '/' + hash;
  if (!imageCache.has(key)) imageCache.set(key, fetchApi('/api/image/' + resource).then(r => r.blob()).then(b => URL.createObjectURL(b)).catch(e => { imageCache.delete(key); throw e; }));
  return imageCache.get(key);
}
function item(id, currentGeneration) {
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
  const destination = state.author_order.filter(id => state.figures.some(f => f.id === id) || state.reading_groups.some(g => g[0] === id)).map(id => [id, shortText(id)]);
  destination.push(['', 'Fim do documento']);
  options('insert-before', destination, state.paragraphs[1]?.id || '');
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
$('save').addEventListener('click', () => action('save', {}, 'Documento salvo e conferido no armazenamento local.'));
$('reopen').addEventListener('click', () => {
  if (state.dirty && !confirm('Reabrir o estado salvo e descartar as alterações que estão apenas em memória?')) return;
  action('reopen', { discard_unsaved: state.dirty }, 'Estado salvo reaberto e conferido.');
});
$('visual-tab').addEventListener('click', () => { mode = 'visual'; render(); });
$('reading-tab').addEventListener('click', () => { mode = 'reading'; render(); });
window.addEventListener('beforeunload', event => { if (state?.dirty) { event.preventDefault(); event.returnValue = ''; } });
(async () => {
  try {
    const response = await fetchApi('/api/state'), result = await response.json();
    if (!result.accepted) throw new Error(result.error);
    state = result.state; render();
  } catch (error) { message(errors[error.message] || 'Não foi possível abrir o documento experimental.', true); }
})();
