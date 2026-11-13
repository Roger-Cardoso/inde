/* Compatibilidade do histórico legado e proteção contra falhas de armazenamento. */
window.Unilit = {
    urlCapitulo(c) { return c.url_publica || 'leitor.html?id='+encodeURIComponent(c.id); },
    urlObra(o) { return o.url_publica || 'index-obra.html'; },
    ler(chave, padrao) {
        try { const valor = JSON.parse(localStorage.getItem(chave)); return valor ?? padrao; }
        catch (_) { return padrao; }
    },
    salvar(chave, valor) { try { localStorage.setItem(chave, JSON.stringify(valor)); } catch (_) {} },
    capitulos(estrutura) {
        return estrutura.sagas.flatMap(s => s.volumes.flatMap(v => v.arcos.flatMap(a => a.capitulos)));
    },
    lidos(obra, estrutura) {
        const caps = this.capitulos(estrutura);
        const chave = `unilit:v2:progresso:${obra}`;
        const atual = this.ler(chave, []);
        const antigo = this.ler(`progresso_${obra}`, []);
        const valores = [...(Array.isArray(atual) ? atual : []), ...(Array.isArray(antigo) ? antigo : [])];
        const ids = [...new Set(valores.map(id => caps.find(c => c.id === id || c.id_legado === id)?.id).filter(Boolean))];
        this.salvar(chave, ids);
        return ids;
    },
    revelar(item, obra, estrutura) {
        if (!item.disponivel_apos_capitulo) return true;
        return this.lidos(obra, estrutura).includes(item.disponivel_apos_capitulo);
    }
};
