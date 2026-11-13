"""Cadastro canônico e participações editoriais por identidade, sem inferir do texto."""
import copy
from contrato_semantico import normalizar_atributos,REFERENCIAS
from hierarquia import nos

def validar_catalogo(info):
 personagens=info.get('personagens',[])
 if not isinstance(personagens,list):raise ValueError('Personagens devem ser uma lista.')
 ids=set()
 for p in personagens:
  if not isinstance(p,dict) or not isinstance(p.get('id'),str) or not p['id'].strip() or p['id'] in ids:raise ValueError('Personagem com identidade ausente ou repetida.')
  ids.add(p['id'])
  if not isinstance(p.get('nome'),str) or not p['nome'].strip():raise ValueError('Preencha o nome do personagem '+p['id']+'.')
  if p.get('tipo_semantico','character') not in {'character','creature'}:raise ValueError('Tipo semântico do personagem deve ser character ou creature.')
  aliases=p.get('aliases',[])
  if not isinstance(aliases,list) or any(not isinstance(a,str) or not a.strip() for a in aliases):raise ValueError('Apelidos devem ser uma lista de nomes não vazios: '+p['nome'])
 return {p['id']:p for p in personagens}

def validar_participacoes(info,arvore):
 catalogo=validar_catalogo(info)
 for no in nos(arvore):
  if no.tipo!='capitulo':continue
  cenas=no.item.get('cenas',[])
  if not isinstance(cenas,list) or any(not isinstance(x,dict) for x in cenas):raise ValueError('As cenas devem ser uma lista de registros: '+no.titulo)
  for item in [no.item,*cenas]:
   ids=item.get('personagens_ids',[])
   if not isinstance(ids,list) or any(not isinstance(x,str) or x not in catalogo for x in ids):raise ValueError('Participação exige personagens cadastrados na obra: '+item.get('titulo',item.get('titulo_cena',no.titulo)))
   if len(ids)!=len(set(ids)):raise ValueError('Personagem repetido nas participações: '+no.titulo)

def indice_participacoes(arvore):
 resultado={}
 for no in nos(arvore):
  if no.tipo!='capitulo':continue
  itens=[(no.item,None,no.endereco+' · '+' / '.join(no.nomes))]
  itens += [(c,c['id'],no.endereco+' · '+' / '.join(no.nomes)+' / '+c['titulo_cena']) for c in no.item.get('cenas',[])]
  for item,cena,caminho in itens:
   for ident in item.get('personagens_ids',[]):
    resultado.setdefault(ident,[]).append({'capitulo':no.id,'cena':cena,'titulo':item.get('titulo',item.get('titulo_cena',no.titulo)),'caminho':caminho,'estado':no.item['estado_publicacao']})
 return resultado

def participacoes(arvore,personagem_id):return indice_participacoes(arvore).get(personagem_id,[])

def personagens_marcados(data,info):
 catalogo=validar_catalogo(info);refs=[]
 metas=[data.get('section',{}),*[x.get('meta',{}) for x in data.get('sections',[])],*[x.get('meta',{}) for x in data.get('semantic',[])]]
 for meta in metas:
  for k,v in normalizar_atributos(meta).items():
   if k in REFERENCIAS:
    for ident in (v if isinstance(v,list) else str(v or '').split()):
     if ident in catalogo and ident not in refs:refs.append(ident)
 return refs

def remapear_meta(meta,origem,destino,tipo="character"):
 novo=copy.deepcopy(meta)
 alvo=novo.get('data-target','');tokens=alvo if isinstance(alvo,list) else str(alvo).split()
 if origem in tokens and novo.get('data-target-type'):novo['data-target-type']=tipo
 for k,v in novo.items():
  if k not in REFERENCIAS | {'data-locutor-id','data-pensador-id','data-pov-id'}:continue
  tokens=v if isinstance(v,list) else str(v or '').split()
  novo[k]=' '.join(dict.fromkeys(destino if x==origem else x for x in tokens))
 return novo

def vincular_entidade(data,origem,destino,info):
 catalogo=validar_catalogo(info)
 if destino not in catalogo:raise ValueError('Escolha um personagem existente na obra.')
 if origem in catalogo:raise ValueError('A entidade já pertence ao cadastro. Este recurso não funde personagens cadastrados.')
 entidade=next((e for e in data.get('entities',[]) if e.get('id')==origem),None)
 if not entidade or entidade.get('type') not in {'character','creature'}:raise ValueError('Escolha um personagem ou criatura da autoria ainda não vinculado.')
 novo=copy.deepcopy(data);personagem=catalogo[destino]
 existente=next((e for e in novo.get('entities',[]) if e.get('id')==destino),{})
 combinado=copy.deepcopy(existente)
 for k,v in entidade.items():
  if k in {'id','name','type'}:continue
  if k in combinado and combinado[k]!=v:raise ValueError('As entidades possuem metadados divergentes em '+k+'. Revise antes de vincular.')
  combinado[k]=copy.deepcopy(v)
 combinado.update(id=destino,name=personagem['nome'],type=personagem.get('tipo_semantico','character'))
 novo['entities']=[e for e in novo.get('entities',[]) if e.get('id') not in {origem,destino}]
 novo['entities'].append(combinado)
 novo['section']=remapear_meta(novo.get('section',{}),origem,destino,personagem.get('tipo_semantico','character'))
 for campo in ('sections','semantic'):
  for item in novo.get(campo,[]):item['meta']=remapear_meta(item.get('meta',{}),origem,destino,personagem.get('tipo_semantico','character'))
 return novo

def referencias_em_html(html,personagem_id):
 from html.parser import HTMLParser
 class Referencias(HTMLParser):
  encontrado=False
  def handle_starttag(self,tag,attrs):
   for k,v in normalizar_atributos(attrs).items():
    if k in REFERENCIAS and personagem_id in str(v or '').split():self.encontrado=True
 p=Referencias();p.feed(html);return p.encontrado

def personagem_referenciado_na_autoria(data,personagem_id):
 metas=[data.get('section',{}),*[x.get('meta',{}) for x in data.get('sections',[])],*[x.get('meta',{}) for x in data.get('semantic',[])]]
 return any(personagem_id in (v if isinstance(v,list) else str(v or '').split()) for meta in metas for k,v in normalizar_atributos(meta).items() if k in REFERENCIAS)
