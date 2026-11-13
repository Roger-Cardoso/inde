"""Fundação local de publicação do INDE. Python padrão; nenhum envio remoto."""
from __future__ import annotations

import argparse
from copy import deepcopy
from datetime import date
from functools import partial
import hashlib
from html import escape
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import sys
import tempfile
import threading
import subprocess
from urllib.parse import urlsplit
import uuid
import webbrowser
from zipfile import ZipFile, ZIP_DEFLATED

BASE = Path(__file__).resolve().parent
sys.path.insert(0, str(BASE))
sys.path.insert(0, str(BASE / 'motor'))
import crud
from gerar_site import gerar


def native_binary():
    packaged = BASE / 'bin/inde_web_bridge'
    return packaged if packaged.is_file() else BASE / 'build-nativo/inde_web_bridge'


def encode(value):
    return json.dumps(value, ensure_ascii=False, indent=2) + '\n'


def digest(value):
    return hashlib.sha256(encode(value).encode()).hexdigest()


def script_json(value):
    return json.dumps(value, ensure_ascii=True).replace('<', '\\u003c').replace('>', '\\u003e').replace('&', '\\u0026')


def save_json(path, value):
    """Substituição atômica no mesmo diretório; usado sob o lock do serviço."""
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(dir=path.parent, prefix='.gravacao-')
    try:
        with os.fdopen(fd, 'w', encoding='utf-8') as stream:
            stream.write(encode(value))
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        Path(temporary).unlink(missing_ok=True)


def identity():
    return str(uuid.uuid4())


def validate(data):
    if data.get('schema_version') != 1:
        raise ValueError('Versão de projeto não suportada.')
    site = data['site']
    for key in ('nome', 'autor', 'descricao'):
        if not isinstance(site.get(key), str) or not site[key].strip() or len(site[key]) > 500:
            raise ValueError('Preencha nome, autoria e descrição, com até 500 caracteres.')
    if site.get('tema') not in ('verde', 'violeta', 'ambar'):
        raise ValueError('Escolha um dos temas disponíveis.')
    origin = site.get('origem', '')
    parts = urlsplit(origin)
    if origin and (parts.scheme != 'https' or not parts.hostname or parts.username or parts.password
                   or parts.port or parts.path not in ('', '/') or parts.query or parts.fragment):
        raise ValueError('Use uma origem HTTPS sem caminho, ou deixe vazia para uso local.')
    site['origem'] = origin.rstrip('/')
    prefix = site.get('caminho_base', '')
    if prefix == '/':
        prefix = ''
    if prefix and not re.fullmatch(r'(?:/[a-zA-Z0-9_-]+)+/?', prefix):
        raise ValueError('Caminho base inválido. Exemplo: /biblioteca.')
    site['caminho_base'] = prefix.rstrip('/')
    destination = data['destino']
    if destination.get('tipo') not in ('local', 'pacote', 'github'):
        raise ValueError('Destino desconhecido.')
    if destination['tipo'] == 'github':
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9_.-]+', destination.get('repositorio', '')):
            raise ValueError('Informe o repositório como proprietário/nome.')
        branch = destination.get('branch', '')
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_./-]{0,100}', branch) or any(x in branch for x in ('..', '//', '@{')) or any(p.startswith('.') or p.endswith(('.', '.lock')) for p in branch.split('/')):
            raise ValueError('Nome de branch inválido.')
        if not site['origem']:
            raise ValueError('Informe a origem pública para preparar o destino GitHub.')
    work = data.get('obra')
    if work is not None:
        if not work.get('titulo', '').strip() or not work.get('sinopse', '').strip():
            raise ValueError('Preencha título e sinopse da Obra.')
        if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', work.get('endereco', '')) or work['endereco'] in {'home', 'assets', 'site', 'publicado', 'scripts', 'css-global', 'js-global'}:
            raise ValueError('Endereço da Obra inválido.')
        if 'pasta_publica' in work:
            folder=work['pasta_publica']
            if not re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*',folder) or len(folder)>100 or folder in {'home','assets','site','publicado','scripts','css-global','js-global'}:raise ValueError('Pasta pública inválida.')
        collection = crud.resources(work)
        resource_ids = [r['id'] for r in collection]
        if len(set(resource_ids)) != len(resource_ids):
            raise ValueError('Identidades de recurso repetidas.')
        for resource in collection:
            if resource.get('tipo') not in ('personagem', 'verbete') or not resource.get('nome', '').strip():
                raise ValueError('Informe nome e tipo válido para cada recurso.')
        characters = {r['id'] for r in collection if r['tipo'] == 'personagem'}
        glossary = {r['id'] for r in collection if r['tipo'] == 'verbete'}
        primary = (work.get('personagem') or {}).get('id', '')
        ids, numbers = set(), set()
        for doc in work['documentos']:
            if doc['id'] in ids or not re.fullmatch(r'[a-zA-Z0-9_-]+', doc['id']):
                raise ValueError('Identidade de Documento inválida ou repetida.')
            ids.add(doc['id'])
            if type(doc.get('numero')) is not int or doc['numero'] < 1 or doc['numero'] in numbers:
                raise ValueError('Cada capítulo precisa de um número público positivo e exclusivo.')
            numbers.add(doc['numero'])
            if not doc['titulo'].strip() or not isinstance(doc['texto'], str) or len(doc['texto']) > 100_000:
                raise ValueError('Documento sem título ou acima do limite de 100 mil caracteres.')
            # O corte inicial associa a cena ao Documento completo. Não há reancoragem implícita.
            if doc.get('perspectiva') and doc.get('pov_id', primary) not in characters:
                raise ValueError('O ponto de vista referencia um personagem inexistente.')
            for annotation in doc.get('anotacoes', []):
                if annotation['tipo'] == 'glossary':
                    if annotation.get('alvo') not in glossary:
                        raise ValueError('Marcação referencia verbete inexistente.')
                elif annotation.get('entidade_id', primary) not in characters:
                    raise ValueError('Marcação referencia personagem inexistente.')
            render_document(doc, primary)
        if work['publica'] and not any(d['publico'] and d['texto'].strip() for d in work['documentos']):
            raise ValueError('Uma Obra pública precisa de ao menos um capítulo com texto público.')
    return data


