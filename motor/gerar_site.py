"""Valida fontes editoriais e gera exclusivamente a saída pública em site/."""
from pathlib import Path
from html.parser import HTMLParser
from datetime import date
import argparse
import enderecos
import copy
import json
import shutil
import tempfile
import os
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from hierarquia import ordenados, validar as validar_hierarquia
from estado_publicacao import assinatura_fontes
from exportar_offline import exportar
from personagens import validar_participacoes
from configuracao_site import carregar as carregar_configuracao,validar_opcao,manifesto_comentarios
from contrato_semantico import validar_referencias

ROOT = Path(__file__).resolve().parents[1]
ESTADOS = {'rascunho', 'publicado', 'arquivado'}
class ErroMetadados(ValueError): pass
class Texto(HTMLParser):
    def __init__(self):
        super().__init__(); self.partes=[]; self.oculto=0
    def handle_starttag(self, tag, attrs):
        if tag in ('script','style'): self.oculto+=1
    def handle_endtag(self, tag):
        if tag in ('script','style'): self.oculto=max(0,self.oculto-1)
    def handle_data(self, data):
        if not self.oculto: self.partes.append(data)
def exigir(cond, msg):
    if not cond: raise ErroMetadados(msg)
def ler(p):
    try: return json.loads(p.read_text(encoding='utf-8'))
    except (OSError, ValueError) as e: raise ErroMetadados(f'{p}: {e}') from e
def gravar(p, obj):
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def texto(html):
    p=Texto();p.feed(html);return ' '.join(p.partes)
def data(valor, contexto):
    if valor is not None:
        try: exigir(date.fromisoformat(valor).isoformat()==valor,contexto+': data deve ser YYYY-MM-DD')
        except (ValueError,TypeError): raise ErroMetadados(contexto+': data deve ser YYYY-MM-DD')
def caminho(base, valor):
    exigir(isinstance(valor,str) and valor, 'Caminho de conteúdo ausente')
    p=(base/valor).resolve()
    exigir(p.is_relative_to(base.resolve()) and p.is_file(),f'Caminho inválido ou inexistente: {valor}')
    return p
def validar_publico(obj, contexto):
    serial=json.dumps(obj,ensure_ascii=False).lower()
    exigir(not any(x in serial for x in ('adicione a sinopse','exemplo de cena','descreva o que acontece')),contexto+': texto de exemplo em conteúdo publicado')
