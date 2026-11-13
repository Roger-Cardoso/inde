'use strict';
const $ = id => document.getElementById(id);
let current = null, dirty = false;
async function api(path, value) {
  const response = await fetch('/api/' + path, value === undefined ? {cache:'no-store'} : {
    method:'POST', headers:{'Content-Type':'application/json','X-INDE-Token':document.querySelector('meta[name="inde-token"]').content}, body:JSON.stringify(value)});
  const data = await response.json(); if (!response.ok) throw Error(data.erro || 'Não foi possível concluir.'); return data;
}
function status(message, error=false) { $('status').textContent=message; $('status').classList.toggle('error',error); }
async function action(fn) {
  const buttons=[...document.querySelectorAll('button')].map(el=>[el,el.disabled]);
  for(const [el] of buttons) el.disabled=true;
  try { await fn(); } catch(error) { status(error.message,true); }
  finally { for(const [el,before] of buttons) if(el.isConnected) el.disabled=before; }
}
function markDirty() { dirty=true; $('resultado').hidden=true; status('Há alterações não salvas. Salve antes de gerar ou executar outra operação.'); }
function requireSaved() { if(dirty) throw Error('Salve as alterações antes desta operação.'); }
async function allowSwitch() { return !dirty || await confirmAction('Há alterações não salvas. Deseja descartá-las para trocar de Site?'); }
function el(tag,text,cls) { const node=document.createElement(tag); if(text!==undefined)node.textContent=text; if(cls)node.className=cls; return node; }
function button(text,fn,cls='subtle') { const node=el('button',text,cls);node.type='button';node.onclick=()=>action(fn);return node; }

function confirmAction(message) {
  return new Promise(resolve => {
    const dialog = el('dialog', undefined, 'confirm-dialog');
    dialog.setAttribute('aria-label', 'Conferir operação');
    const form = document.createElement('form'); form.method = 'dialog';
    form.append(el('h2', 'Conferir operação'), el('p', message));
    const actions = el('div', undefined, 'actions');
    for (const [text, value, cls] of [['Cancelar','cancel','secondary'],['Confirmar operação','confirm','primary']]) {
      const control = el('button', text, cls); control.type = 'submit'; control.value = value; actions.append(control);
    }
    form.append(actions); dialog.append(form); document.body.append(dialog);
    dialog.addEventListener('close', () => { const accepted = dialog.returnValue === 'confirm'; dialog.remove(); resolve(accepted); }, {once:true});
    dialog.showModal();
  });
}

