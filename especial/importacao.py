"""PUB-03A: comparação explícita de fontes, sem gravação no acervo."""
import argparse
from copy import deepcopy
from difflib import unified_diff
from hashlib import sha256
from html.parser import HTMLParser
import json
from pathlib import Path
import uuid

PROTOCOL = 'inde.literature.review'


def hash_bytes(raw):
    return sha256(raw).hexdigest()


def span(value, text):
    start, end = value['start'], value['end']
    if type(start) is not int or type(end) is not int or not 0 <= start < end <= len(text):
        raise ValueError('Intervalo Unicode fora do texto.')
    return start, end


def normalize(data):
    issues = []
    if data.get('version') != 5:
        raise ValueError('Esta etapa aceita autoria .narrproj versão 5.')
    expected = {'version','text','section','sections','lists','inline_structures','horizontal_rules','entities','formatting','semantic','elements','glossary'}
    for key in set(data) - expected:
        issues.append('Campo sem tradutor: ' + key)
    text = data['text']
    if not isinstance(text, str) or not text.strip() or len(text.encode()) > 8*1024*1024:
        raise ValueError('Texto vazio ou maior que 8 MiB.')
    for key in ('lists','inline_structures','horizontal_rules','glossary'):
        if data.get(key): issues.append('Recurso ainda sem tradutor: ' + key)
    if any(data.get('elements', {}).values()): issues.append('Recapitulação/prévia ainda sem tradutor.')
    entities = []
    ids = set()
    for entity in data.get('entities', []):
        if set(entity) - {'id','type','name'} or entity.get('type') != 'character':
            issues.append('Ficha de entidade sem tradutor completo: ' + str(entity.get('id')))
        ident, name = entity['id'], entity['name']
        if not isinstance(ident, str) or not ident or ident in ids or not isinstance(name,str) or not name.strip():
            raise ValueError('Identidade de entidade ausente ou repetida.')
        ids.add(ident); entities.append({'id': ident, 'name': name, 'type': entity.get('type')})
    annotations = []
    seen = set()
    for value in data.get('semantic', []):
        if set(value)-{'id','start','end','meta'}:issues.append('Campos adicionais de marcação sem tradutor.')
        start, end = span(value, text); meta = value['meta']; role = meta.get('data-role')
        field = {'dialogue':'data-speaker','thought':'data-thinker'}.get(role)
        if field is None or set(meta) != {'data-role',field}:
            issues.append('Marcação sem tradutor completo: ' + str(value.get('id')) + ' (' + str(role) + ')'); continue
        target = meta[field]
        if target not in ids: issues.append('Referência de entidade não resolvida: ' + str(target)); continue
        ident=value['id']
        if not isinstance(ident,str) or not ident or ident in seen: raise ValueError('Identidade de marcação ausente ou repetida.')
        seen.add(ident); annotations.append({'id':ident,'kind':role,'start':start,'end':end,'target':target})
    formatting = []
    for style, ranges in data.get('formatting', {}).items():
        if style not in ('fmt_bold','fmt_italic','fmt_underline'):
            if ranges: issues.append('Formatação sem tradutor: ' + style)
            continue
        for start,end in ranges:
            span({'start':start,'end':end},text);formatting.append({'style':style[4:],'start':start,'end':end})
    root = data.get('section', {})
    if not isinstance(root.get('id'),str) or not root.get('id'): issues.append('Identidade de capítulo ausente.')
    if set(root) - {'id','data-scope','data-pov-type','data-pov-character'}: issues.append('Metadados adicionais da seção raiz sem tradutor.')
    if root.get('data-scope')!='chapter':issues.append('A seção raiz precisa ser um capítulo explícito.')
    perspective = None
    if root.get('data-pov-character') or root.get('data-pov-type'):
        if root.get('data-pov-type')=='third_person_limited' and root.get('data-pov-character') in ids:
            perspective={'entity':root['data-pov-character'],'person':'third_person','focalization':'internal'}
        else: issues.append('Perspectiva incompleta ou sem correspondência explícita.')
    scenes=[];scene_ids=set()
    for scene in data.get('sections', []):
        if set(scene)-{'id','start','end','parent_id','meta'}:issues.append('Campos adicionais de cena sem tradutor.')
        ident=scene.get('id')
        if not isinstance(ident,str) or not ident or ident in scene_ids:raise ValueError('Identidade de cena ausente ou repetida.')
        scene_ids.add(ident)
        start,end=span(scene,text)
        if scene.get('parent_id') or set(scene.get('meta',{}))-{'id','data-scope'}: issues.append('Cena com hierarquia/perspectiva local ainda sem tradutor: '+scene['id'])
        if scene.get('meta',{}).get('id')!=scene['id'] or scene.get('meta',{}).get('data-scope')!='scene':issues.append('Metadados de identidade/escopo da cena inconsistentes.')
        scenes.append({'id':scene['id'],'start':start,'end':end})
    return {'text':text,'chapter_id':root.get('id',''),'entities':entities,'annotations':annotations,'formatting':formatting,'scenes':scenes,'perspective':perspective},sorted(set(issues))


