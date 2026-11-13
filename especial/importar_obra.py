"""PUB-03B: uma Obra, capítulos comparados e identidades públicas explícitas."""
import argparse
from copy import deepcopy
import json
from pathlib import Path
import re
import shutil
import uuid
import importacao as chapter

PROTOCOL='inde.literature.work-review'
ID=re.compile(r'[A-Za-z0-9][A-Za-z0-9_-]{0,199}\Z')
SLUG=re.compile(r'[a-z0-9]+(?:-[a-z0-9]+)*\Z')
RESERVED={'home','assets','css-global','js-global','site','publicado','scripts','tests','backend','exports','autoria','obra'}

def read(path,limit=16*1024*1024):
    path=Path(path).resolve()
    if not path.is_file() or path.stat().st_size>limit:raise ValueError('Fonte ausente ou grande demais: '+str(path))
    return path.read_bytes()

def ident(value):
    if not isinstance(value,str) or not ID.fullmatch(value):raise ValueError('Identidade pública insegura ou não suportada: '+str(value))
    return value

def slug(value):
    if not isinstance(value,str) or not SLUG.fullmatch(value) or len(value)>100 or value in RESERVED:raise ValueError('Endereço público inválido ou reservado.')
    return value

def metadata(value):
    if set(value)!={'id','title','synopsis','slug','folder','aliases','next_number'}:raise ValueError('Metadado de Obra sem tradutor.')
    ident(value['id']);slug(value['slug']);slug(value['folder'])
    if not value['title'].strip() or not value['synopsis'].strip():raise ValueError('Preencha título e sinopse da Obra.')
    aliases=value.get('aliases',[])
    if not isinstance(aliases,list) or len(set(aliases))!=len(aliases) or value['slug'] in aliases:raise ValueError('Endereços anteriores repetidos.')
    for alias in aliases:slug(alias)
    if type(value['next_number']) is not int or not 1<=value['next_number']<=1000000:raise ValueError('Próximo número de URL inválido.')
    return deepcopy(value)

def validate_manifest(value):
    if value.get('protocol')!='inde.literature.work' or value.get('version')!=1 or set(value)-{'protocol','version','work','chapters','site'}:raise ValueError('Manifesto de Obra incompatível.')
    work=metadata(value['work']);chapters=value['chapters']
    if not isinstance(chapters,list) or not 1<=len(chapters)<=100:raise ValueError('Selecione entre 1 e 100 capítulos explícitos.')
    ids=set();numbers=set()
    for item in chapters:
        if set(item)!={'id','title','number','html','autoria'}:raise ValueError('Cada capítulo exige ID, título, número e as duas fontes.')
        ident(item['id']);number=item['number']
        if item['id'] in ids or type(number) is not int or not 1<=number<work['next_number'] or number in numbers or not item['title'].strip():raise ValueError('Capítulo repetido, título vazio ou reserva de URL inválida.')
        ids.add(item['id']);numbers.add(number)
    # Site is a new, explicitly selected local configuration, never an account or remote push.
    site=value['site']
    if set(site)!={'name','author','description','theme','origin','base_path'}:raise ValueError('Configuração de Site incompleta.')
    import publicacao
    data=publicacao.new_project();data['site']={target:site[key] for target,key in [('nome','name'),('autor','author'),('descricao','description'),('tema','theme'),('origem','origin'),('caminho_base','base_path')]};publicacao.validate(data)
    if site['origin']!=data['site']['origem'] or site['base_path']!=data['site']['caminho_base']:raise ValueError('Use origem e caminho base sem barra final; para a raiz, deixe o caminho base vazio.')
    return value

def compare(manifest,destination):
    manifest=Path(manifest).resolve();raw=read(manifest);value=validate_manifest(json.loads(raw));destination=Path(destination)
    if destination.exists():raise ValueError('Use uma pasta nova para a conferência da Obra.')
    destination.mkdir(parents=True)
    try:
        chapters=[]
        for index,item in enumerate(value['chapters']):
            report=chapter.compare(manifest.parent/item['html'],manifest.parent/item['autoria'],destination/f'capitulo-{index+1}')
            chapters.append({'id':item['id'],'title':item['title'],'number':item['number'],'review':str(report.relative_to(destination))})
        (destination/'manifesto-origem.json').write_bytes(raw)
        result={'protocol':PROTOCOL,'version':1,'id':str(uuid.uuid4()),'source':{'path':str(manifest),'copy':'manifesto-origem.json','sha256':chapter.hash_bytes(raw)},'work':value['work'],'site':value['site'],'chapters':chapters}
        target=destination/'conferencia-obra.json';target.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n');return target
    except Exception:
        shutil.rmtree(destination);raise

def load_review(path):
    path=Path(path).resolve();report=json.loads(read(path,64*1024*1024))
    if report.get('protocol')!=PROTOCOL or report.get('version')!=1:raise ValueError('Conferência de Obra incompatível.')
    source=report['source'];copy=(path.parent/source['copy']).resolve()
    if copy.parent!=path.parent or chapter.hash_bytes(read(copy))!=source['sha256'] or chapter.hash_bytes(read(source['path']))!=source['sha256']:raise ValueError('O manifesto mudou; confira novamente.')
    manifest=validate_manifest(json.loads(read(copy)))
    if report['work']!=manifest['work'] or report['site']!=manifest['site'] or len(report['chapters'])!=len(manifest['chapters']):raise ValueError('Metadados da conferência adulterados.')
    for item,original in zip(report['chapters'],manifest['chapters']):
        if any(item[key]!=original[key] for key in ('id','title','number')):raise ValueError('Ordem ou identidade da conferência adulterada.')
        review=(path.parent/item['review']).resolve()
        if not review.is_relative_to(path.parent) or review.parent==path.parent:raise ValueError('Conferência de capítulo fora da Obra.')
        data=chapter.load_review(review)
        for kind in ('html','autoria'):
            if Path(data['sources'][kind]['path'])!=(Path(source['path']).parent/original[kind]).resolve():raise ValueError('Fonte de capítulo trocada na conferência.')
    return report

