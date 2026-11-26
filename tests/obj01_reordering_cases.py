"""Real pointer/keyboard workloads for structural ordering and row placement."""
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
import time


def exercise_reordering(test, root, env, process, window, input_, click, wait, state, scroll_to):
    report = []
    baseline = os.environ.get('INDE_REORDER_BASELINE') == '1'

    def ready(s, body):
        return (s.get('content') == body and not s.get('busy')
                and [p['text'] for p in s['objects']['paragraphs']] == body.split('\n'))

    def paste(lines):
        body = '\n'.join(lines)
        wait(lambda s: any(c['name'].startswith('writing-paragraph-') and c.get('focus')
                           for c in s['controls']), 'Especialidade Texto devolveu o foco à escrita')
        input_('key', 'ctrl+a')
        subprocess.run(['xclip', '-selection', 'clipboard'], input=body.encode(), env=env, check=True)
        input_('key', 'ctrl+v')
        wait(lambda s: ready(s, body), 'Texto sintético preparado para reordenação', timeout=25)
        return body

    counts = [int(n) for n in os.environ.get('INDE_REORDER_COUNTS', '20,200').split(',')]
    test.assertTrue(counts and all(4 <= n <= 2000 for n in counts))
    for count in counts:
        click(name='specialty-text')
        lines = ['Linha %03d preservada.' % i for i in range(count)]
        original = paste(lines)
        click(name='specialty-structure')
        objects = state()['objects']['paragraphs']
        first = objects[0]['id']
        original_views = {c['name']: c.get('instance') for c in state()['controls']
                          if c['name'].startswith('writing-paragraph-')}
        scroll_to('native-reading-handle-' + first, 'native-sheet-scroll')
        click(name='native-reading-handle-' + first)
        for step in [1, 2, 3]:
            expected = '\n'.join(lines[1:step+1] + [lines[0]] + lines[step+1:])
            start = time.monotonic()
            input_('key', 'ctrl+Down')
            wait(lambda s: ready(s, expected), 'Movimento estrutural consecutivo', timeout=25)
            current_views = {c['name']: c.get('instance') for c in state()['controls']
                             if c['name'] in original_views}
            kept = sum(value == original_views[name] for name, value in current_views.items())
            sample = dict(stage='move_down', prose_lines=count, step=step,
                          observed_ms=round((time.monotonic()-start)*1000, 3),
                          retained_views=kept, total_views=len(original_views))
            report.append(sample)
            print('OBJ01_REORDER_SAMPLE ' + json.dumps(sample), flush=True)
            if not baseline:
                test.assertEqual(kept, len(original_views), 'Movimento recriou vistas de texto')
        for _ in range(3):
            click(name='native-undo')
        wait(lambda s: ready(s, original), 'Desfazer os três movimentos conserva o texto')

    if not baseline:
        exercise_lateral(test, root, env, window, input_, click, wait, state, scroll_to, paste, report)

    expected = state()['content']
    click(name='native-save')
    wait(lambda s: not s.get('busy') and not s['objects']['dirty'], 'Salvar reordenação')
    input_('key', 'ctrl+q')
    process.wait(timeout=15)
    test.assertEqual(process.returncode, 0)
    project = root / 'Teste.inde'
    for path in project.rglob('*.sqlite3'):
        with sqlite3.connect(path.as_uri()+'?mode=ro', uri=True) as db:
            test.assertEqual(db.execute('pragma integrity_check').fetchone()[0], 'ok')
            test.assertEqual(db.execute('pragma foreign_key_check').fetchall(), [])
            if path.name == 'project.sqlite3':
                test.assertEqual(db.execute('select content from documents').fetchone()[0], expected)
    sidecar = next((project/'documents/composed').glob('*/obj01-piloto.sqlite3'))
    with sqlite3.connect(sidecar.as_uri()+'?mode=ro',uri=True) as db:
        saved = json.loads(db.execute('select snapshot from saved').fetchone()[0])
        test.assertEqual(json.loads(saved['working_text'])['document']['content'],expected)
        if not baseline: test.assertEqual(len(saved['rows']),1)
    artifacts = os.environ.get('INDE_TEST_ARTIFACTS')
    if artifacts:
        target = Path(artifacts); target.mkdir(parents=True, exist_ok=True)
        (target/'reordering.json').write_text(json.dumps(dict(baseline=baseline, samples=report,
             timing_scope='Real X11 input through low-priority state observer; not pure paint latency',
             normal_exit=True, saved_text_exact=True), ensure_ascii=False, indent=2)+'\n')
        shutil.copyfile(root/'application.log', target/'reordering-application.log')
        shutil.copytree(project, target/'Editor-reordenacao.inde')


