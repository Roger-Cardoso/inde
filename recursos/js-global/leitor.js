/* Literature — sessão de leitura local, identificada por obra e capítulo. */
const $ = id => document.getElementById(id);
const area = $('scroll-area');
let obra, estrutura, atual, capitulos = [], pronto = false, timer;
let acionadorPreferencias = $('abrir-preferencias');
const normalizar = t => String(t).normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
const limitar = (v, min, max, padrao) => Number.isFinite(Number(v)) ? Math.min(max, Math.max(min, Number(v))) : padrao;
const padroes = {tema:'dark', fonte:'literaria', tamanho:19, largura:700, entrelinha:1.95, paragrafo:1.5, recuo:false, ampliada:false, foco:false};
let preferencias = {...padroes, ...Unilit.ler('unilit:v2:aparencia', {})};
function aplicar() {
    preferencias.tamanho = limitar(preferencias.tamanho,14,32,19);
    preferencias.largura = limitar(preferencias.largura,520,940,700);
    preferencias.entrelinha = limitar(preferencias.entrelinha,1.4,2.4,1.95);
    preferencias.paragrafo = limitar(preferencias.paragrafo,.6,2.4,1.5);
    if (!['dark','light','sepia','contrast'].includes(preferencias.tema)) preferencias.tema='dark';
    if (!['literaria','classica','moderna'].includes(preferencias.fonte)) preferencias.fonte='literaria';
    document.body.classList.remove('light','sepia','contrast');
    if(preferencias.tema!=='dark') document.body.classList.add(preferencias.tema);
    document.body.classList.toggle('leitura-ampliada',!!preferencias.ampliada);
    document.body.classList.toggle('leitura-foco',!!preferencias.foco);
    const estilo = $('content-area').style;
    estilo.fontSize = preferencias.tamanho+'px';
    estilo.lineHeight = preferencias.entrelinha;
    estilo.fontFamily = {literaria:'Georgia, serif',classica:'Palatino, Georgia, serif',moderna:'Verdana, Arial, sans-serif'}[preferencias.fonte];
    document.documentElement.style.setProperty('--coluna',preferencias.largura+'px');
    document.documentElement.style.setProperty('--paragrafo',preferencias.paragrafo+'em');
    document.documentElement.style.setProperty('--recuo',preferencias.recuo?'1.5em':'0');
    for(const key of Object.keys(padroes)) {
        const el=$('pref-'+key); if(!el) continue;
        if(el.type==='checkbox') el.checked=!!preferencias[key]; else el.value=preferencias[key];
    }
    $('valor-tamanho').textContent=preferencias.tamanho+' px';
    $('sair-foco').hidden=!preferencias.foco;
    area.dispatchEvent(new Event('literature:layout'));
}
function salvarPosicao() {
    if(!pronto) return;
    const max=Math.max(0,area.scrollHeight-area.clientHeight);
    const fracao=max ? area.scrollTop/max : 0;
    const paragrafos=[...$('content-area').querySelectorAll('p')];
    const topo=area.getBoundingClientRect().top;
    const indice=paragrafos.findIndex(p=>p.getBoundingClientRect().bottom>topo+20);
    const p=paragrafos[indice];
    const pos={capitulo:atual.id,fracao,paragrafo:indice,trecho:p?.textContent.trim().slice(0,120)||'',deslocamento:p?topo-p.getBoundingClientRect().top:0};
    Unilit.salvar(`unilit:v2:posicao:${obra.id_obra}:${atual.id}`,pos);
    Unilit.salvar(`unilit:v2:retomar:${obra.id_obra}`,{capitulo:atual.id});
}
function restaurar() {
    if(!obra || !atual) return;
    const pos=Unilit.ler(`unilit:v2:posicao:${obra.id_obra}:${atual.id}`,null);
    if(!pos) return;
    if(pos.fracao===0){area.scrollTop=0;return;}
    const ps=[...$('content-area').querySelectorAll('p')];
    let p=ps[pos.paragrafo];
    if(!p || p.textContent.trim().slice(0,120)!==pos.trecho) p=ps.find(x=>x.textContent.trim().slice(0,120)===pos.trecho);
    if(p && pos.trecho) area.scrollTop+=p.getBoundingClientRect().top-area.getBoundingClientRect().top+limitar(pos.deslocamento,-area.clientHeight,p.offsetHeight,0);
    else area.scrollTop=limitar(pos.fracao,0,1,0)*Math.max(0,area.scrollHeight-area.clientHeight);
}
function painel(id, abrir, devolver=true) {
    const el=$(id), trigger=(id==='sumario'?$('abrir-sumario'):acionadorPreferencias);
    el.hidden=!abrir;trigger.setAttribute('aria-expanded',String(abrir));
    if(id==='settings-menu')for(const botao of [$('abrir-preferencias'),$('abrir-auxilios')])botao.setAttribute('aria-expanded',String(abrir));
    const fechado=$('sumario').hidden&&$('settings-menu').hidden;
    $('painel-fundo').hidden=fechado;
    for(const alvo of document.querySelectorAll('header.top-bar, main, footer.bottom-nav, #abrir-auxilios')) alvo.inert=!fechado;
    if(abrir) { el.querySelector('input,select,button,a')?.focus(); }
    else if(devolver) trigger.focus();
}
function fechar() { painel('sumario',false,false);painel('settings-menu',false,false); }
function sumario() {
    const container=$('sumario-lista');container.replaceChildren();
    const termo=normalizar($('busca-sumario').value);
    const lidos=Unilit.lidos(obra.id_obra,estrutura);
    for(const s of estrutura.sagas) for(const v of s.volumes) for(const a of v.arcos) {
        const lista=a.capitulos.filter(c=>normalizar([s.nome,v.nome,a.nome,c.titulo].join(' ')).includes(termo));
        if(!lista.length)continue;
        const grupo=document.createElement('section'),h=document.createElement('h3');
        h.textContent=[s.nome,v.nome,a.nome].join(' / ');grupo.append(h);
        for(const c of lista) {
            const link=document.createElement('a');link.href=c.url_publica||'?id='+encodeURIComponent(c.id);
            link.textContent=`${c.numero||c.ordem} · ${c.titulo}${lidos.includes(c.id)?' · Concluído':''}`;
            if(c.id===atual.id)link.setAttribute('aria-current','page');
            grupo.append(link);
        }container.append(grupo);
    }
    if(!container.childElementCount)container.textContent='Nenhum capítulo encontrado.';
}
function irCena(ancora) {
    const alvo=ancora==='fim-do-capitulo'?document.querySelector('.navegacao-final'):[...$('content-area').querySelectorAll('[id]')].find(el=>el.id===ancora);
    if(!alvo)return false;
    area.scrollTop+=alvo.getBoundingClientRect().top-area.getBoundingClientRect().top-20;
    alvo.setAttribute('tabindex','-1');alvo.focus({preventScroll:true});
    $('aviso-navegacao').hidden=false;
    $('aviso-navegacao').textContent='Posição alterada pelo guia de cenas. O capítulo não foi marcado como concluído.';
    salvarPosicao();return true;
}
function navegacao(indice) {
    for(const [id,offset,nome] of [['btn-prev',-1,'Anterior'],['btn-next',1,'Próximo'],['fim-prev',-1,'Anterior'],['fim-next',1,'Próximo']]) {
        const el=$(id),dest=capitulos[indice+offset];el.replaceChildren();
        if(dest) {el.href=dest.url_publica||'?id='+encodeURIComponent(dest.id);el.classList.remove('disabled');el.removeAttribute('aria-disabled');}
        else {el.removeAttribute('href');el.classList.add('disabled');el.setAttribute('aria-disabled','true');}
        const label=document.createElement('span');label.textContent=(offset<0?'← ':'')+nome+(offset>0?' →':'');el.append(label);
        if(id.startsWith('fim-')) {const titulo=document.createElement('strong');titulo.textContent=dest?dest.titulo:offset<0?'Início da obra':'Fim dos capítulos publicados';el.append(titulo);}
    }
}
function conclusao() {
    const lidos=Unilit.lidos(obra.id_obra,estrutura);
    $('btn-concluir').textContent=lidos.includes(atual.id)?'Marcar como não lido':'Concluir capítulo';
    $('btn-concluir').setAttribute('aria-pressed',String(lidos.includes(atual.id)));
    sumario();
}
async function iniciar() {
    aplicar();
    const destinoSolicitado=new URLSearchParams(location.search).get('cena');
    if(destinoSolicitado)$('content-area').style.visibility='hidden';
    try {
        const respostas=await Promise.all([fetch('dados/info-obra.json',{cache:'no-store'}),fetch('dados/estrutura.json',{cache:'no-store'})]);
        if(respostas.some(r=>!r.ok))throw Error('Não foi possível abrir os dados da obra.');
        [obra,estrutura]=await Promise.all(respostas.map(r=>r.json()));
        capitulos=Unilit.capitulos(estrutura);
        for(const link of document.querySelectorAll('[data-link-obra]'))link.href=obra.url_publica||'index-obra.html';
        const id=document.querySelector?.('meta[name="literature-capitulo"]')?.content||new URLSearchParams(location.search).get('id');
        const retomar=Unilit.ler(`unilit:v2:retomar:${obra.id_obra}`,{});
        atual=id===null?(capitulos.find(c=>c.id===retomar.capitulo)||capitulos[0]):capitulos.find(c=>c.id===id||String(c.id_legado)===id);
        if(!atual)throw Error('Capítulo não encontrado. Volte ao índice para escolher outra leitura.');
        if(atual.url_publica&&location.pathname!==atual.url_publica){const parametros=new URLSearchParams(location.search);parametros.delete('id');const extra=parametros.toString();location.replace(atual.url_publica+(extra?'?'+extra:'')+location.hash);return;}
        const resposta=await fetch(atual.arquivo);if(!resposta.ok)throw Error('Não foi possível abrir este capítulo.');
        $('content-area').innerHTML=await resposta.text();
        ContextoLiterature.montar({obra,estrutura,capitulo:atual,raiz:$('content-area'),destino:$('contexto-capitulo')});
        SemanticaLiterature.montar({obra,estrutura,raiz:$('content-area'),area,destino:$('auxilios-leitura')});
        document.title=`${atual.titulo} — ${obra.titulo} — ${window.INDE_SITE.nome}`;
        $('top-title').textContent=atual.titulo;$('main-title').textContent=atual.titulo;
        $('sumario-obra').textContent=obra.titulo;
        $('tempo-leitura').textContent=`${atual.palavras.toLocaleString('pt-BR')} palavras · ${Math.max(1,Math.ceil(atual.palavras/225))} min de leitura`;
        navegacao(capitulos.indexOf(atual));conclusao();
        window.ReacoesLiterature?.montar({obra,capitulo:atual,destino:$('reacoes')});
        window.ComentariosLiterature?.montar({obra,capitulo:atual,destino:$('comentarios')});
        const guia=GuiaCenas.montar({capitulo:atual,destino:$('guia-leitura'),raiz:$('content-area'),proximo:capitulos[capitulos.indexOf(atual)+1],ir:irCena});
        if(guia){
            $('voltar-guia').hidden=false;
            $('voltar-guia').onclick=()=>{
                guia.open=true;
                area.scrollTop+=guia.getBoundingClientRect().top-area.getBoundingClientRect().top-20;
                guia.querySelector('summary').focus({preventScroll:true});
            };
        }
        $('btn-concluir').disabled=false;
        $('btn-concluir').onclick=()=>{
            const ids=Unilit.lidos(obra.id_obra,estrutura);
            Unilit.salvar(`unilit:v2:progresso:${obra.id_obra}`,ids.includes(atual.id)?ids.filter(id=>id!==atual.id):[...ids,atual.id]);
            Unilit.salvar(`progresso_${obra.id_obra}`,[]);conclusao();
        };
        await document.fonts.ready;
        requestAnimationFrame(()=>{
            const cena=new URLSearchParams(location.search).get('cena');
            pronto=true;
            if(!cena)restaurar();
            else if(!irCena(cena)){
                $('guia-leitura').querySelector('details')?.setAttribute('open','');
                area.scrollTop=0;
                $('aviso-navegacao').hidden=false;
                $('aviso-navegacao').textContent='O trecho solicitado não está disponível. Consulte o guia antes de continuar.';
            }
            $('content-area').style.visibility='';
            salvarPosicao();
        });
    } catch(e) { $('content-area').style.visibility=''; $('main-title').textContent='Não foi possível abrir a leitura';$('content-area').textContent=e.message; }
}
$('abrir-sumario').onclick=()=>{const abrir=$('sumario').hidden;fechar();painel('sumario',abrir);};
$('abrir-preferencias').onclick=()=>{const abrir=$('settings-menu').hidden;fechar();acionadorPreferencias=$('abrir-preferencias');painel('settings-menu',abrir);};
$('abrir-auxilios').onclick=()=>{
    fechar();acionadorPreferencias=$('abrir-auxilios');painel('settings-menu',true);
    const auxilios=$('auxilios-leitura').querySelector('details');
    if(auxilios){auxilios.open=true;auxilios.querySelector('summary').focus({preventScroll:true});}
    $('settings-menu').scrollTop=0;
};
$('fechar-sumario').onclick=()=>painel('sumario',false);
$('fechar-preferencias').onclick=()=>painel('settings-menu',false);
$('painel-fundo').onclick=()=>{const id=!$('sumario').hidden?'sumario':'settings-menu';painel(id,false);};
$('busca-sumario').oninput=()=>{if(estrutura)sumario();};
for(const key of Object.keys(padroes)) {
    const el=$('pref-'+key);if(!el)continue;
    el.oninput=()=>{salvarPosicao();preferencias[key]=el.type==='checkbox'?el.checked:el.value;aplicar();restaurar();Unilit.salvar('unilit:v2:aparencia',preferencias);};
}
$('restaurar-preferencias').onclick=()=>{salvarPosicao();preferencias={...padroes};aplicar();restaurar();Unilit.salvar('unilit:v2:aparencia',preferencias);};
$('sair-foco').onclick=()=>{preferencias.foco=false;aplicar();Unilit.salvar('unilit:v2:aparencia',preferencias);};
document.addEventListener('keydown',e=>{
    if(document.querySelector('dialog[open]'))return;
    const aberto=!$('sumario').hidden?$('sumario'):!$('settings-menu').hidden?$('settings-menu'):null;
    if(e.key==='Tab' && aberto) {
        const alvos=[...aberto.querySelectorAll('button,input,select,a[href],summary')].filter(el=>el.getClientRects().length && !el.disabled);
        const primeiro=alvos[0],ultimo=alvos[alvos.length-1];
        if(e.shiftKey && document.activeElement===primeiro){e.preventDefault();ultimo.focus();}
        else if(!e.shiftKey && document.activeElement===ultimo){e.preventDefault();primeiro.focus();}
    }
    if(e.key==='Escape') {const aberto=!$('sumario').hidden?'sumario':!$('settings-menu').hidden?'settings-menu':null;if(aberto)painel(aberto,false);}
    if((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k'){e.preventDefault();fechar();painel('sumario',true);$('busca-sumario').focus();}
});
area.addEventListener('scroll',()=>{
    const max=area.scrollHeight-area.clientHeight;
    $('progress-bar').style.width=(max>0?area.scrollTop/max*100:0)+'%';
    clearTimeout(timer);timer=setTimeout(salvarPosicao,180);
},{passive:true});
area.addEventListener('literature:auxilios',salvarPosicao);
window.addEventListener('resize',()=>area.dispatchEvent(new Event('literature:layout')));
window.addEventListener('pagehide',salvarPosicao);
document.addEventListener('visibilitychange',()=>{if(document.hidden)salvarPosicao();});
iniciar();
