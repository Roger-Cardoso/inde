from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.request
BASE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(BASE))
import ambiente
import importacao
import publicacao

class DesktopEnvironmentTests(unittest.TestCase):
    def test_native_profile_does_not_leak_to_browser(self):
        original={'PATH':os.environ['PATH'],'GTK_IM_MODULE':'ibus','XDG_CONFIG_HOME':'/desktop/config','GDK_DEBUG':'original'}
        native=ambiente.native_environment('/tmp/inde-profile',BASE,original,False)
        self.assertNotIn('GTK_IM_MODULE',native)
        native['GSK_RENDERER']='cairo';native['GTK_THEME']='Adwaita';native['GDK_DEBUG']='gl-disable'
        self.assertEqual(ambiente.desktop_environment(native),original)
    def test_installed_input_method_is_preserved(self):
        env=ambiente.native_environment('/tmp/profile',BASE,{'GTK_IM_MODULE':'ibus'},True)
        self.assertEqual(env['GTK_IM_MODULE'],'ibus')
    def test_ibus_detection_uses_real_module_files(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);self.assertFalse(ambiente.ibus_available([root]));module=root/'gtk-4.0/4.0.0/immodules/im-ibus.so';module.parent.mkdir(parents=True);module.touch();self.assertTrue(ambiente.ibus_available([root]))
    def test_failed_opener_preserves_diagnostics_and_address(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);opener=root/'xdg-open';opener.write_text('#!/bin/sh\necho diagnostico-retido >&2\nexit 7\n');opener.chmod(0o700);log=root/'navegador.log'
            with self.assertRaisesRegex(RuntimeError,'http://127.0.0.1:1234/home/'):
                ambiente.open_browser('http://127.0.0.1:1234/home/',log,{'PATH':str(root)})
            self.assertIn('diagnostico-retido',log.read_text())
    def test_unset_desktop_profile_remains_unset(self):
        env=ambiente.native_environment('/tmp/profile',BASE,{},False)
        self.assertNotIn('XDG_CONFIG_HOME',ambiente.desktop_environment(env))

class ImportReviewTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='pub03-import-');self.root=Path(self.temp.name);self.report=importacao.demo(self.root/'demo')
    def tearDown(self):self.temp.cleanup()
    def test_conflict_requires_a_choice_and_originals_are_preserved(self):
        data=importacao.load_review(self.report);self.assertFalse(data['equal'])
        self.assertIn('havia',data['candidates']['html']['content']['text']);self.assertIn('há',data['candidates']['autoria']['content']['text'])
        with self.assertRaises(ValueError):importacao.plan(self.report,None,self.root/'plan.json','Teste')
        for source in data['sources'].values():self.assertEqual(hashlib.sha256(Path(source['path']).read_bytes()).hexdigest(),source['sha256'])
    def test_changed_source_is_refused_before_plan(self):
        data=importacao.load_review(self.report);Path(data['sources']['html']['path']).write_text('Mudou')
        with self.assertRaisesRegex(ValueError,'mudou'):importacao.plan(self.report,'autoria',self.root/'plan.json','Teste')
        self.assertFalse((self.root/'plan.json').exists())
    def test_changed_review_is_refused(self):
        data=json.loads(self.report.read_text());data['candidates']['autoria']['content']['text']='Alterado';self.report.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,'adulterado'):importacao.load_review(self.report)
    def test_unsupported_metadata_blocks_without_silently_dropping_it(self):
        data=json.loads((self.root/'demo/salvo.narrproj').read_text());data['semantic'][0]['meta']['data-addressee']='outra-entidade'
        content,issues=importacao.normalize(data);self.assertTrue(issues)
    def test_multiple_html_chapters_and_scripts_are_blocked(self):
        raw=(self.root/'demo/aplicado.html').read_bytes()
        self.assertFalse(importacao.candidate(raw+b'<section id="extra" data-scope="chapter"><p>Outro</p></section>','html')['convertible'])
        self.assertFalse(importacao.candidate(raw+b'<script>alert(1)</script>','html')['convertible'])
    def test_duplicate_scene_id_is_refused(self):
        value=json.loads((self.root/'demo/salvo.narrproj').read_text());scene={'id':'scene','start':0,'end':4,'meta':{'id':'scene','data-scope':'scene'}};value['sections']=[scene,deepcopy(scene)]
        with self.assertRaisesRegex(ValueError,'repetida'):importacao.normalize(value)
    def test_plan_staleness_is_checked_again_by_native_import(self):
        plan=importacao.plan(self.report,'autoria',self.root/'plan.json','Teste');data=importacao.load_review(self.report);Path(data['sources']['autoria']['path']).write_text('mudou')
        failed=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-literature',plan,self.root/'resultado'],capture_output=True,text=True,env=dict(os.environ,XDG_CONFIG_HOME=str(self.root/'profile')))
        self.assertNotEqual(failed.returncode,0);self.assertFalse((self.root/'resultado').exists());self.assertFalse(list(self.root.glob('.importacao-*')))
    def test_unicode_offsets_and_formatting(self):
        value={'version':5,'text':'Á 😀 Mara','section':{'id':'chapter','data-scope':'chapter'},'entities':[],'formatting':{'fmt_bold':[[2,3]]}}
        content,issues=importacao.normalize(value);self.assertFalse(issues);self.assertEqual(content['text'][2:3],'😀')
    @unittest.skipUnless((BASE/'build-nativo/inde_web_bridge').exists(),'Compile a ponte')
    def test_native_import_keeps_text_and_maps_identities(self):
        plan=importacao.plan(self.report,'autoria',self.root/'plan.json','Obra de conferência')
        env=dict(os.environ,XDG_CONFIG_HOME=str(self.root/'profile'))
        subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-literature',plan,self.root/'resultado'],check=True,capture_output=True,text=True,env=env)
        project=self.root/'resultado/Importado.inde'
        result=subprocess.run([BASE/'build-nativo/inde_web_bridge','--inspecionar-importacao',project],check=True,capture_output=True,text=True,env=env)
        snapshot=json.loads(result.stdout);doc=snapshot['units'][0]['documents'][0]
        self.assertEqual(doc['text'],importacao.load_review(self.report)['candidates']['autoria']['content']['text'])
        receipt=snapshot['import_receipt'];self.assertEqual(receipt['choice'],'autoria');self.assertEqual(receipt['publication'],'pending-public-identities')
        mappings={entry['source']:entry['native'] for entry in receipt['identities']}
        chapter=next(entry for entry in receipt['identities'] if entry['kind']=='chapter');self.assertEqual(chapter['native'],snapshot['units'][0]['id'])
        self.assertEqual(mappings['capitulo-origem'],doc['id']);self.assertNotEqual(mappings['character-origem'],'character-origem')
        self.assertTrue(doc['semantics']['annotations'])
        block=subprocess.run([BASE/'build-nativo/inde_import_tests',project],capture_output=True,text=True,env=env,check=True);self.assertIn('banco autoral OK',block.stdout)
        before=hashlib.sha256((project/'data/project.sqlite3').read_bytes()).hexdigest()
        failed=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-literature',plan,self.root/'resultado'],capture_output=True,text=True,env=env)
        self.assertNotEqual(failed.returncode,0);self.assertEqual(before,hashlib.sha256((project/'data/project.sqlite3').read_bytes()).hexdigest())

    def test_native_styles_scenes_perspective_and_distinct_same_name_entities(self):
        saved=self.root/'demo/salvo.narrproj';value=json.loads(saved.read_text());value['entities'].append({'id':'outra-mara','type':'character','name':'Mara'});value['section'].update({'data-pov-type':'third_person_limited','data-pov-character':'character-origem'});value['formatting']={'fmt_bold':[[0,4]],'fmt_italic':[[5,11]],'fmt_underline':[[12,13]]};value['sections']=[{'id':'cena-origem','start':0,'end':len(value['text']),'meta':{'id':'cena-origem','data-scope':'scene'}}];value['semantic'].append({'id':'pensamento-origem','start':0,'end':4,'meta':{'data-role':'thought','data-thinker':'outra-mara'}});saved.write_text(json.dumps(value,ensure_ascii=False))
        report=importacao.compare(self.root/'demo/aplicado.html',saved,self.root/'outra-conferencia');plan=importacao.plan(report,'autoria',self.root/'plan.json','Conferência completa');env=dict(os.environ,XDG_CONFIG_HOME=str(self.root/'profile'))
        subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-literature',plan,self.root/'resultado'],capture_output=True,check=True,env=env)
        data=json.loads(subprocess.run([BASE/'build-nativo/inde_web_bridge','--inspecionar-importacao',self.root/'resultado/Importado.inde'],capture_output=True,text=True,check=True,env=env).stdout);doc=data['units'][0]['documents'][0]
        self.assertEqual(doc['text'],value['text']);self.assertEqual(len(doc['formatting']),3);self.assertEqual(len(doc['anchors']),3);self.assertEqual(len(doc['semantics']['annotations']),3);self.assertEqual(len(data['entities']),2);self.assertNotEqual(data['entities'][0]['id'],data['entities'][1]['id']);self.assertIn('inde.semantic.perspective',json.dumps(doc['semantics']))

