"""Hierarquia editorial: identidade estável, posição entre irmãos e ordem de leitura."""
from dataclasses import dataclass
import copy
import unicodedata
import uuid

NIVEIS=('saga','volume','arco','capitulo')
FILHOS={'saga':'volumes','volume':'arcos','arco':'capitulos'}

def normalizar(texto):
    return ''.join(c for c in unicodedata.normalize('NFD',str(texto)) if not unicodedata.combining(c)).casefold()

def ordenados(lista):
    return sorted(lista,key=lambda x:x['ordem'])

@dataclass
class No:
    tipo:str
    item:dict
    caminho:tuple
    pai:str|None
    posicao:tuple
    nomes:tuple
    @property
    def id(self):return self.item['id']
    @property
    def endereco(self):return '.'.join(map(str,self.posicao))
    @property
    def titulo(self):return self.item.get('titulo',self.item.get('nome',''))

def nos(arvore):
    """Caminhos apontam para as listas originais; apresentação segue `ordem`."""
    def percorrer(lista,tipo,prefixo,pai,posicao,nomes):
        for indice,(original,item) in enumerate(sorted(enumerate(lista),key=lambda x:x[1]['ordem']),1):
            caminho=prefixo+(original,);titulo=item.get('titulo',item.get('nome',''))
            no=No(tipo,item,caminho,pai,posicao+(indice,),nomes+(titulo,));yield no
            if tipo in FILHOS:
                chave=FILHOS[tipo]
                yield from percorrer(item[chave],NIVEIS[NIVEIS.index(tipo)+1],caminho+(chave,),no.id,no.posicao,no.nomes)
    if 'capitulos' in arvore:yield from percorrer(arvore['capitulos'],'capitulo',('capitulos',),None,(),())
    else:yield from percorrer(arvore.get('sagas',[]),'saga',('sagas',),None,(),())

def validar(arvore):
    if not isinstance(arvore,dict):raise ValueError('A estrutura deve ser um objeto.')
    if arvore.get('schema_version')!=2:raise ValueError('A versão da estrutura editorial deve ser 2.')
    if ('capitulos' in arvore)==('sagas' in arvore):raise ValueError('Use capítulos simples ou sagas, sem misturar as duas estruturas.')
    ids=set()
    def lista(itens,tipo):
        if not isinstance(itens,list):raise ValueError('Agrupamentos e capítulos devem ser listas.')
        ordens=set()
        for item in itens:
            if not isinstance(item,dict):raise ValueError('Item editorial inválido.')
            ident=item.get('id');ordem=item.get('ordem')
            if not isinstance(ident,str) or not ident.strip() or ident in ids:raise ValueError('Identidade editorial ausente ou duplicada: '+str(ident))
            ids.add(ident)
            if type(ordem) is not int or ordem<1 or ordem in ordens:raise ValueError('A ordem deve ser um inteiro positivo, sem repetição entre irmãos: '+ident)
            ordens.add(ordem)
            campo='titulo' if tipo=='capitulo' else 'nome'
            if not isinstance(item.get(campo),str) or not item[campo].strip():raise ValueError('Preencha '+campo+' de '+ident)
            if tipo in FILHOS:lista(item.get(FILHOS[tipo]),NIVEIS[NIVEIS.index(tipo)+1])
    lista(arvore.get('capitulos') if 'capitulos' in arvore else arvore['sagas'],'capitulo' if 'capitulos' in arvore else 'saga')

def encontrar(arvore,ident):
    return next((no for no in nos(arvore) if no.id==ident),None)

def lista_destino(arvore,tipo,pai):
    if tipo=='saga':
        if pai is not None or 'sagas' not in arvore:raise ValueError('Uma saga pertence à raiz da estrutura hierárquica.')
        return arvore['sagas']
    if tipo=='capitulo' and 'capitulos' in arvore:
        if pai is not None:raise ValueError('Esta obra usa capítulos simples.')
        return arvore['capitulos']
    no=encontrar(arvore,pai)
    esperado=NIVEIS[NIVEIS.index(tipo)-1]
    if no is None or no.tipo!=esperado:raise ValueError('Escolha um destino do tipo '+esperado+'.')
    return no.item[FILHOS[no.tipo]]

def _ordenar(lista):
    for i,item in enumerate(lista,1):item['ordem']=i

def _posicao(valor,tamanho):
    if type(valor) is not int or not 1<=valor<=tamanho:raise ValueError(f'A posição deve estar entre 1 e {tamanho}.')

def mover(arvore,ident,pai,posicao):
    validar(arvore);novo=copy.deepcopy(arvore);no=encontrar(novo,ident)
    if no is None:raise ValueError('O item não existe mais. Recarregue a estrutura.')
    origem=lista_destino(novo,no.tipo,no.pai);destino=lista_destino(novo,no.tipo,pai)
    # A hierarquia fixa impede mover um agrupamento para um descendente.
    tamanho=len(destino) if origem is destino else len(destino)+1
    _posicao(posicao,tamanho)
    item=no.item;origem.remove(item);origem[:]=ordenados(origem);_ordenar(origem)
    destino[:]=ordenados(destino);destino.insert(posicao-1,item);_ordenar(destino)
    validar(novo);return novo

def inserir(arvore,item,tipo,pai,posicao):
    validar(arvore);novo=copy.deepcopy(arvore);destino=lista_destino(novo,tipo,pai)
    _posicao(posicao,len(destino)+1);destino[:]=ordenados(destino);destino.insert(posicao-1,copy.deepcopy(item));_ordenar(destino)
    validar(novo);return novo

def novo_grupo(arvore,tipo,nome,pai,posicao):
    if tipo not in FILHOS:raise ValueError('Escolha saga, volume ou arco.')
    ident=tipo+'-'+uuid.uuid4().hex
    item={'id':ident,'nome':nome.strip(),'ordem':1,FILHOS[tipo]:[]}
    return inserir(arvore,item,tipo,pai,posicao),ident

def converter_simples(arvore):
    validar(arvore)
    if 'capitulos' not in arvore:raise ValueError('A obra já possui hierarquia.')
    novo=copy.deepcopy(arvore);caps=novo.pop('capitulos')
    arco={'id':'arco-'+uuid.uuid4().hex,'nome':'Arco 1','ordem':1,'capitulos':ordenados(caps)}
    volume={'id':'volume-'+uuid.uuid4().hex,'nome':'Volume 1','ordem':1,'arcos':[arco]}
    saga={'id':'saga-'+uuid.uuid4().hex,'nome':'Saga 1','ordem':1,'volumes':[volume]}
    novo['sagas']=[saga];validar(novo);return novo
