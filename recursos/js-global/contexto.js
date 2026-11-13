/* Conteúdo contextual exclusivamente editorial. Não infere definições ou relações. */
window.ContextoLiterature = {
    montar({obra, estrutura, capitulo, raiz, destino}) {
        const visivel = item => Unilit.revelar(item, obra.id_obra, estrutura);
        const verbetes = (obra.lore || []).filter(visivel);
        const mapa = new Map(verbetes.map(v => [v.id, v]));
        const dialogo = document.createElement('dialog');
        dialogo.className = 'glossario-dialogo';
        const titulo = document.createElement('h2');titulo.id='glossario-titulo';
        dialogo.setAttribute('aria-labelledby',titulo.id);
        const descricao = document.createElement('p');
        const link = document.createElement('a');link.textContent='Ver no universo da obra ↗';
        const fechar=document.createElement('button');fechar.textContent='Fechar';fechar.type='button';
        fechar.addEventListener('click',()=>dialogo.close());
        dialogo.append(titulo,descricao,link,fechar);destino.append(dialogo);
        const nativos = new Map();
        try {
            const registro=JSON.parse(raiz.querySelector('script#story-glossary[type="application/json"]')?.textContent||'{}');
            for(const [termo,definicao] of Object.entries(registro))if(typeof definicao==='string')nativos.set(termo,definicao);
        }catch(_){}
        for(const termo of raiz.querySelectorAll('[data-glossary-term]')){
            const nome=termo.dataset.glossaryTerm;if(!nativos.has(nome))continue;
            const botao=document.createElement('button');botao.type='button';botao.className='termo-glossario';botao.textContent=termo.textContent;botao.setAttribute('aria-haspopup','dialog');
            botao.onclick=()=>{titulo.textContent=nome;descricao.textContent=nativos.get(nome);link.hidden=true;dialogo.showModal();};termo.replaceWith(botao);
        }
        const abrir = id => {
            const verbete=mapa.get(id);if(!verbete)return;
            titulo.textContent=verbete.titulo;
            descricao.textContent=verbete.descricao;
            link.hidden=!obra.slug;
            if(obra.slug)link.href=(obra.url_publica||'index-obra.html')+'?aba=universo#verbete-'+encodeURIComponent(verbete.id);
            dialogo.showModal();
        };
        for(const termo of raiz.querySelectorAll('[data-verbete-id]')) {
            const id=termo.dataset.verbeteId;
            if(!mapa.has(id))continue; // Mantém o texto sem revelar nomes ou definições bloqueadas.
            const botao=document.createElement('button');botao.type='button';
            botao.className='termo-glossario';botao.textContent=termo.textContent;
            botao.setAttribute('aria-haspopup','dialog');botao.setAttribute('aria-label','Consultar: '+termo.textContent);
            botao.addEventListener('click',()=>abrir(id));termo.replaceWith(botao);
        }
        const painel=document.createElement('details');painel.className='contexto-capitulo';
        const resumo=document.createElement('summary');resumo.textContent='Contexto e notas da leitura';painel.append(resumo);
        const avisos=capitulo.avisos_conteudo||[];
        if(avisos.length){const p=document.createElement('p');p.textContent='Avisos de conteúdo: '+avisos.join(' · ');painel.append(p);}
        for(const nota of (capitulo.notas||[]).filter(visivel)) {
            const artigo=document.createElement('section'),h=document.createElement('h3'),p=document.createElement('p');
            h.textContent=nota.titulo;p.textContent=nota.texto;artigo.append(h,p);painel.append(artigo);
        }
        const permitidos=capitulo.verbetes_ids ? verbetes.filter(v=>capitulo.verbetes_ids.includes(v.id)) : [];
        if(permitidos.length) {
            const h=document.createElement('h3');h.textContent='Neste universo';painel.append(h);
            for(const v of permitidos){const b=document.createElement('button');b.type='button';b.className='contexto-verbete';b.textContent=v.titulo;b.onclick=()=>abrir(v.id);painel.append(b);}
        }
        if(painel.childElementCount>1)destino.append(painel);
        return {dialogo, painel, abrir};
    }
};
