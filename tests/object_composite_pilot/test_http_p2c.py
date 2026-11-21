"""P2-C over the actual local controller, with the four shared safety tests."""
import json
from pathlib import Path
import sqlite3
import sys
import unittest
import test_http as base

base.BINARY = Path(sys.argv.pop(1)).resolve()


class RecoverableHttp(base.HttpPilot):
    profile = 'p2c'

    def op(self, op, accepted=True, **values):
        status, _, body = self.request('/api/request', {
            'op': op, 'expected': self.state()['revision'], **values})
        self.assertEqual(status, 200)
        result = json.loads(body)
        self.assertEqual(result['accepted'], accepted, result)
        return result

    def test_table_work_is_durable_without_promoting_saved(self):
        s = self.op('create_table', before='', title='HTTP recuperável',
                    header=True, cells=['A', 'B', 'C', '1', '2', '3'])['state']
        t = s['tables'][0]
        self.op('set_context', context='text')
        s = self.op('edit_cell', id=t['id'], cell=t['cells'][5]['id'],
                    text='Ainda não salvo 漢🙂')['state']
        with sqlite3.connect(Path(self.tmp.name)/'fixture'/'obj01-piloto.sqlite3') as db:
            self.assertEqual(json.loads(db.execute('SELECT snapshot FROM saved').fetchone()[0])['tables'], [])
            work = json.loads(db.execute('SELECT snapshot FROM working').fetchone()[0])
            self.assertIn('Ainda não salvo 漢🙂', json.dumps(work, ensure_ascii=False))
        reopened = self.op('reopen_working', discard_unsaved=True)['state']
        self.assertEqual(reopened['tables'], s['tables'])
        self.assertTrue(reopened['dirty'])
        self.assertFalse(reopened['can_undo'])

    def test_instruments_preserve_target_and_guard_wrong_operation(self):
        s = self.state()
        target = s['paragraphs'][0]['id']
        self.op('select', id=target)
        for context in ['text', 'composition', 'structure']:
            changed = self.op('set_context', context=context)['state']
            self.assertEqual(changed['selection'], target)
        self.op('set_context', context='composition')
        before = self.state()
        refused = self.op('create_table', accepted=False, before='', title='Não',
                          header=False, cells=['']*6)
        self.assertEqual(refused['error'], 'editing_context_required')
        self.assertEqual(self.state(), before)
        refused = self.op('export_legacy', accepted=False)
        self.assertEqual(refused['error'], 'composite_publication_not_supported')
        self.assertEqual(self.state(), before)

    def test_complete_text_group_move_uses_owner_order_and_frozen_hash(self):
        before = self.state()
        after = self.op('reorder_text', group=before['text_groups'][1]['id'],
                        before=before['paragraphs'][0]['id'])['state']
        self.assertEqual([p['id'] for p in after['paragraphs']],
                         [p['id'] for p in before['paragraphs'][1:]] +
                         [before['paragraphs'][0]['id']])
        self.assertEqual(after['source_sha256'], before['source_sha256'])
        self.assertNotEqual(after['source_body_sha256'], before['source_body_sha256'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
