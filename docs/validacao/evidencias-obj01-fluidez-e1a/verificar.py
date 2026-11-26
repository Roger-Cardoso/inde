#!/usr/bin/env python3
"""Read-only evidence and validated source verification; no project is opened."""
import hashlib
import json
import re
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
local = json.loads((here / 'resultados.json').read_text())
local_log = (here / 'python-99.txt').read_text()
if (local['commit'] != code['commit'] or local['result'] != 'pass'
        or local['tests'] != 99 or local['real_input_cases'] != 2
        or not re.search(r'Ran 99 tests in [\d.]+s\n\nOK', local_log)):
    raise SystemExit('Resultado local não comprova a suíte final declarada.')
fluency = json.loads((here / 'fluency.json').read_text())
if not fluency['normal_exit'] or not fluency['saved_text_exact']:
    raise SystemExit('Percurso de fluidez sem salvamento exato ou saída normal.')
for count, mixed in [(20, False), (200, False), (200, True)]:
    if len([s for s in fluency['samples'] if s['stage'] == 'delete'
            and s['paragraphs'] == count and s['mixed'] == mixed]) != 3:
        raise SystemExit('Amostras de exclusão incompletas.')
integrity = json.loads((here / 'integridade-projeto-teste.json').read_text())['databases']
if len(integrity) != 3 or any(d['integrity'] != 'ok' or d['foreign_key_violations'] for d in integrity):
    raise SystemExit('Conferência dos bancos sintéticos incompleta.')
sanitizer = json.loads((here / 'sanitizer-live.json').read_text())
if sanitizer['exit_code'] != 0 or sanitizer['source_commit'] != code['commit']:
    raise SystemExit('Prova de memória do vínculo vivo não corresponde ao código final.')
print('Local: 99 testes, três perfis de exclusão, três bancos íntegros e prova de memória')
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
log = (here / 'ci-nativo-testes.txt').read_text()
if 'test_empty_paragraph_fluency_and_save' not in log or 'Ran 2 tests' not in log:
    raise SystemExit('Log nativo não comprova os dois percursos de entrada real.')
print('CI:', len(ci['jobs']), 'trabalhos aprovados, incluindo os dois percursos X11')
print('OK — nenhuma gravação realizada; não substitui passe humano')