def new_project(example=False):
    data = {'schema_version': 1, 'id': identity(),
            'site': {'nome': 'Minha biblioteca', 'autor': 'Sua autoria',
                     'descricao': 'Histórias e universos para descobrir.', 'tema': 'violeta',
                     'origem': '', 'caminho_base': ''},
            'destino': {'tipo': 'local', 'repositorio': '', 'branch': 'main'}, 'obra': None}
    return add_example(data) if example else data


def add_example(data):
    if data['obra'] is not None:
        raise ValueError('Este projeto já tem uma Obra. A amostra não substituirá seu conteúdo.')
    data = deepcopy(data)
    speaker = identity()
    work_id = identity()
    first = 'Mara chegou à estação antes do amanhecer.\n\n— Ainda há tempo — disse ela.\n\nO farol permanecia aceso. Talvez alguém estivesse esperando.'
    second = 'Quando a porta se abriu, Mara reconheceu o cheiro de chuva.\n\nA viagem começava ali.'
    speech = '— Ainda há tempo — disse ela.'
    thought = 'Talvez alguém estivesse esperando.'
    data['obra'] = {'id': work_id, 'titulo': 'A estação das marés', 'endereco': 'estacao-das-mares',
                    'sinopse': 'Uma viajante encontra uma estação que só abre quando o mar recua. Conteúdo fictício para experimentar a publicação.',
                    'publica': True, 'personagem': {'id': speaker, 'nome': 'Mara'},
                    'documentos': [
                        {'id': identity(), 'titulo': 'O farol', 'numero': 1, 'publico': True, 'texto': first,
                         'perspectiva': True, 'anotacoes': [
                             {'id': identity(), 'tipo': 'glossary', 'inicio': first.index('estação'), 'fim': first.index('estação') + len('estação'), 'trecho': 'estação', 'alvo': work_id + '-verbete'},
                             {'id': identity(), 'tipo': 'dialogue', 'inicio': first.index(speech), 'fim': first.index(speech) + len(speech), 'trecho': speech},
                             {'id': identity(), 'tipo': 'thought', 'inicio': first.index(thought), 'fim': first.index(thought) + len(thought), 'trecho': thought}]},
                        {'id': identity(), 'titulo': 'A porta', 'numero': 2, 'publico': True,
                         'texto': second, 'perspectiva': True, 'anotacoes': []}]}
    return data


def render_document(document, entity_id):
    text = document['texto']
    annotations = sorted(document.get('anotacoes', []), key=lambda a: a['inicio'])
    previous = 0
    for annotation in annotations:
        start, end = annotation['inicio'], annotation['fim']
        if type(start) is not int or type(end) is not int or not previous <= start < end <= len(text) or annotation['tipo'] not in ('dialogue', 'thought', 'glossary'):
            raise ValueError('Marcação inválida ou sobreposta. Este corte suporta fala, pensamento e verbete sem sobreposição.')
        if text[start:end] != annotation['trecho']:
            raise ValueError('O texto marcado mudou. Remova ou refaça as marcações antes de gerar; nada será reancorado automaticamente.')
        previous = end
    formats=document.get('formatacao',[])
    for f in formats:
        if f['style'] not in ('bold','italic','underline') or f.get('value') or type(f['start']) is not int or type(f['end']) is not int or not 0<=f['start']<f['end']<=len(text):raise ValueError('Formatação inválida ou sem tradutor.')
    scenes=document.get('cenas',[])
    ids=set()
    for scene in scenes:
        if scene['id'] in ids or not 0<=scene['start']<scene['end']<=len(text):raise ValueError('Cena inválida ou repetida.')
        ids.add(scene['id'])
    root_scene=document.get('ancora_cena','cena-'+document['id'])
    rendered_scenes={root_scene}
    def scene_marker(scene):
        if scene['id'] in rendered_scenes:return ''
        rendered_scenes.add(scene['id']);return '<span id="'+escape(scene['id'],quote=True)+'"></span>'
    def fragment_html(start,stop):
        cuts=sorted({start,stop,*[max(start,min(stop,f[k])) for f in formats for k in ('start','end')],*[max(start,min(stop,s['start'])) for s in scenes]})
        parts=[]
        for a,b in zip(cuts,cuts[1:]):
            for scene in scenes:
                if scene['start']==a and scene['id']!=root_scene:parts.append(scene_marker(scene))
            piece=escape(text[a:b]).replace('\n','<br>')
            for style,tag in [('bold','strong'),('italic','em'),('underline','u')]:
                if any(f['style']==style and f['start']<=a and f['end']>=b for f in formats):piece='<'+tag+'>'+piece+'</'+tag+'>'
            parts.append(piece)
        return ''.join(parts)
    paragraphs = []
    offset = 0
    for paragraph in text.split('\n\n'):
        end = offset + len(paragraph)
        fragment, cursor = [], offset
        for scene in scenes:
            if offset>0 and offset-2<=scene['start']<offset:fragment.append(scene_marker(scene))
        for annotation in annotations:
            start, stop = annotation['inicio'], annotation['fim']
            if start >= end or stop <= offset:
                continue
            if start < offset or stop > end:
                raise ValueError('Uma marcação atravessa parágrafos. Divida a marcação neste corte.')
            fragment.append(fragment_html(cursor,start))
            if annotation['tipo'] == 'glossary':
                attributes = f'data-verbete-id="{escape(annotation["alvo"], quote=True)}"'
            else:
                field = 'speaker' if annotation['tipo'] == 'dialogue' else 'thinker'
                attributes = f'data-role="{annotation["tipo"]}" data-{field}="{escape(annotation.get("entidade_id", entity_id))}"'
            fragment.append(f'<span {attributes} data-text-id="{escape(annotation["id"])}">{fragment_html(start,stop)}</span>')
            cursor = stop
        fragment.append(fragment_html(cursor,end))
        paragraphs.append('<p>' + ''.join(fragment).replace('\n', '<br>') + '</p>')
        offset = end + 2
    pov = f' data-pov-character="{escape(document.get("pov_id", entity_id))}" data-pov-type="{escape(document.get("pov_type", "third_person_limited"))}"' if document.get('perspectiva') else ''
    scene = document.get('ancora_cena', 'cena-' + document['id'])
    return f'<section id="{escape(scene, quote=True)}" data-scope="scene"{pov}>' + ''.join(paragraphs) + '</section>'


