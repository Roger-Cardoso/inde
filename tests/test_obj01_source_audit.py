"""Verifier adversarial cases use tiny new archives, never the pinned originals."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import audit_obj01_sources as audit


class ArchiveAuditTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='obj01-source-verifier-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'metadata').mkdir()
        for name in ['old', 'original', 'integrated']:
            self.archive(name)
        self.manifests()
        self.patch = patch.object(audit, 'ROOT', self.root)
        self.patch.start()
        self.addCleanup(self.patch.stop)

    def archive(self, name, member='source.cpp', kind=tarfile.REGTYPE, duplicate=False):
        with tarfile.open(self.root / (name + '.tar.gz'), 'w:gz') as target:
            info = tarfile.TarInfo(member)
            info.type = kind
            data = b'int main() {}\n'
            if kind == tarfile.SYMTYPE:
                info.linkname = '/tmp/outside-obj01'
            else:
                info.size = len(data)
            target.addfile(info, io.BytesIO(data) if kind == tarfile.REGTYPE else None)
            if duplicate:
                target.addfile(info, io.BytesIO(data))

    def manifests(self):
        def sha(name):
            return hashlib.sha256((self.root / (name + '.tar.gz')).read_bytes()).hexdigest()
        (self.root / 'metadata/BASE-INDE.json').write_text(json.dumps({
            'arquivo_fonte_local': 'old.tar.gz', 'arquivo_fonte_sha256': sha('old')}))
        (self.root / 'metadata/INTEGRACAO-INDE.json').write_text(json.dumps({
            'arquivo_original': 'original.tar.gz', 'sha256_original': sha('original'),
            'arquivo_integrado': 'integrated.tar.gz', 'sha256_integrado': sha('integrated'),
            'arquivos_integrados': {'source.cpp': hashlib.sha256(b'int main() {}\n').hexdigest()}}))

    def test_extract_and_compare_bytes(self):
        result = audit.audit(self.root / 'extracted')
        self.assertTrue(result['extracted_bytes_match'])
        self.assertEqual(len(result['archives']), 3)
        self.assertEqual(result['comparisons']['original37..integrated37']['changed'], [])

    def test_archive_hash_mismatch_precedes_extraction(self):
        (self.root / 'original.tar.gz').write_bytes(b'corruption')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'):
            audit.audit(self.root / 'not-created')
        self.assertFalse((self.root / 'not-created').exists())

    def test_manifest_inventory_mismatch(self):
        self.archive('integrated', 'another.cpp')
        self.manifests()
        with self.assertRaisesRegex(ValueError, 'inventory mismatch'):
            audit.audit()

    def test_unsafe_and_duplicate_members(self):
        for name, kind, duplicate in [('../escape', tarfile.REGTYPE, False),
                                      ('/absolute', tarfile.REGTYPE, False),
                                      ('link', tarfile.SYMTYPE, False),
                                      ('duplicate', tarfile.REGTYPE, True)]:
            with self.subTest(name=name):
                self.archive('old', name, kind, duplicate)
                self.manifests()
                with self.assertRaises(ValueError):
                    audit.audit(self.root / 'not-created')
                self.assertFalse((self.root / 'not-created').exists())

    def test_existing_destination_is_preserved(self):
        dest = self.root / 'existing'
        dest.mkdir()
        sentinel = dest / 'preserve'
        sentinel.write_text('unchanged')
        with self.assertRaises(FileExistsError):
            audit.audit(dest)
        self.assertEqual(sentinel.read_text(), 'unchanged')
