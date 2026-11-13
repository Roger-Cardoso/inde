"""Empacota somente os componentes do laboratório, sem dados nem credenciais."""
import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED

BASE = Path(__file__).resolve().parent
MANIFEST = BASE / 'metadata/MANIFESTO-FONTES.json'


def package(target):
    manifest = json.loads(MANIFEST.read_text())
    paths = [MANIFEST, *(BASE / name for name in manifest['arquivos'])]
    for name, expected in manifest['arquivos'].items():
        path = (BASE / name).resolve()
        if not path.is_relative_to(BASE) or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Arquivo diferente do manifesto: ' + name)
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(target, 'x', ZIP_DEFLATED) as archive:
        for path in sorted(paths):
            archive.write(path, Path('INDE-0.2.0') / path.relative_to(BASE))
    checksum = hashlib.sha256(target.read_bytes()).hexdigest()
    print(str(target.resolve()))
    print('SHA-256: ' + checksum)
    return checksum


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('destino', type=Path)
    package(parser.parse_args().destino)