def brand_html(template, site):
    for token, key in [('__INDE_NOME__', 'nome'), ('__INDE_AUTOR__', 'autor'), ('__INDE_DESCRICAO__', 'descricao')]:
        template = template.replace(token, escape(site[key], quote=True))
    public_site = {key: site[key] for key in ('nome', 'autor', 'descricao', 'tema')}
    config = '<script id="inde-site" type="application/json">' + script_json(public_site) + '</script><script>window.INDE_SITE=JSON.parse(document.getElementById("inde-site").textContent);</script>'
    return template.replace('<head>', '<head>\n' + config, 1)


def materialize(data, root):
    site = data['site']
    works = data.get('obras', [data['obra']] if data.get('obra') else [])
    folders = [w.get('pasta_publica',('obra-'+w['id']) if 'obras' in data else 'obra') for w in works]
    prefix = site['caminho_base']
    for folder in ('css-global', 'js-global'):
        shutil.copytree(BASE / 'recursos' / folder, root / folder)
    (root / 'assets/imagens').mkdir(parents=True)
    shutil.copy2(BASE / 'recursos/personagem.svg', root / 'assets/imagens/personagem.svg')
    (root / 'assets/imagens/marca.svg').write_text('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 100"><rect width="80" height="100" rx="8" fill="#232139"/><path d="M20 24h40M20 36h40M20 48h28" stroke="#c4b5fd" stroke-width="3"/><path d="M20 70h40" stroke="#c4b5fd"/><text x="40" y="90" text-anchor="middle" fill="#eee" font-size="11">INDE</text></svg>')
    home = brand_html((BASE / 'recursos/home.html').read_text(), site)
    if site['origem']:
        canonical = site['origem'] + prefix + '/home/'
        home = home.replace('<head>', '<head><link rel="canonical" href="' + escape(canonical, quote=True) + '">', 1)
    (root / 'home').mkdir()
    (root / 'home/index.html').write_text(home)
    redirect = '<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta http-equiv="refresh" content="0;url=home/"><title>' + escape(site['nome']) + '</title><a href="home/">Abrir biblioteca</a></html>'
    for name in ('index.html', 'index-app.html'):
        (root / name).write_text(redirect)
    (root / '404.html').write_text('<!doctype html><html lang="pt-BR"><meta charset="utf-8"><title>Página não encontrada</title><a href="' + prefix + '/home/">Voltar à biblioteca</a></html>')
    (root / 'privacidade.html').write_text('<!doctype html><html lang="pt-BR"><meta charset="utf-8"><title>Leitura e privacidade</title><h1>Leitura e privacidade</h1><p>Este site guarda preferências e progresso apenas no navegador. Comentários e reações estão desativados nesta edição.</p><p>Responsável pela publicação: ' + escape(site['autor']) + '.</p></html>')
    (root / '.nojekyll').touch()
    if site['origem']:
        # Somente um domínio próprio exige CNAME. github.io usa a configuração do Pages.
        hostname = urlsplit(site['origem']).hostname
        if not hostname.endswith('.github.io'):
            (root / 'CNAME').write_text(hostname + '\n')
    save_json(root / 'configuracao-site.json', {'schema_version': 2, 'url_publica': site['origem'], 'caminho_base': prefix,
              'comentarios': {'ativado': False, 'obras': True, 'capitulos': True, 'api_url': '', 'turnstile_site_key': ''},
              'reacoes': {'ativado': False, 'obras': True, 'capitulos': True}})
    save_json(root / 'obras.json', {'schema_version': 2, 'obras': folders})
    colors = {'verde': '#8be8d0', 'violeta': '#c4b5fd', 'ambar': '#f2cb88'}
    with (root / 'css-global/global.css').open('a') as stream:
        stream.write('\n:root{--acento-primario:' + colors[site['tema']] + ';--acento-hover:#eeeaf9;}')
    for order, (folder, work) in enumerate(zip(folders, works), 1):
        (root / folder / 'dados').mkdir(parents=True)
        (root / folder / 'writed').mkdir()
        for name in ('index-obra.html', 'leitor.html'):
            (root / folder / name).write_text(brand_html((BASE / 'recursos/modelos' / name).read_text(), site))
        collection = crud.resources(work)
        primary = (work.get('personagem') or {}).get('id', '')
        info = {'schema_version': 2, 'id_obra': work['id'], 'slug': folder, 'titulo': work['titulo'], 'autor': site['autor'], 'idioma': 'pt-BR',
                'endereco_publico': work['endereco'], 'enderecos_anteriores':work.get('enderecos_anteriores',[]), 'sinopse': work['sinopse'], 'estado_obra': 'em_andamento',
                'estado_publicacao': 'publicado' if work['publica'] else 'rascunho', 'ordem_catalogo': order, 'formato': 'Publicação seriada',
                'generos': [], 'temas': [], 'tags': [], 'avisos_conteudo': [],
                'capa': {'src': prefix + '/assets/imagens/marca.svg', 'alt': 'Capa de ' + work['titulo']},
                'personagens': [{'id': r['id'], 'nome': r['nome'], 'descricao_curta': r.get('descricao', ''), 'aliases': []} for r in collection if r['tipo'] == 'personagem'],
                'lore': [{'id': r['id'], 'titulo': r['nome'], 'descricao': r.get('descricao', '')} for r in collection if r['tipo'] == 'verbete']}
        save_json(root / folder / 'dados/info-obra.json', info)
        chapters = []
        for document in work['documentos']:
            path = 'writed/' + document['id'] + '.html'
            (root / folder / path).write_text(render_document(document, primary))
            chapters.append({'id': document['id'], 'titulo': document['titulo'], 'numero': str(document['numero']),
                             'ordem': len(chapters) + 1, 'numero_url': document['numero'], 'arquivo': path,
                             'estado_publicacao': 'publicado' if document['publico'] else 'rascunho', 'publicado_em': date.today().isoformat(),
                             'personagens_ids': [], 'cenas': [{'id': 'guia-' + document['id'], 'ancora': document.get('ancora_cena', 'cena-' + document['id']),
                             'titulo_cena': document['titulo'], 'visibilidade': 'publica', 'relevancia': 'nao_avaliada',
                             'avaliacao_conteudo': 'pendente', 'avisos_conteudo': [],
                             'descricao': ''}]})
            if document.get('cenas'):
                chapters[-1]['cenas']=[{'id':'guia-'+s['id'],'ancora':s['id'],'titulo_cena':s['title'],'visibilidade':'publica','relevancia':'nao_avaliada','avaliacao_conteudo':'pendente','avisos_conteudo':[],'descricao':''} for s in document['cenas']]
        save_json(root / folder / 'dados/editorial.json', {'schema_version': 2, 'proximo_numero_url':work.get('proximo_numero_url',max((d['numero'] for d in work['documentos']),default=0)+1), 'capitulos': chapters})

