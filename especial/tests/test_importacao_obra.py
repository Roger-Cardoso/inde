from copy import deepcopy
from html.parser import HTMLParser
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from zipfile import ZipFile
BASE=Path(__file__).resolve().parents[1];sys.path.insert(0,str(BASE))
import importar_obra as work_import
import importacao
import publicacao

class Text(HTMLParser):
    def __init__(self):super().__init__();self.parts=[]
    def handle_data(self,value):self.parts.append(value)
    def handle_endtag(self,tag):
        if tag=='p':self.parts.append('\n\n')
    def handle_starttag(self,tag,attrs):
        if tag=='br':self.parts.append('\n')

class WorkImportTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='pub03b-suite-');self.root=Path(self.temp.name);self.report=work_import.demo(self.root/'demo');self.manifest=self.root/'demo/obra-literature.json';self.env=dict(os.environ,XDG_CONFIG_HOME=str(self.root/'perfil'))
    def tearDown(self):self.temp.cleanup()
    def choices(self):return {item['id']:('autoria' if i==0 else 'html') for i,item in enumerate(work_import.load_review(self.report)['chapters'])}
    def plan(self,choices=None):return work_import.plan(self.report,self.choices() if choices is None else choices,self.root/'plano.json')
    def native(self,plan=None):
        plan=plan or self.plan();result=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-obra-literature',plan,self.root/'resultado'],env=self.env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr);return self.root/'resultado/Importado.inde'
    def snapshot(self,project):return json.loads(subprocess.run([BASE/'build-nativo/inde_web_bridge','--inspecionar-importacao',project],env=self.env,capture_output=True,text=True,check=True).stdout)
    def generate(self,project,folder='geracao'):
        payload=Path(subprocess.run([BASE/'build-nativo/inde_web_bridge','--preparar-obra-importada',project],env=self.env,capture_output=True,text=True,check=True).stdout.strip());self.last_payload=json.loads(payload.read_text());output=self.root/folder/'site';publicacao.build_native_edition(payload,output);return output
    def recompare(self):self.report=work_import.compare(self.manifest,self.root/'nova-conferencia')
    def edit_second(self,edit):
        path=self.root/'demo/capitulo-2/salvo.narrproj';value=json.loads(path.read_text());edit(value);path.write_text(json.dumps(value,ensure_ascii=False));self.recompare()
    def test_each_chapter_requires_a_choice(self):
        with self.assertRaisesRegex(ValueError,'cada capítulo'):self.plan({})
        with self.assertRaises(ValueError):self.plan({'capitulo-origem-1':'autoria','capitulo-origem-2':'automatico'})
        self.assertFalse((self.root/'plano.json').exists())
    def test_duplicate_numbers_and_counter_regression_refused(self):
        value=json.loads(self.manifest.read_text());value['chapters'][1]['number']=7
        with self.assertRaises(ValueError):work_import.validate_manifest(value)
        value=json.loads(self.manifest.read_text());value['work']['next_number']=11
        with self.assertRaises(ValueError):work_import.validate_manifest(value)
    def test_missing_and_extra_chapters_are_refused(self):
        choices=self.choices();choices['fantasma']='html'
        with self.assertRaises(ValueError):self.plan(choices)
    def test_manifest_staleness_is_refused(self):
        self.manifest.write_text(self.manifest.read_text()+'\n')
        with self.assertRaisesRegex(ValueError,'manifesto mudou'):self.plan()
    def test_unselected_source_staleness_also_refused(self):
        source=self.root/'demo/capitulo-2/salvo.narrproj';source.write_text(source.read_text()+'\n')
        with self.assertRaisesRegex(ValueError,'fonte mudou'):self.plan()
    def test_shared_id_with_conflicting_names_is_refused(self):
        self.edit_second(lambda value:value['entities'][0].update(name='Outra Mara'))
        choices={key:'autoria' for key in self.choices()}
        with self.assertRaisesRegex(ValueError,'incompatíveis'):self.plan(choices)
    def test_bad_formatting_and_semantic_overlap_block_before_creation(self):
        def edit(value):value['semantic'].append(deepcopy(value['semantic'][0]));value['semantic'][-1]['id']='outra-fala'
        self.edit_second(edit)
        with self.assertRaisesRegex(ValueError,'sobreposta'):self.plan({key:'autoria' for key in self.choices()})
    def test_order_is_explicit_and_not_derived_from_url_numbers(self):
        value=json.loads(self.manifest.read_text());value['chapters'].reverse();self.manifest.write_text(json.dumps(value));self.recompare();project=self.native();output=self.generate(project)
        structure=json.loads((output/'obra-1/dados/estrutura.json').read_text());caps=structure['sagas'][0]['volumes'][0]['arcos'][0]['capitulos'];self.assertEqual([c['numero_url'] for c in caps],[11,7])
    def test_native_ids_are_distinct_but_public_identity_is_original(self):
        project=self.native();data=self.snapshot(project);self.assertNotEqual(data['work']['id'],'obra-literature-origem');self.assertEqual(len(data['units']),2);self.assertEqual(len(data['entities']),1)
        self.assertIn('Ainda há tempo',data['units'][0]['documents'][0]['text']);self.assertIn('Ainda havia tempo',data['units'][1]['documents'][0]['text']);self.assertEqual(data['import_receipt']['version'],2)
        output=self.generate(project);info=json.loads((output/'obra-1/dados/info-obra.json').read_text());self.assertEqual(info['id_obra'],'obra-literature-origem');self.assertEqual(info['personagens'][0]['id'],'character-origem')
    def test_routes_aliases_old_folder_and_numbers_are_preserved(self):
        project=self.native();output=self.generate(project)
        for number in (7,11):
            self.assertTrue((output/f'mares-literature/capitulo-{number}/index.html').is_file());self.assertTrue((output/f'mares-antigas/capitulo-{number}/index.html').is_file())
        self.assertTrue((output/'obra-1/leitor.html').exists());structure=json.loads((output/'obra-1/dados/estrutura.json').read_text());caps=structure['sagas'][0]['volumes'][0]['arcos'][0]['capitulos'];self.assertEqual([c['id'] for c in caps],['capitulo-origem-1','capitulo-origem-2']);self.assertEqual(self.last_payload['editions'][0]['edition']['next_number'],18)
    def test_same_name_different_ids_remain_distinct(self):
        def edit(value):
            value['entities'][0]['id']='outra-mara';value['section']['data-pov-character']='outra-mara'
            for annotation in value['semantic']:annotation['meta']['data-speaker']='outra-mara'
        self.edit_second(edit);project=self.native(self.plan({key:'autoria' for key in self.choices()}));data=self.snapshot(project);self.assertEqual(len(data['entities']),2);self.assertEqual({e['name'] for e in data['entities']},{'Mara'});self.assertEqual(len({e['id'] for e in data['entities']}),2)
        output=self.generate(project);info=json.loads((output/'obra-1/dados/info-obra.json').read_text());self.assertEqual({p['id'] for p in info['personagens']},{'character-origem','outra-mara'})
    def test_native_revalidates_late_change_and_does_not_leave_destination(self):
        plan=self.plan();self.manifest.write_text(self.manifest.read_text()+'\n');result=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-obra-literature',plan,self.root/'resultado'],env=self.env,capture_output=True,text=True);self.assertNotEqual(result.returncode,0);self.assertFalse((self.root/'resultado').exists());self.assertFalse(list(self.root.glob('.importacao-obra-*')))
    def test_tampered_plan_refused(self):
        plan=self.plan();value=json.loads(plan.read_text());value['chapters'][1]['content']['text']='Texto trocado';plan.write_text(json.dumps(value));result=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-obra-literature',plan,self.root/'resultado'],env=self.env,capture_output=True,text=True);self.assertNotEqual(result.returncode,0);self.assertFalse((self.root/'resultado').exists())
    def test_existing_destination_is_preserved(self):
        plan=self.plan();target=self.root/'resultado';target.mkdir();(target/'marcador').write_text('preservar');result=subprocess.run([BASE/'build-nativo/inde_web_bridge','--importar-obra-literature',plan,target],env=self.env,capture_output=True,text=True);self.assertNotEqual(result.returncode,0);self.assertEqual((target/'marcador').read_text(),'preservar');self.assertEqual(len(list(target.iterdir())),1)
    def test_publication_operations_and_future_chapter_keep_reservations(self):
        project=self.native();result=subprocess.run([BASE/'build-nativo/inde_work_import_tests',project],env=self.env,capture_output=True,text=True);self.assertEqual(result.returncode,0,result.stderr)
        output=self.root/'futuro/site';publicacao.build_native_edition(self.root/'resultado/com-novo-capitulo.json',output);structure=json.loads((output/'obra-1/dados/estrutura.json').read_text());caps=structure['sagas'][0]['volumes'][0]['arcos'][0]['capitulos'];self.assertEqual([c['numero_url'] for c in caps],[7,11,18]);self.assertEqual(json.loads((self.root/'resultado/com-novo-capitulo.json').read_text())['editions'][0]['edition']['next_number'],19)
    def test_zip_excludes_import_receipt_and_sources(self):
        project=self.native();output=self.generate(project)
        with ZipFile(output.parent/'site-publico.zip') as archive:
            self.assertFalse(any('importacao' in name or name.endswith(('.sqlite3','.narrproj')) for name in archive.namelist()));raw=b'\n'.join(archive.read(name) for name in archive.namelist());self.assertNotIn(str(self.root).encode(),raw)
    def test_three_styles_and_scene_anchors_reach_reader(self):
        def edit(value):value['formatting']={'fmt_bold':[[0,4]],'fmt_italic':[[5,11]],'fmt_underline':[[12,13]]};value['sections']=[{'id':'cena-publica-1','start':0,'end':len(value['text']),'meta':{'id':'cena-publica-1','data-scope':'scene'}}]
        self.edit_second(edit);project=self.native(self.plan({key:'autoria' for key in self.choices()}));output=self.generate(project);raw=(output/'obra-1/writed/capitulo-origem-2.html').read_text();self.assertIn('<strong>Mara</strong>',raw);self.assertIn('<em>',raw);self.assertIn('<u>',raw);self.assertIn('id="cena-publica-1"',raw);self.assertIn('data-text-id="fala-origem"',raw)
    def test_crossing_styles_and_scene_in_paragraph_gap_preserve_text(self):
        text='Á 😀\n\nUma fala.';doc={'id':'cap','texto':text,'anotacoes':[{'id':'fala','tipo':'dialogue','entidade_id':'mara','inicio':5,'fim':8,'trecho':text[5:8]}],'formatacao':[{'style':'bold','start':0,'end':len(text)},{'style':'italic','start':2,'end':9}],'cenas':[{'id':'cena-gap','start':3,'end':len(text)}]}
        raw=publicacao.render_document(doc,'mara');parser=Text();parser.feed(raw);self.assertEqual(''.join(parser.parts)[:-2],text);self.assertEqual(raw.count('id="cena-gap"'),1)
    def test_bound_payload_cannot_be_downgraded_to_old_protocol(self):
        project=self.native();self.generate(project);self.assertEqual(self.last_payload['version'],2);payload=deepcopy(self.last_payload);payload['version']=1;path=self.root/'protocolo-antigo.json';path.write_text(json.dumps(payload))
        with self.assertRaisesRegex(ValueError,'Versão'):publicacao.build_native_edition(path,self.root/'recusado/site')
        self.assertFalse((self.root/'recusado/site').exists())
    def test_actual_comment_identifiers_keep_work_and_chapter_ids(self):
        project=self.native();output=self.generate(project);script=self.root/'comentarios.js';script.write_text("const fs=require('fs'),vm=require('vm'),assert=require('assert');const context={window:{},URL};vm.runInNewContext(fs.readFileSync(process.argv[2],'utf8'),context);const work=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));const tree=JSON.parse(fs.readFileSync(process.argv[4],'utf8'));const cap=tree.sagas[0].volumes[0].arcos[0].capitulos[0];const result=context.window.ComentariosLiterature.identidade({url_publica:'https://exemplo.invalid',caminho_base:'/biblioteca'},work,cap);assert.equal(result.identifier,JSON.stringify(['literature','obra-literature-origem','capitulo-origem-1']));console.log('Identificador de discussão original preservado');")
        result=subprocess.run(['node',script,BASE/'recursos/js-global/comentarios.js',output/'obra-1/dados/info-obra.json',output/'obra-1/dados/estrutura.json'],check=True,capture_output=True,text=True);self.assertIn('preservado',result.stdout)
    def test_actual_progress_script_accepts_existing_public_ids(self):
        project=self.native();output=self.generate(project);script=self.root/'progresso.js';script.write_text("const fs=require('fs'),vm=require('vm'),assert=require('assert');const map=new Map([['unilit:v2:progresso:obra-literature-origem',JSON.stringify(['capitulo-origem-1'])]]);const context={window:{},localStorage:{getItem:k=>map.get(k)||null,setItem:(k,v)=>map.set(k,v)}};vm.runInNewContext(fs.readFileSync(process.argv[2],'utf8'),context);const tree=JSON.parse(fs.readFileSync(process.argv[3],'utf8'));assert.equal(JSON.stringify(context.window.Unilit.lidos('obra-literature-origem',tree)),JSON.stringify(['capitulo-origem-1']));console.log('Progresso original reconhecido pelo leitor real');")
        result=subprocess.run(['node',script,BASE/'recursos/js-global/metadados.js',output/'obra-1/dados/estrutura.json'],check=True,capture_output=True,text=True);self.assertIn('reconhecido',result.stdout)

if __name__=='__main__':unittest.main()
