"""Contratos do corte: isolamento, conteúdo público, endereços e portabilidade."""
from copy import deepcopy
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import urljoin, urlsplit
from zipfile import ZipFile

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
import publicacao as pub


class References(HTMLParser):
    def __init__(self):
        super().__init__()
        self.base = None
        self.urls = []

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if tag == 'base':
            self.base = attrs.get('href')
        for key in ('href', 'src'):
            if key in attrs and tag != 'base':
                self.urls.append(attrs[key])


class ReadableText(HTMLParser):
    def __init__(self):
        super().__init__()
        self.parts = []

    def handle_data(self, value):
        self.parts.append(value)

    def handle_starttag(self, tag, attrs):
        if tag == 'br':
            self.parts.append('\n')

    def handle_endtag(self, tag):
        if tag == 'p':
            self.parts.append('\n\n')


class PublicationContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def build(self, data=None):
        return pub.build(data or pub.new_project(True), self.root / 'site')

    def test_empty_site_needs_no_account_domain_or_work(self):
        output = self.build(pub.new_project())
        self.assertEqual(json.loads((output / 'catalogo.json').read_text()), [])
        self.assertFalse((output / 'CNAME').exists())
        self.assertTrue((output / 'home/index.html').is_file())
        self.assertNotIn('canonical', (output / 'home/index.html').read_text())

    def test_real_reader_gets_explicit_semantics_and_scene_anchors(self):
        data = pub.new_project(True)
        output = self.build(data)
        document = data['obra']['documentos'][0]
        text = (output / 'obra/writed' / (document['id'] + '.html')).read_text()
        self.assertIn('data-speaker="' + data['obra']['personagem']['id'], text)
        self.assertIn('data-thinker="', text)
        self.assertIn('data-pov-character="', text)
        self.assertIn('data-verbete-id="' + data['obra']['id'] + '-verbete', text)
        self.assertIn('id="cena-' + document['id'], text)
        self.assertTrue((output / 'js-global/leitor.js').is_file())
        self.assertTrue((output / 'estacao-das-mares/offline.html').exists() is False)
        self.assertTrue((output / 'obra/offline.html').is_file())

    def test_drafts_and_destination_stay_out_of_public_zip(self):
        data = pub.new_project(True)
        data['destino'] = {'tipo': 'github', 'repositorio': 'privado/destino-reservado', 'branch': 'revisao-privada'}
        data['site']['origem'] = 'https://autora.github.io'
        data['site']['caminho_base'] = '/contos'
        data['obra']['documentos'][1].update(publico=False, texto='SEGREDO-DO-RASCUNHO')
        data['obra']['documentos'][1]['anotacoes'] = []
        data['anotacoes_privadas'] = 'SEGREDO-DO-PROJETO'
        data['site']['campo_nao_publico'] = 'SEGREDO-NA-CONFIGURACAO'
        output = self.build(data)
        archive = self.root / 'publico.zip'
        pub.package_public(output, archive)
        with ZipFile(archive) as package:
            corpus = b'\n'.join(package.read(name) for name in package.namelist())
            for secret in (b'SEGREDO-DO-RASCUNHO', b'SEGREDO-DO-PROJETO', b'SEGREDO-NA-CONFIGURACAO', b'privado/destino-reservado', b'revisao-privada'):
                self.assertNotIn(secret, corpus)
            self.assertNotIn('projeto.json', package.namelist())
        self.assertFalse((output / 'CNAME').exists())

    def test_subdirectory_routes_and_resources_resolve(self):
        data = pub.new_project(True)
        data['site'].update(origem='https://autora.github.io', caminho_base='/biblioteca')
        output = self.build(data)
        info = json.loads((output / 'obra/dados/info-obra.json').read_text())
        self.assertEqual(info['url_publica'], '/biblioteca/estacao-das-mares/')
        chapter = output / 'estacao-das-mares/capitulo-1/index.html'
        self.assertIn('https://autora.github.io/biblioteca/estacao-das-mares/capitulo-1/', chapter.read_text())
        self.assertIn('<base href="/biblioteca/obra/">', chapter.read_text())
        self.assertFalse((output / 'biblioteca').exists())
        self.assert_local_references(output, '/biblioteca')

    def assert_local_references(self, output, prefix):
        for path in output.rglob('*.html'):
            parser = References()
            parser.feed(path.read_text())
            page_url = 'http://local' + prefix + '/' + str(path.relative_to(output))
            base = urljoin(page_url, parser.base) if parser.base else page_url
            for link in parser.urls:
                target = urlsplit(urljoin(base, link))
                if target.netloc != 'local':
                    continue
                relative = target.path.removeprefix(prefix).lstrip('/')
                file = output / relative
                if target.path.endswith('/'):
                    file = file / 'index.html'
                self.assertTrue(file.exists(), f'{path.relative_to(output)}: {link} -> {relative}')

    def test_root_routes_and_resources_resolve(self):
        self.assert_local_references(self.build(), '')

    def test_custom_domain_generates_cname(self):
        data = pub.new_project(True)
        data['site']['origem'] = 'https://historias.example.org'
        output = self.build(data)
        self.assertEqual((output / 'CNAME').read_text().strip(), 'historias.example.org')

    def test_unicode_text_is_preserved_and_stale_annotations_refuse_generation(self):
        data = pub.new_project(True)
        document = data['obra']['documentos'][0]
        text = ReadableText()
        text.feed(pub.render_document(document, data['obra']['personagem']['id']))
        self.assertEqual(''.join(text.parts).removesuffix('\n\n'), document['texto'])
        document['texto'] = 'Prefixo alterado. ' + document['texto']
        with self.assertRaisesRegex(ValueError, 'texto marcado mudou'):
            self.build(data)
        self.assertFalse((self.root / 'site').exists())

    def test_optimistic_save_preserves_external_changes(self):
        workshop = pub.Workshop(self.root)
        original = workshop.create()
        changed = deepcopy(original['projeto'])
        changed['site']['nome'] = 'Primeira sessão'
        workshop.save(changed, original['revisao'])
        with self.assertRaisesRegex(ValueError, 'outra sessão'):
            workshop.save(original['projeto'], original['revisao'])
        self.assertEqual(workshop.read(changed['id'])['projeto']['site']['nome'], 'Primeira sessão')

    def test_independent_projects_get_distinct_content_identities(self):
        a, b = pub.new_project(True), pub.new_project(True)
        self.assertNotEqual(a['id'], b['id'])
        self.assertNotEqual(a['obra']['id'], b['obra']['id'])
        self.assertNotEqual(a['obra']['personagem']['id'], b['obra']['personagem']['id'])

    def test_content_identity_survives_title_and_site_changes(self):
        data = pub.new_project(True)
        before = [d['id'] for d in data['obra']['documentos']]
        data['site'].update(nome='Outra biblioteca', autor='Outro coletivo', caminho_base='/edicao')
        data['obra']['titulo'] = 'Título revisto'
        pub.validate(data)
        self.assertEqual(before, [d['id'] for d in data['obra']['documentos']])

    def test_existing_chapter_number_cannot_be_transferred(self):
        workshop = pub.Workshop(self.root)
        record = workshop.create(True)
        data = deepcopy(record['projeto'])
        data['obra']['documentos'][0]['numero'] = 3
        with self.assertRaisesRegex(ValueError, 'permanente'):
            workshop.save(data, record['revisao'])

    def test_presentation_text_cannot_close_script_or_inject_markup(self):
        data = pub.new_project(True)
        data['site']['nome'] = '</script><img src=x onerror=alert(1)>'
        output = self.build(data)
        home = (output / 'home/index.html').read_text()
        self.assertNotIn('</script><img src=x', home)
        self.assertIn('\\u003c/script\\u003e', home)

    def test_invalid_base_and_repository_are_rejected(self):
        for prefix in ('/../segredo', '//outro', '/a?b', '/a#b'):
            data = pub.new_project()
            data['site']['caminho_base'] = prefix
            with self.assertRaises(ValueError):
                pub.validate(data)
        data = pub.new_project()
        data['destino'].update(tipo='github', repositorio='https://qualquer.example/token')
        with self.assertRaises(ValueError):
            pub.validate(data)

    def test_public_manifest_verifies_every_file(self):
        output = self.build()
        receipt = json.loads((output / 'inde-publicacao.json').read_text())
        for name, expected in receipt['arquivos'].items():
            self.assertEqual(hashlib.sha256((output / name).read_bytes()).hexdigest(), expected)

    def test_portable_copy_runs_without_original_workspace(self):
        copy = self.root / 'pacote'
        copy.mkdir()
        
        for filename in ('publicacao.py', 'crud.py', 'ponte_inde.py'):
            shutil.copy2(BASE / filename, copy)
        for name in ('motor', 'recursos'):
            shutil.copytree(BASE / name, copy / name, ignore=shutil.ignore_patterns('__pycache__'))
        output = self.root / 'outro-computador'
        result = subprocess.run([sys.executable, '-I', str(copy / 'publicacao.py'), '--gerar-exemplo', str(output)], cwd=self.root, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((output / 'home/index.html').exists())


if __name__ == '__main__':
    unittest.main()