def build(data, destination):
    """Geração isolada e publicação local do diretório completo somente após sucesso."""
    data = deepcopy(data)
    if 'obras' in data:
        if data.get('obra') is not None or not isinstance(data['obras'], list) or len(data['obras']) > 100:
            raise ValueError('Catálogo inválido: use uma lista de até 100 Obras.')
        validate({**data, 'obra': None})
        ids, addresses = set(), set()
        for work in data['obras']:
            validate({**data, 'obra': work})
            if not re.fullmatch(r'[a-zA-Z0-9_-]+', work['id']) or work['id'] in ids or work['endereco'] in addresses:
                raise ValueError('Obra ou endereço repetidos no catálogo.')
            ids.add(work['id']); addresses.add(work['endereco'])
    else:
        data = validate(data)
    destination = Path(destination)
    if destination.exists():
        raise ValueError('A pasta de geração precisa ser nova.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix='.geracao-') as temporary:
        root = Path(temporary)
        materialize(data, root)
        gerar(root)
        output = root / 'site'
        prefix = data['site']['caminho_base']
        # Recursos com endereço absoluto em modelos legados também respeitam o caminho base.
        if prefix:
            for path in output.rglob('*.html'):
                content = path.read_text()
                content = re.sub(r'((?:href|src|action)=")(/(?!/)[^\"]*)', lambda m: m[1] + (m[2] if m[2] == prefix or m[2].startswith(prefix + '/') else prefix + m[2]), content)
                path.write_text(content)
        files = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(output.rglob('*')) if p.is_file()}
        # O recibo público descreve apenas arquivos públicos; nunca guarda o retrato de autoria.
        save_json(output / 'inde-publicacao.json', {'schema_version': 1, 'motor': 'inde-publicacao-laboratorio-0.6', 'arquivos': files})
        os.replace(output, destination)
    return destination