class AppliedHTML(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True);self.text='';self.stack=[];self.semantic=[];self.sections=[];self.formatting={};self.root={};self.issues=[];self.entities=[];self.script=None;self.script_text='';self.paragraph=False
    def handle_starttag(self, tag, attrs):
        attrs=dict(attrs)
        if tag=='script':
            self.script=attrs.get('id') if attrs.get('type')=='application/json' else 'unsupported';self.script_text='';return
        if tag not in ('section','p','br','span','b','strong','i','em','u'):
            self.issues.append('Elemento HTML sem tradutor: '+tag)
        if tag=='br': self.text+='\n';return
        if tag=='p':
            if self.paragraph:self.text+='\n\n'
            self.paragraph=True
        self.stack.append((tag,attrs,len(self.text)))
        if tag=='section' and attrs.get('data-scope')=='chapter':
            if self.root:self.issues.append('Mais de um capítulo no HTML; use um par de fontes por conferência.')
            self.root=attrs
        if tag=='section' and attrs.get('data-scope') not in ('chapter','scene'):self.issues.append('Seção HTML sem tradutor.')
    def handle_endtag(self, tag):
        if tag=='script':
            if self.script=='narrative-entities':self.entities=json.loads(self.script_text)
            else:self.issues.append('Script sem tradutor: '+str(self.script))
            self.script=None;return
        if not self.stack:return
        if self.stack[-1][0]!=tag:self.issues.append('Estrutura HTML não balanceada: '+tag);return
        _,attrs,start=self.stack.pop();end=len(self.text)
        if 'data-role' in attrs:
            self.semantic.append({'id':attrs.get('id',''),'start':start,'end':end,'meta':{k:v for k,v in attrs.items() if k!='id'}})
        elif tag not in ('section',) and set(attrs)-{'id'}:self.issues.append('Atributos HTML sem tradutor: '+tag)
        if tag in ('b','strong','i','em','u'):
            style={'b':'bold','strong':'bold','i':'italic','em':'italic','u':'underline'}[tag];self.formatting.setdefault('fmt_'+style,[]).append([start,end])
        if tag=='section' and attrs.get('data-scope')=='scene':self.sections.append({'id':attrs.get('id',''),'start':start,'end':end,'parent_id':None,'meta':attrs})
    def handle_data(self, value):
        if self.script is not None:self.script_text+=value
        elif self.stack and (value.strip() or any(tag in ('p','span','b','strong','i','em','u') for tag,_,_ in self.stack)):self.text+=value
    def finish(self):
        if self.stack:self.issues.append('Elementos HTML não encerrados.')
        content,issues=normalize({'version':5,'text':self.text,'section':self.root,'sections':self.sections,'entities':self.entities,'formatting':self.formatting,'semantic':self.semantic})
        return content,sorted(set(issues+self.issues))


def candidate(raw, kind):
    try:
        if kind=='autoria': content,issues=normalize(json.loads(raw))
        else:
            parser=AppliedHTML();parser.feed(raw.decode('utf-8'));parser.close();content,issues=parser.finish()
        return {'content':content,'issues':issues,'convertible':not issues}
    except (ValueError,KeyError,TypeError,UnicodeError) as error:
        return {'content':None,'issues':[str(error)],'convertible':False}


