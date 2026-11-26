"""Real X11 input exercises asynchronous confirmation and its commit boundary.
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

try:
    with (root/'wm.log').open('w') as out:manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=out,stderr=out)
    time.sleep(.8);assert manager.poll() is None
    seq,initial=saved();start('cancel');document();append(' Cancelar e retomar.')
    expected=observed()['content'];gate('reading');heartbeat()
    assert not any(c.get('sensitive') for c in observed()['controls'] if c['name'].startswith('writing-paragraph-'))
    input_('type','--clearmodifiers','IGNORAR');input_('key','ctrl+s')
    click(label='Cancelar salvamento')
    wait(lambda s:not s.get('file_task',{}).get('active') and 'cancelado' in s['file_task']['message'],'Cancelamento concluído')
    hold.unlink(missing_ok=True);assert saved()==(seq,initial);assert observed()['content']==expected
    gate('committing')
    wait(lambda s:any(c.get('mapped') and c.get('label')=='Cancelar salvamento' and not c.get('sensitive') for c in s['controls']),'Cancelamento indisponível após fronteira')
    input_('key','ctrl+q');wait(lambda s:s['file_task']['exit_requested'],'Sair ficou pendente');heartbeat()
    click(label='Continuar no editor');wait(lambda s:not s['file_task']['exit_requested'],'Retirar pedido de saída')
    input_('key','Escape');heartbeat();hold.unlink();settled()
    assert saved()==(seq+1,expected)
    # Kill the owned process while candidate rows exist, before COMMIT. The
    # next native open must roll back that candidate and offer the workspace.
    append(' Interrompido durante a confirmacao.');crash_text=observed()['content'];gate('committing');heartbeat()
    os.killpg(app.pid,signal.SIGKILL);app.wait(timeout=10);log.close();hold.unlink()
    start('rollback-reopen');click(label='Retomar o trabalho preservado');document()
    assert saved()==(seq+1,expected);assert observed()['content']==crash_text
    input_('key','ctrl+s');wait(lambda s:saved()[1]==crash_text,'Recuperação confirmada');settled()
    assert saved()[0]==seq+2
    append(' Sair depois de salvar.');exit_text=observed()['content'];gate('reading')
    input_('key','alt+F4');wait(lambda s:s['file_task']['exit_requested'],'Fechamento aguarda operação');heartbeat()
    hold.unlink();assert app.wait(timeout=30)==0;log.close();assert saved()==(seq+3,exit_text)
    start('failed-exit');document();append(' Rascunho da falha.');failed_text=observed()['content']
    os.chmod(packet.parent,0o500);input_('key','ctrl+q');click(label='Encerrar preservando o trabalho')
    assert app.wait(timeout=30)==0;log.close();os.chmod(packet.parent,0o700)
    start('recover-failed-exit');click(label='Retomar o trabalho preservado');document()
    assert observed()['content']==failed_text
    input_('key','ctrl+s');wait(lambda s:saved()[1]==failed_text,'Salvar após falha');settled()
    input_('key','ctrl+shift+w')
    wait(lambda s:not s.get('objects') and not s.get('file_task',{}).get('active'),'Fechar projeto depois de confirmar')
    quit_normal()
    for p in root.glob('*.log'):assert not any(x in p.read_text() for x in ['Gtk-CRITICAL','Gdk-CRITICAL','GLib-GObject-CRITICAL']),p
    (root/'resultado.json').write_text(json.dumps(dict(responsive_event_loop=True,editing_paused=True,early_cancel_preserves_file_and_work=True,retry_after_cancel=True,
        cancel_refused_after_seal=True,quit_queued=True,quit_withdrawn=True,window_close_waits=True,crash_before_commit_rolled_back=True,
        crash_workspace_recovered=True,failed_async_quit_preserves_work=True,failed_quit_reopened=True,close_project_async=True,no_duplicate_revision=True),indent=2)+'\n')
    print('P5-D GTK: responsive loop, cancel boundary, rollback, queued exit, recovery and project close OK')
finally:
    if app and app.poll() is None and not (root/'resultado.json').exists():
        try:subprocess.run(['import','-window',str(observed().get('window','root')),str(root/'falha.png')],env=env,timeout=5,capture_output=True)
        except (OSError,subprocess.TimeoutExpired):pass
    hold.unlink(missing_ok=True);os.chmod(packet.parent,0o700)
    if app and app.poll() is None:os.killpg(app.pid,signal.SIGTERM);app.wait(timeout=10)
    if log and not log.closed:log.close()
    if manager and manager.poll() is None:manager.terminate();manager.wait(timeout=10)
