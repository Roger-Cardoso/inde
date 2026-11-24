"""Mouse/keyboard regression: no emitted widget signals or direct text setters."""
import json
import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import unittest
import zlib

from ambiente import native_environment

BASE = Path(__file__).resolve().parents[1]


class RealWritingInput(unittest.TestCase):
    def test_blank_document_specialties_validation_recovery_and_quit(self):
        binary = BASE / 'build-nativo/inde-especial'
        if (not binary.exists() or not os.environ.get('DISPLAY') or not shutil.which('xdotool')
                or os.environ.get('INDE_REAL_INPUT_OWN_DISPLAY') != '1'):
            self.skipTest('Use python3 tests/run_obj01_real_input.py para um display isolado')
        with tempfile.TemporaryDirectory(prefix='inde-real-input-') as d:
            root = Path(d)
            state_file = root / 'state.json'
            env = native_environment(root / 'profile', BASE)
            env.update(XDG_DATA_HOME=str(root / 'data'), XDG_CACHE_HOME=str(root / 'cache'),
                       GSK_RENDERER='cairo', GTK_A11Y='none', GTK_IM_MODULE='simple', G_DEBUG='fatal-criticals', GTK_USE_PORTAL='0')
            log = (root / 'application.log').open('w+')
            command = [str(binary), '--escrita-usabilidade', str(root / 'Teste.inde'), str(state_file)]
            if os.environ.get('INDE_TEST_GDB'):
                command = ['gdb', '-q', '-batch', '-ex', 'run', '-ex', 'thread apply all bt 25', '--args', *command]
            process = subprocess.Popen(command, env=env, stdout=log, stderr=log)
            last = {}
            expected_body = 'Primeira linha á\nSegunda linha'

            def state():
                nonlocal last
                try:
                    last = json.loads(state_file.read_text())
                except (FileNotFoundError, json.JSONDecodeError):
                    pass
                return last

            def wait(predicate, description, timeout=12):
                until = time.monotonic() + timeout
                while time.monotonic() < until:
                    if process.poll() is not None:
                        log.flush()
                        artifacts = os.environ.get('INDE_TEST_ARTIFACTS')
                        if artifacts:
                            target = Path(artifacts); target.mkdir(parents=True, exist_ok=True)
                            shutil.copyfile(root / 'application.log', target / 'application.log')
                            (target / 'state.json').write_text(json.dumps(last, ensure_ascii=False, indent=2))
                        self.fail(f'Aplicativo encerrou ({process.returncode}; {description}): {(root / "application.log").read_text()}')
                    if predicate(state()):
                        return
                    time.sleep(.08)
                compact = dict(last)
                compact['controls'] = [c for c in last.get('controls', []) if c['mapped']]
                artifacts = os.environ.get('INDE_TEST_ARTIFACTS')
                if artifacts:
                    target = Path(artifacts)
                    target.mkdir(parents=True, exist_ok=True)
                    (target / 'state.json').write_text(json.dumps(compact, ensure_ascii=False, indent=2))
                    shutil.copyfile(root / 'application.log', target / 'application.log')
                    if shutil.which('import'):
                        subprocess.run(['import', '-window', 'root', str(target / 'failure.png')], env=env, check=False)
                    if shutil.which('xwininfo'):
                        with (target / 'windows.txt').open('w') as output:
                            subprocess.run(['xwininfo', '-root', '-tree'], env=env, stdout=output, check=False)
                self.fail(f'{description}: {json.dumps(compact, ensure_ascii=False)[:18000]}\n'
                          + (root / 'application.log').read_text())

            def snapshot(name):
                artifacts = os.environ.get('INDE_TEST_ARTIFACTS')
                if artifacts and shutil.which('import'):
                    target = Path(artifacts); target.mkdir(parents=True, exist_ok=True)
                    subprocess.run(['import', '-window', 'root', str(target / (name + '.png'))],
                                   env=env, check=True, timeout=10)

            def input_(*args):
                subprocess.run(['xdotool', *args], env=env, check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)

            def candidates(s, name=None, label=None, native_window=None):
                return [c for c in s.get('controls', [])
                        if c['mapped'] and c['sensitive'] and c.get('hittable') and
                        (native_window is None or str(c.get('window', window)) == str(native_window)) and
                        ((name and c['name'] == name) or (label and c['label'].replace('_', '') == label))]

            def click(name=None, label=None, native_window=None, allow_closed=False):
                previous = None
                previous_frame = None
                stable = 0
                def settled(s):
                    nonlocal previous, previous_frame, stable
                    if allow_closed and not any(str(c.get('window', '')) == str(native_window)
                                                for c in s['controls']):
                        return True
                    values = candidates(s, name, label, native_window)
                    if not values:
                        stable = 0
                        return False
                    c = values[0]
                    rectangle = tuple(c.get(k) for k in ['x', 'y', 'width', 'height'])
                    frame = state_file.stat().st_mtime_ns
                    if frame != previous_frame:
                        stable = stable + 1 if rectangle == previous else 0
                        previous, previous_frame = rectangle, frame
                    return stable >= 2
                wait(settled, f'Controle visível e estável: {name or label}')
                found = candidates(state(), name, label, native_window)
                if allow_closed and not found:
                    return
                c = found[0]
                self.assertGreater(c.get('width', 0), 0)
                self.assertGreater(c.get('height', 0), 0)
                input_('mousemove', '--window', str(c.get('window', window)), str(c['x'] + c['width'] // 2),
                       str(c['y'] + c['height'] // 2), 'click', '1')


            def right_click(name):
                wait(lambda s: bool(candidates(s, name=name)), 'Alvo do botão direito: ' + name)
                c = candidates(state(), name=name)[0]
                input_('mousemove', '--window', str(c.get('window', window)),
                       str(c['x'] + min(30, c['width'] // 2)),
                       str(c['y'] + c['height'] // 2), 'click', '3')

            def menu_action(label):
                wait(lambda s: any(c.get('mapped') and c.get('label') == label and
                                  c['name'].startswith('native-writing-action-')
                                  for c in s['controls']), 'Menu especializado: ' + label)
                name = next(c['name'] for c in state()['controls'] if c.get('mapped') and
                            c.get('label') == label and c['name'].startswith('native-writing-action-'))
                scroll_to(name, 'native-context-scroll')
                click(name=name)

            def scroll_to(name, area):
                for _ in range(24):
                    if candidates(state(), name=name):
                        return
                    regions = candidates(state(), name=area)
                    self.assertTrue(regions, 'Área de rolagem indisponível: ' + area)
                    c = regions[0]
                    input_('mousemove', '--window', str(c.get('window', window)), str(c['x'] + c['width'] // 2),
                           str(c['y'] + c['height'] // 2), 'click', '5')
                    time.sleep(.15)
                self.fail('Controle não alcançado pela rolagem: ' + name)

            def write_field(name, value, area='native-edit-scroll'):
                scroll_to(name, area)
                click(name=name)
                input_('key', 'ctrl+a')
                input_('type', '--clearmodifiers', '--delay', '40', value)

            def choose_path(title, selected):
                chooser_window = None
                def chooser_ready(s):
                    nonlocal chooser_window
                    found = subprocess.run(['xdotool', 'search', '--all', '--onlyvisible',
                        '--pid', str(s['pid']), '--name', title], env=env, text=True,
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                    if found.returncode == 0 and found.stdout.strip():
                        chooser_window = found.stdout.splitlines()[0]
                        return True
                    return False
                wait(chooser_ready, 'Seletor nativo: ' + title)
                input_('windowactivate', '--sync', chooser_window)
                input_('key', 'ctrl+l', 'ctrl+a')
                subprocess.run(['xclip', '-selection', 'clipboard', '-in'], input=selected,
                               text=True, env=env, check=True, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL, timeout=5)
                input_('key', 'ctrl+v')
                wait(lambda s: any(c.get('value') == selected for c in s['controls']),
                     'Caminho colado no seletor nativo')
                if title == 'Criar projeto INDE':
                    click(label='Criar', native_window=chooser_window, allow_closed=True)
                else:
                    click(name='GtkFileChooserEntry', native_window=chooser_window)
                    wait(lambda s: any(c.get('value') == selected and c.get('focus')
                                       and str(c.get('window')) == str(chooser_window)
                                       for c in s['controls']),
                         'Campo de caminho do seletor com foco')
                    input_('key', '--delay', '100', 'Return')
                    wait(lambda s: not any(str(c.get('window', '')) == str(chooser_window)
                                          for c in s['controls']),
                         'Seletor aceita o caminho confirmado')

            try:
                wait(lambda s: bool(s.get('controls')), 'Aplicativo sem projeto aberto')
                window = subprocess.check_output(
                    ['xdotool', 'search', '--all', '--onlyvisible', '--pid', str(state()['pid']),
                     '--name', 'INDE'], env=env, text=True).splitlines()[0]
                input_('windowactivate', '--sync', window)
                input_('key', 'ctrl+n')
                choose_path('Criar projeto INDE', str(root / 'Teste.inde'))
                wait(lambda s: (root / 'Teste.inde/manifest.json').exists(), 'Projeto criado pela interface')
                input_('windowactivate', '--sync', window)
                input_('key', 'ctrl+1')
                click(label='Novo Documento')
                wait(lambda s: any(c.get('role') == 'entry' and c.get('hittable')
                                  for c in s['controls']), 'Nome do novo Documento')
                title = next(c for c in state()['controls'] if c.get('role') == 'entry' and c.get('hittable'))
                click(name=title['name'])
                input_('type', '--clearmodifiers', '--delay', '40', 'Documento de uso real')
                click(label='Criar e escrever')
                wait(lambda s: 'objects' in s and not s.get('busy') and s.get('content') == '',
                     'Documento vazio criado pela interface')
                paragraph = next((c['name'] for c in state()['controls']
                                  if c['name'].startswith('writing-paragraph-')), None)
                self.assertIsNotNone(paragraph, 'Documento vazio não oferece área para escrever')
                click(name=paragraph)
                input_('type', '--clearmodifiers', '--delay', '60', 'Primeira linha ')
                input_('key', '--delay', '150', 'dead_acute')
                wait(lambda s: s.get('composing'), 'Acento provisório recebido pelo GTK')
                input_('key', 'a')
                wait(lambda s: s.get('content') == 'Primeira linha á' and not s.get('composing'),
                     'Acento confirmado no Documento')
                input_('key', 'Return')
                input_('type', '--clearmodifiers', '--delay', '60', 'Segunda linha')
                wait(lambda s: s.get('content') == expected_body, 'Digitação real, tecla morta e Enter')
                for specialty in ['semantic', 'linguistic', 'structure', 'bindings',
                                  'composition', 'tables', 'formulas', 'math', 'text']:
                    click(name='specialty-' + specialty)
                    wait(lambda s: not s.get('busy') and any(c.get('active') for c in
                         candidates(s, name='specialty-' + specialty)) and s.get('content') ==
                         expected_body, 'Troca de especialidade ' + specialty)
                    tool = {'text': ('label', 'Estatísticas'),
                            'semantic': ('label', 'Marcações do texto'),
                            'linguistic': ('label', 'Palavras repetidas'),
                            'structure': ('label', 'Parágrafos e identidades…'),
                            'bindings': ('label', 'Trechos marcados'),
                            'composition': ('name', 'native-new-image'),
                            'tables': ('name', 'native-new-table'),
                            'formulas': ('name', 'native-new-formula'),
                            'math': ('name', 'native-new-equation')}[specialty]
                    if not state().get('tools_open'):
                        click(name='native-tools')
                    wait(lambda s: bool(candidates(s, **{tool[0]: tool[1]})),
                         'Ferramentas presentes em ' + specialty)
                    self.assertGreater(next(c['width'] for c in state()['controls'] if c['name'] == 'native-sheet-scroll'), 400)
                    if specialty == 'text':
                        click(label='Estatísticas')
                        wait(lambda s: bool(candidates(s, name='native-stats-words')) and
                             any('palavra' in c.get('label', '') for c in
                                 candidates(s, name='native-stats-words')),
                             'Estatísticas reais no painel da folha comum')
                        click(label='Meta')
                        wait(lambda s: bool(candidates(s, label='Usar meta de palavras')),
                             'Controles de meta recuperados')
                    elif specialty == 'semantic':
                        click(label='Marcações do texto')
                        wait(lambda s: bool(candidates(s, label='Fechar inspetor')),
                             'Inspetor semântico original na folha comum')
                        self.assertGreater(next(c['width'] for c in state()['controls']
                                                if c['name'] == 'native-sheet-scroll'), 400)
                        snapshot('semantica-inspetor')
                        click(label='Fechar inspetor')
                    elif specialty == 'structure':
                        click(label='Parágrafos e identidades…')
                        wait(lambda s: bool(candidates(s, label='Fechar')),
                             'Identidades dos parágrafos acessíveis em Estrutura')
                        click(label='Fechar')
                    elif specialty == 'bindings':
                        click(label='Trechos marcados')
                        click(label='Marcar trecho')
                        wait(lambda s: bool(candidates(s, label='Criar trecho marcado')),
                             'Vínculos abriu o formulário original')
                        entry = next(c for c in state()['controls'] if c.get('role') == 'entry' and
                                     c.get('hittable') and c.get('value') == '')
                        click(name=entry['name'])
                        input_('type', '--clearmodifiers', 'Trecho preservado')
                        click(label='Criar trecho marcado')
                        wait(lambda s: not candidates(s, label='Criar trecho marcado'),
                             'Trecho marcado criado pela interface')
                    if state().get('tools_open'):
                        if specialty == 'text':
                            input_('key', 'F4')
                            wait(lambda s: not s.get('tools_open'), 'F4 fecha o painel da folha comum')
                        else:
                            click(name='native-tools')
                    if specialty in ['text', 'semantic', 'linguistic', 'structure', 'bindings']:
                        paragraph = next(c['name'] for c in state()['controls']
                                         if c['name'].startswith('writing-paragraph-'))
                        click(name=paragraph)
                        input_('key', 'Home', 'shift+End')
                        wait(lambda s: abs(s['cursor'] - s['bound']) == len(expected_body.splitlines()[0]),
                             'Seleção no primeiro parágrafo')
                        selection = (state()['cursor'], state()['bound'])
                        if specialty == 'text':
                            input_('key', 'shift+F10')
                            wait(lambda s: any(c.get('mapped') and c.get('label') == 'Sugerir alteração…'
                                              for c in s['controls']), 'Menu especializado também pelo teclado')
                            input_('key', 'Escape')
                        right_click(paragraph)
                        expected_menu = {'text': 'Sugerir alteração…',
                            'semantic': 'Comentar seleção…',
                            'linguistic': 'Consultar seleção no dicionário…',
                            'structure': 'Criar alternativa do trecho',
                            'bindings': 'Vincular elemento da história existente…'}[specialty]
                        wait(lambda s: any(c.get('mapped') and c.get('label') == expected_menu
                                          for c in s['controls']), 'Menu próprio de ' + specialty)
                        wait(lambda s: any(c['name'] == 'native-context-scroll' and c['mapped'] and
                                          c['width'] >= 260 for c in s['controls']),
                             'Menu especializado com largura legível após alocação')
                        self.assertGreaterEqual(next(c['width'] for c in state()['controls']
                                                     if c['name'] == 'native-context-scroll' and c['mapped']), 260,
                                                'Menu especializado ficou estreito e ilegível')
                        self.assertEqual((state()['cursor'], state()['bound']), selection,
                                         'Botão direito alterou a seleção autoritativa')
                        if specialty == 'linguistic':
                            snapshot('linguistica-menu-contexto')
                            menu_action(expected_menu)
                            wait(lambda s: bool(candidates(s, label='Consultar alternativas')),
                                 'Consulta linguística abriu pelo botão direito')
                            self.assertTrue(any(c.get('value') == expected_body.splitlines()[0]
                                                for c in state()['controls']), 'Dicionário recebeu outra seleção')
                            click(label='Fechar')
                        elif specialty == 'semantic':
                            menu_action(expected_menu)
                            wait(lambda s: bool(candidates(s, label='Salvar anotação')),
                                 'Comentário abriu com a seleção da folha')
                            click(label='Cancelar')
                        else:
                            input_('key', 'Escape')
                        wait(lambda s: not any(c.get('mapped') and c['name'].startswith('native-writing-action-')
                                              for c in s['controls']), 'Fechar menu da especialidade')
                # Structural movement changes the actual owner text; undo restores it.
                click(name='specialty-structure')
                first = state()['objects']['paragraphs'][0]['id']
                second = state()['objects']['paragraphs'][1]['id']
                snapshot('estrutura-circulos')
                handle = 'native-reading-handle-' + first
                click(name=handle)
                wait(lambda s: any(c['name'] == 'native-object-' + first and c.get('selected') for c in s['controls']),
                     'Destaque visual acompanha o círculo escolhido')
                self.assertEqual(sum(c.get('selected', False) for c in state()['controls']
                                     if c['name'].startswith('native-object-')), 1)
                input_('key', 'ctrl+Down')
                wait(lambda s: s.get('content') == 'Segunda linha\nPrimeira linha á',
                     'Reordenação estrutural pelo teclado na folha')
                input_('key', 'ctrl+Up')
                wait(lambda s: s.get('content') == expected_body,
                     'O círculo conserva foco para o segundo movimento por teclado')
                click(name='native-undo')
                wait(lambda s: s.get('content') == 'Segunda linha\nPrimeira linha á',
                     'Desfazer o segundo movimento')
                click(name='native-undo')
                wait(lambda s: s.get('content') == expected_body, 'Desfazer reordenação estrutural')
                source = candidates(state(), name=handle)[0]
                target = next(c for c in state()['controls'] if c['name'] == 'native-object-' + second)
                sx, sy = source['x'] + source['width']//2, source['y'] + source['height']//2
                tx, ty = target['x'] + 30, target['y'] + target['height'] - 3
                input_('mousemove', '--window', window, str(sx), str(sy), 'mousedown', '1')
                for fraction in [.25, .5, .75, 1.0]:
                    input_('mousemove', '--window', window, str(round(sx+(tx-sx)*fraction)),
                           str(round(sy+(ty-sy)*fraction)))
                    time.sleep(.08)
                input_('mouseup', '1')
                wait(lambda s: s.get('content') == 'Segunda linha\nPrimeira linha á',
                     'Arrasto real dos círculos de Estrutura')
                click(name='native-undo')
                wait(lambda s: s.get('content') == expected_body, 'Desfazer arrasto conserva texto')
                click(name='specialty-math')
                click(name='native-new-equation')
                wait(lambda s: s.get('draft'), 'Edição de equação aberta')
                input_('key', 'F4')
                wait(lambda s: s.get('draft') and not s.get('tools_open'),
                     'F4 não invade o formulário de edição aberto')
                input_('key', 'ctrl+q')
                click(label='Continuar editando')
                wait(lambda s: s.get('draft') and not candidates(s, label='Continuar editando'),
                     'Recusar fechamento conserva rascunho')
                click(name='native-cancel')
                wait(lambda s: not s.get('draft') and candidates(s, name='native-sheet-scroll'),
                     'Cancelar pelo botão real retorna à folha')
                click(name='specialty-math')
                click(name='native-new-equation')
                wait(lambda s: s.get('draft'), 'Rascunho para troca de especialidade')
                click(name='specialty-tables')
                click(label='Descartar rascunho e continuar')
                wait(lambda s: not s.get('draft'), 'Troca de especialidade resolve rascunho')
                click(name='specialty-composition')
                click(name='native-new-image')
                selected = os.environ.get('INDE_TEST_PNG')
                if not selected:
                    def chunk(kind, data):
                        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
                    png = root / 'figura.png'
                    scanlines = (b'\0' + b'\x40\x80\xc0\xff' * 640) * 480
                    png.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 640, 480, 8, 6, 0, 0, 0))
                                    + chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))
                    selected = str(png)
                choose_path('Inserir figura PNG', selected)
                wait(lambda s: s.get('draft') and candidates(s, name='native-field-caption'), 'Arquivo escolhido')
                image_hash = hashlib.sha256(Path(selected).read_bytes()).hexdigest()
                input_('windowactivate', '--sync', window)
                click(name='native-apply')
                wait(lambda s: bool(s.get('error')), 'Erro de validação oferece motivo')
                self.assertEqual(state()['error'], 'alt_text_required')
                self.assertTrue(state()['draft'])
                click(name='native-cancel')
                wait(lambda s: not s.get('draft') and candidates(s, name='native-sheet-scroll'),
                     'Cancelar depois de falha de validação libera o editor')
                click(name='specialty-composition')
                click(name='native-new-image')
                choose_path('Inserir figura PNG', selected)
                wait(lambda s: s.get('draft') and candidates(s, name='native-field-caption'),
                     'Reabrir inserção após cancelar o erro')
                click(name='native-field-decorative')
                click(name='native-apply')
                wait(lambda s: not s.get('draft') and len(s['objects']['figures']) == 1,
                     'Corrigir validação e aplicar imagem')
                click(name='specialty-tables')
                click(name='native-new-table')
                wait(lambda s: s.get('draft'), 'Inserir tabela')
                write_field('native-field-title', 'Tabela de teste')
                click(name='native-apply')
                wait(lambda s: not s.get('draft') and len(s['objects']['tables']) == 1,
                     'Tabela aplicada pelo formulário real')
                table_id = state()['objects']['tables'][0]['id']
                click(name='specialty-composition')
                click(name='specialty-tables')
                click(name='native-table-style')
                wait(lambda s: s.get('draft') and candidates(s, name='native-field-preset'),
                     'Troca de especialidade conserva a tabela escolhida para aparência')
                click(name='native-cancel')
                wait(lambda s: not s.get('draft') and candidates(s, name='native-sheet-scroll'),
                     'Cancelar aparência devolve a folha antes de editar célula')
                self.assertEqual(state()['objects']['tables'][0]['id'], table_id)
                cell = state()['objects']['tables'][0]['cells'][0]['id']
                scroll_to('native-cell-' + cell, 'native-sheet-scroll')
                click(name='native-cell-' + cell)
                wait(lambda s: s.get('draft'), 'Editar célula da tabela')
                write_field('native-field-cell', 'Conteudo da celula')
                click(name='native-apply')
                wait(lambda s: not s.get('draft') and s['objects']['tables'][0]['cells'][0]['text'] ==
                     'Conteudo da celula', 'Célula editada com teclado')
                click(name='specialty-formulas')
                click(name='native-new-formula')
                click(name='native-kind-value')
                click(name='native-apply')
                wait(lambda s: not s.get('draft') and len(s['objects']['formulas']) == 1,
                     'Cartão de valor criado pela interface')
                self.assertEqual(state()['objects']['formulas'][0]['display'], '100')
                click(name='specialty-math')
                click(name='native-new-equation')
                wait(lambda s: s.get('draft'), 'Inserir equação')
                write_field('native-field-source', 'x^2+1')
                click(name='native-apply')
                wait(lambda s: not s.get('draft') and len(s['objects']['equations']) == 1,
                     'Equação editada e aplicada pela interface')
                self.assertEqual(state()['objects']['equations'][0]['source'], 'x^2+1')
                click(name='specialty-structure')
                original_order = state()['objects']['author_order']
                image = state()['objects']['figures'][0]['id']
                image_handle = 'native-reading-handle-' + image
                scroll_to(image_handle, 'native-sheet-scroll')
                click(name=image_handle)
                input_('key', 'ctrl+Up')
                wait(lambda s: not s.get('busy') and s['objects']['author_order'] != original_order,
                     'Objeto também participa da reordenação estrutural')
                self.assertEqual(state()['content'], expected_body)
                click(name='native-undo')
                wait(lambda s: s['objects']['author_order'] == original_order,
                     'Desfazer movimentação do objeto preserva a folha mista')
                click(name='specialty-text')
                click(name='native-save')
                wait(lambda s: not s.get('busy') and not s['objects']['dirty'], 'Salvar pelo controle real')
                click(name='specialty-math')
                click(name='native-new-equation')
                wait(lambda s: s.get('draft'), 'Novo rascunho para testar saída')
                input_('key', 'ctrl+q')
                click(label='Descartar rascunho e continuar')
                process.wait(timeout=15)
                self.assertEqual(process.returncode, 0, (root / 'application.log').read_text())
                import sqlite3
                connection = sqlite3.connect((root / 'Teste.inde/data/project.sqlite3').as_uri() + '?mode=ro', uri=True)
                self.assertEqual(connection.execute('select content from documents').fetchone()[0],
                                 expected_body)
                self.assertEqual(connection.execute('select title from documents').fetchone()[0],
                                 'Documento de uso real')
                self.assertEqual(connection.execute('pragma integrity_check').fetchone()[0], 'ok')
                self.assertEqual(connection.execute('select label,start_offset,end_offset from document_anchors').fetchall(),
                                 [('Trecho preservado', 0, len(expected_body.splitlines()[0]))],
                                 'Reordenação e Desfazer perderam ou deslocaram o trecho marcado')
                self.assertEqual(connection.execute('pragma foreign_key_check').fetchall(), [])
                connection.close()
                self.assertEqual(hashlib.sha256(Path(selected).read_bytes()).hexdigest(), image_hash,
                                 'A imagem de origem foi modificada')
                sidecar = next((root / 'Teste.inde/documents/composed').glob('*/obj01-piloto.sqlite3'))
                connection = sqlite3.connect(sidecar.as_uri() + '?mode=ro', uri=True)
                saved = json.loads(connection.execute('select snapshot from saved').fetchone()[0])
                self.assertEqual(len(saved['figures']), 1)
                self.assertEqual(len(saved['tables']), 1)
                self.assertEqual(len(saved['formulas']), 1)
                self.assertEqual(len(saved['composition']['equations']), 1)
                connection.close()
            finally:
                if process.poll() is None:
                    # Cleanup affects only this test-owned disposable process.
                    process.terminate()
                    process.wait(timeout=10)
                log.close()


if __name__ == '__main__':
    unittest.main(verbosity=2)
