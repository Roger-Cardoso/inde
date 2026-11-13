from copy import deepcopy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

BASE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BASE))
import publicacao as pub
import ponte_inde


class ManagementContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.workshop = pub.Workshop(self.temp.name)
        self.record = self.workshop.create()

    def tearDown(self):
        self.workshop.close()
        self.temp.cleanup()

    def operate(self, action, **payload):
        self.record = self.workshop.operate(self.record['projeto']['id'], self.record['revisao'], action, payload)
        return self.record

    def test_work_and_chapter_crud_preserve_recovery_and_reserved_urls(self):
        self.operate('criar_obra')
        original_work = self.record['projeto']['obra']['id']
        self.operate('criar_capitulo')
        chapter = self.record['projeto']['obra']['documentos'][0]
        data = deepcopy(self.record['projeto'])
        data['obra']['documentos'][0].update(titulo='Texto editado', texto='Texto original com acentos: estação.')
        self.record = self.workshop.save(data, self.record['revisao'])
        self.operate('excluir_capitulo', id=chapter['id'])
        trash_id = self.record['projeto']['lixeira'][0]['id']
        self.operate('criar_capitulo')
        self.assertEqual(self.record['projeto']['obra']['documentos'][0]['numero'], 2)
        self.operate('restaurar_item', id=trash_id)
        restored = self.record['projeto']['obra']['documentos'][0]
        self.assertEqual(restored['id'], chapter['id'])
        self.assertEqual(restored['numero'], 1)
        self.assertEqual(restored['texto'], 'Texto original com acentos: estação.')
        self.assertFalse(restored['publico'])
        self.operate('excluir_obra')
        self.assertIsNone(self.record['projeto']['obra'])
        self.operate('restaurar_item', id=self.record['projeto']['lixeira'][0]['id'])
        self.assertEqual(self.record['projeto']['obra']['id'], original_work)
        self.assertFalse(self.record['projeto']['obra']['publica'])

    def test_resource_crud_and_reference_protection(self):
        self.record = self.workshop.create(True)
        primary = self.record['projeto']['obra']['personagem']['id']
        with self.assertRaisesRegex(ValueError, 'Recurso em uso'):
            self.operate('excluir_recurso', id=primary)
        self.operate('criar_recurso', tipo='personagem')
        new = self.record['recursos'][-1]
        data = deepcopy(self.record['projeto'])
        data['obra']['recursos'][-1].update(nome='Nara', descricao='Outra personagem.')
        self.record = self.workshop.save(data, self.record['revisao'])
        self.assertEqual(self.record['recursos'][-1]['nome'], 'Nara')
        self.assertEqual(self.record['recursos'][0]['id'], primary)
        self.operate('excluir_recurso', id=new['id'])
        self.operate('restaurar_item', id=self.record['projeto']['lixeira'][0]['id'])
        self.assertEqual(self.record['recursos'][-1]['id'], new['id'])
        self.assertEqual(self.record['recursos'][-1]['nome'], 'Nara')
        pub.build(self.record['projeto'], Path(self.temp.name) / 'edicao')

    def test_glossary_resource_is_protected_by_text_reference(self):
        self.record = self.workshop.create(True)
        target = self.record['projeto']['obra']['id'] + '-verbete'
        with self.assertRaisesRegex(ValueError, 'Recurso em uso'):
            self.operate('excluir_recurso', id=target)

    def test_site_removal_and_restore_preserve_identity_and_content(self):
        self.record = self.workshop.create(True)
        data = self.record['projeto']
        receipt = self.workshop.delete_site(data['id'], self.record['revisao'])
        self.assertFalse(self.workshop.path(data['id']).exists())
        self.assertEqual(self.workshop.trash_sites()[0]['site_id'], data['id'])
        restored = self.workshop.restore_site(receipt['recuperacao'])
        self.assertEqual(restored['projeto']['id'], data['id'])
        self.assertEqual(restored['projeto']['obra']['documentos'], data['obra']['documentos'])
        self.assertFalse(restored['projeto']['obra']['publica'])

    def test_tampered_recovery_refuses_restore(self):
        receipt = self.workshop.delete_site(self.record['projeto']['id'], self.record['revisao'])
        file = self.workshop.directory / 'lixeira' / receipt['recuperacao'] / 'site/projeto.json'
        data = json.loads(file.read_text())
        data['site']['nome'] = 'Alteração externa'
        pub.save_json(file, data)
        with self.assertRaisesRegex(ValueError, 'modificada'):
            self.workshop.restore_site(receipt['recuperacao'])
        self.assertTrue(file.exists())

    def test_stale_delete_and_management_do_not_modify_current_data(self):
        previous = self.record['revisao']
        changed = deepcopy(self.record['projeto'])
        changed['site']['nome'] = 'Outra sessão'
        self.record = self.workshop.save(changed, previous)
        with self.assertRaises(ValueError):
            self.workshop.delete_site(changed['id'], previous)
        with self.assertRaises(ValueError):
            self.workshop.operate(changed['id'], previous, 'criar_obra')
        self.assertEqual(self.workshop.read(changed['id'])['projeto']['site']['nome'], 'Outra sessão')


