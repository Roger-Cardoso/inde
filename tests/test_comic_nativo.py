"""COMIC-01: real C++ source + existing publication packaging boundary."""
import base64
from copy import deepcopy
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from urllib.parse import urljoin, urlsplit
from zipfile import ZipFile
BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
import publicacao as pub

class Links(HTMLParser):
    def __init__(self):
        super().__init__(); self.links = []
    def handle_starttag(self, tag, attrs):
        for k, v in attrs:
            if k in ('href', 'src'):
                self.links.append(v)

class NativeComic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        binary = BASE / 'build-nativo/inde_comic_tests'
        if not binary.is_file():
            raise unittest.SkipTest('Compile inde_comic_tests para verificar a integração C++ real.')
        cls.temp = tempfile.TemporaryDirectory(prefix='comic-native-tests-')
        cls.root = Path(cls.temp.name)
        cls.fixture = cls.root / 'fixture'
        result = subprocess.run([str(binary), str(cls.fixture), str(BASE / 'recursos')],
                                capture_output=True, text=True, timeout=120)
        if result.returncode:
            cls.temp.cleanup()
            raise RuntimeError(result.stdout + result.stderr)
        cls.native_result = result.stdout.strip()
        cls.payload = json.loads((cls.fixture / 'comic-preparado.json').read_text())
    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()
    def setUp(self):
        self.local = tempfile.TemporaryDirectory(dir=self.root)
        self.target = Path(self.local.name) / 'site'
    def tearDown(self):
        self.local.cleanup()
    def build(self, payload=None):
        return pub.build_native_comic(deepcopy(payload or self.payload), self.target)
    def test_native_model_persistence_conflicts_recovery_and_copy(self):
        self.assertIn('verificações nativas aprovadas', self.native_result)
    def test_same_publication_entry_point_dispatches_native_comic(self):
        result = pub.build_native_edition(self.fixture / 'comic-preparado.json', self.target)
        self.assertEqual(result['site'], str(self.target.resolve()))
        self.assertTrue((self.target / 'a-passagem/texto.html').is_file())
    def test_all_local_html_links_resolve_under_subdirectory(self):
        self.build()
        for path in self.target.rglob('*.html'):
            parser = Links(); parser.feed(path.read_text())
            page_url = 'https://example.test/biblioteca/' + str(path.relative_to(self.target))
            for href in parser.links:
                target = urlsplit(urljoin(page_url, href))
                self.assertTrue(target.path.startswith('/biblioteca/'), href)
                name = target.path[len('/biblioteca/'):]
                dest = self.target / name
                if dest.is_dir(): dest = dest / 'index.html'
                self.assertTrue(dest.is_file(), (path, href))
    def test_public_zip_manifest_and_private_sources(self):
        self.build()
        receipt = json.loads((self.target / 'inde-publicacao.json').read_text())
        self.assertEqual(receipt['media'], 'comic')
        for name, digest in receipt['arquivos'].items():
            self.assertEqual(hashlib.sha256((self.target/name).read_bytes()).hexdigest(), digest)
        with ZipFile(self.target.parent / 'site-publico.zip') as archive:
            self.assertIsNone(archive.testzip())
            self.assertFalse(any('sqlite' in n or 'retrato' in n or 'projeto' in n for n in archive.namelist()))
            content = archive.read('a-passagem/index.html').decode()
            self.assertIn('Som grave do portal.', content)
            self.assertNotIn('source_document', content)
    def test_continuous_text_is_script_free(self):
        self.build()
        text = (self.target / 'a-passagem/texto.html').read_text()
        self.assertNotIn('<script', text)
        self.assertIn('lang="ar"', text)
        self.assertIn('تستمر القصة', text)
    def test_version_and_media_cannot_silently_downgrade(self):
        for key, value in [('version', 2), ('media', 'novel')]:
            bad=deepcopy(self.payload); bad[key]=value
            with self.assertRaises(ValueError): self.build(bad)
            self.assertFalse(self.target.exists())
    def test_paths_cannot_escape_public_site(self):
        for name in ['../private.json', '/tmp/x.html', 'a/../../x.html', 'a\\x.html', 'comic/source.sqlite3']:
            bad=deepcopy(self.payload); bad['files'][name]=base64.b64encode(b'bad').decode()
            with self.assertRaises(ValueError): self.build(bad)
            self.assertFalse(self.target.exists())
    def test_generation_does_not_overwrite_previous_site(self):
        self.build()
        before=(self.target/'index.html').read_bytes()
        with self.assertRaises(ValueError): self.build()
        self.assertEqual((self.target/'index.html').read_bytes(),before)
    def test_domain_and_pages_path_follow_existing_configuration(self):
        for origin, cname in [('https://author.github.io',False),('https://comic.example.test',True)]:
            bad=deepcopy(self.payload);bad['site']['origin']=origin
            target=self.target.parent/('domain' if cname else 'pages')/'site'
            pub.build_native_comic(bad,target)
            self.assertEqual((target/'CNAME').exists(),cname)
            self.assertEqual(json.loads((target.parent/'previa.json').read_text())['caminho_base'],'/biblioteca')
    def test_removed_site_and_invalid_base64_are_rejected(self):
        bad=deepcopy(self.payload);bad['site']['removed']=True
        with self.assertRaises(ValueError): self.build(bad)
        bad=deepcopy(self.payload);bad['files']['index.html']='!!!'
        with self.assertRaises(ValueError): self.build(bad)
        self.assertFalse(self.target.exists())
