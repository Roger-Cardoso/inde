"""Endereços públicos independentes das pastas e identidades editoriais."""
import copy
import html
import json
import re
import unicodedata
from urllib.parse import urlsplit
from hierarquia import nos

RESERVADOS={'home','assets','css-global','js-global','site','publicado','scripts','tests','backend','exports','autoria'}
PADRAO=re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*\Z')
def sugerir(titulo):
 texto=unicodedata.normalize('NFKD',str(titulo)).encode('ascii','ignore').decode().lower()
 return re.sub(r'[^a-z0-9]+','-',texto).strip('-')[:100].rstrip('-') or 'obra'
def validar_slug(valor):
 if not isinstance(valor,str) or not PADRAO.fullmatch(valor) or len(valor)>100 or valor in RESERVADOS:raise ValueError('Endereço público inválido: use até 100 letras minúsculas sem acentos, números e hífens; não use nomes reservados.')
 return valor
def obra(info,anterior=None):
 novo=copy.deepcopy(info);antigo=anterior or {}
 atual=validar_slug(novo.get('endereco_publico') or antigo.get('endereco_publico') or sugerir(antigo.get('titulo',novo.get('titulo','obra'))))
 if any(not isinstance(x.get('enderecos_anteriores',[]),list) for x in (novo,antigo)):raise ValueError('Os endereços anteriores devem ser uma lista.')
 aliases=list(antigo.get('enderecos_anteriores',[]))
 for alias in novo.get('enderecos_anteriores',[]):
  if alias not in aliases:aliases.append(alias)
 antes=antigo.get('endereco_publico') or (sugerir(antigo['titulo']) if antigo.get('titulo') else None)
 if antes and antes!=atual and antes not in aliases:aliases.append(antes)
 novo['endereco_publico']=atual;novo['enderecos_anteriores']=[validar_slug(a) for a in aliases if a!=atual]
 return novo
def numerar(arvore,anterior=None):
 novo=copy.deepcopy(arvore);antigo=copy.deepcopy(anterior) if anterior else None
 if antigo:
  antigo=numerar(antigo);anteriores={n.id:n.item['numero_url'] for n in nos(antigo) if n.tipo=='capitulo'}
 else:anteriores={}
 caps=[n.item for n in nos(novo) if n.tipo=='capitulo'];usados=set()
 for c in caps:
  if c['id'] in anteriores:
   if 'numero_url' in c and c['numero_url']!=anteriores[c['id']]:raise ValueError('O número do endereço é permanente e não pode ser transferido para outro capítulo.')
   c['numero_url']=anteriores[c['id']]
  if 'numero_url' in c:
   n=c['numero_url']
   if antigo and c['id'] not in anteriores and type(n) is int and n<antigo['proximo_numero_url']:raise ValueError('Esse número de endereço já foi reservado. Use o próximo número disponível.')
   if type(n) is not int or not 1<=n<=999999 or n in usados:raise ValueError('Cada capítulo precisa de um número de endereço inteiro, positivo e único na obra.')
   usados.add(n)
 contador=novo.get('proximo_numero_url',1)
 if type(contador) is not int or not 1<=contador<=1000000:raise ValueError('Contador de endereços inválido.')
 contador=max(contador,(antigo or {}).get('proximo_numero_url',1),max(usados,default=0)+1)
 for c in caps:
  if 'numero_url' not in c:
   if contador>999999:raise ValueError('Limite de endereços de capítulos atingido.')
   c['numero_url']=contador;usados.add(contador);contador+=1
 novo['proximo_numero_url']=contador
 return novo

def publicar(info,estrutura,base_path=""):
 info=obra(info);estrutura=numerar(estrutura)
 info['url_publica']=base_path+'/'+info['endereco_publico']+'/'
 for n in nos(estrutura):
  if n.tipo=='capitulo':n.item['url_publica']=info['url_publica']+'capitulo-'+str(n.item['numero_url'])+'/'
 return info,estrutura

def validar_catalogo(registro):
 rotas={};pastas={p for p,_ in registro}
 for pasta,info in registro:
  for slug in [info['endereco_publico'],*info.get('enderecos_anteriores',[])]:
   validar_slug(slug)
   if slug in pastas and slug!=pasta:raise ValueError('O endereço público coincide com a pasta de outra obra: '+slug)
   if slug in rotas and rotas[slug]!=pasta:raise ValueError('Endereço público ou antigo já utilizado por outra obra: '+slug)
   rotas[slug]=pasta

def pagina(fonte,pasta,caminho,origem,capitulo=None):
 # A base conserva o funcionamento de recursos e dados relativos da obra.
 partes=urlsplit(origem)
 base_path=partes.path.rstrip('/')
 extras='<base href="'+html.escape(base_path+'/'+pasta+'/',quote=True)+'">\n'
 if partes.netloc:extras+='<link rel="canonical" href="'+html.escape(partes.scheme+'://'+partes.netloc+caminho,quote=True)+'">\n'
 if capitulo:extras+='<meta name="literature-capitulo" content="'+html.escape(capitulo['id'],quote=True)+'">\n'
 return fonte.replace('<head>','<head>\n'+extras,1).replace('href="#','href="'+caminho+'#')

def redirecionar(caminho):
 alvo=json.dumps(caminho,ensure_ascii=True)
 return '<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Literature</title><script>location.replace('+alvo+'+location.search+location.hash);</script></head><body><p><a href="'+html.escape(caminho,quote=True)+'">Continuar para a obra</a></p></body></html>'