@unittest.skipUnless(pub.native_binary().is_file(), 'Ponte C++ não compilada: execute Preparar-ponte.sh.')
class NativeBridgeContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.workshop = pub.Workshop(self.temp.name)
        self.record = self.workshop.create_native()
        self.folder = self.workshop.path(self.record['projeto']['id'])
        self.snapshot = json.loads((self.folder / 'origem-inde/retrato.json').read_text())

    def tearDown(self):
        self.workshop.close()
        self.temp.cleanup()

    def test_saved_native_project_projects_exact_text_ids_and_semantics(self):
        self.assertTrue((self.folder / 'origem-inde/Publicacao.inde/data/project.sqlite3').exists())
        output = pub.build(self.record['projeto'], self.folder / 'saida')
        for unit in self.snapshot['units']:
            source = unit['documents'][0]
            projected = next(d for d in self.record['projeto']['obra']['documentos'] if d['id'] == source['id'])
            self.assertEqual(projected['texto'], source['text'])
            self.assertEqual(projected['revisao_origem'], source['revision'])
            body = (output / 'obra/writed' / (source['id'] + '.html')).read_text()
            self.assertIn('id="' + source['anchors'][0]['id'] + '"', body)
        corpus = '\n'.join(p.read_text() for p in output.rglob('*') if p.is_file())
        self.assertNotIn('SEGREDO-PRIVADO-INDE-PUB01', corpus)
        self.assertNotIn('Referência privada', corpus)
        self.assertIn('data-speaker', corpus)
        self.assertIn('data-thinker', corpus)
        self.assertIn('data-pov-character', corpus)

    def test_refresh_uses_native_services_without_changing_original_project(self):
        origin = self.folder / 'origem-inde'
        def hashes():
            return {str(p.relative_to(origin)): hashlib.sha256(p.read_bytes()).hexdigest() for p in origin.rglob('*') if p.is_file()}
        before = hashes()
        updated = self.workshop.refresh_native(self.record['projeto']['id'], self.record['revisao'])
        self.assertEqual(hashes(), before)
        self.assertEqual(updated['projeto']['obra'], self.record['projeto']['obra'])
        self.assertTrue(list((self.folder / 'retratos').glob('*.json')))

    def test_native_authorship_cannot_be_changed_in_projection(self):
        data = deepcopy(self.record['projeto'])
        data['obra']['documentos'][1]['texto'] = 'Não substituir a autoridade nativa.'
        with self.assertRaisesRegex(ValueError, 'pertence ao INDE'):
            self.workshop.save(data, self.record['revisao'])
        data = deepcopy(self.record['projeto']);data.pop('origem_inde')
        with self.assertRaisesRegex(ValueError, 'pertence ao INDE'):
            self.workshop.save(data,self.record['revisao'])
        with self.assertRaisesRegex(ValueError, 'projeção do INDE'):
            self.workshop.operate(data['id'], self.record['revisao'], 'criar_capitulo')

    def test_unmapped_semantics_and_unresolved_anchors_refuse_projection(self):
        data = deepcopy(self.snapshot)
        doc = data['units'][0]['documents'][0]
        doc['semantics']['anchors'][0]['state'] = 'orphaned'
        with self.assertRaisesRegex(ValueError, 'resolução'):
            ponte_inde.project_work(data)
        doc['semantics']['anchors'][0]['state'] = 'resolved'
        doc['semantics']['annotations'][0]['type'] = 'project.unknown'
        with self.assertRaisesRegex(ValueError, 'correspondência'):
            ponte_inde.project_work(data)

    def test_text_hash_mismatch_refuses_projection(self):
        data = deepcopy(self.snapshot)
        data['units'][0]['documents'][0]['text'] += ' adulterado'
        with self.assertRaisesRegex(ValueError, 'hash nativo'):
            ponte_inde.project_work(data)

    def test_direct_refresh_of_origin_is_refused_before_open(self):
        origin = self.folder / 'origem-inde'
        before = (origin / 'retrato.json').read_bytes()
        result = subprocess.run([str(pub.native_binary()), '--snapshot-copia', str(origin)], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(before, (origin / 'retrato.json').read_bytes())


if __name__ == '__main__':
    unittest.main()
