"""Adaptação explícita do retrato C++ para a edição web. Não acessa SQLite."""
import hashlib
import re


def project_work(snapshot):
    if snapshot.get('protocol') != 'inde.web.snapshot' or snapshot.get('version') not in (1,2):
        raise ValueError('Protocolo de retrato INDE incompatível.')
    if snapshot.get('source_base') != '89b47f27e83d76e1ef61a46cb6c29b8c75c305c4' or snapshot.get('source_schema') not in (28, 37):
        raise ValueError('A base deste retrato não foi validada nesta entrega.')
    if (snapshot['version']==2)!=(snapshot.get('public_identity') is not None):raise ValueError('Versão do retrato diverge da presença do mapa público.')
    work = snapshot['work']
    if work.get('language') != 'pt-BR':
        raise ValueError('Este corte ainda não adapta outros idiomas do leitor.')
    resources = []
    for entity in snapshot['entities']:
        if entity['type'] != 'character':
            raise ValueError('Tipo de entidade ainda sem correspondência web: ' + entity['type'])
        resources.append({'id': entity['id'], 'tipo': 'personagem', 'nome': entity['name'], 'descricao': entity['summary']})
    character_ids = {r['id'] for r in resources}
    documents, seen = [], set()
    diagnostics = list(snapshot.get('omissions', []))
    for unit in snapshot['units']:
        if unit['type'] != 'chapter' or unit.get('parent_id') is not None or len(unit['documents']) != 1:
            raise ValueError('Este corte exige capítulos de nível superior com um Documento selecionado cada.')
        for source in unit['documents']:
            if source['id'] in seen:
                raise ValueError('Documento duplicado na seleção de produção.')
            seen.add(source['id'])
            text = source['text']
            if hashlib.sha256(text.encode('utf-8')).hexdigest() != source['text_sha256']:
                raise ValueError('O texto do retrato diverge de seu hash nativo.')
            if source.get('unsupported_layers'):
                raise ValueError('Há formatação ou camadas ainda sem correspondência nesta projeção. Nenhuma foi descartada.')
            anchors = {a['id']: a for a in source['anchors']}
            if len(anchors) != len(source['anchors']):
                raise ValueError('Âncora repetida no retrato.')
            semantics = source['semantics']
            if semantics.get('version') not in (2, 3) or semantics.get('relations'):
                raise ValueError('Versão ou relações semânticas ainda sem projeção web.')
            evidence = {a['id']: a for a in semantics['anchors']}
            document = {'formatacao':source.get('formatting',[]),'id': source['id'], 'titulo': unit['title'], 'numero': source.get('public_number', len(documents) + 1),
                        'publico': True, 'texto': text, 'perspectiva': False, 'anotacoes': [],
                        'unidade_editorial_id': unit['id'], 'revisao_origem': source['revision']}
            full_scene = [a for a in anchors.values() if a['start'] == 0 and a['end'] == len(text)]
            semantic_anchor_ids={a.get('anchor') for a in semantics['annotations']}
            scenes=[a for a in anchors.values() if a['id'] not in semantic_anchor_ids]
            document['cenas']=[{'id':a['id'],'start':a['start'],'end':a['end'],'title':a['label']} for a in scenes]
            full_scene=[a for a in full_scene if a['id'] not in semantic_anchor_ids]
            if len(full_scene) > 1:
                raise ValueError('Mais de uma âncora cobre a cena completa. Escolha explicitamente antes de projetar.')
            if full_scene:
                document['ancora_cena'] = full_scene[0]['id']
            for annotation in semantics['annotations']:
                if annotation['status'] == 'archived':
                    diagnostics.append('Marcação arquivada omitida: ' + annotation['id'])
                    continue
                if annotation['status'] != 'active' or annotation['version'] != 1:
                    raise ValueError('Estado ou versão de marcação não suportados.')
                if annotation.get('fictional_time') or annotation.get('dates') or annotation.get('opaque'):
                    raise ValueError('Marcação com dados adicionais ainda sem projeção.')
                fields = annotation['fields']

                def values(key):
                    return fields.get(key, {}).get('values', [])

                kind = annotation['type']
                if kind == 'inde.semantic.perspective':
                    if annotation['scope'] != 'document' or document['perspectiva']:
                        raise ValueError('Perspectiva por trecho ou ambígua ainda exige adaptação.')
                    mapping = {('third_person', 'internal'): 'third_person_limited',
                               ('third_person', 'omniscient'): 'third_person_omniscient',
                               ('first_person', 'internal'): 'first_person',
                               ('second_person', 'internal'): 'second_person'}
                    key = tuple(values('person') + values('focalization'))
                    if key not in mapping or len(values('viewpoint')) != 1 or values('viewpoint')[0] not in character_ids:
                        raise ValueError('A perspectiva não tem uma correspondência unívoca no leitor.')
                    if set(fields) - {'person', 'focalization', 'viewpoint', 'note'}:
                        raise ValueError('Campo de perspectiva sem correspondência.')
                    if values('note'):
                        diagnostics.append('Nota privada de perspectiva mantida somente no retrato local: ' + annotation['id'])
                    document.update(perspectiva=True, pov_id=values('viewpoint')[0], pov_type=mapping[key])
                    continue
                role = {'inde.semantic.dialogue': ('dialogue', 'speaker'), 'inde.semantic.thought': ('thought', 'thinker')}.get(kind)
                if role is None or annotation['scope'] != 'interval':
                    raise ValueError('Marcação ainda sem correspondência: ' + kind)
                if set(fields) - {role[1]}:
                    raise ValueError('Há campos adicionais de fala/pensamento ainda sem projeção.')
                if len(values(role[1])) != 1 or values(role[1])[0] not in character_ids:
                    raise ValueError('Falante ou pensador sem identidade válida.')
                anchor = anchors.get(annotation['anchor'])
                if not anchor or evidence.get(anchor['id'], {}).get('state') != 'resolved':
                    raise ValueError('Âncora sem resolução inequívoca.')
                start, end = anchor['start'], anchor['end']
                if not 0 <= start < end <= len(text):
                    raise ValueError('Intervalo Unicode inválido.')
                document['anotacoes'].append({'id': annotation['id'], 'tipo': role[0], 'entidade_id': values(role[1])[0],
                    'inicio': start, 'fim': end, 'trecho': text[start:end], 'ancora_origem': anchor['id']})
            documents.append(document)
    if not documents:
        raise ValueError('O retrato não seleciona nenhum Documento.')
    result={'id': work['id'], 'titulo': work['title'], 'sinopse': work['synopsis'], 'endereco': 'estacao-inde',
            'publica': True, 'personagem': None, 'recursos': resources, 'documentos': documents,
            'proximo_numero_url': max(d['numero'] for d in documents) + 1}
    import identidade_publica
    return identidade_publica.apply(result,snapshot), diagnostics
