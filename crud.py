"""Gestão do laboratório: identidade permanente, vínculos e recuperação local."""
from copy import deepcopy
from datetime import datetime, timezone
from uuid import uuid4


def resources(work):
    if not work:
        return []
    primary = work.get('personagem')
    result = ([{**primary, 'tipo': 'personagem'}] if primary else []) + deepcopy(work.get('recursos', []))
    legacy = work['id'] + '-verbete'
    if not any(r['id'] == legacy for r in result) and any(a.get('alvo') == legacy for d in work['documentos'] for a in d.get('anotacoes', [])):
        result.append({'id': legacy, 'tipo': 'verbete', 'nome': 'Estação das marés', 'descricao': 'Uma estação fictícia que abre quando o mar recua.'})
    return result


def uses(work, identifier):
    result = []
    primary = (work.get('personagem') or {}).get('id', '')
    for doc in work['documentos']:
        if doc.get('perspectiva') and doc.get('pov_id', primary) == identifier:
            result.append(doc['titulo'] + ': ponto de vista')
        for annotation in doc.get('anotacoes', []):
            target = annotation.get('alvo') if annotation['tipo'] == 'glossary' else annotation.get('entidade_id', primary)
            if target == identifier:
                result.append(doc['titulo'] + ': ' + annotation['tipo'])
    return result


def new_work():
    return {'id': str(uuid4()), 'titulo': 'Minha Obra', 'endereco': 'minha-obra',
            'sinopse': 'Apresentação da sua história.', 'publica': False,
            'personagem': None, 'recursos': [], 'documentos': [], 'proximo_numero_url': 1}


def mutate(data, action, payload):
    data = explicit_references(deepcopy(data))
    work = data.get('obra')
    if data.get('origem_inde') and action not in ('excluir_obra',):
        raise ValueError('Esta Obra é uma projeção do INDE. Faça a alteração no Projeto de origem e atualize o retrato.')
    trash = data.setdefault('lixeira', [])

    def discard(kind, value):
        trash.append({'id': str(uuid4()), 'tipo': kind, 'obra_id': work['id'] if work else None,
                      'valor': value, 'removido_em': datetime.now(timezone.utc).isoformat()})

    if action == 'criar_obra':
        if work is not None:
            raise ValueError('Este corte admite uma Obra por Site. Remova a atual antes de criar outra.')
        data['obra'] = new_work()
    elif action == 'excluir_obra':
        if not work:
            raise ValueError('Não há Obra para remover.')
        discard('obra', {'obra': work, 'origem_inde': data.pop('origem_inde', None)})
        data['obra'] = None
    elif action == 'criar_capitulo':
        if not work:
            raise ValueError('Crie uma Obra primeiro.')
        number = max(work.get('proximo_numero_url', 1), max((d['numero'] + 1 for d in work['documentos']), default=1))
        work['documentos'].append({'id': str(uuid4()), 'titulo': 'Novo capítulo', 'numero': number,
                                  'publico': False, 'texto': '', 'perspectiva': False, 'anotacoes': []})
        work['proximo_numero_url'] = number + 1
    elif action == 'excluir_capitulo':
        selected = next((d for d in work['documentos'] if d['id'] == payload['id']), None)
        if selected is None:
            raise ValueError('Capítulo não encontrado.')
        work['proximo_numero_url'] = max(work.get('proximo_numero_url', 1), selected['numero'] + 1)
        discard('capitulo', selected)
        work['documentos'] = [d for d in work['documentos'] if d['id'] != selected['id']]
        if not any(d['publico'] and d['texto'].strip() for d in work['documentos']):
            work['publica'] = False
    elif action == 'criar_recurso':
        kind = payload['tipo']
        if kind not in ('personagem', 'verbete'):
            raise ValueError('Tipo de recurso não suportado.')
        work['recursos'] = resources(work)
        work['personagem'] = None
        work['recursos'].append({'id': str(uuid4()), 'tipo': kind, 'nome': 'Novo personagem' if kind == 'personagem' else 'Novo verbete', 'descricao': ''})
        # Referências legadas ao personagem principal passam a ser explícitas.
        explicit_references(data)
    elif action == 'excluir_recurso':
        referenced = uses(work, payload['id'])
        if referenced:
            raise ValueError('Recurso em uso: ' + '; '.join(referenced))
        item = next((r for r in resources(work) if r['id'] == payload['id']), None)
        if item is None:
            raise ValueError('Recurso não encontrado.')
        discard('recurso', item)
        work['recursos'] = [r for r in resources(work) if r['id'] != item['id']]
        work['personagem'] = None
    elif action == 'restaurar_item':
        item = next((r for r in trash if r['id'] == payload['id']), None)
        if not item:
            raise ValueError('Item de recuperação não encontrado.')
        kind, value = item['tipo'], deepcopy(item['valor'])
        if kind == 'obra':
            if work is not None:
                raise ValueError('O Site já possui uma Obra. Remova-a antes de restaurar esta.')
            data['obra'] = value['obra']
            data['obra']['publica'] = False
            if value.get('origem_inde'):
                data['origem_inde'] = value['origem_inde']
        else:
            if not work or work['id'] != item['obra_id']:
                raise ValueError('Restaure primeiro a Obra à qual o item pertence.')
            if kind == 'capitulo':
                if any(d['id'] == value['id'] or d['numero'] == value['numero'] for d in work['documentos']):
                    raise ValueError('Identidade ou número do capítulo já ocupado.')
                value['publico'] = False
                work['documentos'].append(value)
                work['documentos'].sort(key=lambda d: d['numero'])
            elif kind == 'recurso':
                if any(r['id'] == value['id'] for r in resources(work)):
                    raise ValueError('Identidade de recurso já ocupada.')
                work['recursos'] = resources(work) + [value]
                work['personagem'] = None
        data['lixeira'] = [r for r in trash if r['id'] != item['id']]
    else:
        raise ValueError('Operação de gestão desconhecida.')
    return data


def explicit_references(data):
    work = data.get('obra')
    if not work:
        return data
    primary = (work.get('personagem') or {}).get('id')
    if primary:
        for doc in work['documentos']:
            if doc.get('perspectiva'):
                doc.setdefault('pov_id', primary)
            for annotation in doc.get('anotacoes', []):
                if annotation['tipo'] in ('dialogue', 'thought'):
                    annotation.setdefault('entidade_id', primary)
    return data
