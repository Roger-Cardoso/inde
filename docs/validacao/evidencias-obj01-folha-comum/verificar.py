#!/usr/bin/env python3
"""Conferência somente de leitura. --locais inclui demos preservadas neste Work."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--locais', action='store_true')
args = parser.parse_args()
evidence = Path(__file__).resolve().parent
root = evidence.parents[2]


def check(base, entries):
    for name, expected in entries.items():
        path = base / name
        if not path.is_file():
            raise SystemExit(f'Ausente: {path}')
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit(f'Hash divergente: {path}')
    return len(entries)


manifest = {}
for line in (evidence / 'SHA256SUMS').read_text().splitlines():
    digest, name = line.split('  ', 1)
    manifest[name] = digest
print('Evidências:', check(evidence, manifest))
code = json.loads((evidence / 'codigo-validado.json').read_text())
print('Arquivos do commit', code['commit'], ':', check(root, code['files']))
archives = json.loads((evidence / 'arquivos-originais.json').read_text())
print('Compactados originais:', check(root, archives))
if args.locais:
    preserved = json.loads((evidence / 'preservados-inicio.json').read_text())
    print('Arquivos anteriores preservados:', check(root, preserved))
    demo = json.loads((evidence / 'integridade-demo.json').read_text())
    print('Bancos da demonstração:', check(root / demo['project'], {
        name: data['sha256'] for name, data in demo['databases'].items()
    }))
    print('Executável local:', check(root, {
        code['binary']['path']: code['binary']['sha256']
    }))
print('OK — nenhuma gravação realizada')
