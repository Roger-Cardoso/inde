/* Somente identidades e marcações fornecidas pelo autor. */
window.SemanticaLiterature={
 montar({obra,estrutura,raiz,area,destino}){
  const personagens=new Map((obra.personagens||[]).filter(p=>Unilit.revelar(p,obra.id_obra,estrutura)).map(p=>[p.id,p.nome]));
  try {
   const registro=JSON.parse(raiz.querySelector('script#story-entities[type="application/json"]')?.textContent||'[]');
   const canonicos=new Set((obra.personagens||[]).map(p=>p.id));
   for(const p of registro)if(p.id && p.name && !canonicos.has(p.id))personagens.set(p.id,p.name);
  }catch(_){}
  const nomeIds=valor=>String(valor||'').trim().split(/\s+/).map(id=>personagens.get(id)).filter(Boolean).join(', ');
  const nomeMarcado=(el,pov=false)=>{const exibido=pov?el.dataset.povDisplayName:el.dataset.displayName;return exibido?.trim()||nomeIds(pov?(el.dataset.povId||el.dataset.povCharacter):(el.dataset.locutorId||el.dataset.pensadorId||el.dataset.speaker||el.dataset.thinker));};
  const salva=Unilit.ler('unilit:v2:semantica',{});
  const configuracao=salva && typeof salva==='object'?salva:{};
  const painel=document.createElement('details');painel.className='contexto-capitulo auxilios-opcoes';
  const resumo=document.createElement('summary');resumo.textContent='Auxílios de leitura';painel.append(resumo);
  const status=document.createElement('p');status.className='contexto-pov';status.hidden=true;status.setAttribute('aria-live','polite');
  raiz.before(status);
  const controles={};
  for(const [chave,texto] of [['locutores','Identificar locutores e pensamentos'],['pov','Mostrar ponto de vista marcado']]){
   const label=document.createElement('label'),input=document.createElement('input');input.type='checkbox';input.checked=configuracao[chave]===true;
   label.className='opcao';label.append(input,document.createTextNode(texto));painel.append(label);controles[chave]=input;
   input.onchange=()=>{
    const topo=area.getBoundingClientRect().top;
    const ancora=[...raiz.querySelectorAll('p')].find(p=>p.getBoundingClientRect().bottom>topo);
    const antes=ancora?.getBoundingClientRect().top;
    configuracao[chave]=input.checked;Unilit.salvar('unilit:v2:semantica',configuracao);aplicar();
    if(ancora)area.scrollTop+=ancora.getBoundingClientRect().top-antes;
    atualizar();area.dispatchEvent(new Event('literature:auxilios'));
   };
  }
  const marcados=[];
  for(const el of raiz.querySelectorAll('[data-locutor-id], [data-pensador-id], [data-speaker], [data-thinker]')){
   const nome=nomeMarcado(el);if(!nome)continue;
   const etiqueta=document.createElement('span');etiqueta.className='etiqueta-locutor';etiqueta.textContent=((el.dataset.pensadorId||el.dataset.thinker)?'Pensamento · ':'')+nome;
   el.before(etiqueta);marcados.push(etiqueta);
  }
  const secoes=[...raiz.querySelectorAll('[data-pov-id], [data-pov-character]')].filter(el=>nomeMarcado(el,true));
  function atualizar(){
   if(!controles.pov.checked||!secoes.length){status.hidden=true;return;}
   const limite=area.getBoundingClientRect().top+area.clientHeight*.3;
   const ativa=secoes.filter(el=>el.getBoundingClientRect().top<=limite).at(-1);
   status.hidden=!ativa;
   const mensagem=ativa?'Ponto de vista · '+nomeMarcado(ativa,true):'';
   if(status.textContent!==mensagem)status.textContent=mensagem;
  }
  controles.locutores.disabled=!marcados.length;controles.pov.disabled=!secoes.length;
  for(const [chave,mensagem] of [['locutores','Sem locutores ou pensamentos identificados neste capítulo.'],['pov','Sem ponto de vista marcado neste capítulo.']]){
   if(controles[chave].disabled){const aviso=document.createElement('p');aviso.className='aviso-local';aviso.textContent=mensagem;painel.append(aviso);}
  }
  area.addEventListener('literature:layout',atualizar);
  function aplicar(){for(const etiqueta of marcados)etiqueta.hidden=!controles.locutores.checked;atualizar();}
  if(marcados.length||secoes.length){destino.append(painel);area.addEventListener('scroll',atualizar,{passive:true});aplicar();}
  else {
   status.remove();
   for(const input of Object.values(controles))input.disabled=true;
   const aviso=document.createElement('p');aviso.className='aviso-local';aviso.textContent='Este capítulo não possui marcações disponíveis de locutores ou ponto de vista. Os ajustes de conforto continuam disponíveis abaixo.';painel.append(aviso);destino.append(painel);
  }
  return {painel,atualizar};
 }
};
