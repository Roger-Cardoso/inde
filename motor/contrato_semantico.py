"""Referências semânticas compartilhadas pela importação e publicação.

A autoria usa os atributos nativos; aliases antigos continuam aceitos na entrada.
"""
ALIASES = {'data-locutor-id':'data-speaker', 'data-pensador-id':'data-thinker', 'data-pov-id':'data-pov-character'}
REFERENCIAS = {'data-speaker','data-thinker','data-pov-character','data-addressee','data-target'}

def normalizar_atributos(attrs):
    resultado=dict(attrs)
    for antigo,novo in ALIASES.items():
        if antigo in resultado:
            resultado[novo]=' '.join(dict.fromkeys((str(resultado.get(novo) or '')+' '+str(resultado.pop(antigo) or '')).split()))
    if not resultado.get('data-role'):
        if resultado.get('data-speaker'):resultado['data-role']='dialogue'
        elif resultado.get('data-thinker'):resultado['data-role']='thought'
    return resultado

def validar_referencias(html,personagens,verbetes,tipos=None):
    """Valida aliases, múltiplas referências e registros nativos antes de gravar/publicar."""
    from html.parser import HTMLParser
    import json
    class Marcacoes(HTMLParser):
        def __init__(self):
            super().__init__();self.refs=[];self.termos=[];self.registro=None;self.partes=[];self.entidades=set();self.glossario={}
        def handle_starttag(self,tag,attrs):
            a=normalizar_atributos(attrs)
            if tag=='script' and a.get('id') in {'story-entities','story-glossary'} and a.get('type')=='application/json':
                self.registro=a['id'];self.partes=[]
            for k,v in a.items():
                if k in REFERENCIAS:self.refs.extend((k,x) for x in str(v or '').split())
                if k=='data-verbete-id' and v not in verbetes:raise ValueError(f'Verbete no texto inexistente: {v}')
                if k=='data-glossary-term':self.termos.append(v)
        def handle_data(self,txt):
            if self.registro:self.partes.append(txt)
        def handle_endtag(self,tag):
            if tag!='script' or not self.registro:return
            registro=self.registro;self.registro=None
            try:dados=json.loads(''.join(self.partes))
            except ValueError as e:raise ValueError(f'Registro JSON inválido: {registro}') from e
            if registro=='story-glossary':
                if not isinstance(dados,dict) or not all(isinstance(v,str) for v in dados.values()):raise ValueError('Glossário nativo deve conter definições textuais.')
                self.glossario.update(dados);return
            if not isinstance(dados,list):raise ValueError('Entidades nativas devem ser uma lista.')
            for item in dados:
                if not isinstance(item,dict) or not isinstance(item.get('id'),str) or not item['id'].strip() or not isinstance(item.get('name'),str):raise ValueError('Entidade nativa sem id/nome.')
                if item['id'] in self.entidades:raise ValueError('Entidade nativa duplicada: '+item['id'])
                if tipos and item['id'] in tipos and item.get('type')!=tipos[item['id']]:raise ValueError('Tipo semântico desatualizado na autoria: '+item['id'])
                self.entidades.add(item['id'])
    p=Marcacoes();p.feed(html);p.close()
    for atributo,ref in p.refs:
        if ref not in set(personagens) | p.entidades:raise ValueError(f'Referência semântica inexistente em {atributo}: {ref}')
    for termo in p.termos:
        if termo not in p.glossario:raise ValueError(f'Termo sem definição no glossário nativo: {termo}')