async function list() {
  const all=await api('projetos');$('sites').replaceChildren();
  for(const record of all) { const node=button(record.projeto.site.nome,async()=>{if(await allowSwitch())open(record);},'');node.setAttribute('aria-current',String(record.projeto.id===current?.projeto.id));$('sites').append(node); }
  const state=await api('estado');$('nativo').disabled=!state.ponte_nativa;
  $('nativo').title=state.ponte_nativa?'Cria um Projeto pelos serviços C++ do INDE.':'Execute Preparar-ponte.sh para habilitar.';
  $('sites-recuperar').replaceChildren();
  for(const item of state.sites_removidos) $('sites-recuperar').append(button('Restaurar Site: '+item.nome,async()=>{
    if(!await allowSwitch())return;open(await api('restaurar-site',{id:item.id}));status('Site restaurado. Confira sua publicação antes de gerar.');
  }));
}
function open(record) {
  current=structuredClone(record);dirty=false;$('welcome').hidden=true;$('editor').hidden=false;$('resultado').hidden=true;
  const s=current.projeto.site;
  for(const [id,key] of [['nome','nome'],['autor','autor'],['descricao','descricao'],['tema','tema'],['origem','origem'],['caminho','caminho_base']])$(id).value=s[key];
  const d=current.projeto.destino;$('destino').value=d.tipo;$('repositorio').value=d.repositorio;$('branch').value=d.branch;$('github').hidden=d.tipo!=='github';
  const native=current.projeto.origem_inde;$('origem-inde').hidden=!native;
  if(native){$('caminho-inde').value=record.caminho_inde||native.projeto_relativo;$('resumo-origem').textContent='Retrato com '+Object.keys(native.revisoes_documentos).length+' Documentos. '+native.avisos.length+(native.avisos.length===1?' omissão de seleção registrada.':' omissões de seleção registradas.');}
  renderWork();status('Projeto aberto. Configure, salve e gere uma edição para conferir.');list().catch(e=>status(e.message,true));
}
function field(label,value,multiline,change) { const wrapper=el('label',label),input=document.createElement(multiline?'textarea':'input');if(multiline)input.rows=6;input.value=value;input.oninput=()=>change(input.value);wrapper.append(input);return wrapper; }
function renderWork() {
  const work=current.projeto.obra;$('sem-obra').hidden=!!work;$('obra').hidden=!work;$('titulo').disabled=!work;$('conteudo-autoral').disabled=!!current.projeto.origem_inde;
  $('capitulos').replaceChildren();$('recursos').replaceChildren();$('recuperacao').replaceChildren();
  for(const item of current.projeto.lixeira||[]){const title=item.tipo==='obra'?item.valor.obra.titulo:item.valor.titulo||item.valor.nome;$('recuperacao').append(button('Restaurar '+item.tipo+': '+title,()=>operate('restaurar_item',{id:item.id})));}
  if(!work)return;$('titulo').value=work.titulo;$('endereco').value=work.endereco;$('sinopse').value=work.sinopse;$('obra-publica').checked=work.publica;
  for(const doc of work.documentos){
    const card=el('div',undefined,'chapter'),head=el('div',undefined,'chapter-head');head.append(el('h3','Capítulo '+doc.numero));
    const label=el('label',undefined,'check'),checkbox=document.createElement('input');checkbox.type='checkbox';checkbox.checked=doc.publico;checkbox.onchange=()=>{doc.publico=checkbox.checked;};label.append(checkbox,document.createTextNode('Incluir na edição'));head.append(label);
    card.append(head,field('Título',doc.titulo,false,v=>{doc.titulo=v;}),field('Texto do Documento',doc.texto,true,v=>{doc.texto=v;}),el('p',doc.anotacoes.length+' marcações explícitas de autoria.','annotation-note'));
    if(doc.anotacoes.length)card.append(button('Remover marcações deste Documento',()=>{current.projeto=gather();current.projeto.obra.documentos.find(d=>d.id===doc.id).anotacoes=[];renderWork();markDirty();}));
    card.append(button('Remover capítulo com recuperação',async()=>{requireSaved();if(await confirmAction('Retirar “'+doc.titulo+'”? Texto e identidade ficam na recuperação. O número público continuará reservado. Sem capítulos públicos, a Obra volta a rascunho.'))await operate('excluir_capitulo',{id:doc.id});},'danger'));
    $('capitulos').append(card);
  }
  for(const resource of current.recursos||[]){
    const row=el('div',undefined,'resource');row.append(el('h4',resource.tipo==='personagem'?'Personagem':'Verbete'));
    const name=field('Nome do recurso',resource.nome,false,()=>{}),description=field('Descrição pública',resource.descricao||'',true,()=>{});
    name.querySelector('input').dataset.resourceName=resource.id;description.querySelector('textarea').dataset.resourceDescription=resource.id;
    row.append(name,description,el('small','Identidade: '+resource.id),button('Remover recurso com recuperação',async()=>{requireSaved();if(await confirmAction('Remover “'+resource.nome+'”? A operação será bloqueada se houver referências.'))await operate('excluir_recurso',{id:resource.id});},'danger'));$('recursos').append(row);
  }
}
function gather() {
  const data=structuredClone(current.projeto),s=data.site;
  for(const [id,key]of [['nome','nome'],['autor','autor'],['descricao','descricao'],['tema','tema'],['origem','origem'],['caminho','caminho_base']])s[key]=$(id).value.trim();
  data.destino={tipo:$('destino').value,repositorio:$('repositorio').value.trim(),branch:$('branch').value.trim()};
  if(data.obra&&!data.origem_inde){
    const w=data.obra;w.titulo=$('titulo').value.trim();w.endereco=$('endereco').value.trim();w.sinopse=$('sinopse').value.trim();w.publica=$('obra-publica').checked;
    const primary=w.personagem?.id;if(primary)for(const d of w.documentos){if(d.perspectiva)d.pov_id??=primary;for(const a of d.anotacoes)if(['dialogue','thought'].includes(a.tipo))a.entidade_id??=primary;}
    w.recursos=current.recursos.map(r=>({...r,nome:document.querySelector('[data-resource-name="'+r.id+'"]').value.trim(),descricao:document.querySelector('[data-resource-description="'+r.id+'"]').value.trim()}));w.personagem=null;
  }
  return data;
}
async function operate(acao,dados={}) { requireSaved();open(await api('operar',{id:current.projeto.id,revisao:current.revisao,acao,dados}));status('Operação concluída. Remoções podem ser restauradas abaixo. Gere novamente para atualizar o site.'); }
async function create(example,native=false){if(!await allowSwitch())return;status(native?'Criando o Projeto pelos serviços C++ do INDE…':'Criando Site…');open(await api(native?'criar-nativo':'criar',{exemplo:example}));status(native?'Projeto .inde criado pelo núcleo C++. O retrato está pronto para gerar.':example?'Amostra criada com identidades próprias.':'Site vazio criado. Crie uma Obra ou gere a biblioteca vazia.');}
$('novo').onclick=()=>action(()=>create(false));$('comecar').onclick=$('novo').onclick;$('amostra').onclick=()=>action(()=>create(true));$('nativo').onclick=()=>action(()=>create(false,true));
$('editor').addEventListener('input',e=>{if(e.target.id!=='caminho-inde')markDirty();});$('editor').addEventListener('change',e=>{if(e.target.id!=='caminho-inde')markDirty();});
$('destino').addEventListener('change',()=>{$('github').hidden=$('destino').value!=='github';});
$('editor').onsubmit=e=>{e.preventDefault();action(async()=>{open(await api('salvar',{projeto:gather(),revisao:current.revisao}));status('Projeto salvo. Gere novamente para conferir as alterações.');});};
$('gerar').onclick=()=>action(async()=>{requireSaved();status('Gerando a edição com o leitor Literature…');const result=await api('gerar',{id:current.projeto.id,revisao:current.revisao});$('previa').href=result.previa;$('pacote').href=result.pacote;$('estado-geracao').textContent=result.destino==='github'?'Prévia e pacote prontos. Nenhum envio ao GitHub foi realizado.':'Prévia e pacote público prontos.';$('resultado').hidden=false;status('Edição gerada e validada. A prévia usa somente arquivos públicos.');$('resultado').scrollIntoView({behavior:'smooth',block:'nearest'});});
$('criar-obra').onclick=()=>action(()=>operate('criar_obra'));$('capitulo-novo').onclick=()=>action(()=>operate('criar_capitulo'));
$('personagem-novo').onclick=()=>action(()=>operate('criar_recurso',{tipo:'personagem'}));$('verbete-novo').onclick=()=>action(()=>operate('criar_recurso',{tipo:'verbete'}));
$('excluir-obra').onclick=()=>action(async()=>{requireSaved();if(await confirmAction('Retirar esta Obra da edição? Textos e recursos ficam na recuperação. O Projeto INDE de origem será preservado.'))await operate('excluir_obra');});
$('excluir-site').onclick=()=>action(async()=>{requireSaved();if(!await confirmAction('Remover este Site local? Todo o projeto será guardado para restauração. Nenhuma hospedagem remota será alterada.'))return;await api('excluir-site',{id:current.projeto.id,revisao:current.revisao});current=null;dirty=false;$('editor').hidden=true;$('welcome').hidden=false;await list();status('Site removido. Use Restaurar Site na lateral para recuperá-lo.');});
$('atualizar-inde').onclick=()=>action(async()=>{requireSaved();open(await api('atualizar-inde',{id:current.projeto.id,revisao:current.revisao}));status('Retrato atualizado a partir de uma cópia estável da origem. O Projeto original foi preservado.');});
window.addEventListener('beforeunload',e=>{if(dirty){e.preventDefault();e.returnValue='';}});
list().catch(error=>status(error.message,true));
