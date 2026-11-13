"""Regressão da fronteira entre autoria estruturada e edição pública."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
import publicacao
import ponte_inde


@unittest.skipUnless((BASE / 'build-nativo/inde_integration_tests').is_file(),
                     'Compile a integração C++ antes da validação nativa')
class IntegratedAuthorshipTests(unittest.TestCase):
    def test_structured_authorship_remains_private_and_publication_uses_full_current_text(self):
        with tempfile.TemporaryDirectory(prefix='inde-integracao-') as directory:
            root = Path(directory) / 'fixture'
            subprocess.run([BASE / 'build-nativo/inde_web_bridge', '--demo', root],
                           check=True, capture_output=True, text=True)
            result = subprocess.run([BASE / 'build-nativo/inde_integration_tests', root],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            payload = root / 'integracao-para-gerar.json'
            source = json.loads(payload.read_text())['snapshot']
            self.assertEqual(source['source_schema'], 37)
            self.assertIn('cópia OK', result.stdout)
            output = root / 'geracao/site'
            publicacao.build_native_edition(payload, output)
            with ZipFile(output.parent / 'site-publico.zip') as archive:
                content = b'\n'.join(archive.read(name) for name in archive.namelist())
                self.assertIn(b'CORPO INTEGRAL', content)
                self.assertIn(b'PUBLICO CORRENTE', content)
                self.assertNotIn(b'ALTERNATIVA PRIVADA INT01', content)
                self.assertFalse(any(name.endswith('.sqlite3') for name in archive.namelist()))
            # Schemas desconhecidos não passam pela ampliação explícita 28/37.
            source['source_schema'] = 38
            with self.assertRaisesRegex(ValueError, 'base'):
                ponte_inde.project_work(source)


if __name__ == '__main__':
    unittest.main()
