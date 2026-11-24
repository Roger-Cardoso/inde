#!/usr/bin/env python3
"""Read-only evidence and validated source verification; no project is opened."""
import hashlib
import json
from pathlib import Path

here = Path(__file__).resolve().parent
root = here.parents[2]

def check(base, entries):
    for name, expected in entries.items():
        path = base / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit('Ausente ou divergente: ' + str(path))
    return len(entries)

manifest = dict((line.split('  ', 1)[1], line.split('  ', 1)[0])
                for line in (here / 'SHA256SUMS').read_text().splitlines())
print('Evidências:', check(here, manifest))
code = json.loads((here / 'codigo-validado.json').read_text())
print('Fontes do commit', code['commit'], ':', check(root, code['files']))
archives = json.loads((here / 'auditor-fontes.json').read_text())
print('Compactados:', check(root, {a['path']: a['sha256'] for a in archives['archives']}))
ci = json.loads((here / 'ci.json').read_text())
if ci['headSha'] != code['commit'] or ci['conclusion'] != 'success':
    raise SystemExit('CI não corresponde ao código validado ou não concluiu com sucesso.')
if len(ci['jobs']) != 26 or any(j['conclusion'] != 'success' for j in ci['jobs']):
    raise SystemExit('Trabalhos de CI incompletos ou sem sucesso.')
native = next((job for job in ci['jobs'] if job['name'] == 'native-objects'), None)
required = 'Verify real keyboard and mouse from project creation through normal exit'
if native is None or not any(step['name'] == required and step['conclusion'] == 'success'
                             for step in native['steps']):
    raise SystemExit('Percurso X11 obrigatório não concluiu com sucesso na CI.')
print('CI:', len(ci['jobs']), 'trabalhos aprovados, incluindo percurso X11')
print('OK — nenhuma gravação realizada; não substitui passe humano')
