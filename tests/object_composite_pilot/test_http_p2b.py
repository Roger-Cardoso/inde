"""P2-B browser controller operations use the real C++ aggregate."""
import json
from pathlib import Path
import sys
import unittest
import test_http as base

base.BINARY=Path(sys.argv.pop(1)).resolve()


class TableHttp(base.HttpPilot):
    profile='p2b'

    def op(self,op,**values):
        current=self.state()
        status,_,body=self.request('/api/request',{'op':op,'expected':current['revision'],**values})
        self.assertEqual(status,200)
        result=json.loads(body)
        self.assertTrue(result['accepted'],result)
        return result['state']

    def test_table_cells_history_and_reopen(self):
        s=self.op('create_table',before=self.state()['paragraphs'][1]['id'],title='Notas HTTP',header=True,cells=['A','B','C','1','2','🙂'])
        t=s['tables'][0];cell=t['cells'][4]['id']
        s=self.op('edit_cell',id=t['id'],cell=cell,text='Água\n漢🙂')
        self.assertEqual(s['tables'][0]['cells'][4]['text'],'Água\n漢🙂')
        self.op('undo')
        self.assertEqual(self.state()['tables'][0]['cells'][4]['text'],'2')
        self.op('redo')
        self.op('save')
        s=self.op('reopen',discard_unsaved=False)
        self.assertEqual(s['tables'][0]['cells'][4]['text'],'Água\n漢🙂')
        self.assertFalse(s['can_undo'])
        self.op('remove',id=t['id'])
        self.assertEqual(self.state()['tables'],[])
        self.op('undo')
        self.assertEqual(self.state()['tables'][0],s['tables'][0])

    def test_invalid_cell_has_no_effect(self):
        s=self.op('create_table',before='',title='Notas',header=False,cells=['','','','','',''])
        t=s['tables'][0]
        status,_,raw=self.request('/api/request',{'op':'edit_cell','expected':s['revision'],'id':t['id'],'cell':t['cells'][0]['id'],'text':'x'*4097})
        self.assertEqual(status,200)
        response=json.loads(raw)
        self.assertFalse(response['accepted'])
        self.assertEqual(response['error'],'cell_text_limit')
        self.assertEqual(self.state(),s)


if __name__=='__main__':unittest.main(verbosity=2)
