"""Projeção pública conserva identidades do Literature sem alterar IDs do INDE."""
from copy import deepcopy
import re

def apply(work,snapshot):
    identity=snapshot.get('public_identity')
    if identity is None:return work
    if identity.get('version')!=1 or set(identity)!={'version','work','slug','folder','aliases','next_number','documents','entities','anchors','annotations'}:raise ValueError('Mapa público incompatível.')
    def mappings(kind,context=False):
        result={};seen=set()
        for entry in identity[kind]:
            native,source=entry['native'],entry['source'];prefix=entry['document']+':' if context else ''
            if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,199}',source) or prefix+native in result or prefix+source in seen:raise ValueError('Identidade pública inválida ou repetida.')
            result[prefix+native]=source;seen.add(prefix+source)
        return result
    docs=mappings('documents');entities=mappings('entities');anchors=mappings('anchors',True);annotations=mappings('annotations',True)
    result=deepcopy(work);result['id']=identity['work'];result['pasta_publica']=identity['folder'];result['endereco']=identity['slug'];result['enderecos_anteriores']=identity['aliases'];result['proximo_numero_url']=max(identity['next_number'],result['proximo_numero_url'])
    for resource in result['recursos']:resource['id']=entities.get(resource['id'],resource['id'])
    for doc in result['documentos']:
        native=doc['id'];doc['id']=docs.get(native,native)
        reserved=next((entry['number'] for entry in identity['documents'] if entry['native']==native),None)
        if reserved is not None and doc['numero']!=reserved:raise ValueError('Número de URL diverge da reserva importada.')
        if 'pov_id' in doc:doc['pov_id']=entities.get(doc['pov_id'],doc['pov_id'])
        if 'ancora_cena' in doc:doc['ancora_cena']=anchors.get(native+':'+doc['ancora_cena'],doc['ancora_cena'])
        for scene in doc.get('cenas',[]):scene['id']=anchors.get(native+':'+scene['id'],scene['id'])
        for annotation in doc['anotacoes']:
            annotation['id']=annotations.get(native+':'+annotation['id'],annotation['id']);annotation['entidade_id']=entities.get(annotation['entidade_id'],annotation['entidade_id'])
            annotation['ancora_origem']=anchors.get(native+':'+annotation['ancora_origem'],annotation['ancora_origem'])
    if len({d['id'] for d in result['documentos']})!=len(result['documentos']) or len({e['id'] for e in result['recursos']})!=len(result['recursos']):raise ValueError('Identidade nova colide com uma identidade pública preservada.')
    return result