def engine_revision():
    files = [BASE / name for name in ('publicacao.py', 'crud.py', 'ponte_inde.py', 'identidade_publica.py') if (BASE / name).is_file()]
    for name in ('motor', 'recursos'):
        files.extend(p for p in (BASE / name).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    return digest({str(p.relative_to(BASE)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)})


def build_native_comic(payload, destination):
    """Materializa arquivos públicos preparados em C++; não interpreta autoria comic."""
    import base64
    from pathlib import PurePosixPath
    if payload.get('version') != 1 or payload.get('media') != 'comic':
        raise ValueError('Versão ou mídia do retrato comic incompatível.')
    site = payload['site']
    if site['removed']:
        raise ValueError('Site retirado.')
    data = new_project()
    data['site'] = {target: site[key] for target, key in (
        ('nome','name'),('autor','author'),('descricao','description'),
        ('tema','theme'),('origem','origin'),('caminho_base','base_path'))}
    data['destino'] = {'tipo':site['destination'],'repositorio':site['repository'],'branch':site['branch']}
    validate(data)
    encoded = payload.get('files')
    if not isinstance(encoded, dict) or not 7 <= len(encoded) <= 2000:
        raise ValueError('Lista de arquivos públicos comic inválida.')
    files, total = {}, 0
    for name, value in encoded.items():
        if not isinstance(name, str) or not isinstance(value, str):
            raise ValueError('Arquivo público inválido.')
        path = PurePosixPath(name)
        if (path.is_absolute() or any(p in ('', '.', '..') for p in name.split('/'))
                or not re.fullmatch(r'[A-Za-z0-9_./-]+', name)
                or path.suffix not in ('.html','.css','.js','.png','.jpg','.webp','.gif','.ttf','.otf','.woff','.woff2') and name != '.nojekyll'):
            raise ValueError('Caminho público comic inválido: ' + name)
        if len(value) > 45 * 1024 * 1024:
            raise ValueError('Arquivo público comic muito extenso.')
        raw = base64.b64decode(value, validate=True)
        total += len(raw)
        if total > 160 * 1024 * 1024:
            raise ValueError('Pacote comic muito extenso.')
        files[name] = raw
    if not {'index.html','home/index.html','.nojekyll'}.issubset(files):
        raise ValueError('Faltam as entradas públicas do comic.')
    destination = Path(destination)
    if destination.exists():
        raise ValueError('A pasta de geração precisa ser nova.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix='.comic-') as temporary:
        output = Path(temporary) / 'site'
        output.mkdir()
        for name, raw in files.items():
            path = output / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(raw)
        hostname = urlsplit(data['site']['origem']).hostname
        if hostname and not hostname.endswith('.github.io'):
            (output / 'CNAME').write_text(hostname + '\n')
        hashes = {str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in sorted(output.rglob('*')) if p.is_file()}
        save_json(output / 'inde-publicacao.json', {
            'schema_version': 1, 'motor': 'inde-comic-cpp-01', 'media': 'comic', 'arquivos': hashes})
        os.replace(output, destination)
    package_public(destination, destination.parent / 'site-publico.zip')
    save_json(destination.parent / 'previa.json', {'caminho_base': data['site']['caminho_base']})
    return {'site': str(destination.resolve()), 'avisos': []}


def build_native_edition(source, destination):
    import ponte_inde
    payload = json.loads(Path(source).read_text())
    protocol = payload.get('protocol')
    if protocol == 'inde.web.comic':
        return build_native_comic(payload, destination)
    if protocol not in ('inde.web.edition', 'inde.web.catalog') or payload.get('version') not in (1,2):
        raise ValueError('Retrato de Edição ou catálogo incompatível.')
    site = payload['site']
    if site['removed']:
        raise ValueError('Site ou Edição retirados ou incompatíveis.')
    data = new_project()
    data['site'] = {target: site[key] for target, key in (
        ('nome', 'name'), ('autor', 'author'), ('descricao', 'description'),
        ('tema', 'theme'), ('origem', 'origin'), ('caminho_base', 'base_path'))}
    data['destino'] = {'tipo': site['destination'], 'repositorio': site['repository'], 'branch': site['branch']}
    items = payload['editions'] if protocol == 'inde.web.catalog' else [payload]
    if not isinstance(items, list) or len(items) > 100:
        raise ValueError('Catálogo inválido.')
    if protocol == 'inde.web.catalog' and [item['edition']['id'] for item in items] != site['catalog']:
        raise ValueError('O catálogo não corresponde às escolhas do Site.')
    if (payload['version']==2)!=any(item['snapshot'].get('public_identity') is not None for item in items):raise ValueError('Versão de produção incompatível com o mapa público.')
    works, diagnostics, ids, projects = [], [], set(), set()
    for item in items:
        edition, snapshot = item['edition'], item['snapshot']
        projects.add(snapshot['project_id'])
        if len(projects) > 1:
            raise ValueError('O catálogo reúne fontes de Projetos distintos.')
        if edition['removed'] or edition['site_id'] != site['id']:
            raise ValueError('Site ou Edição retirados ou incompatíveis.')
        if edition['work_id'] != snapshot['work']['id'] or edition['structure_id'] != snapshot['structure_id']:
            raise ValueError('As fontes não correspondem à Edição.')
        if edition['id'] in ids:
            raise ValueError('Edição repetida no catálogo.')
        ids.add(edition['id'])
        try:
            work, notes = ponte_inde.project_work(snapshot)
        except ValueError as error:
            raise ValueError('Edição ' + edition['title'] + ': ' + str(error)) from error
        if snapshot.get('public_identity') is not None and work['endereco']!=edition['slug']:raise ValueError('Endereço da Edição diverge da identidade preservada.')
        work['endereco'] = edition['slug']
        works.append(work); diagnostics.extend(notes)
    if protocol == 'inde.web.catalog':
        data['obras'] = works
    else:
        data['obra'] = works[0]
    output = build(data, destination)
    package_public(output, Path(output).parent / 'site-publico.zip')
    save_json(Path(output).parent / 'previa.json', {'caminho_base': site['base_path']})
    return {'site': str(output.resolve()), 'avisos': diagnostics}


def serve_native_edition(directory, open_browser=True, control=None):
    import ambiente
    import signal
    token = secrets.token_urlsafe(32)
    directory = Path(directory).resolve()
    receipt = json.loads((directory / 'inde-publicacao.json').read_text())
    for name, expected in receipt['arquivos'].items():
        path = (directory / name).resolve()
        if not path.is_relative_to(directory) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('A edição pública mudou: ' + name)
    # The base path is public; a tiny separate receipt avoids reading any project data.
    prefix = json.loads((directory.parent / 'previa.json').read_text())['caminho_base']
    receipt_hash = hashlib.sha256((directory / 'inde-publicacao.json').read_bytes()).hexdigest()
    class PreparedPreview(PreviewHandler):
        def do_GET(self):
            if self.path == '/__inde_previa/' + token:
                raw = json.dumps({'token': token, 'receipt_sha256': receipt_hash}).encode()
                self.send_response(200); self.send_header('Content-Type', 'application/json'); self.send_header('Content-Length', str(len(raw))); self.end_headers(); self.wfile.write(raw)
                return
            super().do_GET()

        def do_HEAD(self):
            path = urlsplit(self.path)
            if self.prefix:
                if not path.path.startswith(self.prefix + '/'):
                    self.send_error(404); return
                self.path = path.path[len(self.prefix):]
            super().do_HEAD()

        def send_head(self):
            target = Path(self.translate_path(self.path)).resolve()
            if target.is_dir():
                target = target / 'index.html'
            if not target.is_relative_to(directory) or str(target.relative_to(directory)) not in receipt['arquivos']:
                self.send_error(404)
                return None
            return super().send_head()

    server = ThreadingHTTPServer(('127.0.0.1', 0), partial(PreparedPreview, directory=str(directory), prefix=prefix))
    url = f'http://127.0.0.1:{server.server_port}{prefix}/home/'
    print(url, flush=True)
    control = Path(control) if control else None
    if control:
        save_json(control, {'version': 1, 'pid': os.getpid(), 'token': token, 'url': url, 'receipt_sha256': receipt_hash})
    if open_browser:
        def open_default():
            try:
                ambiente.open_browser(url, directory.parent / 'navegador.log')
            except (OSError, RuntimeError) as error:
                print('[Prévia] ' + str(error), file=sys.stderr, flush=True)
        threading.Thread(target=open_default, daemon=True).start()
    def terminate(*_):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, terminate)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        if control and control.exists():
            try:
                if json.loads(control.read_text()).get('token') == token:
                    control.unlink()
            except (OSError, ValueError):
                pass


def reopen_native_preview(control):
    import ambiente
    import time
    import urllib.request
    control = Path(control)
    for _ in range(60):
        if control.is_file():
            break
        time.sleep(.05)
    data = json.loads(control.read_text())
    url = urlsplit(data['url'])
    if data.get('version') != 1 or url.scheme != 'http' or url.hostname != '127.0.0.1' or not url.port or url.username or url.password or url.query or url.fragment:
        raise ValueError('Controle da prévia inválido.')
    request = f'http://127.0.0.1:{url.port}/__inde_previa/' + data['token']
    with urllib.request.build_opener(urllib.request.ProxyHandler({})).open(request, timeout=2) as response:
        check = json.load(response)
    if check != {'token': data['token'], 'receipt_sha256': data['receipt_sha256']}:
        raise ValueError('A prévia não corresponde à geração preparada.')
    ambiente.open_browser(data['url'], control.parent / 'navegador.log')


def package_public(output, target):
    with ZipFile(target, 'x', compression=ZIP_DEFLATED) as archive:
        for path in sorted(Path(output).rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(output))


class PreviewHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, directory, prefix='', **kwargs):
        self.prefix = prefix
        super().__init__(*args, directory=directory, **kwargs)

    def do_GET(self):
        path = urlsplit(self.path)
        if self.prefix:
            if not path.path.startswith(self.prefix + '/'):
                self.send_error(404)
                return
            self.path = path.path[len(self.prefix):] + ('?' + path.query if path.query else '')
        super().do_GET()

    def log_message(self, *_):
        pass