def compare(html, authoring, destination):
    destination=Path(destination)
    if destination.exists():raise ValueError('Use uma pasta nova para a conferência.')
    values={};sources={};raws={}
    for kind,path in [('html',html),('autoria',authoring)]:
        path=Path(path).resolve()
        if not path.is_file() or path.stat().st_size>16*1024*1024:raise ValueError('Fonte inválida ou maior que 16 MiB.')
        raw=path.read_bytes()
        if len(raw)>16*1024*1024:raise ValueError('Fonte maior que 16 MiB; nada foi truncado.')
        raws[kind]=raw;sources[kind]={'path':str(path),'sha256':hash_bytes(raw),'copy':'aplicado.html' if kind=='html' else 'autoria.narrproj'};values[kind]=candidate(raw,kind)
    destination.mkdir(parents=True)
    for kind,raw in raws.items():(destination/sources[kind]['copy']).write_bytes(raw)
    text=lambda k:(values[k]['content'] or {}).get('text','')
    report={'protocol':PROTOCOL,'version':1,'id':str(uuid.uuid4()),'sources':sources,'candidates':values,'equal':values['html']['content']==values['autoria']['content'],'diff':'\n'.join(unified_diff(text('html').splitlines(),text('autoria').splitlines(),fromfile='HTML aplicado',tofile='Autoria salva',lineterm='')),'publication':'pending-public-identities'}
    (destination/'conferencia.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    return destination/'conferencia.json'


def load_review(path):
    path=Path(path).resolve()
    if path.stat().st_size>64*1024*1024:raise ValueError('Conferência maior que 64 MiB.')
    report=json.loads(path.read_text())
    if report.get('protocol')!=PROTOCOL or report.get('version')!=1:raise ValueError('Conferência incompatível.')
    if set(report['sources'])!={'html','autoria'} or set(report['candidates'])!={'html','autoria'}:raise ValueError('A conferência precisa conter as duas fontes.')
    for kind,source in report['sources'].items():
        original=Path(source['path']);copy=(path.parent/source['copy']).resolve()
        if not original.is_file() or not copy.is_file() or max(original.stat().st_size,copy.stat().st_size)>16*1024*1024:raise ValueError('Fonte ausente ou maior que 16 MiB.')
        if not copy.is_relative_to(path.parent) or hash_bytes(copy.read_bytes())!=source['sha256'] or hash_bytes(original.read_bytes())!=source['sha256']:raise ValueError('A fonte mudou desde a comparação; confira novamente.')
        if candidate(copy.read_bytes(),kind)!=report['candidates'][kind]:raise ValueError('Diagnóstico ou conteúdo da conferência adulterado.')
    return report


def plan(path, choice, destination, title):
    report=load_review(path)
    if choice not in ('html','autoria'):raise ValueError('Escolha explicitamente uma das fontes.')
    selected=report['candidates'][choice]
    if not selected['convertible']:raise ValueError('A fonte escolhida ainda contém recursos sem tradutor: '+'; '.join(selected['issues']))
    value={'protocol':'inde.literature.import','version':1,'review_id':report['id'],'choice':choice,'title':title,'content':selected['content'],'sources':report['sources'],'review':str(Path(path).resolve()),'review_sha256':hash_bytes(Path(path).read_bytes()),'publication':'pending-public-identities'}
    destination=Path(destination)
    with destination.open('x') as output:output.write(json.dumps(value,ensure_ascii=False,indent=2)+'\n')
    return destination


def demo(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=False)
    text='Mara chegou à estação.\n\n— Ainda há tempo — disse ela.'
    start=text.index('—');entity={'id':'character-origem','type':'character','name':'Mara'}
    data={'version':5,'text':text,'section':{'id':'capitulo-origem','data-scope':'chapter'},'sections':[],'entities':[entity],'formatting':{},'semantic':[{'id':'fala-origem','start':start,'end':len(text),'meta':{'data-role':'dialogue','data-speaker':entity['id']}}]}
    (root/'salvo.narrproj').write_text(json.dumps(data,ensure_ascii=False))
    (root/'aplicado.html').write_text('<section id="capitulo-origem" data-scope="chapter"><p>Mara chegou à estação.</p><p><span id="fala-origem" data-role="dialogue" data-speaker="character-origem">— Ainda havia tempo — disse ela.</span></p></section><script id="narrative-entities" type="application/json">'+json.dumps([entity],ensure_ascii=False)+'</script>')
    return compare(root/'aplicado.html',root/'salvo.narrproj',root/'revisao')


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--html');parser.add_argument('--autoria');parser.add_argument('--saida');parser.add_argument('--demo');parser.add_argument('--plano');parser.add_argument('--escolha',choices=['html','autoria']);parser.add_argument('--titulo',default='Obra importada do Literature')
    args=parser.parse_args()
    try:
        if args.demo:result=demo(args.demo)
        elif args.plano:result=plan(args.plano,args.escolha,Path(args.saida),args.titulo)
        else:result=compare(args.html,args.autoria,args.saida)
        print(result)
    except (ValueError,OSError,KeyError) as error:parser.exit(1,str(error)+'\n')
