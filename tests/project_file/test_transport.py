"""Finite compatibility/failure cases; every project is newly created here."""
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest
import uuid
BINARY=Path(sys.argv.pop(1)).resolve()

class ProjectFile(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='inde-p5-transport-')
        self.root=Path(self.tmp.name)
        self.source=self.root/'Origem com acentos á.inde'
        self.source.mkdir();(self.source/'data').mkdir();(self.source/'vazia').mkdir();(self.source/'imagens').mkdir()
        self.owner=str(uuid.uuid4())
        (self.source/'manifest.json').write_text(json.dumps(dict(format='INDE',format_version=2,project_id=self.owner,name='Fictício'),ensure_ascii=False))
        with sqlite3.connect(self.source/'data/project.sqlite3') as db:
            db.executescript('CREATE TABLE projects(id TEXT PRIMARY KEY); CREATE TABLE schema_migrations(version INTEGER); CREATE TABLE documents(id TEXT PRIMARY KEY,content TEXT);')
            db.execute('INSERT INTO projects VALUES(?)',(self.owner,));db.execute('INSERT INTO schema_migrations VALUES(37)');db.execute('INSERT INTO documents VALUES(?,?)',('doc','Texto inicial 🙂'))
        (self.source/'imagens/recurso.bin').write_bytes(bytes(range(256))*4)
        (self.source/'nota.txt').write_text('Conteúdo privado preservado.\n')
        self.package=self.root/'Projeto único.inde'
    def tearDown(self): self.tmp.cleanup()
    def run_cli(self,*args,ok=True):
        r=subprocess.run([str(BINARY),*map(str,args)],capture_output=True,text=True,timeout=40)
        if ok:self.assertEqual(r.returncode,0,r.stdout+r.stderr)
        else:self.assertNotEqual(r.returncode,0,r.stdout+r.stderr)
        return json.loads(r.stdout) if ok else r
    def inspect(self):return self.run_cli('inspecionar',self.package)
    def pack(self):return self.run_cli('converter',self.source,self.package)
    def files(self,path):return {p.relative_to(path).as_posix():p.read_bytes() for p in path.rglob('*') if p.is_file()}
    def request(self,edits,state=None):
        s=state or self.inspect()
        value=dict(format='INDE-P5-REQUEST-1',project_id=s['project_id'],store_id=s['store_id'],expected_sequence=s['sequence'],expected_hash=s['hash'],label='Ensaio de integridade',edits=edits)
        p=self.root/'pedido.json';p.write_text(json.dumps(value,ensure_ascii=False));return p
    def test_exact_roundtrip_without_original_location(self):
        original=self.files(self.source);s=self.pack();self.assertEqual(s['sequence'],1)
        self.assertTrue(self.package.is_file());self.assertEqual(self.package.read_bytes()[:16],b'SQLite format 3\0')
        self.source.rename(self.root/'Origem reservada.inde')
        target=self.root/'Outro local.inde';self.run_cli('reabrir',self.package,target)
        self.assertEqual(self.files(target),original);self.assertTrue((target/'vazia').is_dir())
        self.run_cli('verificar',self.package)
    def test_update_versions_restore_both_directions_and_resource_reuse(self):
        first=self.pack();before=self.inspect();initial_count=None
        with sqlite3.connect(self.package) as db:initial_count=db.execute('select count(*) from blobs').fetchone()[0]
        work=self.root/'Trabalho.inde';self.run_cli('reabrir',self.package,work)
        with sqlite3.connect(work/'data/project.sqlite3') as db:db.execute('UPDATE documents SET content=?',('Texto alterado',))
        second=self.run_cli('registrar',self.package,work,first['sequence'],first['hash'],'Editar texto')
        self.assertEqual(second['sequence'],2)
        old={r['key']:r for r in before['records']};new={r['key']:r for r in second['records']}
        self.assertEqual(new['file/imagens/recurso.bin']['version'],old['file/imagens/recurso.bin']['version'])
        self.assertNotEqual(new['file/data/project.sqlite3']['version'],old['file/data/project.sqlite3']['version'])
        with sqlite3.connect(self.package) as db:self.assertEqual(db.execute('select count(*) from blobs').fetchone()[0],initial_count+1)
        restored=self.run_cli('restaurar',self.package,1,second['sequence'],second['hash'])
        self.assertEqual(restored['sequence'],3);self.assertEqual(restored['hash'],first['hash'])
        target=self.root/'Estado posterior preservado.inde';self.run_cli('reabrir',self.package,target,2)
        with sqlite3.connect(target/'data/project.sqlite3') as db:self.assertEqual(db.execute('select content from documents').fetchone()[0],'Texto alterado')
        later=self.run_cli('restaurar',self.package,2,restored['sequence'],restored['hash'])
        self.assertEqual(later['hash'],second['hash']);self.assertEqual(len(self.run_cli('historico',self.package)),5)
    def test_stale_request_and_existing_destination_preserve_bytes(self):
        first=self.pack();before=self.package.read_bytes()
        self.run_cli('restaurar',self.package,0,first['sequence'],first['hash'])
        stable=self.package.read_bytes();r=self.run_cli('restaurar',self.package,1,first['sequence'],first['hash'],ok=False)
        self.assertIn('stale_state',r.stderr);self.assertEqual(self.package.read_bytes(),stable)
        self.run_cli('converter',self.source,self.package,ok=False);self.assertEqual(self.package.read_bytes(),stable)
        self.assertNotEqual(before,stable)
        existing=self.root/'Existente.inde';existing.mkdir();(existing/'guardar').write_text('Não substituir')
        self.run_cli('reabrir',self.package,existing,ok=False);self.assertEqual((existing/'guardar').read_text(),'Não substituir')
    def test_source_wal_is_refused_without_touching_source(self):
        with sqlite3.connect(self.source/'data/project.sqlite3') as db:self.assertEqual(db.execute('pragma journal_mode=wal').fetchone()[0],'wal')
        before=self.files(self.source)
        r=self.run_cli('converter',self.source,self.package,ok=False)
        self.assertIn('source_wal',r.stderr);self.assertEqual(self.files(self.source),before);self.assertFalse(self.package.exists())
    def test_unknown_links_and_destination_inside_source_are_refused(self):
        before=self.files(self.source)
        r=self.run_cli('converter',self.source,self.source/'Dentro.inde',ok=False);self.assertIn('destination_inside_source',r.stderr)
        (self.source/'atalho').symlink_to(self.root)
        self.run_cli('converter',self.source,self.package,ok=False)
        self.assertFalse(self.package.exists());(self.source/'atalho').unlink();self.assertEqual(self.files(self.source),before)
    def test_corruption_in_past_state_is_detected_even_after_restore(self):
        first=self.pack();s=self.inspect();record=next(r for r in s['records'] if r['key']=='file/nota.txt')
        value={k:record[k] for k in ['kind','payload','blob','references']};value['payload']['bytes']=999
        self.run_cli('aplicar',self.package,self.request([dict(key=record['key'],expected_version=record['version'],value=value)],s))
        head=self.inspect();self.run_cli('restaurar',self.package,first['sequence'],head['sequence'],head['hash'])
        with sqlite3.connect(self.package) as db:
            trigger=db.execute("select sql from sqlite_schema where name='immutable_record_versions_UPDATE'").fetchone()[0]
            db.execute('DROP TRIGGER immutable_record_versions_UPDATE')
            db.execute("UPDATE record_versions SET payload='{}' WHERE id=(SELECT max(id) FROM record_versions)")
            db.execute(trigger)
        r=self.run_cli('verificar',self.package,ok=False);self.assertIn('state_hash',r.stderr)
    def test_materialization_rejects_path_escape_before_writing(self):
        self.pack();s=self.inspect();blob=next(r['blob'] for r in s['records'] if r['key']=='file/nota.txt')
        data=dict(kind='legacy-file',payload=dict(path='../fora',bytes=0),blob=blob,references=['project'])
        self.run_cli('aplicar',self.package,self.request([dict(key='file/../fora',expected_version=0,value=data)],s))
        target=self.root/'Perigoso.inde';r=self.run_cli('reabrir',self.package,target,ok=False)
        self.assertIn('unsafe_path',r.stderr);self.assertFalse((self.root/'fora').exists());self.assertFalse(target.exists())
    def test_other_project_and_noop_capture(self):
        first=self.pack()
        again=self.run_cli('registrar',self.package,self.source,first['sequence'],first['hash'],'Sem alterações')
        self.assertEqual(again['sequence'],first['sequence'])
        manifest=self.source/'manifest.json';m=json.loads(manifest.read_text());m['project_id']=str(uuid.uuid4());manifest.write_text(json.dumps(m))
        r=self.run_cli('registrar',self.package,self.source,again['sequence'],again['hash'],'Outro projeto',ok=False)
        self.assertIn('source_identity',r.stderr);self.assertEqual(self.inspect()['sequence'],first['sequence'])
    def test_unknown_file_and_sqlite_are_untouched(self):
        for data in [b'Outra obra que deve permanecer intacta.', b'SQLite format 3\0' + bytes(200)]:
            self.package.write_bytes(data)
            self.run_cli('verificar',self.package,ok=False)
            self.assertEqual(self.package.read_bytes(),data)
        self.package.unlink()
        with sqlite3.connect(self.package) as db:db.execute('CREATE TABLE unrelated(a)')
        data=self.package.read_bytes()
        self.run_cli('verificar',self.package,ok=False)
        self.assertEqual(self.package.read_bytes(),data)
    def test_outer_wal_and_truncation_are_refused(self):
        self.pack()
        with sqlite3.connect(self.package) as db:self.assertEqual(db.execute('pragma journal_mode=wal').fetchone()[0],'wal')
        before=self.package.read_bytes()
        r=self.run_cli('verificar',self.package,ok=False)
        self.assertIn('unsupported_journal',r.stderr)
        self.assertEqual(self.package.read_bytes(),before)
        self.package.write_bytes(before[:80])
        r=self.run_cli('verificar',self.package,ok=False)
        self.assertIn('unknown_format',r.stderr)
        self.assertEqual(self.package.read_bytes(),before[:80])
    def test_source_invalid_foreign_key_is_refused(self):
        with sqlite3.connect(self.source/'data/project.sqlite3') as db:
            db.executescript('CREATE TABLE dangling(value TEXT REFERENCES projects(id)); INSERT INTO dangling VALUES("absent");')
        before=self.files(self.source)
        r=self.run_cli('converter',self.source,self.package,ok=False)
        self.assertIn('source_foreign_key',r.stderr)
        self.assertEqual(self.files(self.source),before)
        self.assertFalse(self.package.exists())
    def test_demo_and_strict_request_fields(self):
        result=self.run_cli('demonstrar',self.package)
        self.assertEqual(result['initial']['hash'],result['restored']['hash']);self.assertTrue(result['later_state_preserved'])
        s=self.inspect();p=self.request([],s);v=json.loads(p.read_text());v['unexpected']=True;p.write_text(json.dumps(v))
        self.run_cli('aplicar',self.package,p,ok=False);self.assertEqual(self.inspect()['sequence'],s['sequence'])

if __name__=='__main__':unittest.main(verbosity=2)
