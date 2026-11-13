/* Reações por navegador, sem cadastro. O servidor é a autoridade sobre votos e totais. */
(function(global){
 'use strict';
 const KEY='literature:reacoes:visitante:v1',UUID=/^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;
 let sequencia=0,widget=null;
 const criar=(tag,texto,classe)=>{const e=document.createElement(tag);if(texto)e.textContent=texto;if(classe)e.className=classe;return e;};
 function permitido(config,obra,capitulo){
  const r=config.reacoes,c=config.comentarios;
  return !!(r?.ativado&&c?.api_url&&c?.turnstile_site_key&&obra.reacoes!=='desativado'&&capitulo?.reacoes!=='desativado'&&((capitulo||obra).reacoes==='ativado'||r[capitulo?'capitulos':'obras']===true));
 }
 function visitante(criarNovo=false){
  try{
   const salvo=global.localStorage.getItem(KEY);if(salvo&&UUID.test(salvo))return salvo;if(!criarNovo)return null;
   const id=crypto.randomUUID();global.localStorage.setItem(KEY,id);if(global.localStorage.getItem(KEY)!==id)throw Error();return id;
  }catch{if(criarNovo)throw Error('Para reagir, permita o armazenamento local deste site. Os totais continuam disponíveis.');return null;}
 }
 async function montar({obra,capitulo=null,destino}){
  if(!destino)return;const vez=++sequencia;destino.replaceChildren();destino.hidden=true;
  if(widget!==null&&global.turnstile){try{global.turnstile.remove(widget);}catch{}widget=null;}
  try{
   const r=await fetch('../configuracao-site.json',{cache:'no-store'});if(!r.ok)return;const config=await r.json();if(vez!==sequencia||!permitido(config,obra,capitulo))return;
   const base=new URL(config.comentarios.api_url);if(base.protocol!=='https:'||base.username||base.password||base.port||base.search||base.hash||!['','/'].includes(base.pathname))throw Error();
   const {identifier:thread}=global.ComentariosLiterature.identidade(config,obra,capitulo);
   destino.hidden=false;destino.classList.add('reacoes-literature');destino.append(criar('h2',capitulo?'O que achou deste capítulo?':'O que achou desta obra?'));
   if(location.origin!==new URL(config.url_publica).origin||location.protocol!=='https:'){destino.append(criar('p','Reações disponíveis no site publicado. A prévia local não registra votos.'));return;}
   const grupo=criar('div',null,'reacoes-botoes');grupo.setAttribute('role','group');grupo.setAttribute('aria-label','Sua reação');destino.append(grupo);
   const gostei=criar('button','Gostei · —'),naoGostei=criar('button','Não gostei · —');for(const b of [gostei,naoGostei]){b.type='button';b.disabled=true;b.setAttribute('aria-pressed','false');grupo.append(b);}
   destino.append(criar('p','Uma reação por navegador em cada obra ou capítulo. Clique novamente para retirar ou escolha a outra opção para trocar.','reacoes-nota'));
   const status=criar('p');status.setAttribute('role','status');destino.append(status);
   const verificarArea=criar('div');destino.append(verificarArea);
   const atualizar=criar('button','Atualizar reações');atualizar.type='button';atualizar.className='reacoes-atualizar';destino.append(atualizar);
   let state=null,ocupado=false;
   const atual=()=>vez===sequencia;
   function travar(valor){ocupado=valor;destino.setAttribute('aria-busy',String(valor));for(const b of [gostei,naoGostei])b.disabled=valor||!state||Boolean(state.paused);atualizar.disabled=valor;}
   async function api(path,body){
    const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),15000);
    try{
     const response=await fetch(base.origin+path,{method:body?'POST':'GET',...(body?{headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{}),credentials:'omit',signal:controller.signal});
     const data=await response.json();if(!response.ok){const error=Error(data.error||'Reações temporariamente indisponíveis.');error.code=data.code;throw error;}return data;
    }catch(e){if(e.name==='AbortError')throw Error('O serviço demorou a responder. Atualize para conferir sua reação antes de tentar novamente.');throw e;}finally{clearTimeout(timer);}
   }
   function mostrar(data){
    if(!atual())return;
    if(!Number.isSafeInteger(data.likes)||data.likes<0||!Number.isSafeInteger(data.dislikes)||data.dislikes<0||![undefined,-1,0,1].includes(data.mine))throw Error('Não foi possível consultar os totais.');
    state={...data,mine:data.mine||0};gostei.textContent='Gostei · '+data.likes;naoGostei.textContent='Não gostei · '+data.dislikes;
    gostei.setAttribute('aria-pressed',String(state.mine===1));naoGostei.setAttribute('aria-pressed',String(state.mine===-1));
    status.textContent=state.paused?'As reações estão pausadas. Os votos registrados foram preservados.':state.mine===1?'Você marcou Gostei.':state.mine===-1?'Você marcou Não gostei.':'Você ainda não reagiu neste navegador.';
   }
   async function carregar(){
    if(ocupado)return;travar(true);status.textContent='Consultando reações…';
    try{const id=visitante();mostrar(await api(id?'/v1/reactions/state':'/v1/reactions?'+new URLSearchParams({thread}),id?{thread,visitor:id}:null));}
    catch(e){if(atual())status.textContent=e.message;}finally{if(atual())travar(false);}
   }
   async function verificar(){
    status.textContent='Verificando a conexão para reagir…';await global.ComentariosLiterature.carregarVerificacao();if(!atual())throw Error('Página alterada.');
    if(widget!==null){global.turnstile.remove(widget);widget=null;}verificarArea.replaceChildren();
    return new Promise((resolve,reject)=>{
     const timer=setTimeout(()=>reject(Error('A verificação expirou. Clique na reação para tentar novamente.')),120000);
     const terminar=(func,valor)=>{clearTimeout(timer);func(valor);};
     try{widget=global.turnstile.render(verificarArea,{sitekey:config.comentarios.turnstile_site_key,action:'reaction',theme:'auto',callback:t=>terminar(resolve,t),'expired-callback':()=>terminar(reject,Error('A verificação expirou. Tente novamente.')),'error-callback':()=>terminar(reject,Error('Não foi possível verificar. Clique na reação para tentar novamente.'))});}catch(e){terminar(reject,e);}
    });
   }
   async function reagir(valor){
    if(ocupado||!state||state.paused)return;travar(true);status.textContent='Registrando reação…';
    try{
     const id=visitante(true),consulta=await api('/v1/reactions/state',{thread,visitor:id});if(!atual())return;
     if(consulta.paused){mostrar(consulta);return;}
     // O alvo é explícito: repetir um pedido após falha não alterna o voto no servidor.
     const alvo=state.mine===valor?0:valor;let token=consulta.can_vote?null:await verificar();if(!atual())return;
     let data;
     try{data=await api('/v1/reactions',{thread,visitor:id,value:alvo,turnstile_token:token});}
     catch(e){if(e.code!=='verification_required'||token)throw e;token=await verificar();if(!atual())return;data=await api('/v1/reactions',{thread,visitor:id,value:alvo,turnstile_token:token});}
     mostrar(data);
    }catch(e){if(atual())status.textContent=e.message;}
    finally{if(atual()){travar(false);if(widget!==null&&global.turnstile){global.turnstile.remove(widget);widget=null;}verificarArea.replaceChildren();}}
   }
   gostei.onclick=()=>reagir(1);naoGostei.onclick=()=>reagir(-1);atualizar.onclick=carregar;await carregar();
  }catch{if(vez===sequencia)destino.hidden=true;}
 }
 global.ReacoesLiterature={montar,permitido};
})(window);
