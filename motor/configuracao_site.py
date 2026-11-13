"""Configuração pública. Segredos de moderação e Turnstile nunca são exportados."""
import json
from urllib.parse import urlsplit

COMENTARIOS={'ativado':False,'api_url':'','turnstile_site_key':'','obras':True,'capitulos':True}
REACOES={'ativado':False,'obras':True,'capitulos':True}
PADRAO={'schema_version':2,'url_publica':'','comentarios':COMENTARIOS.copy(),'reacoes':REACOES.copy()}

def origem_https(url):
    if not isinstance(url,str):raise ValueError('Endereço inválido.')
    partes=urlsplit(url)
    if partes.scheme!='https' or not partes.hostname or partes.username or partes.password or partes.port or partes.path not in ('','/') or partes.query or partes.fragment:raise ValueError('Use a origem HTTPS, sem caminhos, portas ou parâmetros.')
    return url.rstrip('/')

def validar(config,cname=None):
    if not isinstance(config,dict) or config.get('schema_version') not in (1,2):raise ValueError('Versão da configuração do site inválida.')
    url=origem_https(config['url_publica']) if config.get('url_publica') else ''
    if cname and urlsplit(url).hostname!=cname.strip():raise ValueError('O endereço público precisa corresponder ao domínio do arquivo CNAME.')
    # Configurações antigas são migradas para o provedor próprio, desativado.
    c=config.get('comentarios',COMENTARIOS.copy())
    if not isinstance(c,dict) or any(type(c.get(k)) is not bool for k in ('ativado','obras','capitulos')):raise ValueError('Informe as opções de comentários como verdadeiro ou falso.')
    api=c.get('api_url','');key=c.get('turnstile_site_key','')
    if not isinstance(api,str) or not isinstance(key,str):raise ValueError('Endereço do serviço e chave pública devem ser textos.')
    if api:api=origem_https(api)
    if key and (not key.isascii() or not all(x.isalnum() or x in '_-' for x in key) or len(key)>100):raise ValueError('Chave pública do Turnstile inválida.')
    if c['ativado'] and (not api or not key):raise ValueError('Informe a URL do Worker e a chave pública do Turnstile antes de ativar.')
    r=config.get('reacoes',REACOES.copy())
    if not isinstance(r,dict) or any(type(r.get(k)) is not bool for k in REACOES):raise ValueError('Informe as opções de reações como verdadeiro ou falso.')
    if r['ativado'] and (not api or not key):raise ValueError('Informe a URL do Worker e a chave pública do Turnstile antes de ativar as reações.')
    return {'caminho_base':config.get('caminho_base',''),'reacoes':{k:r[k] for k in REACOES},'schema_version':2,'url_publica':url,'comentarios':{'ativado':c['ativado'],'api_url':api,'turnstile_site_key':key,'obras':c['obras'],'capitulos':c['capitulos']}}

def carregar(root):
    arquivo=root/'configuracao-site.json'
    config=json.loads(arquivo.read_text(encoding='utf-8')) if arquivo.exists() else json.loads(json.dumps(PADRAO))
    return validar(config,(root/'CNAME').read_text(encoding='utf-8').strip() if (root/'CNAME').exists() else None)

def validar_opcao(obj,contexto):
    for campo in ('comentarios','reacoes'):
        if obj.get(campo,'herdar') not in {'herdar','ativado','desativado'}:raise ValueError(contexto+': opção de '+campo+' inválida.')

def comentarios_permitidos(config,obra,capitulo=None):
    c=config['comentarios']
    if not c['ativado'] or obra.get('comentarios')=='desativado' or capitulo and capitulo.get('comentarios')=='desativado':return False
    return (capitulo or obra).get('comentarios')=='ativado' or c['capitulos' if capitulo else 'obras']

def reacoes_permitidas(config,obra,capitulo=None):
    r=config.get('reacoes',REACOES)
    if not r['ativado'] or obra.get('reacoes')=='desativado' or capitulo and capitulo.get('reacoes')=='desativado':return False
    return (capitulo or obra).get('reacoes')=='ativado' or r['capitulos' if capitulo else 'obras']

def manifesto_comentarios(config,resultados):
    from urllib.parse import urlencode
    threads=[]
    for slug,info,estrutura,_ in resultados:
        def incluir(cap=None):
            ident=json.dumps(['literature',info['id_obra'],cap['id'] if cap else None],ensure_ascii=False,separators=(',',':'))
            url=config['url_publica']+((cap or info).get('url_publica') or '/'+slug+('/leitor.html?'+urlencode({'id':cap['id']}) if cap else '/index-obra.html'))
            threads.append({'id':ident,'title':cap['titulo']+' — '+info['titulo'] if cap else info['titulo'],'url':url,'comments_enabled':comentarios_permitidos(config,info,cap),'reactions_enabled':reacoes_permitidas(config,info,cap)})
        if comentarios_permitidos(config,info) or reacoes_permitidas(config,info):incluir()
        for s in estrutura['sagas']:
            for v in s['volumes']:
                for a in v['arcos']:
                    for cap in a['capitulos']:
                        if comentarios_permitidos(config,info,cap) or reacoes_permitidas(config,info,cap):incluir(cap)
    return {'schema_version':1,'threads':threads}
