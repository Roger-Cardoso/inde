"""Assinatura das fontes para distinguir edição, validação e geração local."""
from functools import lru_cache
from pathlib import Path
import hashlib
import json

@lru_cache(maxsize=2048)
def _hash(caminho,mtime,tamanho):
    return hashlib.sha256(Path(caminho).read_bytes()).digest()

def assinatura_fontes(root):
    root=Path(root);registro=json.loads((root/'obras.json').read_text(encoding='utf-8'))
    pastas=['home','css-global','js-global','assets',*registro['obras']]
    arquivos={root/n for n in ('configuracao-site.json','scripts/configuracao_site.py','scripts/enderecos.py','obras.json','index.html','index-app.html','404.html','privacidade.html','CNAME','.nojekyll','scripts/gerar_site.py','scripts/exportar_offline.py','scripts/contrato_semantico.py','scripts/estado_publicacao.py','scripts/hierarquia.py','scripts/personagens.py','scripts/identidade_semantica.py') if (root/n).is_file()}
    for pasta in pastas:
        base=root/pasta
        if not base.resolve().is_relative_to(root.resolve()):raise ValueError('Pasta de fontes fora do projeto.')
        if not base.is_dir():continue
        for p in base.rglob('*'):
            if not p.is_file() or 'autoria' in p.relative_to(base).parts or p.suffix in {'.py','.pyc','.narrproj'} or '__pycache__' in p.parts:continue
            if not p.resolve().is_relative_to(root.resolve()):raise ValueError('Arquivo de fontes fora do projeto.')
            arquivos.add(p)
    h=hashlib.sha256()
    for p in sorted(arquivos):
        stat=p.stat();h.update(str(p.relative_to(root)).encode());h.update(b'\0');h.update(_hash(str(p.resolve()),stat.st_mtime_ns,stat.st_size))
    return h.hexdigest()

def assinatura_gerada(root):
    try:return json.loads((Path(root)/'site/geracao.json').read_text(encoding='utf-8')).get('fontes')
    except (OSError,ValueError):return None