def preparar(root):
    config=carregar_configuracao(root)
    registro=ler(root/'obras.json'); exigir(registro.get('schema_version')==2,'Versão de obras.json inválida')
    vistos=set(); recursos_globais={}; resultados=[];enderecos_obras=[]
    def identidade(obj):
        ident=obj.get('id',obj.get('id_obra'))
        exigir(isinstance(ident,str) and ident and ident not in vistos,f'Identidade ausente ou duplicada: {ident}')
        vistos.add(ident)
    for pasta in registro['obras']:
        exigir(isinstance(pasta,str) and Path(pasta).name==pasta and not pasta.startswith('.'),f'Pasta inválida: {pasta}')
        base=root/pasta; info=ler(base/'dados/info-obra.json'); arvore=ler(base/'dados/editorial.json')
        exigir(info.get('schema_version')==arvore.get('schema_version')==2,f'{pasta}: versão inválida')
        identidade(info)
        validar_opcao(info,pasta)
        exigir(isinstance(info.get('sinopse'),str),f'{pasta}: sinopse deve ser texto')
        exigir(isinstance(info.get('capa'),dict) and all(isinstance(info['capa'].get(k),str) and info['capa'][k].strip() for k in ('src','alt')),f'{pasta}: capa exige src e alt')
        for campo in ('personagens','lore'):
            exigir(isinstance(info.get(campo,[]),list),f'{pasta}: {campo} deve ser lista')
        for campo in ('titulo','autor','idioma','slug'):
            exigir(isinstance(info.get(campo),str) and info[campo].strip(),f'{pasta}: {campo} obrigatório')
        exigir(info['slug']==pasta,f'{pasta}: slug deve corresponder à pasta nesta versão')
        info=enderecos.obra(info);enderecos_obras.append((pasta,info));arvore=enderecos.numerar(arvore)
        exigir(info.get('estado_publicacao') in ESTADOS,f'{pasta}: publicação inválida')
        exigir(info.get('estado_obra') in {'planejamento','em_andamento','pausada','concluida'},f'{pasta}: estado da obra inválido')
        for campo in ('tags','temas','generos','avisos_conteudo'):
            exigir(isinstance(info.get(campo,[]),list) and all(isinstance(x,str) for x in info.get(campo,[])),f'{pasta}: {campo} deve ser lista de textos')
        exigir(isinstance(info.get('ordem_catalogo',0),int),f'{pasta}: ordem do catálogo inválida')
        try:validar_hierarquia(arvore);validar_participacoes(info,arvore)
        except ValueError as e:raise ErroMetadados(f'{pasta}: {e}') from e
        fonte=copy.deepcopy(arvore); caps=[]; arquivos=[]; legados=set()
        if 'capitulos' in fonte:
            exigir(not fonte.get('sagas'),f'{pasta}: use capítulos simples ou sagas, não ambos')
            fonte['sagas']=[{'id':info['id_obra']+'-grupo-simples','nome':'Obra','ordem':1,'volumes':[{'id':info['id_obra']+'-volume-simples','nome':'Leitura','ordem':1,'arcos':[{'id':info['id_obra']+'-arco-simples','nome':'Capítulos','ordem':1,'capitulos':fonte.pop('capitulos')}]}]}]
        def grupos(itens,tipo):
            exigir(isinstance(itens,list),f'{pasta}: {tipo} deve ser lista')
            ordens=set()
            for item in itens:
                identidade(item); exigir(isinstance(item.get('ordem'),int) and item['ordem'] not in ordens,f'{pasta}: ordem ausente/duplicada em {tipo}');ordens.add(item['ordem'])
                exigir(isinstance(item.get('nome',item.get('titulo')),str),f'{pasta}: título ausente em {tipo}')
            return ordenados(itens)
        fonte['sagas']=grupos(fonte.get('sagas',[]),'sagas')
        for s in fonte['sagas']:
            s['volumes']=grupos(s['volumes'],'volumes')
            for v in s['volumes']:
                v['arcos']=grupos(v['arcos'],'arcos')
                for a in v['arcos']:
                    a['capitulos']=grupos(a['capitulos'],'capítulos')
                    publicos=[]
                    for c in a['capitulos']:
                        exigir(c.get('estado_publicacao') in ESTADOS,f"{c['id']}: estado inválido")
                        validar_opcao(c,c['id'])
                        data(c.get('publicado_em'),c['id']);data(c.get('atualizado_em'),c['id'])
                        if c.get('atualizado_em') and c.get('publicado_em'): exigir(c['atualizado_em']>=c['publicado_em'],c['id']+': atualização anterior à publicação')
                        if 'id_legado' in c:
                            exigir(isinstance(c['id_legado'],int) and c['id_legado'] not in legados,'ID legado duplicado/inválido');legados.add(c['id_legado'])
                        p=caminho(base,c['arquivo']); conteudo=p.read_text(encoding='utf-8')
                        if c['estado_publicacao']=='publicado':
                            exigir(texto(conteudo).strip(),c['id']+': capítulo publicado vazio')
                            exigir(c.get('publicado_em'),c['id']+': data de publicação obrigatória')
                            exigir(c['publicado_em']<=date.today().isoformat(),c['id']+': publicação futura não suportada')
                            validar_publico({k:v for k,v in c.items() if k!='cenas'},c['id'])
                            c['palavras']=len(texto(conteudo).split());c['data_publicacao']=c['publicado_em']
                            publicos.append(c);caps.append(c);arquivos.append((c['arquivo'],p))
                    a['capitulos']=publicos
                v['arcos']=[a for a in v['arcos'] if a['capitulos']]
            s['volumes']=[v for v in s['volumes'] if v['arcos']]
        fonte['sagas']=[s for s in fonte['sagas'] if s['volumes']]
        capids={c['id'] for c in caps}
        personagens_ids={p['id'] for p in info.get('personagens',[])}
        verbetes_ids={p['id'] for p in info.get('lore',[])}
        for _,arquivo in arquivos:
            try:validar_referencias(arquivo.read_text(encoding='utf-8'),personagens_ids,verbetes_ids,{p['id']:p.get('tipo_semantico','character') for p in info.get('personagens',[])})
            except ValueError as e:raise ErroMetadados(f'{pasta}: {e}') from e
        recursos_locais=set()
        for campo in ('personagens','lore'):
            for item in info.get(campo,[]):
                ident=item.get('id')
                exigir(isinstance(ident,str) and ident and ident not in recursos_locais, 'Identidade de recurso ausente ou duplicada nesta obra')
                recursos_locais.add(ident)
                if ident in recursos_globais:
                    exigir(recursos_globais[ident]==(campo,item), 'O mesmo recurso tem definições públicas incompatíveis entre obras: '+ident)
                else:
                    identidade(item);recursos_globais[ident]=(campo,copy.deepcopy(item))
                marco=item.get('disponivel_apos_capitulo')
                exigir(marco is None or marco in capids,f'{pasta}: marco de spoiler não publicado/inexistente: {marco}')
        for c in caps:
            for ref in c.get('verbetes_ids',[]): exigir(ref in {x['id'] for x in info.get('lore',[])},f"{c['id']}: verbete inexistente: {ref}")
            for nota in c.get('notas',[]):
                identidade(nota)
                exigir(all(isinstance(nota.get(k),str) and nota[k].strip() for k in ('titulo','texto')),f"{c['id']}: nota sem título/texto")
                exigir(nota.get('disponivel_apos_capitulo') is None or nota['disponivel_apos_capitulo'] in capids,f"{c['id']}: marco de nota inválido")
            for ref in c.get('personagens_ids',[]): exigir(ref in {x['id'] for x in info.get('personagens',[])},f"{c['id']}: personagem inexistente: {ref}")
            for cena in c.get('cenas',[]):
                identidade(cena);exigir(cena.get('visibilidade') in {'publica','privada'},'Cena sem visibilidade explícita')
            c['cenas']=[x for x in c.get('cenas',[]) if x['visibilidade']=='publica']
            validar_publico(c,c['id'])
            class Ancoras(HTMLParser):
                def __init__(self): super().__init__();self.ids=[]
                def handle_starttag(self,tag,attrs):
                    self.ids.extend(v for k,v in attrs if k=='id')
            ancoras=Ancoras();ancoras.feed(dict(arquivos)[c['arquivo']].read_text(encoding='utf-8'))
            anteriores=[]
            for cena in c['cenas']:
                exigir(cena.get('relevancia') in {'essencial','complementar','opcional','alta','media','baixa','nao_avaliada'},'Relevância de cena pública inválida')
                exigir(isinstance(cena.get('titulo_cena'),str) and cena['titulo_cena'].strip(),'Cena sem identificação pública')
                exigir(cena.get('avaliacao_conteudo','pendente') in {'pendente','revisada'},'Estado da avaliação de conteúdo inválido')
                ancora=cena.get('ancora')
                exigir(isinstance(ancora,str) and ancora!='fim-do-capitulo' and ancoras.ids.count(ancora)==1,f"{c['id']}: âncora de cena ausente ou duplicada: {ancora}")
                exigir(ancora not in anteriores,f"{c['id']}: duas cenas usam a mesma âncora")
                if anteriores: exigir(ancoras.ids.index(ancora)>ancoras.ids.index(anteriores[-1]),f"{c['id']}: cenas fora da ordem do texto")
                anteriores.append(ancora)
                exigir(isinstance(cena.get('avisos_conteudo',[]),list) and all(isinstance(x,str) and x.strip() for x in cena.get('avisos_conteudo',[])),'Avisos da cena devem ser lista de textos')
                for campo in ('tratamento','descricao','consequencia_pular','resumo_alternativo'):
                    exigir(campo not in cena or isinstance(cena[campo],str),f'Campo de cena deve ser texto: {campo}')
        fonte.update(total_capitulos=len(caps),total_palavras=sum(c['palavras'] for c in caps))
        if info['estado_publicacao']=='publicado':
            validar_publico(info,pasta);exigir(caps,f'{pasta}: obra publicada sem capítulos')
            info=copy.deepcopy(info)
            info['capa_url']=info['capa']['src']
            info['status']={'planejamento':'Em Planejamento','em_andamento':'Em Lançamento','pausada':'Pausada','concluida':'Concluída'}[info['estado_obra']]
            info.update(total_capitulos=fonte['total_capitulos'],total_palavras=fonte['total_palavras'])
            info,fonte=enderecos.publicar(info,fonte,config.get("caminho_base",""))
            fonte.pop('proximo_numero_url',None)  # Contador editorial, sem utilidade pública.
            resultados.append((pasta,info,fonte,arquivos))
    enderecos.validar_catalogo(enderecos_obras)
    return resultados

