from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile
BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
import publicacao as pub


@unittest.skipUnless((BASE / 'build-nativo/inde_catalog_tests').is_file(), 'Compile a suíte nativa de catálogo')
class NativeCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pub02b-suite-')
        cls.root = Path(cls.temp.name)
        cls.fixture = cls.root / 'fixture'
        subprocess.run([BASE / 'build-nativo/inde_web_bridge', '--demo-catalogo', cls.fixture], check=True, capture_output=True, text=True)
        result = subprocess.run([BASE / 'build-nativo/inde_catalog_tests', cls.fixture], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        cls.report = result.stdout
        cls.payload = json.loads((cls.fixture / 'catalogo-para-gerar.json').read_text())
        cls.output = cls.root / 'initial/site'
        pub.build_native_edition(cls.fixture / 'catalogo-para-gerar.json', cls.output)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def render(self, name):
        folder = Path(tempfile.mkdtemp(dir=self.root))
        pub.build_native_edition(self.fixture / name, folder / 'site')
        return folder / 'site'

    @staticmethod
    def catalog(path):
        return json.loads((path / 'catalogo.json').read_text())

    @staticmethod
    def inventory(path):
        return {str(p.relative_to(path)): hashlib.sha256(p.read_bytes()).hexdigest() for p in path.rglob('*') if p.is_file()}

    def test_native_selection_recovery_revision_and_legacy_migration(self):
        self.assertIn('migração com backup OK', self.report)

    def test_two_works_share_the_same_canonical_character(self):
        catalog = self.catalog(self.output)
        self.assertEqual([item['id_obra'] for item in catalog], [item['snapshot']['work']['id'] for item in self.payload['editions']])
        people = []
        for item in catalog:
            info = json.loads((self.output / item['slug'] / 'dados/info-obra.json').read_text())
            people.append(info['personagens'][0]['id'])
            self.assertTrue((self.output / item['endereco_publico'] / 'capitulo-1/index.html').is_file())
            self.assertTrue((self.output / item['slug'] / 'offline.html').is_file())
        self.assertEqual(people[0], people[1])

    def test_reordering_and_removal_keep_public_addresses_and_numbers(self):
        reordered = self.render('catalogo-reordenado.json')
        original = self.catalog(self.output)
        self.assertEqual([item['id_obra'] for item in self.catalog(reordered)], [item['id_obra'] for item in reversed(original)])
        for item in original:
            path = Path(item['slug']) / 'dados/estrutura.json'
            self.assertEqual(json.loads((self.output / path).read_text()), json.loads((reordered / path).read_text()))
            self.assertTrue((reordered / item['endereco_publico'] / 'capitulo-1/index.html').is_file())
        removed = self.render('catalogo-sem-mares.json')
        self.assertEqual(len(self.catalog(removed)), 1)
        self.assertFalse((removed / 'estacao-inde').exists())
        self.assertTrue((removed / 'observatorio-das-estrelas/capitulo-1/index.html').is_file())

    def test_empty_catalog_really_exports_no_work(self):
        output = self.render('catalogo-vazio.json')
        self.assertEqual(self.catalog(output), [])
        self.assertFalse(list(output.glob('obra-*')))
        self.assertTrue((output / 'home/index.html').is_file())

    def test_public_zip_excludes_unselected_work_and_private_data(self):
        with ZipFile(self.output.parent / 'site-publico.zip') as archive:
            content = b'\n'.join(archive.read(name) for name in archive.namelist())
            self.assertFalse(any(name.endswith(('.sqlite3', '.inde')) or 'retrato' in name for name in archive.namelist()))
        for marker in (b'SEGREDO-TERCEIRA-OBRA-PUB02B', b'SEGREDO-PRIVADO-INDE-PUB01', b'Arquivo de bordo', b'catalogo-preparado'):
            self.assertNotIn(marker, content)
        for name, expected in json.loads((self.output / 'inde-publicacao.json').read_text())['arquivos'].items():
            self.assertEqual(hashlib.sha256((self.output / name).read_bytes()).hexdigest(), expected)

    def test_later_work_failure_keeps_previous_output(self):
        payload = deepcopy(self.payload)
        payload['editions'][1]['snapshot']['units'][0]['documents'][0]['formatting'] = [{'style': 'font-family', 'start': 0, 'end': 2, 'value': 'unmapped'}]
        source = self.root / 'incompatible.json'
        source.write_text(json.dumps(payload))
        before = self.inventory(self.output)
        target = self.root / 'invalid/site'
        with self.assertRaises(ValueError):
            pub.build_native_edition(source, target)
        self.assertFalse(target.exists())
        self.assertEqual(before, self.inventory(self.output))

    def test_shared_id_with_different_definition_is_refused(self):
        payload = deepcopy(self.payload)
        payload['editions'][1]['snapshot']['entities'][0]['summary'] = 'Outra descrição para a mesma identidade'
        source = self.root / 'resource-conflict.json'
        source.write_text(json.dumps(payload))
        target = self.root / 'resource-conflict/site'
        with self.assertRaisesRegex(ValueError, 'incompatíveis'):
            pub.build_native_edition(source, target)
        self.assertFalse(target.exists())

    def test_combining_unrelated_project_sources_is_refused(self):
        payload = deepcopy(self.payload)
        payload['editions'][1]['snapshot']['project_id'] = 'outro-projeto'
        source = self.root / 'cross-project.json'
        source.write_text(json.dumps(payload))
        with self.assertRaisesRegex(ValueError, 'Projetos distintos'):
            pub.build_native_edition(source, self.root / 'cross-project/site')


if __name__ == '__main__':
    unittest.main()