def exercise_lateral(test, root, env, window, input_, click, wait, state, scroll_to, paste, report):
    click(name='specialty-text')
    lines = ['Bloco %d para arrastar.' % i for i in range(6)]
    body = paste(lines)
    click(name='specialty-structure')
    ids = [p['id'] for p in state()['objects']['paragraphs']]

    def control(name):
        return next(c for c in state()['controls'] if c['name'] == name)

    def drag(source, target, direction, cancel=False, invalid=False, straight=False):
        scroll_to('native-reading-handle-' + source, 'native-sheet-scroll')
        # Keep this short fixture's drop centre away from the autoscroll bands.
        # An already hittable but partly clipped row can otherwise move under
        # a stationary pointer while the driver is waiting for an indicator.
        for _ in range(8):
            area = control('native-sheet-scroll')
            t = control('native-reading-unit-'+target)
            centre = t['y']+t['height']//2
            if area['y']+40 < centre < area['y']+area['height']-40: break
            input_('mousemove','--window',window,str(area['x']+8),str(area['y']+area['height']//2),
                   'click','4' if centre <= area['y']+40 else '5')
            time.sleep(.2)
        previous = None
        frame = None
        stable = 0
        def geometry_ready(s):
            nonlocal previous, frame, stable
            controls = {c['name']: c for c in s['controls']}
            names = ['native-reading-handle-'+source, 'native-reading-unit-'+target]
            if s.get('busy') or any(name not in controls for name in names): return False
            current = tuple(tuple(controls[name].get(k) for k in ['x','y','width','height']) for name in names)
            updated = (root/'state.json').stat().st_mtime_ns
            if updated != frame:
                stable = stable+1 if current == previous else 0
                previous, frame = current, updated
            return stable >= 2
        wait(geometry_ready, 'Origem e destino do arrasto com geometria estável')
        h = control('native-reading-handle-' + source)
        t = control('native-reading-unit-' + target)
        sx, sy = h['x']+h['width']//2, h['y']+h['height']//2
        tx = t['x']+6 if direction == 'left' else t['x']+t['width']-6 if direction == 'right' else t['x']+t['width']//2
        ty = t['y']+3 if direction == 'before' else t['y']+t['height']-3 if direction == 'after' else t['y']+t['height']//2
        if straight:
            tx, ty = sx, t['y']+round(t['height']*.6)
        area = control('native-sheet-scroll')
        test.assertGreater(ty, area['y'], 'Destino do arrasto não está na folha visível')
        test.assertLess(ty, area['y']+area['height'], 'Destino do arrasto está fora da folha visível')
        original = state()['objects']
        input_('mousemove', '--window', window, str(sx), str(sy), 'mousedown', '1')
        for f in [.25, .5, .75, 1.]:
            input_('mousemove', '--window', window, str(round(sx+(tx-sx)*f)), str(round(sy+(ty-sy)*f)))
            time.sleep(.07)
        if invalid:
            wait(lambda s: 'ocupada' in s.get('status','') and not any(c.get('drop') for c in s['controls']),
                 'Destino ocupado não anuncia soltura válida')
        else:
            wait(lambda s: any(c['name'] == 'native-reading-unit-'+target and c.get('drop') == direction
                              for c in s['controls']), 'Indicador de soltura '+direction)
        artifacts = os.environ.get('INDE_TEST_ARTIFACTS')
        if artifacts and not invalid and shutil.which('import'):
            path = Path(artifacts); path.mkdir(parents=True, exist_ok=True)
            subprocess.run(['import','-window','root',str(path/('drop-'+direction+'.png'))], env=env, check=True)
        if cancel:
            input_('key','Escape')
            wait(lambda s: not any(c.get('drop') for c in s['controls']), 'Escape limpa o indicador')
        start = time.monotonic()
        input_('mouseup','1')
        if cancel or invalid:
            time.sleep(.25)
            test.assertEqual(state()['objects'], original, 'Arrasto cancelado/recusado alterou o agregado')
        else:
            wait(lambda s: not s.get('busy') and s['objects']['revision'] != original['revision']
                           and not any(c.get('drop') for c in s['controls']), 'Soltura aplicada e indicador limpo')
            report.append(dict(stage='drop_'+direction, observed_ms=round((time.monotonic()-start)*1000,3)))

    author = state()['objects']['author_order']
    drag(ids[0],ids[1],'after',straight=True)
    wait(lambda s: s['content'] == '\n'.join([lines[1],lines[0]]+lines[2:]),
         'Arrasto reto dos círculos move verticalmente, sem formar colunas')
    test.assertFalse(state()['objects']['rows'])
    click(name='native-undo')
    wait(lambda s: s['content'] == body, 'Desfazer arrasto reto')
    drag(ids[0], ids[1], 'right', cancel=True)
    drag(ids[0], ids[1], 'right')
    test.assertEqual(state()['content'], body)
    test.assertEqual(state()['objects']['author_order'], author)
    row = state()['objects']['rows'][0]
    test.assertEqual(row['children'], [ids[1],ids[0]])
    left, right = [control('native-reading-unit-'+i) for i in row['children']]
    test.assertLess(left['x'],right['x'])
    test.assertLess(abs(left['y']-right['y']),3)
    test.assertLess(left['width'], control('native-sheet-scroll')['width']*.7)
    click(name='native-undo')
    wait(lambda s: not s['objects']['rows'], 'Desfazer linha lateral')
    click(name='native-redo')
    wait(lambda s: s['objects']['rows'] == [row], 'Refazer restaura a mesma linha')
    click(name='native-reading-handle-'+ids[0])
    input_('key','ctrl+Down')
    expected = '\n'.join([lines[2],lines[0],lines[1]]+lines[3:])
    wait(lambda s: s['content'] == expected and not s.get('busy'), 'Mover a linha após arranjo divergente')
    test.assertEqual(state()['objects']['rows'], [row])
    click(name='native-undo')
    wait(lambda s: s['content'] == body and s['objects']['rows'] == [row], 'Desfazer movimento da linha')
    drag(ids[2], ids[0], 'right', invalid=True)
    # Reverse the two existing columns through the opposite lateral edge.
    drag(ids[0], ids[1], 'left')
    test.assertEqual(state()['objects']['rows'][0]['children'], ids[:2])
    test.assertEqual(state()['content'], body)
    # Use two real small tables, then drag their circles into a row.
    click(name='native-undo')
    wait(lambda s: s['objects']['rows'] == [row], 'Voltar ao par original')
    click(name='native-undo')
    wait(lambda s: not s['objects']['rows'], 'Voltar à coluna única antes de inserir tabelas')
    for count in [1,2]:
        click(name='specialty-tables'); click(name='native-new-table')
        wait(lambda s: s.get('draft'), 'Formulário de tabela aberto')
        for field in ['rows','columns']:
            scroll_to('native-field-'+field,'native-edit-scroll')
            click(name='native-field-'+field); input_('key','ctrl+a');input_('type','1')
        click(name='native-apply')
        wait(lambda s: not s.get('draft') and len(s['objects']['tables'])==count, 'Tabela 1x1 inserida')
    tables = [t['id'] for t in state()['objects']['tables']]
    click(name='specialty-structure')
    drag(tables[1],tables[0],'left')
    table_row = state()['objects']['rows'][0]
    test.assertEqual(table_row['children'],tables[::-1])
    test.assertEqual(state()['content'],body)
    # The last text block and the table row are visible together near the end.
    drag(tables[1],ids[-1],'before')
    test.assertEqual(state()['objects']['rows'],[table_row])
    order = state()['objects']['visual_order']
    test.assertLess(order.index(tables[1]),order.index(ids[-1]))
    test.assertEqual(state()['content'],body)
    click(name='native-undo')
    wait(lambda s: s['objects']['visual_order'].index(tables[0]) > s['objects']['visual_order'].index(ids[-1]),
         'Desfazer movimento mantém par de tabelas')
    click(name='native-redo')
    wait(lambda s: s['objects']['visual_order'].index(tables[0]) < s['objects']['visual_order'].index(ids[-1]),
         'Refazer movimento mantém par de tabelas')
    test.assertEqual(state()['objects']['rows'],[table_row])
    test.assertEqual(state()['content'],body)
    for specialty in ['text','composition','tables','structure']:
        click(name='specialty-'+specialty)
        wait(lambda s: not s.get('busy') and s['objects']['rows'] == [table_row]
                       and s['content'] == body, 'A mesma linha permanece na especialidade '+specialty)