class Workshop:
    def __init__(self, directory):
        self.directory = Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=True)
        self.previews = {}
        self.lock = threading.RLock()

    def path(self, identifier):
        if not re.fullmatch(r'[a-f0-9-]{36}', identifier):
            raise ValueError('Identificador inválido.')
        return self.directory / identifier

    def read(self, identifier):
        data = json.loads((self.path(identifier) / 'projeto.json').read_text())
        result = {'projeto': data, 'revisao': digest(data), 'recursos': crud.resources(data.get('obra'))}
        if data.get('origem_inde'):
            result['caminho_inde'] = str(self.path(identifier) / 'origem-inde/Publicacao.inde')
        return result

    def create(self, example=False):
        data = new_project(example)
        folder = self.path(data['id'])
        folder.mkdir()
        save_json(folder / 'projeto.json', data)
        return self.read(data['id'])

    def save(self, data, revision, *, management=False):
        current = self.read(data['id'])
        if current['revisao'] != revision:
            raise ValueError('O projeto mudou em outra sessão. Recarregue antes de salvar.')
        if current['projeto'].get('origem_inde') and not management and (data.get('obra') != current['projeto'].get('obra') or data.get('origem_inde') != current['projeto'].get('origem_inde')):
            raise ValueError('O conteúdo pertence ao INDE. Altere o Projeto de origem e atualize o retrato.')
        validate(data)
        old_work, new_work = current['projeto'].get('obra'), data.get('obra')
        if old_work and new_work:
            if old_work['id'] != new_work['id']:
                raise ValueError('A identidade da Obra é permanente.')
            numbers = {d['id']: d['numero'] for d in old_work['documentos']}
            if any(d['id'] in numbers and numbers[d['id']] != d['numero'] for d in new_work['documentos']):
                raise ValueError('O número público de um capítulo é permanente.')
            reserved = max(old_work.get('proximo_numero_url', 1), max((n + 1 for n in numbers.values()), default=1))
            if not management and any(d['id'] not in numbers and d['numero'] < reserved for d in new_work['documentos']):
                raise ValueError('Este número de capítulo já foi reservado.')
            new_work['proximo_numero_url'] = max(reserved, max((d['numero'] + 1 for d in new_work['documentos']), default=1))
        if digest(data) != revision:
            save_json(self.path(data['id']) / 'historico' / (revision + '.json'), current['projeto'])
        save_json(self.path(data['id']) / 'projeto.json', data)
        return self.read(data['id'])

    def operate(self, identifier, revision, action, payload=None):
        current = self.read(identifier)
        if current['revisao'] != revision:
            raise ValueError('O projeto mudou. Recarregue antes da operação.')
        changed = crud.mutate(current['projeto'], action, payload or {})
        return self.save(changed, revision, management=True)

    def delete_site(self, identifier, revision):
        current = self.read(identifier)
        if current['revisao'] != revision:
            raise ValueError('O Site mudou. Reabra a remoção.')
        trash_id = identity()
        target = self.directory / 'lixeira' / trash_id
        target.mkdir(parents=True)
        save_json(target / 'manifesto.json', {'id': trash_id, 'site_id': identifier,
                  'nome': current['projeto']['site']['nome'], 'revisao': revision})
        os.replace(self.path(identifier), target / 'site')
        for key in list(self.previews):
            if key[0] == identifier:
                server = self.previews.pop(key)
                server.shutdown()
                server.server_close()
        return {'removido': identifier, 'recuperacao': trash_id}

    def trash_sites(self):
        return [json.loads(p.read_text()) for p in (self.directory / 'lixeira').glob('*/manifesto.json') if (p.parent / 'site/projeto.json').is_file()]

    def restore_site(self, trash_id):
        self.path(trash_id)  # Valida a identidade antes de compor o caminho.
        folder = self.directory / 'lixeira' / trash_id
        manifest = json.loads((folder / 'manifesto.json').read_text())
        source = folder / 'site'
        data = json.loads((source / 'projeto.json').read_text())
        if digest(data) != manifest['revisao'] or data['id'] != manifest['site_id']:
            raise ValueError('A cópia de recuperação foi modificada. Nenhum dado foi substituído.')
        target = self.path(manifest['site_id'])
        if target.exists():
            raise ValueError('Já existe um Site com esta identidade.')
        os.replace(source, target)
        if data.get('obra'):
            data['obra']['publica'] = False
        save_json(target / 'projeto.json', data)
        return self.read(data['id'])

    def native_available(self):
        return native_binary().is_file()

    def create_native(self):
        import ponte_inde
        if not self.native_available():
            raise ValueError('Compile a ponte nativa pelo script Preparar-ponte.sh antes de usar esta demonstração.')
        data = new_project()
        folder = self.path(data['id'])
        folder.mkdir()
        native = folder / 'origem-inde'
        process = subprocess.run([str(native_binary()), '--demo', str(native)], capture_output=True, text=True, timeout=90)
        if process.returncode:
            raise ValueError('A ponte INDE recusou a criação: ' + process.stderr[-1500:])
        snapshot = json.loads((native / 'retrato.json').read_text())
        data['obra'], diagnostics = ponte_inde.project_work(snapshot)
        data['origem_inde'] = {'protocolo': snapshot['protocol'], 'versao': snapshot['version'],
            'project_id': snapshot['project_id'], 'retrato_sha256': hashlib.sha256((native / 'retrato.json').read_bytes()).hexdigest(),
            'projeto_relativo': 'origem-inde/Publicacao.inde', 'avisos': diagnostics,
            'revisoes_documentos': {d['id']: d['revision'] for u in snapshot['units'] for d in u['documents']}}
        data['site'].update(nome='Biblioteca do INDE', autor='Autoria de demonstração INDE')
        validate(data)
        save_json(folder / 'projeto.json', data)
        return self.read(data['id'])

    def refresh_native(self, identifier, revision):
        import ponte_inde
        current = self.read(identifier)
        if current['revisao'] != revision or not current['projeto'].get('origem_inde'):
            raise ValueError('A origem ou a revisão do Projeto mudou. Reabra a edição.')
        folder = self.path(identifier)
        source = folder / 'origem-inde'

        def inventory():
            files = {}
            for path in sorted(source.rglob('*')):
                if path.is_symlink():
                    raise ValueError('A origem contém vínculo simbólico. A cópia de leitura foi recusada.')
                if path.is_file():
                    if path.name.endswith(('-wal', '-shm', '-journal')):
                        raise ValueError('Feche o Projeto no INDE antes de atualizar esta demonstração.')
                    files[str(path.relative_to(source))] = hashlib.sha256(path.read_bytes()).hexdigest()
            return files

        before = inventory()
        with tempfile.TemporaryDirectory(dir=folder, prefix='.retrato-') as temporary:
            copy = Path(temporary) / 'origem-copiada'
            shutil.copytree(source, copy)
            if inventory() != before:
                raise ValueError('O Projeto mudou durante a cópia. Feche-o e atualize novamente.')
            (copy / '.copia-para-projecao').write_text('Cópia descartável para leitura pelos serviços INDE.\n')
            process = subprocess.run([str(native_binary()), '--snapshot-copia', str(copy)], capture_output=True, text=True, timeout=90)
            if process.returncode:
                raise ValueError('A atualização foi recusada pelo INDE: ' + process.stderr[-1500:])
            if inventory() != before:
                raise ValueError('A origem mudou durante a projeção. O resultado não será aplicado.')
            raw = (copy / 'retrato.json').read_bytes()
        snapshot = json.loads(raw)
        work, diagnostics = ponte_inde.project_work(snapshot)
        data = current['projeto']
        previous = data['obra']
        by_id = {d['id']: d for d in previous['documentos']}
        work['publica'] = previous['publica']
        for document in work['documentos']:
            if document['id'] in by_id:
                document['numero'] = by_id[document['id']]['numero']
                document['publico'] = by_id[document['id']]['publico']
        data['obra'] = work
        data['origem_inde'].update(retrato_sha256=hashlib.sha256(raw).hexdigest(), avisos=diagnostics,
            revisoes_documentos={d['id']: d['revision'] for u in snapshot['units'] for d in u['documents']})
        receipt = folder / 'retratos' / (hashlib.sha256(raw).hexdigest() + '.json')
        if not receipt.exists():
            save_json(receipt, snapshot)
        return self.save(data, revision, management=True)

    def generate(self, identifier, revision):
        current = self.read(identifier)
        if current['revisao'] != revision:
            raise ValueError('O projeto mudou. Recarregue antes de gerar.')
        data = current['projeto']
        edition = digest({'revisao': revision, 'motor': engine_revision()})
        folder = self.path(identifier) / 'geracoes' / edition
        output = folder / 'site'
        if not output.exists():
            build(data, output)
        package = folder / 'site-publico.zip'
        if not package.exists():
            package_public(output, package)
        key = (identifier, edition)
        if key not in self.previews:
            server = ThreadingHTTPServer(('127.0.0.1', 0), partial(PreviewHandler, directory=str(output), prefix=data['site']['caminho_base']))
            threading.Thread(target=server.serve_forever, daemon=True).start()
            self.previews[key] = server
        port = self.previews[key].server_port
        return {'previa': f'http://127.0.0.1:{port}' + data['site']['caminho_base'] + '/home/',
                'pacote': '/pacote/' + identifier + '/' + edition,
                'destino': data['destino']['tipo'], 'revisao': revision, 'edicao': edition}

    def close(self):
        for server in self.previews.values():
            server.shutdown()
            server.server_close()


