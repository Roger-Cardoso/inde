"""Seal the keyboard candidate only with exact-build evidence. Never overwrite."""
import argparse,csv,hashlib,json,os,re,shutil,sqlite3,subprocess,tarfile
from pathlib import Path
from reform_snapshots import snapshot
p=argparse.ArgumentParser(description=__doc__)
for name in ('release','debug','asan','lab','startup','ctest','policy','preservation','visual'):
    p.add_argument('--'+name,required=True,type=Path)
p.add_argument('--regression',action='append',required=True,type=Path)
p.add_argument('--build-log',action='append',required=True,type=Path)
a=p.parse_args();source=Path(__file__).resolve().parents[1];root=source.parent;out=root.parents[1]/'outputs'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
artifact=lambda p:dict(path=str(p.resolve()),sha256=digest(p))
git=lambda *v:subprocess.check_output(['git',*v],cwd=source,text=True).strip()
subprocess.run(['git','diff','--check'],cwd=source,check=True)
assert 'no work to do' in subprocess.check_output(['ninja','-n'],cwd=root/'build',text=True)
assert git('rev-parse','HEAD')=='995fbb27f8ee9359643a90f0220bc71269ef5e9a'
main=digest(root/'build/inde');lab=digest(root/'build/inde_text_blocks_lab')
assert '100% tests passed out of 35' in a.ctest.read_text()
assert 'PASS keyboard policy 3334' in a.policy.read_text()
for log in a.build_log:
    assert not re.search(r'warning:|error:|FAILED:|killed',log.read_text(),re.I),str(log)
preservation=json.loads(a.preservation.read_text())
assert preservation['passed'] and preservation['accepted_deliveries_preserved'] and preservation['core_persistence_schema_unchanged']
reports=[]
for path,mode,is_lab in ((a.release,'release',False),(a.debug,'debug',False),(a.asan,'asan',False),(a.lab,'release',True)):
    r=json.loads(path.read_text());assert r['complete'] and r['configuration']==mode and r['laboratory']==is_lab
    assert set(r['cases'])=={'atlas-light','atlas-dark','empty-light','empty-dark'}
    assert all(v['passed'] and v['prior_rows_preserved'] for v in r['cases'].values())
    if is_lab:assert r['laboratory_binary_sha256']==lab
    else:assert r['binary_sha256']==main
    driver=root/'build'/('inde_ui_keyboard_lab_smoke' if is_lab else 'inde_ui_keyboard_smoke' if mode=='release' else 'inde_ui_keyboard_'+mode)
    assert r['driver_sha256']==digest(driver)
    reports.append(r)
startup=json.loads(a.startup.read_text());assert startup['complete']
assert startup['checks']['inde']['binary_sha256']==main and startup['checks']['inde_text_blocks_lab']['binary_sha256']==lab
visual=json.loads(a.visual.read_text());assert visual['reviewed_by_agent'] and visual['binary_sha256']==main
for frame in visual['screenshots']:assert digest(Path(frame['path']))==frame['sha256']
for path in a.regression:
    r=json.loads(path.read_text())
    if 'binary_sha256' in r:assert r['binary_sha256']==main and r['complete']
    else:assert r['binary']['sha256']==main and r['origins_unchanged']
    assert all(v['passed'] for v in r['checks'].values())
previous=out/'reforma-vocabulario-autoral-entrega-2026-10-12.json';old=json.loads(previous.read_text())
for group in ('binaries','source_artifacts'):
    for item in old[group].values():assert digest(Path(item['path']))==item['sha256']
review=root/'review-atalhos';config=root/'review-atalhos-config';manifest=out/'reforma-atalhos-entrega-2026-10-12.json'
archive=out/'inde-fonte-atalhos-2026-10-12.tar.gz'
names=dict(inde='inde-reforma-atalhos-2026-10-12',laboratory='inde-laboratorio-atalhos-2026-10-12')
catalog_json=out/'inde-atalhos-implementados-2026-10-12.json';catalog_csv=out/'inde-atalhos-implementados-2026-10-12.csv'
assert not any(path.exists() for path in (review,config,manifest,archive,catalog_json,catalog_csv,*(out/n for n in names.values())))
catalog=[]
for report,is_lab in ((reports[0],False),(reports[3],True)):
    cases=[]
    for case in report['cases']:
        cases.append(json.loads((Path(report['cases'][case]['log']).parent/(case+'-commands.json')).read_text()))
    assert all(c==cases[0] for c in cases),'Runtime catalog changed by fixture/theme'
    catalog.extend(dict(c,window='laboratory' if is_lab else 'inde') for c in cases[0])
catalog_json.write_text(json.dumps(dict(commands=catalog,scope='Explicit runtime commands, not all native controls or 417 unique chords',human_acceptance='pending'),ensure_ascii=False,indent=2)+'\n')
with catalog_csv.open('w',newline='') as stream:
    writer=csv.DictWriter(stream,fieldnames=('window','id','label','context','accelerator'));writer.writeheader();writer.writerows(catalog)