class PreviewReuseTests(unittest.TestCase):
    def test_reopen_preserves_port_and_restores_desktop_environment(self):
        with tempfile.TemporaryDirectory(prefix='pub03-preview-') as folder:
            root=Path(folder);fixture=root/'fixture'
            subprocess.run([BASE/'build-nativo/inde_web_bridge','--demo',fixture],check=True,capture_output=True)
            subprocess.run([BASE/'build-nativo/inde_publication_tests',fixture],check=True,capture_output=True)
            publicacao.build_native_edition(fixture/'edicao-para-gerar.json',root/'site')
            bindir=root/'bin';bindir.mkdir();opener=bindir/'xdg-open';record=root/'opener.jsonl'
            opener.write_text('#!'+sys.executable+'\nimport json,os,sys\nwith open('+repr(str(record))+',"a") as f:f.write(json.dumps({"url":sys.argv[1],"config":os.environ.get("XDG_CONFIG_HOME"),"im":os.environ.get("GTK_IM_MODULE")})+"\\n")\n');opener.chmod(0o700)
            original=dict(os.environ,PATH=str(bindir)+os.pathsep+os.environ['PATH'],XDG_CONFIG_HOME=str(root/'desktop'),GTK_IM_MODULE='ibus')
            env=ambiente.native_environment(root/'inde',BASE,original,False);control=root/'control.json'
            process=subprocess.Popen([sys.executable,BASE/'publicacao.py','--servir-edicao',root/'site','--controle-previa',control],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                deadline=time.monotonic()+8
                while not control.exists() and time.monotonic()<deadline:time.sleep(.04)
                self.assertTrue(control.exists());before=json.loads(control.read_text())
                subprocess.run([sys.executable,BASE/'publicacao.py','--reabrir-previa',control],env=env,check=True,capture_output=True,text=True)
                after=json.loads(control.read_text());self.assertEqual(before,after)
                while (not record.exists() or len(record.read_text().splitlines())<2) and time.monotonic()<deadline:time.sleep(.04)
                requests=[json.loads(line) for line in record.read_text().splitlines()]
                self.assertEqual(len(requests),2);self.assertEqual({r['url'] for r in requests},{before['url']});self.assertTrue(all(r['config']==str(root/'desktop') and r['im']=='ibus' for r in requests))
                with urllib.request.urlopen(before['url']) as response:self.assertEqual(response.status,200)
                # Runtime control and browser diagnostics must be outside public files.
                self.assertFalse((root/'site/control.json').exists())
            finally:
                process.terminate();process.communicate(timeout=5)
            self.assertFalse(control.exists())

if __name__=='__main__':unittest.main()
