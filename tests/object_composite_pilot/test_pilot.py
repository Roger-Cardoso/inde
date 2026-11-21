"""P2-A acceptance at the real C++ process/SQLite boundary; synthetic data only."""
import base64
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

BINARY = str(Path(sys.argv.pop(1)).resolve()) if __name__ == '__main__' else None


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(width=80, height=48, color=(70, 128, 126, 255)):
    header = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)
    # Independently generated standards-based input, no product encoder involved.
    rows = (b'\0' + bytes(color) * width) * height
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


class Client:
    def __init__(self, folder, profile='p2a', seed=None, binary=None, fault=None):
        self.folder = Path(folder)
        args = [binary or BINARY, '--workspace', str(folder), '--profile', profile]
        if seed is not None:
            args += ['--seed-p2b' if profile=='p2c' else '--seed-p2a', str(seed)]
        if fault is not None: args += ['--test-fault', fault]
        self.process = subprocess.Popen(args, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
        self.state = self.send({'op': 'view'})['state']

    def send(self, request):
        self.process.stdin.write(json.dumps(request, ensure_ascii=False) + '\n')
        self.process.stdin.flush()
        line = self.process.stdout.readline()
        if not line:
            raise AssertionError('C++ process ended: ' + self.process.stderr.read())
        response = json.loads(line)
        if 'state' in response:
            self.state = response['state']
        return response

    def op(self, op, **values):
        return self.send({'op': op, 'expected': self.state['revision'], **values})

    def image(self, data=None, before=None, **values):
        return self.op('import', data=base64.b64encode(data if data is not None else png()).decode(),
                       before=before if before is not None else self.state['paragraphs'][1]['id'],
                       caption=values.get('caption', 'Figura sintética'), alt=values.get('alt', 'Campo verde'),
                       decorative=values.get('decorative', False))

    def close(self, expected=0):
        if self.process.stdout.closed: return
        if self.process.stdin and not self.process.stdin.closed:
            self.process.stdin.close()
        self.process.wait(timeout=15)
        error = self.process.stderr.read()
        self.process.stdout.close()
        self.process.stderr.close()
        if self.process.returncode != expected:
            raise AssertionError(f'C++ exit {self.process.returncode}: {error}')


class Pilot(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='obj01-p2a-test-')
        self.folder = Path(self.tmp.name) / 'piloto'
        self.client = Client(self.folder)
        self.origin = (self.folder / 'origem-documento.json').read_bytes()

    def tearDown(self):
        self.client.close()
        self.assertEqual((self.folder / 'origem-documento.json').read_bytes(), self.origin)
        self.tmp.cleanup()

    def accepted(self, result):
        self.assertTrue(result['accepted'], result)
        return result['state']

    def refused_unchanged(self, request, error):
        before = self.client.state
        result = request()
        self.assertFalse(result['accepted'], result)
        self.assertEqual(result['error'], error)
        self.assertEqual(result['state'], before)

    def test_p01_capture_and_private_layers(self):
        s = self.client.state
        self.assertEqual(len(s['paragraphs']), 3)
        self.assertIn('Água', s['paragraphs'][0]['text'])
        self.assertIn('🙂', s['paragraphs'][2]['text'])
        self.assertEqual(s['private_alternatives'], 1)
        self.assertEqual(s['visibility_profiles'], 1)
        self.assertEqual([len(g) for g in s['reading_groups']], [1, 2])
        self.assertIn('FOLHA PRIVADA', self.origin.decode())
        self.assertNotIn('FOLHA PRIVADA', json.dumps(s, ensure_ascii=False))
        self.assertFalse(s['dirty'])

    def test_p02_real_png_and_distinct_identities(self):
        body = self.client.state['source_body_sha256']
        s = self.accepted(self.client.image())
        f = s['figures'][0]
        self.assertEqual(len({f['id'], f['object'], f['resource']}), 3)
        self.assertEqual((f['width'], f['height']), (80, 48))
        self.assertEqual(f['sha256'], hashlib.sha256(png()).hexdigest())
        resource = self.client.send({'op': 'image', 'resource': f['resource']})
        self.assertEqual(base64.b64decode(resource['data']), png())
        self.assertEqual(s['source_body_sha256'], body)

    def test_p03_insert_between_complete_groups(self):
        original = list(self.client.state['author_order'])
        s = self.accepted(self.client.image())
        self.assertEqual(s['author_order'], [original[0], s['figures'][0]['id'], *original[1:]])
        self.assertEqual(s['author_order'], s['visual_order'])
        self.refused_unchanged(lambda: self.client.image(before=original[2]), 'reading_group_split')

    def test_p04_row_profile_and_stack(self):
        s = self.accepted(self.client.image())
        f = s['figures'][0]['id']
        para = s['paragraphs'][1]['id']
        order = list(s['author_order'])
        s = self.accepted(self.client.op('arrange', id=f, paragraph=para, first=.4, second=.6, gap=24))
        self.assertEqual(s['rows'][0]['children'], [f, para])
        self.assertEqual((s['rows'][0]['first'], s['rows'][0]['second']), (.4, .6))
        self.assertEqual(s['author_order'], order)
        self.assertEqual(s['visual_order'], order)
        self.refused_unchanged(lambda: self.client.op('arrange', id=f, paragraph=para, first=.5, second=.6, gap=24), 'invalid_row_profile')
        self.refused_unchanged(lambda: self.client.op('arrange', id=f, paragraph=para, first=.5, second=.5, gap=-1), 'invalid_row_profile')
        self.assertEqual(self.accepted(self.client.op('stack', id=f))['rows'], [])

    def test_p05_reuse_and_move_only_selected_use(self):
        s = self.accepted(self.client.image())
        first = s['figures'][0]
        s = self.accepted(self.client.op('duplicate', id=first['id'], before=''))
        second = s['figures'][1]
        self.assertNotEqual(first['id'], second['id'])
        self.assertEqual(first['object'], second['object'])
        self.assertEqual(first['resource'], second['resource'])
        original = list(s['author_order'])
        s = self.accepted(self.client.op('move', id=first['id'], before=s['paragraphs'][0]['id'], intent='authorial'))
        self.assertEqual(s['author_order'], [first['id'], *[i for i in original if i != first['id']]])
        self.assertEqual(s['figures'], [first, second])
        self.assertEqual(s['author_order'][-1], second['id'])

    def test_visual_intent_requires_explicit_restore(self):
        s = self.accepted(self.client.image())
        image = s['figures'][0]['id']
        original = list(s['author_order'])
        s = self.accepted(self.client.op('move', id=image, before='', intent='visual'))
        self.assertEqual(s['author_order'], original)
        self.assertFalse(s['aligned'])
        self.refused_unchanged(lambda: self.client.op('move', id=image, before='', intent='authorial'), 'authorial_requires_alignment')
        s = self.accepted(self.client.op('restore'))
        self.assertEqual(s['visual_order'], original)
        self.assertTrue(s['aligned'])

    def test_row_move_preserves_other_use_and_source(self):
        s = self.accepted(self.client.image())
        image = s['figures'][0]['id']
        self.accepted(self.client.op('arrange', id=image, paragraph=s['paragraphs'][1]['id'], first=.5, second=.5, gap=20))
        s = self.accepted(self.client.op('move', id=image, before='', intent='authorial'))
        self.assertEqual(s['rows'], [])
        self.assertEqual(s['author_order'][-1], image)
        self.assertEqual(s['author_order'], s['visual_order'])

    def test_p09_save_new_process_without_external_png(self):
        file = Path(self.tmp.name) / 'entrada.png'
        file.write_bytes(png())
        s = self.accepted(self.client.image(data=file.read_bytes(), caption='<script>literal</script>', alt='Imagem & texto'))
        f = s['figures'][0]
        s = self.accepted(self.client.op('arrange', id=f['id'], paragraph=s['paragraphs'][1]['id'], first=.3, second=.7, gap=18))
        s = self.accepted(self.client.op('save'))
        self.assertFalse(s['dirty'])
        file.unlink()
        self.client.close()
        self.client = Client(self.folder)
        reopened = self.client.state
        for key in ('author_order','visual_order','paragraphs','figures','rows','selection','source_sha256','source_body_sha256'):
            self.assertEqual(s[key], reopened[key], key)
        self.assertEqual(base64.b64decode(self.client.send({'op':'image','resource':f['resource']})['data']), png())
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
            self.assertEqual(db.execute('PRAGMA foreign_key_check').fetchall(), [])
            self.assertEqual(db.execute('SELECT data FROM blobs').fetchone()[0], png())

    def test_fractional_profile_roundtrips_without_precision_loss(self):
        s = self.accepted(self.client.image())
        first = .12345678912345678
        gap = 20.123456789012345
        s = self.accepted(self.client.op('arrange', id=s['figures'][0]['id'], paragraph=s['paragraphs'][1]['id'], first=first, second=1-first, gap=gap))
        self.assertEqual(s['rows'][0]['first'], first)
        self.assertEqual(s['rows'][0]['gap'], gap)
        self.accepted(self.client.op('save'))
        reopened = self.accepted(self.client.op('reopen', discard_unsaved=False))
        self.assertEqual(reopened['rows'], s['rows'])

    def test_failed_sqlite_write_rolls_back_blobs_and_snapshot(self):
        self.accepted(self.client.image())
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            before = db.execute('SELECT * FROM saved').fetchall()
            db.execute("CREATE TRIGGER refuse_save BEFORE UPDATE ON saved BEGIN SELECT RAISE(ABORT,'injected'); END")
        self.refused_unchanged(lambda: self.client.op('save'), 'sqlite_step_failed')
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            self.assertEqual(db.execute('SELECT count(*) FROM blobs').fetchone()[0], 0)
            self.assertEqual(db.execute('SELECT * FROM saved').fetchall(), before)

    def test_missing_blob_refuses_open_without_partial_state(self):
        self.accepted(self.client.image())
        self.accepted(self.client.op('save'))
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            db.execute('DELETE FROM blobs')
        self.refused_unchanged(lambda: self.client.op('reopen', discard_unsaved=False), 'blob_missing')

    def test_unknown_snapshot_version_is_not_overwritten(self):
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            raw = db.execute('SELECT snapshot FROM saved').fetchone()[0]
            value = json.loads(raw)
            value['format'] = 'future-version'
            raw = json.dumps(value, ensure_ascii=False)
            db.execute('UPDATE saved SET snapshot=?,sha256=?', (raw, hashlib.sha256(raw.encode()).hexdigest()))
        self.refused_unchanged(lambda: self.client.op('reopen', discard_unsaved=False), 'unsupported_snapshot_version')

    def test_unsaved_reopen_requires_confirmation(self):
        self.accepted(self.client.image())
        self.refused_unchanged(lambda: self.client.op('reopen', discard_unsaved=False), 'unsaved_confirmation_required')
        s = self.accepted(self.client.op('reopen', discard_unsaved=True))
        self.assertEqual(s['figures'], [])
        self.assertFalse(s['unsaved_recovery'])
        self.assertFalse(s['dirty'])

    def test_invalid_png_crc_and_animation_are_atomic(self):
        self.refused_unchanged(lambda: self.client.image(data=b'nao png'), 'invalid_png')
        corrupt = bytearray(png()); corrupt[50] ^= 1
        self.refused_unchanged(lambda: self.client.image(data=bytes(corrupt)), 'png_crc_mismatch')
        animated = png()[:33] + chunk(b'acTL', struct.pack('>II',1,0)) + png()[33:]
        self.refused_unchanged(lambda: self.client.image(data=animated), 'animated_png_not_supported')
        self.refused_unchanged(lambda: self.client.image(data=png()+b'extra'), 'invalid_png_end')
        self.refused_unchanged(lambda: self.client.image(data=png(width=8193, height=1)), 'png_dimensions_limit')
        self.refused_unchanged(lambda: self.client.image(alt=''), 'alt_text_required')

    def test_stale_revision_and_unknown_operation(self):
        s = self.accepted(self.client.image())
        self.refused_unchanged(lambda: self.client.send({'op':'save','expected':s['revision']-1}), 'stale_session_revision')
        self.refused_unchanged(lambda: self.client.op('delete_everything'), 'unsupported_operation')
        self.refused_unchanged(lambda: self.client.op('save', unexpected=True), 'unknown_or_missing_fields')

    def test_cas_conflict_does_not_overwrite(self):
        self.accepted(self.client.image())
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            db.execute('UPDATE saved SET revision=revision+1')
        self.refused_unchanged(lambda: self.client.op('save'), 'persisted_revision_conflict')
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            self.assertEqual(db.execute('SELECT count(*) FROM blobs').fetchone()[0], 0)
        self.assertTrue(self.client.state['dirty'])

    def test_corrupt_blob_refuses_reopen_without_partial_state(self):
        self.accepted(self.client.image())
        self.accepted(self.client.op('save'))
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            db.execute('UPDATE blobs SET data=?', (png(color=(255,0,0,255)),))
        self.refused_unchanged(lambda: self.client.op('reopen', discard_unsaved=False), 'blob_hash_mismatch')

    def test_unknown_database_version_and_frozen_source_are_preserved(self):
        with sqlite3.connect(self.folder / 'obj01-piloto.sqlite3') as db:
            db.execute('PRAGMA user_version=999')
        self.refused_unchanged(lambda: self.client.op('reopen', discard_unsaved=False), 'unsupported_database_version')
        self.refused_unchanged(lambda: self.client.op('save'), 'unsupported_database_version')

    def test_exclusive_workspace_lease_and_legacy_path_refusal(self):
        p = subprocess.run([BINARY,'--workspace',str(self.folder)], input='',capture_output=True,text=True,timeout=15)
        self.assertEqual(p.returncode, 1)
        self.assertIn('workspace_already_open', p.stderr)
        legacy = Path(self.tmp.name) / 'obra.inde'
        p = subprocess.run([BINARY,'--workspace',str(legacy)], input='',capture_output=True,text=True,timeout=15)
        self.assertEqual(p.returncode, 1)
        self.assertIn('legacy_project_path_forbidden', p.stderr)
        self.assertFalse(legacy.exists())


if __name__ == '__main__':
    unittest.main(verbosity=2)