review.mkdir();copies=[];atlas=review/'Atlas de Véspera — Projeto Integral.inde'
copies.append(snapshot(Path(old['launch_targets']['atlas']['project']),atlas))
empty=review/'Projeto vazio — sem exemplos.inde';lab_empty=review/'Laboratório vazio.inde'
for target in (empty,lab_empty):
    subprocess.run([str(root/'build/inde_text_blocks_empty_fixture'),str(target)],env=dict(os.environ,XDG_CONFIG_HOME=str(config)),check=True)
    copies.append(dict(path=str(target),origin='new synthetic pristine fixture',mutable_review=True))
for target in (atlas,empty,lab_empty):
    (target/'KEYBOARD_REVIEW_COPY').write_text('INDE_KEYBOARD_REVIEW_COPY_2026_10_12\n')
    (target/'TEXT_BLOCKS_LAB_COPY').write_text('INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n')
    (target/'TEXT_WORKSHOP_COPY').write_text('INDE_TEXT_WORKSHOP_COPY_V1\n')
    with sqlite3.connect((target/'data/project.sqlite3').as_uri()+'?mode=ro',uri=True) as db:
        assert db.execute('PRAGMA integrity_check').fetchall()==[('ok',)] and not db.execute('PRAGMA foreign_key_check').fetchall()
        assert db.execute('SELECT max(version) FROM schema_migrations').fetchone()==(28,)
        if target!=atlas:
            for table in ('documents','works','intellectual_properties'):assert db.execute('SELECT count(*) FROM '+table).fetchone()==(0,)
result=dict(stage='contextual_keyboard',schema=28,technical_verification_passed=True,human_acceptance='pending',published=False,
    whole_reform_complete=False,source=str(source),branch=git('branch','--show-current'),base_commit=git('rev-parse','HEAD'),
    source_uncommitted=True,source_status=git('status','--porcelain'),predecessor=str(previous),previous_frozen_deliveries_preserved=True,
    review_copies=copies,binaries={},source_artifacts={},launch_targets={},verification_artifacts=[],
    verification=dict(ctest_passed=35,keyboard_policy_cases=3334,ui_task_cases=16,focal_units=11,leak_sanitizer=False,
        startup_only_checks=2,explicit_main_commands=sum(c['window']=='inde' for c in catalog),
        explicit_laboratory_commands=sum(c['window']=='laboratory' for c in catalog)),
    limits=['Debug/ASan/UBSan instrument eleven routing/UI units and task driver; other objects and dependencies Release; LSan disabled',
        'Native popup shadows in non-composited Xvfb captures can have black transparent margins; desktop compositor rendering remains a human gate',
        'Task probes and image inspection do not certify physical ABNT2, IBus/IME lifecycle, Wayland, screen readers, HiDPI or long sessions',
        '417 approved families are not 417 unique keys nor 417 independently exercised live routes; native and contextual navigation remain separate',
        'Only explicitly opted-in safe forms use Ctrl+Enter; applying/discarding montage or deleting/exporting never receives blanket consent',
        'Review snapshots do not capture unsaved live buffers; no commit, push, promotion or publication'],agent_visual_review=visual)
evidence=[a.release,a.debug,a.asan,a.lab,a.startup,a.ctest,a.policy,a.preservation,a.visual,*a.regression,*a.build_log,
    source/'docs/keyboard-workflow-2026-10-12.md',out/'reforma-atalhos-entrega-2026-10-12.md',out/'inde-atalhos-abrir.sh',catalog_json,catalog_csv]
for path in evidence:assert path.is_file();result['verification_artifacts'].append(artifact(path))
for kind,name in names.items():
    destination=out/name;shutil.copy2(root/'build'/('inde' if kind=='inde' else 'inde_text_blocks_lab'),destination);destination.chmod(0o750)
    result['binaries'][kind]=artifact(destination)
files=sorted(set(filter(None,subprocess.check_output(['git','ls-files','-co','--exclude-standard','-z'],cwd=source).decode().split('\0'))))
with tarfile.open(archive,'w:gz') as tar:
    for name in files:
        path=source/name;assert path.is_file() and not path.is_symlink();tar.add(path,arcname='inde/'+name,recursive=False)
result['source_artifacts']['complete_source']=dict(artifact(archive),files=len(files),includes_nonignored_untracked=True)
for choice,binary,target in (('atlas','inde',atlas),('vazio','inde',empty),('laboratorio-atlas','laboratory',atlas),('laboratorio-vazio','laboratory',lab_empty)):
    result['launch_targets'][choice]=dict(binary=binary,project=str(target),marker='KEYBOARD_REVIEW_COPY',marker_value='INDE_KEYBOARD_REVIEW_COPY_2026_10_12')
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('SEALED private keyboard candidate',manifest,'; human acceptance pending')
