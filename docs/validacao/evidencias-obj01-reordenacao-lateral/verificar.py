#!/usr/bin/env python3
"""Read-only verification of recorded evidence and matching source files."""
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
result = json.loads((here / 'resultados.json').read_text())
if result['commit'] != code['commit'] or result['result'] != 'pass' or result['tests'] != 100:
    raise SystemExit('Resultado local não corresponde ao código final.')
if not re.search(r'Ran 100 tests in [\d.]+s\n\nOK', (here / 'python-100.txt').read_text()):
    raise SystemExit('Log local não comprova os 100 testes.')
for name in ['referencia.json', 'reordering.json']:
    data = json.loads((here / name).read_text())
    if not data['normal_exit'] or not data['saved_text_exact']:
        raise SystemExit('Percurso sem saída normal ou salvamento exato: ' + name)
    for count in [20, 200]:
        samples = [s for s in data['samples'] if s['stage'] == 'move_down' and s['prose_lines'] == count]
        if len(samples) != 3:
            raise SystemExit('Amostras incompletas: ' + name)
        if name == 'reordering.json' and any(s['retained_views'] != count for s in samples):
            raise SystemExit('Vistas não conservadas em todos os movimentos finais.')
    if name == 'reordering.json' and not {'drop_before', 'drop_after', 'drop_left', 'drop_right'} <= {s['stage'] for s in data['samples']}:
        raise SystemExit('Quatro destinos de arrasto não comprovados.')
integrity = json.loads((here / 'integridade.json').read_text())['databases']
if len(integrity) != 3 or any(d['integrity'] != 'ok' or d['foreign_key_violations'] for d in integrity):
    raise SystemExit('Conferência dos três bancos incompleta.')
asan = json.loads((here / 'dominio-asan.json').read_text())
if asan['exit_code'] != 0 or asan['source_commit'] != code['commit']:
    raise SystemExit('Prova local de memória não corresponde à implementação.')
check(root, asan['domain_files'])
ci = json.loads((here / 'ci.json').read_text())
if ci['headSha'] != code['commit'] or ci['conclusion'] != 'success' or len(ci['jobs']) != 26:
    raise SystemExit('CI final incompleta ou sem correspondência de código.')
if any(j['conclusion'] != 'success' for j in ci['jobs']):
    raise SystemExit('CI contém trabalho sem aprovação.')
native = next(j for j in ci['jobs'] if j['name'] == 'native-objects')
stage = 'Verify real keyboard and mouse from project creation through normal exit'
if not any(s['name'] == stage and s['conclusion'] == 'success' for s in native['steps']):
    raise SystemExit('Percurso real obrigatório não passou na CI.')
if not any(s['name'] == 'Preserve synthetic real-input evidence' and s['conclusion'] == 'success' for s in native['steps']):
    raise SystemExit('Evidências sintéticas não foram preservadas pela CI final.')
native_log = (here / 'ci-nativo-testes.txt').read_text()
if 'test_structural_reorder_and_lateral_drag' not in native_log or 'Ran 3 tests' not in native_log:
    raise SystemExit('Log remoto não comprova os três percursos reais.')
domain_result = json.loads((here / 'ci-dominio.json').read_text())
if domain_result['headSha'] != code['commit']:
    raise SystemExit('Provas remotas de domínio pertencem a outra revisão.')
domain = domain_result['jobs']
if len(domain) != 4 or any(j['conclusion'] != 'success' or not any('inde_object_structural_move_tests' in line and 'Passed' in line for line in j['test_lines']) for j in domain):
    raise SystemExit('Prova estrutural não está aprovada nas quatro configurações do domínio.')
print('Local: 100 testes, três percursos reais, quatro bordas, três bancos íntegros e ASan/UBSan')
print('CI: 26 trabalhos; prova estrutural em GCC/Clang, com e sem sanitizers')
print('OK — nenhuma gravação realizada; não substitui aceite humano')
