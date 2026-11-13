/* Orientação editorial de leitura. Não classifica conteúdo automaticamente. */
window.GuiaCenas={
 relevancias:{nao_avaliada:'Relevância ainda não avaliada',essencial:'Essencial para acompanhar a trama',complementar:'Complementar',opcional:'Opcional para a continuidade',alta:'Essencial para acompanhar a trama',media:'Complementar',baixa:'Opcional para a continuidade'},
 montar({capitulo,destino,raiz=null,proximo=null,ir=null}){
  const cenas=capitulo.cenas||[];if(!cenas.length)return null;
  const painel=document.createElement('details');painel.className='guia-cenas';
  const summary=document.createElement('summary');summary.textContent='Guia de cenas · relevância e conteúdo';painel.append(summary);
  const aviso=document.createElement('p');aviso.textContent='Consulte os avisos antes de escolher o que ler. Detalhes e resumos podem revelar acontecimentos. Pular não marca o capítulo como concluído.';painel.append(aviso);
  function acao(texto,ancora){
   const a=document.createElement('a');a.textContent=texto;
   a.href=raiz?(capitulo.url_publica||'')+'#'+encodeURIComponent(ancora):(capitulo.url_publica?capitulo.url_publica+'?':'leitor.html?id='+encodeURIComponent(capitulo.id)+'&')+'cena='+encodeURIComponent(ancora);
   if(raiz)a.onclick=e=>{e.preventDefault();ir(ancora);};return a;
  }
  cenas.forEach((cena,i)=>{
   const sec=document.createElement('section'),h=document.createElement('h3');h.textContent=cena.titulo_cena;sec.append(h);
   const relevancia=document.createElement('p');relevancia.className='guia-relevancia';relevancia.textContent=this.relevancias[cena.relevancia]||cena.relevancia;sec.append(relevancia);
   const avisos=document.createElement('p');avisos.textContent=cena.avisos_conteudo?.length?'Conteúdo: '+cena.avisos_conteudo.join(' · '):(cena.avaliacao_conteudo==='revisada'?'Avaliação registrada sem avisos específicos.':'Conteúdo sensível ainda não avaliado. A ausência de avisos não indica ausência de conteúdo sensível.');sec.append(avisos);
   if(cena.tratamento){const p=document.createElement('p');p.textContent='Apresentação do conteúdo: '+cena.tratamento;sec.append(p);}
   const textos=[['Consequência de pular',cena.consequencia_pular],['Resumo alternativo',cena.resumo_alternativo],['Descrição',cena.descricao]];
   if(textos.some(([,t])=>t)){
    const detalhes=document.createElement('details'),s=document.createElement('summary');s.textContent='Detalhes opcionais · contém spoilers';detalhes.append(s);
    for(const [titulo,texto] of textos)if(texto){const p=document.createElement('p'),b=document.createElement('strong');b.textContent=titulo+': ';p.append(b,document.createTextNode(texto));detalhes.append(p);}
    sec.append(detalhes);
   }
   const nav=document.createElement('nav');nav.setAttribute('aria-label','Navegação da cena '+(i+1));
   nav.append(acao('Ir ao início desta cena',cena.ancora),acao(i+1<cenas.length?'Pular para a próxima cena':'Pular para o fim do capítulo',cenas[i+1]?.ancora||'fim-do-capitulo'));
   sec.append(nav);painel.append(sec);
  });
  if(proximo){const a=document.createElement('a');a.href=proximo.url_publica||'leitor.html?id='+encodeURIComponent(proximo.id);a.textContent='Pular capítulo → '+proximo.titulo;painel.append(a);}
  else {const p=document.createElement('p');p.textContent='Este é o último capítulo publicado.';painel.append(p);}
  destino.append(painel);return painel;
 }
};
