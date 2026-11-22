"""Verify pinned native math dependencies, including documented local patches."""
import hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]/'cpp/vendor'
class NativeSources(unittest.TestCase):
 def test_imported_bytes_and_explicit_patch(self):
  checked=0
  for name in ['microtex','tinyxml2']:
   root=ROOT/name;meta=json.loads((root/'PROVENANCE.json').read_text())
   self.assertEqual(len(meta['commit']),40)
   patches=json.loads((root/'PATCHES.json').read_text()) if (root/'PATCHES.json').exists() else {}
   for relative,digest in meta['files'].items():
    path=(root/relative).resolve();self.assertTrue(path.is_relative_to(root.resolve()));self.assertFalse((root/relative).is_symlink())
    if relative in patches:
     patch=patches[relative];self.assertEqual(patch['upstream_sha256'],digest);self.assertTrue(patch['reason']);digest=patch['patched_sha256']
    self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),digest,relative);checked+=1
  print(f'Pinned native math files verified: {checked}')
if __name__=='__main__':unittest.main(verbosity=2)