def gerar(root=ROOT, destino=None):
    assinatura=assinatura_fontes(root)
    config=carregar_configuracao(root)
    resultados=preparar(root)  # nenhuma escrita antes da validação completa
    destino=Path(destino) if destino else root/'site'
    exigir(destino.resolve()==(root/'site').resolve(),'A saída deve ser a pasta site/ do projeto')
    with tempfile.TemporaryDirectory(dir=root,prefix='.unilit-build-') as tmp:
        out=Path(tmp)/'site';out.mkdir()
        for nome in ('home','css-global','js-global'):
            shutil.copytree(root/nome,out/nome)
        if (root/'assets').is_dir(): shutil.copytree(root/'assets',out/'assets')
        for nome in ('index.html','index-app.html','404.html','privacidade.html','CNAME','.nojekyll'):
            if (root/nome).is_file():shutil.copy2(root/nome,out/nome)
        gravar(out/'configuracao-site.json',config)
        gravar(out/'comentarios-manifesto.json',manifesto_comentarios(config,resultados))
        catalogo=[];busca=[]
        for pasta,info,estrutura,arquivos in resultados:
            for pagina in ('index-obra.html','leitor.html'):
                caminho(root/pasta,pagina)
            (out/pasta).mkdir(exist_ok=True)
            fonte_obra=(root/pasta/'index-obra.html').read_text()
            fonte_leitor=(root/pasta/'leitor.html').read_text()
            # O acesso legado do leitor resolve o ID antigo antes de encaminhar à rota atual.
            (out/pasta/'leitor.html').write_text(fonte_leitor,encoding='utf-8')
            (out/pasta/'index-obra.html').write_text(enderecos.redirecionar(info['url_publica']),encoding='utf-8')
            destino_obra=out/info['endereco_publico'];destino_obra.mkdir(exist_ok=True)
            (destino_obra/'index.html').write_text(enderecos.pagina(fonte_obra,pasta,info['url_publica'],config['url_publica']+config.get('caminho_base','')),encoding='utf-8')
            if pasta!=info['endereco_publico']:(out/pasta/'index.html').write_text(enderecos.redirecionar(info['url_publica']),encoding='utf-8')
            for alias in info.get('enderecos_anteriores',[]):
                destino_alias=out/alias;destino_alias.mkdir(exist_ok=True);(destino_alias/'index.html').write_text(enderecos.redirecionar(info['url_publica']),encoding='utf-8')
            for s in estrutura['sagas']:
                for v in s['volumes']:
                    for a in v['arcos']:
                        for cap in a['capitulos']:
                            destino_cap=out/cap['url_publica'][len(config.get('caminho_base','')):].strip('/');destino_cap.mkdir(parents=True,exist_ok=True)
                            (destino_cap/'index.html').write_text(enderecos.pagina(fonte_leitor,pasta,cap['url_publica'],config['url_publica']+config.get('caminho_base',''),cap),encoding='utf-8')
                            for alias in info.get('enderecos_anteriores',[]):
                                legado=out/alias/('capitulo-'+str(cap['numero_url']));legado.mkdir(parents=True,exist_ok=True);(legado/'index.html').write_text(enderecos.redirecionar(cap['url_publica']),encoding='utf-8')
            gravar(out/pasta/'dados/info-obra.json',info);gravar(out/pasta/'dados/estrutura.json',estrutura)
            recursos_publicados=set()
            for relativo,p in arquivos:
                alvo=out/pasta/relativo;exigir(not alvo.exists() or relativo in recursos_publicados,'Recurso sobrepõe uma página pública: '+relativo);recursos_publicados.add(relativo);alvo.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,alvo)
            (out/pasta/'offline.html').write_text(exportar(info,estrutura,arquivos,root),encoding='utf-8')
            item={k:info[k] for k in ('id_obra','slug','titulo','autor','sinopse','capa_url','status','formato','tags','temas','generos','total_capitulos','total_palavras','ordem_catalogo','url_publica','endereco_publico') if k in info}
            item['ultima_publicacao']=max(c['publicado_em'] for s in estrutura['sagas'] for v in s['volumes'] for a in v['arcos'] for c in a['capitulos'])
            catalogo.append(item)
            busca.append({'tipo':'obra','titulo':info['titulo'],'texto':' '.join([info['sinopse'],*info.get('tags',[]),*info.get('temas',[]),*info.get('generos',[])]),'url':info['url_publica']})
            for s in estrutura['sagas']:
                for v in s['volumes']:
                    for a in v['arcos']:
                        for c in a['capitulos']:busca.append({'tipo':'capitulo','titulo':c['titulo'],'texto':c.get('sinopse',''),'url':c['url_publica']})
        gravar(out/'catalogo.json',sorted(catalogo,key=lambda x:(x['ordem_catalogo'],x['id_obra'])))
        gravar(out/'busca.json',{'schema_version':2,'itens':busca})
        exigir(assinatura_fontes(root)==assinatura,'As fontes mudaram durante a geração. Gere novamente.')
        gravar(out/'geracao.json',{'schema_version':1,'fontes':assinatura})
        antigo=root/'.site-anterior'
        exigir(not antigo.exists(),'Remova ou recupere .site-anterior antes de gerar')
        if destino.exists():os.replace(destino,antigo)
        try:os.replace(out,destino)
        except Exception:
            if antigo.exists():os.replace(antigo,destino)
            raise
        if antigo.exists():shutil.rmtree(antigo)
    return resultados
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--validar',action='store_true');args=parser.parse_args()
    try:
        resultado=preparar(ROOT) if args.validar else gerar()
        print(f"{'Validação concluída' if args.validar else 'Site gerado em site/'}: {len(resultado)} obra(s) pública(s).")
    except (ErroMetadados,KeyError,TypeError) as e:parser.exit(1,f'Erro de metadados: {e}\n')
