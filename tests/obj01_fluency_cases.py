"""Editing workloads entered through the real clipboard and X11 keys only."""
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time


def exercise_blank_paragraphs(test, root, env, process, window, input_, click, wait, state, scroll_to):
    counts = [int(v) for v in os.environ.get('INDE_FLUENCY_PARAGRAPHS', '20,200').split(',')]
    test.assertTrue(counts and all(2 <= v <= 2000 for v in counts))
    report = []
    blanks = 9
    prefix = 'Abertura preservada'
    final = ''

    def settled(body):
        return lambda s: (s.get('content') == body and not s.get('busy')
                          and len(s.get('objects', {}).get('paragraphs', [])) == body.count('\n') + 1)

    def record(stage, count, started):
        report.append({'stage': stage, 'paragraphs': count, 'mixed': mixed,
                       'observed_ms': round((time.monotonic() - started) * 1000, 3)})
        print('OBJ01_FLUENCY_SAMPLE ' + json.dumps(report[-1]), flush=True)

    row = None
    for count, mixed in [(v, False) for v in counts] + [(counts[-1], True)]:
        if mixed:
            click(name='specialty-tables')
            click(name='native-new-table')
            click(name='native-apply')
            wait(lambda s: not s.get('draft') and len(s['objects']['tables']) == 1,
                 'Tabela real para o percurso de edição mista')
            table = state()['objects']['tables'][0]['id']
            click(name='specialty-math')
            click(name='native-new-equation')
            click(name='native-apply')
            wait(lambda s: not s.get('draft') and len(s['objects']['equations']) == 1,
                 'Equação real para compor com a tabela')
            click(name='specialty-composition')
            click(name='native-layout')
            wait(lambda s: s.get('draft') and any(c['name'] == 'native-field-other' for c in s['controls']),
                 'Formulário de posição aberto antes de acessar seus campos')
            choices = [v for v in state()['objects']['visual_order']
                       if v != state()['objects']['selection']]
            scroll_to('native-field-other', 'native-edit-scroll')
            click(name='native-field-other')
            # GtkTreePopover binds Down/Return; End belongs to its scroller,
            # and does not select the final item of an open combo popup.
            remaining = choices.index(table)
            while remaining:
                batch = min(64, remaining)
                input_('key', '--delay', '5', *(['Down'] * batch))
                remaining -= batch
            input_('key', 'Return')
            wait(lambda s: any(c['name'] == 'native-field-other' and c.get('value') == table
                              for c in s['controls']), 'Tabela escolhida como vizinha pela interface')
            scroll_to('native-pair', 'native-edit-scroll')
            click(name='native-pair')
            wait(lambda s: not s.get('draft') and len(s['objects']['rows']) == 1,
                 'Compor tabela e equação pela interface real')
            row = state()['objects']['rows']
            test.assertIn(table, row[0]['children'])
            click(name='specialty-text')
            # Select the existing paragraph through a real click before paste.
            input_('mousemove', '--window', window, '600', '450', 'click', '--repeat', '80', '--delay', '0', '4')
            name = 'writing-paragraph-' + state()['objects']['paragraphs'][0]['paragraph']
            for _ in range(80):
                if any(c['name'] == name and c['hittable'] for c in state()['controls']):break
                input_('click', '4');time.sleep(.1)
            click(name=name)
        body = prefix + '\n' * (blanks + 1) + '\n'.join(
            'Linha ' + str(i) + ' preservada.' for i in range(count))
        subprocess.run(['xclip', '-selection', 'clipboard', '-in'], input=body,
                       text=True, env=env, check=True, stdout=subprocess.DEVNULL,
                       stderr=subprocess.DEVNULL, timeout=5)
        input_('key', 'ctrl+a')
        started = time.monotonic()
        input_('key', 'ctrl+v')
        wait(settled(body), 'Colagem com parágrafos vazios: ' + str(count), timeout=60)
        record('paste', count, started)
        # Separate the paste from the following typing undo group.
        time.sleep(.8)
        first = state()['objects']['paragraphs'][0]['paragraph']
        area = next(c for c in state()['controls'] if c['name'] == 'native-sheet-scroll' and c['hittable'])
        input_('mousemove', '--window', str(area.get('window', window)),
               str(area['x'] + area['width'] // 2), str(area['y'] + 30),
               'click', '--repeat', str(max(30, count * 2)), '--delay', '0', '4')
        for _ in range(80):
            if any(c['name'] == 'writing-paragraph-' + first and c['hittable'] for c in state()['controls']):
                break
            input_('click', '4')
            time.sleep(.1)
        click(name='writing-paragraph-' + first)
        input_('key', 'Home', 'End')
        retained_name = 'writing-paragraph-' + state()['objects']['paragraphs'][-1]['paragraph']
        retained = next(c for c in state()['controls'] if c['name'] == retained_name).get('instance')
        test.assertIsNotNone(retained, 'O observador deve identificar a vista cuja conservação é testada')
        expected = body
        for _ in range(3):
            expected = expected.replace('\n\n', '\n', 1)
            started = time.monotonic()
            input_('key', 'Delete')
            wait(settled(expected), 'Excluir um vazio mantendo a folha: ' + str(count), timeout=60)
            record('delete', count, started)
            test.assertFalse(state().get('error'), state().get('error'))
            if retained:
                test.assertEqual(next(c for c in state()['controls'] if c['name'] == retained_name).get('instance'),
                                 retained, 'Excluir vazio recriou a vista de um parágrafo não afetado')
        # Backspace from the empty paragraph and a key burst exercise both
        # boundary directions, retaining the author's intentional empty lines.
        input_('key', 'Right')
        wait(lambda s: s.get('cursor') == len(prefix) + 1 and any(
            c['name'] == 'writing-paragraph-' + s['objects']['paragraphs'][1]['paragraph'] and c.get('focus')
            for c in s['controls']), 'Navegação até o vazio confirmada antes do Backspace', timeout=60)
        expected = expected.replace('\n\n', '\n', 1)
        started = time.monotonic()
        input_('key', 'BackSpace')
        wait(settled(expected), 'Backspace retira outro vazio', timeout=60)
        record('backspace', count, started)
        expected = expected.replace('\n\n', '\n', 3)
        started = time.monotonic()
        input_('key', '--delay', '40', 'Delete', 'Delete', 'Delete')
        wait(settled(expected), 'Exclusões consecutivas sem perda de entrada', timeout=60)
        record('delete_burst', count, started)
        # Native typing must still reach the same logical insertion point.
        input_('type', '--clearmodifiers', '--delay', '20', '!')
        expected = expected.replace(prefix, prefix + '!', 1)
        wait(settled(expected), 'Digitação continua no cursor após exclusões', timeout=60)
        time.sleep(.8)
        # A slow baseline can split the group into multiple undo entries.
        # Every restored intermediate state must preserve all nonempty prose.
        undone = 0
        for _ in range(12):
            if state().get('content') == body:
                break
            previous = state()['content']
            input_('key', 'ctrl+z')
            wait(lambda s: s.get('content') != previous and settled(s['content'])(s),
                 'Desfazer exclusão com conteúdo íntegro', timeout=60)
            undone += 1
            test.assertEqual(state()['content'].replace('\n', '').replace('!', ''),
                             body.replace('\n', ''))
        test.assertEqual(state()['content'], body, 'Desfazer não restaurou os vazios autorais')
        for _ in range(undone):
            previous = state()['content']
            input_('key', 'ctrl+shift+z')
            wait(lambda s: s.get('content') != previous and settled(s['content'])(s),
                 'Refazer restaura a revisão com exclusões', timeout=60)
        test.assertEqual(state()['content'], expected)
        if mixed:
            test.assertEqual(state()['objects']['rows'], row, 'Revisão textual desmontou o arranjo')
        final = expected
    click(name='native-save')
    wait(lambda s: not s.get('busy') and not s['objects']['dirty'], 'Salvar após revisão com vazios', timeout=60)
    input_('key', 'ctrl+q')
    process.wait(timeout=15)
    test.assertEqual(process.returncode, 0, (root / 'application.log').read_text())
    db = sqlite3.connect((root / 'Teste.inde/data/project.sqlite3').as_uri() + '?mode=ro', uri=True)
    test.assertEqual(db.execute('select content from documents').fetchone()[0], final)
    test.assertEqual(db.execute('pragma integrity_check').fetchone()[0], 'ok')
    test.assertEqual(db.execute('pragma foreign_key_check').fetchall(), [])
    db.close()
    sidecar = next((root / 'Teste.inde/documents/composed').glob('*/obj01-piloto.sqlite3'))
    db = sqlite3.connect(sidecar.as_uri() + '?mode=ro', uri=True)
    saved = json.loads(db.execute('select snapshot from saved').fetchone()[0])
    test.assertEqual(json.loads(saved['working_text'])['document']['content'], final,
                     'Auxiliar salvo diverge do Documento canônico')
    test.assertEqual(len(saved['tables']), 1)
    test.assertEqual(len(saved['composition']['equations']), 1)
    test.assertEqual(saved['rows'][0]['children'], row[0]['children'])
    test.assertEqual(db.execute('pragma integrity_check').fetchone()[0], 'ok')
    test.assertEqual(db.execute('pragma foreign_key_check').fetchall(), [])
    db.close()
    output = {'timing_scope': 'X11 input through low-priority observer; includes polling, not pure paint latency',
              'samples': report, 'normal_exit': True, 'saved_text_exact': True}
    print('OBJ01_FLUENCY ' + json.dumps(output), flush=True)
    if os.environ.get('INDE_TEST_ARTIFACTS'):
        target = Path(os.environ['INDE_TEST_ARTIFACTS'])
        target.mkdir(parents=True, exist_ok=True)
        (target / 'fluency.json').write_text(json.dumps(output, indent=2) + '\n')
        shutil.copyfile(root / 'application.log', target / 'fluency-application.log')
        shutil.copytree(root / 'Teste.inde', target / 'Editor-fluidez.inde', dirs_exist_ok=False)