def plan(report,choices,destination):
    report=Path(report).resolve();value=load_review(report)
    if set(choices)!=set(item['id'] for item in value['chapters']):raise ValueError('Escolha uma fonte para cada capítulo, sem omissões nem capítulos extras.')
    entries=[];entities={}
    for item in value['chapters']:
        review_path=(report.parent/item['review']).resolve();review=chapter.load_review(review_path);choice=choices[item['id']]
        if choice not in ('html','autoria'):raise ValueError('Escolha explicitamente HTML ou autoria em cada capítulo.')
        candidate=review['candidates'][choice]
        if not candidate['convertible']:raise ValueError(item['title']+': '+'; '.join(candidate['issues']))
        content=candidate['content']
        if content['chapter_id']!=item['id']:raise ValueError('O ID da fonte escolhida difere do capítulo declarado: '+item['id'])
        for entity in content['entities']:
            ident(entity['id'])
            if entity['id'] in entities and entities[entity['id']]!=entity:raise ValueError('O mesmo ID de Personagem possui definições incompatíveis entre capítulos: '+entity['id'])
            entities[entity['id']]=entity
        if len(content['text'])>100000:raise ValueError('Capítulo acima do limite público de 100 mil caracteres.')
        import publicacao
        preview={'id':item['id'],'texto':content['text'],'anotacoes':[{'id':a['id'],'tipo':a['kind'],'entidade_id':a['target'],'inicio':a['start'],'fim':a['end'],'trecho':content['text'][a['start']:a['end']]} for a in content['annotations']],'formatacao':content['formatting'],'cenas':[{'id':scene['id'],'start':scene['start'],'end':scene['end']} for scene in content['scenes']]}
        if sum(s['start']==0 and s['end']==len(content['text']) for s in content['scenes'])>1:raise ValueError('Mais de uma cena cobre o capítulo completo; escolha a estrutura antes de importar.')
        publicacao.render_document(preview,'')
        for group in ('annotations','scenes'):
            for annotation in content[group]:ident(annotation['id'])
        entries.append({**item,'review':str(review_path),'review_sha256':chapter.hash_bytes(read(review_path,64*1024*1024)),'choice':choice,'sources':review['sources'],'content':content})
    result={'protocol':'inde.literature.work-import','version':1,'review':str(report),'review_sha256':chapter.hash_bytes(read(report,64*1024*1024)),'work':value['work'],'site':value['site'],'chapters':entries,'entities':list(entities.values())}
    with Path(destination).open('x') as output:output.write(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    return Path(destination)

def demo(root):
    root=Path(root);root.mkdir(parents=True,exist_ok=False)
    items=[]
    for index,(name,number) in enumerate([('A chegada',7),('O retorno',11)]):
        folder=root/f'capitulo-{index+1}';review=chapter.demo(folder);saved=folder/'salvo.narrproj';html=folder/'aplicado.html';source_id=f'capitulo-origem-{index+1}'
        data=json.loads(saved.read_text());data['section'].update({'id':source_id,'data-pov-type':'third_person_limited','data-pov-character':'character-origem'})
        if index:data['text']=data['text'].replace('chegou','voltou')
        data['semantic'][0].update(start=data['text'].index('—'),end=len(data['text']));data['formatting']={'fmt_bold':[[0,4]]};data['sections']=[{'id':f'cena-origem-{index+1}','start':0,'end':len(data['text']),'meta':{'id':f'cena-origem-{index+1}','data-scope':'scene'}}];saved.write_text(json.dumps(data,ensure_ascii=False))
        applied=html.read_text().replace('capitulo-origem',source_id).replace('<p>Mara','<p><strong>Mara</strong>')
        if index:applied=applied.replace('chegou','voltou')
        position=applied.index('>');applied=applied[:position]+' data-pov-type="third_person_limited" data-pov-character="character-origem"'+applied[position:position+1]+f'<section id="cena-origem-{index+1}" data-scope="scene">'+applied[position+1:];applied=applied.replace('</section><script','</section></section><script');html.write_text(applied)
        # chapter.demo's preliminary comparison is discarded only inside this newly created fixture.
        shutil.rmtree(folder/'revisao')
        items.append({'id':source_id,'title':name,'number':number,'html':str(html.relative_to(root)),'autoria':str(saved.relative_to(root))})
    manifest={'protocol':'inde.literature.work','version':1,'work':{'id':'obra-literature-origem','title':'Marés: uma Obra importada','synopsis':'Dois capítulos fictícios com identidades públicas preservadas.','slug':'mares-literature','folder':'obra-1','aliases':['mares-antigas'],'next_number':18},'site':{'name':'Biblioteca da importação','author':'Autoria de demonstração','description':'Obras trazidas para a oficina INDE','theme':'violeta','origin':'','base_path':'/biblioteca'},'chapters':items}
    path=root/'obra-literature.json';path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n');return compare(path,root/'revisao-obra')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--demo');parser.add_argument('--manifesto');parser.add_argument('--conferencia');parser.add_argument('--escolhas');parser.add_argument('--saida',required=False);args=parser.parse_args()
    try:
        if args.demo:result=demo(args.demo)
        elif args.conferencia:result=plan(args.conferencia,json.loads(Path(args.escolhas).read_text()),args.saida)
        else:result=compare(args.manifesto,args.saida)
        print(result)
    except (ValueError,KeyError,TypeError,OSError) as error:parser.exit(1,str(error)+'\n')
