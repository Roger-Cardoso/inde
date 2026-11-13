"""Freeze reviewed local adaptations; preserve the original P03c archive."""
import hashlib
import json
from pathlib import Path
import tarfile

BASE = Path(__file__).resolve().parent


def freeze():
    record = BASE / 'INTEGRACAO-INDE.json'
    identity = json.loads(record.read_text())
    original = BASE / identity['arquivo_original']
    if hashlib.sha256(original.read_bytes()).hexdigest() != identity['sha256_original']:
        raise ValueError('A origem P03c foi alterada.')
    root = BASE / 'fontes/inde-integrado-schema37'
    files = sorted(p for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    originals = {}
    with tarfile.open(original) as archive:
        for member in archive:
            if member.isfile():
                originals[member.name] = hashlib.sha256(archive.extractfile(member).read()).hexdigest()
    actual = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    archive = BASE / 'fontes/inde-integrado-schema37.tar.gz'
    temporary = archive.with_suffix('.tmp')
    with tarfile.open(temporary, 'w:gz') as target:
        for path in files:
            target.add(path, arcname=path.relative_to(root), recursive=False)
    temporary.replace(archive)
    identity.update(arquivo_integrado=archive.relative_to(BASE).as_posix(),
                    sha256_integrado=hashlib.sha256(archive.read_bytes()).hexdigest(),
                    arquivos_integrados=actual,
                    adaptacoes_locais=sorted(name for name in actual if actual[name] != originals.get(name)),
                    arquivos_originais_ausentes=sorted(set(originals) - set(actual)))
    record.write_text(json.dumps(identity, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'arquivos': len(actual), 'adaptacoes': identity['adaptacoes_locais'],
                      'sha256': identity['sha256_integrado']}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    freeze()
