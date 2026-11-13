"""Confere a fonte aceita antes de reutilizá-la na compilação do laboratório."""
import hashlib
import json
from pathlib import Path
import tarfile
import argparse

BASE = Path(__file__).resolve().parent


def accepted_archive():
    identity = json.loads((BASE / 'metadata/BASE-INDE.json').read_text())
    archive = BASE / identity['arquivo_fonte_local']
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    if checksum != identity['arquivo_fonte_sha256']:
        raise ValueError('A fonte arquivada diverge da base aceita.')
    return identity, archive, checksum


def extract():
    # Validate the pinned archive before writing any extracted file.
    _, archive, _ = accepted_archive()
    root = BASE / 'fontes/inde-schema28'
    root.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        source.extractall(root, filter='data')
    return verify()


def verify():
    identity, archive, checksum = accepted_archive()
    root = BASE / 'fontes/inde-schema28'
    count = 0
    with tarfile.open(archive) as source:
        for member in source:
            if not member.isfile():
                continue
            path = (root / member.name).resolve()
            if not path.is_relative_to(root.resolve()) or not path.is_file():
                raise ValueError('Arquivo ausente ou fora da base: ' + member.name)
            original = source.extractfile(member).read()
            if hashlib.sha256(path.read_bytes()).digest() != hashlib.sha256(original).digest():
                raise ValueError('Arquivo da base foi alterado: ' + member.name)
            count += 1
    return {'commit': identity['commit'], 'arquivo_sha256': checksum, 'arquivos_conferidos': count, 'resultado': 'ok'}


def verify_integrated(extract_source=False):
    """Validate the packaged integrated source independently of its old location."""
    identity = json.loads((BASE / 'metadata/INTEGRACAO-INDE.json').read_text())
    archive = BASE / identity['arquivo_integrado']
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    if checksum != identity['sha256_integrado']:
        raise ValueError('A fonte integrada arquivada diverge do manifesto.')
    root = BASE / 'fontes/inde-integrado-schema37'
    if extract_source:
        root.mkdir(parents=True, exist_ok=True)
        with tarfile.open(archive) as source:
            source.extractall(root, filter='data')
    actual = {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
              for p in root.rglob('*') if p.is_file() and '__pycache__' not in p.parts}
    if actual != identity['arquivos_integrados']:
        raise ValueError('O inventário ou conteúdo da fonte integrada diverge do manifesto.')
    return {'schema': identity['schema'], 'arquivos_conferidos': len(actual),
            'arquivo_sha256': checksum, 'resultado': 'ok'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--extrair', action='store_true')
    parser.add_argument('--integrado', action='store_true')
    args = parser.parse_args()
    result = verify_integrated(args.extrair) if args.integrado else extract() if args.extrair else verify()
    print(json.dumps(result, ensure_ascii=False, indent=2))
