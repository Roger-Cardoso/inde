"""Real X11 input exercises native preparation and object confirmation.
Worker gates are enabled only by the explicit observer host, never normal INDE.
"""
import json
import os
from pathlib import Path
import signal
import sqlite3
import subprocess
import sys
import time
BASE=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(BASE))
from ambiente import native_environment
root=Path(sys.argv[1]).absolute();root.mkdir(parents=True,exist_ok=False)
env=native_environment(root/'profile',BASE)
env.update(GDK_BACKEND='x11',GSK_RENDERER='cairo',GTK_A11Y='none',GTK_USE_PORTAL='0',G_DEBUG='fatal-criticals',
           P5_TRANSPORT_BLOCKS='1',XDG_STATE_HOME=str(root/'state'),XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'))
if not env.get('DISPLAY'):raise SystemExit('Use X11/D-Bus privados.')
subprocess.run([str(BASE/'build-nativo/inde_project_file_native_tests'),str(root/'fixture')],env=env,check=True)
packet=root/'fixture/Projeto-portatil.inde';state_file=root/'widgets.json';hold=root/'file-task-hold';entered=root/'file-task-entered'
app=manager=log=None

def observed():
    try:return json.loads(state_file.read_text())
    except (FileNotFoundError,json.JSONDecodeError):return {}
def wait(predicate,why,timeout=25):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if app.poll() is not None:raise RuntimeError(f'Aplicativo encerrou em {why}: {app.returncode}')
        s=observed()
        if predicate(s):return s
        time.sleep(.04)
    raise RuntimeError(why+': '+json.dumps(observed(),ensure_ascii=False)[:18000])
def input_(*args):subprocess.run(['xdotool',*map(str,args)],env=env,check=True,capture_output=True,timeout=8)
def controls(s,label=None,name=None,sensitive=True):
    return [c for c in s.get('controls',[]) if c.get('mapped') and c.get('hittable') and (not sensitive or c.get('sensitive')) and ((label and c.get('label')==label) or (name and c.get('name')==name))]
def click(label=None,name=None):
    s=wait(lambda s:bool(controls(s,label,name)),str(label or name));c=controls(s,label,name)[0]
    input_('mousemove','--window',c.get('window',s['window']),c['x']+c['width']//2,c['y']+c['height']//2,'click','1')
def start(tag):
    global app,log
    state_file.unlink(missing_ok=True);log=(root/(tag+'.log')).open('w')
    app=subprocess.Popen([str(BASE/'build-nativo/inde-especial'),'--arquivo-usabilidade',str(packet),str(state_file)],env=env,stdout=log,stderr=log,start_new_session=True)
    s=wait(lambda s:'window' in s,'Janela');input_('windowactivate','--sync',s['window'])
def document():
    wait(lambda s:bool(s.get('objects')) or bool(controls(s,label='Uma obra transportável')),'Documento disponível')
    time.sleep(.2)
    if not observed().get('objects'):click(label='Uma obra transportável')
    wait(lambda s:bool(s.get('objects')) and not s.get('busy'),'Documento pronto')
def append(text):
    s=wait(lambda s:any(c.get('hittable') and c.get('sensitive') and c['name'].startswith('writing-paragraph-') for c in s['controls']),'Parágrafo disponível')
    c=[c for c in s['controls'] if c.get('hittable') and c.get('sensitive') and c['name'].startswith('writing-paragraph-')][-1]
    click(name=c['name']);input_('key','ctrl+End');input_('type','--clearmodifiers','--delay','15',text)
    wait(lambda s:s.get('content','').endswith(text),'Escrita real')
def saved():
    with sqlite3.connect(f'file:{packet}?mode=ro',uri=True) as db:
        seq=db.execute('SELECT seq FROM head').fetchone()[0]
        kind,payload,blob=db.execute("SELECT v.kind,v.payload,v.blob_hash FROM entries e JOIN record_versions v ON v.id=e.version_id WHERE e.seq=? AND e.key='file/data/project.sqlite3'",(seq,)).fetchone()
        hashes=json.loads(payload)['chunks'] if kind=='chunked-file' else [blob]
        data=b''.join(db.execute('SELECT data FROM blobs WHERE hash=?',(h,)).fetchone()[0] for h in hashes)
    with sqlite3.connect(':memory:') as db:
        db.deserialize(data);text=db.execute("SELECT content FROM documents WHERE title='Uma obra transportável'").fetchone()[0]
    return seq,text

def gate(phase):
    entered.unlink(missing_ok=True);hold.write_text(phase)
    input_('key','ctrl+s')
    return wait(lambda s:s.get('file_task',{}).get('active') and s['file_task'].get('phase')==phase and entered.exists(),'Operação em '+phase)
def heartbeat():
    tick=observed()['observer_tick'];s=wait(lambda s:s.get('observer_tick',0)>=tick+4,'Janela respondeu durante operação')
    assert s['file_task']['active']
def settled():
    tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>tick and not s.get('file_task',{}).get('active'),'Confirmação concluída')
def quit_normal():
    input_('key','ctrl+q');assert app.wait(timeout=30)==0;log.close()

def object_revision():
    for meta in (root/'state').rglob('session.json'):
        v=json.loads(meta.read_text())
        if Path(v['file'])!=packet:continue
        db_path=next((meta.parent/v['working']/'documents/composed').rglob('obj01-piloto.sqlite3'))
        with sqlite3.connect(f'file:{db_path}?mode=ro',uri=True) as db:
            return db.execute('SELECT revision FROM saved WHERE id=1').fetchone()[0]
    raise RuntimeError('Sessão nativa ausente')

def resumed_text():
    wait(lambda s:any(c.get('hittable') and c.get('sensitive') and c['name'].startswith('writing-paragraph-') for c in s.get('controls',[])),'Escrita reativada')

try:
    with (root/'wm.log').open('w') as out:manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=out,stderr=out)
    time.sleep(.8);assert manager.poll() is None
    seq,initial=saved();start('native-preparation');document();append(' Texto mais recente na preparacao.')
    expected=observed()['content']
    gate('preparing');heartbeat()
    assert not any(c.get('sensitive') for c in observed()['controls'] if c['name'].startswith('writing-paragraph-'))
    click(label='Cancelar salvamento');settled();hold.unlink(missing_ok=True)
    assert saved()==(seq,initial) and observed()['content']==expected
    resumed_text()
    meta=next((root/'state').rglob('session.json'));v=json.loads(meta.read_text())
    native_db=meta.parent/v['working']/'data/project.sqlite3'
    # Hold a real SQLite write reservation. The native writer must wait while
    # GTK continues observing ticks, rather than blocking in drain().
    with sqlite3.connect(native_db,isolation_level=None) as lock:
        lock.execute('BEGIN IMMEDIATE')
        append(' Escrita espera o banco nativo.');locked_text=observed()['content']
        input_('key','ctrl+s')
        wait(lambda s:s.get('file_task',{}).get('active') and s['file_task'].get('phase')=='preparing','Espera cooperativa do escritor nativo')
        heartbeat();assert observed()['file_task']['phase']=='preparing'
        lock.execute('ROLLBACK')
    wait(lambda s:saved()[1]==locked_text,'Escritor terminou após liberar a reserva');settled()
    seq,initial=saved()
    resumed_text();append(' Continua depois de cancelar.')
    expected=observed()['content'];gate('objects');heartbeat()
    input_('key','ctrl+q');wait(lambda s:s['file_task']['exit_requested'],'Saída durante objetos')
    click(label='Continuar no editor');wait(lambda s:not s['file_task']['exit_requested'],'Retirar saída na preparação dos objetos')
    click(label='Cancelar salvamento');settled();hold.unlink(missing_ok=True)
    assert saved()==(seq,initial) and observed()['content']==expected
    resumed_text();input_('key','ctrl+s');wait(lambda s:saved()[1]==expected,'Texto mais recente confirmado');settled()
    assert saved()[0]==seq+1
    revision=object_revision();input_('key','ctrl+s');settled();assert object_revision()==revision
    meta=next((root/'state').rglob('session.json'));v=json.loads(meta.read_text())
    recovery=next((meta.parent/v['working']/'recovery/editor-v1').glob('*.sqlite3'))
    with sqlite3.connect(recovery) as db:original_records=db.execute('SELECT * FROM recovery_records ORDER BY session_id').fetchall()
    interrupted_writer="""import sqlite3,sys,os
c=sqlite3.connect(sys.argv[1]);c.execute('BEGIN IMMEDIATE');c.execute("UPDATE recovery_records SET title=title||' interrupted'");os._exit(0)
"""
    subprocess.run([sys.executable,'-c',interrupted_writer,str(recovery)],env=env,check=True)
    journal=Path(str(recovery)+'-journal');assert journal.exists() and journal.read_bytes()[:8]==bytes(8)
    gate('objects');heartbeat();hold.unlink();settled()
    assert not journal.exists()
    with sqlite3.connect(recovery) as db:assert db.execute('SELECT * FROM recovery_records ORDER BY session_id').fetchall()==original_records
    append(' Interrompido antes de confirmar os objetos.');interrupted=observed()['content'];before_crash=saved()
    gate('objects');heartbeat();os.killpg(app.pid,signal.SIGKILL);app.wait(timeout=10);log.close();hold.unlink()
    assert saved()==before_crash
    start('recover-native-preparation');click(label='Retomar o trabalho preservado');document()
    assert observed()['content']==interrupted
    input_('key','ctrl+s');wait(lambda s:saved()[1]==interrupted,'Recuperar a fronteira texto/objetos');settled()
    append(' Rascunho na falha da preparacao.');failed_text=observed()['content'];before_failure=saved()
    meta=next((root/'state').rglob('session.json'));v=json.loads(meta.read_text())
    data_folder=meta.parent/v['working']/'data'
    os.chmod(data_folder,0o500)
    try:
        input_('key','ctrl+q');click(label='Fechar')
        settled();assert not observed()['file_task']['exit_requested'];assert app.poll() is None and saved()==before_failure and observed()['content']==failed_text
        assert not any(c.get('mapped') and c.get('label')=='Encerrar preservando o trabalho' for c in observed()['controls'])
    finally:os.chmod(data_folder,0o700)
    input_('key','ctrl+s');wait(lambda s:saved()[1]==failed_text,'Repetir após falha na preparação');settled()
    append(' Fechar depois dos objetos.');closing=observed()['content'];gate('objects');heartbeat()
    input_('key','alt+F4');wait(lambda s:s['file_task']['exit_requested'],'Fechar aguarda os objetos')
    hold.unlink();assert app.wait(timeout=40)==0;log.close();assert saved()[1]==closing
    start('reopen-prepared');document();assert observed()['content']==closing
    # A normal close from the compound sheet goes through preparation too.
    quit_normal()
    for p in root.glob('*.log'):assert not any(x in p.read_text() for x in ['Gtk-CRITICAL','Gdk-CRITICAL','GLib-GObject-CRITICAL']),p
    checks=['failed_native_preparation_keeps_editor_open','retry_after_native_preparation_failure','interrupted_cold_journal_recovered','recovery_records_unchanged','crash_between_native_writers_preserves_file','crash_between_native_writers_recovers_text','recovered_native_preparation_confirms','native_sqlite_wait_responsive','native_sqlite_wait_completed','responsive_before_file_capture','editing_paused_during_preparation','preparation_cancel_preserves_file','preparation_cancel_preserves_text','objects_worker_responsive','objects_cancel_preserves_file','retry_captures_latest_text','object_noop_preserves_revision','quit_queued_during_objects','quit_withdrawn_during_objects','window_close_waits_for_objects','clean_native_reopen','normal_quit','no_gtk_criticals']
    (root/'resultado.json').write_text(json.dumps(dict.fromkeys(checks,True),indent=2)+'\n')
    print('P5-D3 GTK: cooperative native preparation, object worker, cancel, retry, queued quit and clean reopen OK')
finally:
    hold.unlink(missing_ok=True)
    if app and app.poll() is None:
        try:subprocess.run(['import','-window',str(observed().get('window','root')),str(root/'falha.png')],env=env,timeout=5,capture_output=True)
        except (OSError,subprocess.TimeoutExpired):pass
        os.killpg(app.pid,signal.SIGTERM);app.wait(timeout=10)
    if log and not log.closed:log.close()
    if manager and manager.poll() is None:manager.terminate();manager.wait(timeout=10)