def handler_for(workshop, token):
    class Handler(SimpleHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def respond(self, value, status=200):
            body = encode(value).encode()
            self.send_response(status)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def valid_host(self):
            return self.headers.get('Host') == f'127.0.0.1:{self.server.server_port}'

        def do_GET(self):
            if not self.valid_host():
                self.send_error(403)
                return
            path = urlsplit(self.path).path
            if path in ('/', '/interface.css', '/interface.js'):
                name = {'/': 'index.html', '/interface.css': 'interface.css', '/interface.js': 'interface.js'}[path]
                content = (BASE / 'interface' / name).read_text().replace('__CSRF__', token).encode()
                self.send_response(200)
                self.send_header('Content-Type', {'/': 'text/html', '/interface.css': 'text/css', '/interface.js': 'text/javascript'}[path] + '; charset=utf-8')
                self.send_header('Cache-Control', 'no-store')
                self.end_headers()
                self.wfile.write(content)
                return
            try:
                with workshop.lock:
                    if path == '/api/projetos':
                        records = [workshop.read(p.parent.name) for p in workshop.directory.glob('*/projeto.json')]
                        self.respond(records)
                        return
                    if path == '/api/estado':
                        self.respond({'ponte_nativa': workshop.native_available(), 'sites_removidos': workshop.trash_sites()})
                        return
                    match = re.fullmatch(r'/pacote/([a-f0-9-]{36})/([a-f0-9]{64})', path)
                    if match:
                        package = workshop.path(match[1]) / 'geracoes' / match[2] / 'site-publico.zip'
                        content = package.read_bytes()
                        self.send_response(200)
                        self.send_header('Content-Type', 'application/zip')
                        self.send_header('Content-Disposition', 'attachment; filename="site-publico.zip"')
                        self.end_headers()
                        self.wfile.write(content)
                        return
                self.send_error(404)
            except (OSError, ValueError) as error:
                self.respond({'erro': str(error)}, 400)

        def do_POST(self):
            if not self.valid_host() or self.headers.get('X-INDE-Token') != token or self.headers.get('Origin', f'http://127.0.0.1:{self.server.server_port}') != f'http://127.0.0.1:{self.server.server_port}':
                self.respond({'erro': 'Solicitação local não autorizada.'}, 403)
                return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 0 < size <= 1_000_000:
                    raise ValueError('Solicitação vazia ou acima do limite.')
                request = json.loads(self.rfile.read(size))
                with workshop.lock:
                    path = urlsplit(self.path).path
                    if path == '/api/criar':
                        result = workshop.create(request.get('exemplo') is True)
                    elif path == '/api/criar-nativo':
                        result = workshop.create_native()
                    elif path == '/api/atualizar-inde':
                        result = workshop.refresh_native(request['id'], request['revisao'])
                    elif path == '/api/salvar':
                        result = workshop.save(request['projeto'], request['revisao'])
                    elif path == '/api/operar':
                        result = workshop.operate(request['id'], request['revisao'], request['acao'], request.get('dados'))
                    elif path == '/api/excluir-site':
                        result = workshop.delete_site(request['id'], request['revisao'])
                    elif path == '/api/restaurar-site':
                        result = workshop.restore_site(request['id'])
                    elif path == '/api/gerar':
                        result = workshop.generate(request['id'], request['revisao'])
                    else:
                        self.send_error(404)
                        return
                self.respond(result)
            except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
                self.respond({'erro': str(error)}, 400)
    return Handler


def main():
    parser = argparse.ArgumentParser(description='Demo funcional local — INDE Publicação')
    parser.add_argument('--dados', type=Path, default=BASE / 'dados')
    parser.add_argument('--porta', type=int, default=0)
    parser.add_argument('--sem-navegador', action='store_true')
    parser.add_argument('--gerar-exemplo', type=Path, help='Gera diretamente uma edição fictícia em uma pasta nova.')
    parser.add_argument('--gerar-edicao', type=Path, help='Consome um retrato preparado pelo INDE; nunca abre SQLite.')
    parser.add_argument('--destino', type=Path)
    parser.add_argument('--servir-edicao', type=Path)
    parser.add_argument('--controle-previa', type=Path)
    parser.add_argument('--reabrir-previa', type=Path)
    args = parser.parse_args()
    if args.gerar_edicao:
        if not args.destino:
            parser.error('--gerar-edicao exige --destino em uma pasta nova.')
        try:
            result = build_native_edition(args.gerar_edicao, args.destino)
            print(json.dumps(result, ensure_ascii=False))
        except (ValueError, KeyError, OSError) as error:
            parser.exit(1, str(error) + '\n')
        return
    if args.reabrir_previa:
        try:
            reopen_native_preview(args.reabrir_previa)
        except (OSError,ValueError,RuntimeError) as error:
            parser.exit(1,str(error)+"\n")
        return
    if args.servir_edicao:
        serve_native_edition(args.servir_edicao, not args.sem_navegador, args.controle_previa)
        return
    if args.gerar_exemplo:
        print(build(new_project(True), args.gerar_exemplo))
        return
    workshop = Workshop(args.dados)
    server = ThreadingHTTPServer(('127.0.0.1', args.porta), handler_for(workshop, secrets.token_urlsafe(32)))
    url = f'http://127.0.0.1:{server.server_port}/'
    print('INDE Publicação — laboratório local: ' + url, flush=True)
    print('Dados: ' + str(workshop.directory) + '\nEncerre com Ctrl+C.', flush=True)
    if not args.sem_navegador:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        workshop.close()


if __name__ == '__main__':
    main()
