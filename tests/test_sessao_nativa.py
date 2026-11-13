"""PUB-02: fixtures sintéticas, serviço compartilhado e pacote público."""
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
import ponte_inde
import publicacao as pub


@unittest.skipUnless((BASE / 'build-nativo/inde_publication_tests').is_file(), 'Compile o serviço nativo para validar PUB-02')
class NativeSessionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='pub02-suite-')
        cls.root = Path(cls.temp.name)
        cls.fixture = cls.root / 'fixture'
        subprocess.run([BASE / 'build-nativo/inde_web_bridge', '--demo', cls.fixture], check=True, capture_output=True, text=True)
        result = subprocess.run([BASE / 'build-nativo/inde_publication_tests', cls.fixture], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        cls.report = result.stdout
        cls.payload = json.loads((cls.fixture / 'edicao-para-gerar.json').read_text())

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_service_crud_persistence_conflicts_restore_and_copy(self):
        self.assertIn('persistência OK', self.report)

    def test_public_numbers_come_from_persistent_reservations(self):
        source = deepcopy(self.payload['snapshot'])
        for unit, number in zip(source['units'], (7, 11)):
            unit['documents'][0]['public_number'] = number
        work, _ = ponte_inde.project_work(source)
        self.assertEqual([d['numero'] for d in work['documentos']], [7, 11])

    def test_bundled_reader_and_public_package_exclude_private_source(self):
        output = self.root / 'generation/site'
        result = pub.build_native_edition(self.fixture / 'edicao-para-gerar.json', output)
        self.assertEqual(result['site'], str(output))
        with ZipFile(output.parent / 'site-publico.zip') as archive:
            self.assertTrue(any('capitulo-1' in name for name in archive.namelist()))
            self.assertFalse(any(name.endswith(('.sqlite3', '.inde')) or 'retrato' in name for name in archive.namelist()))
            content = b'\n'.join(archive.read(name) for name in archive.namelist())
        self.assertNotIn(b'SEGREDO-PRIVADO-INDE-PUB01', content)
        self.assertNotIn(b'Refer\xc3\xaancia privada', content)
        self.assertIn(b'/biblioteca/', content)
        receipt = json.loads((output / 'inde-publicacao.json').read_text())
        for name, expected in receipt['arquivos'].items():
            self.assertEqual(hashlib.sha256((output / name).read_bytes()).hexdigest(), expected)

    def test_removed_or_incompatible_edition_is_refused(self):
        data = deepcopy(self.payload)
        data['edition']['removed'] = True
        path = self.root / 'retirada.json'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'retirados'):
            pub.build_native_edition(path, self.root / 'refused/site')
        self.assertFalse((self.root / 'refused/site').exists())


if __name__ == '__main__':
    unittest.main()
