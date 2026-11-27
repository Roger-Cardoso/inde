"""Real X11 input checks the combined asynchronous file lifecycle.
Use in a fresh Xvfb/D-Bus session. Every project is made by the native fixture.
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
env.update(GDK_BACKEND='x11',GSK_RENDERER='cairo',GTK_A11Y='none',GTK_USE_PORTAL='0',
           G_DEBUG='fatal-criticals',P5_TRANSPORT_BLOCKS='1',P5_HISTORY_PAGES='1',XDG_STATE_HOME=str(root/'state'),
           XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'))
if not env.get('DISPLAY'):raise SystemExit('Use um X11 privado com xvfb-run e dbus-run-session.')
subprocess.run([str(BASE/'build-nativo/inde_project_file_native_tests'),str(root/'fixture')],env=env,check=True)
packet=root/'fixture/Projeto-portatil.inde'
state_file=root/'widgets.json';app=None;log=None

def saved_text(path=None,title='Uma obra transportável'):
    if path is None:path=packet
    with sqlite3.connect(f'file:{path}?mode=ro',uri=True) as db:
        seq=db.execute('SELECT seq FROM head').fetchone()[0]
        kind,payload,blob_hash=db.execute("SELECT v.kind,v.payload,v.blob_hash FROM entries e JOIN record_versions v ON v.id=e.version_id WHERE e.seq=? AND e.key='file/data/project.sqlite3'",(seq,)).fetchone()
        if kind=='chunked-file':
            parts=json.loads(payload)['chunks']
            blob=b''.join(db.execute('SELECT data FROM blobs WHERE hash=?',(h,)).fetchone()[0] for h in parts)
        else:blob=db.execute('SELECT data FROM blobs WHERE hash=?',(blob_hash,)).fetchone()[0]
    with sqlite3.connect(':memory:') as inner:
        inner.deserialize(blob)
        row=inner.execute("SELECT content FROM documents WHERE title=?",(title,)).fetchone();content=row[0] if row else None
    return seq,content

def observed():
    try:return json.loads(state_file.read_text())
    except (FileNotFoundError,json.JSONDecodeError):return {}

def wait(predicate,what,timeout=20):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        if app.poll() is not None:raise RuntimeError(f'Aplicativo encerrou durante {what}; consulte os logs.')
        s=observed()
        if predicate(s):return s
        time.sleep(.08)
    raise RuntimeError(what+': '+json.dumps(observed(),ensure_ascii=False)[:15000])

def input_(*args):subprocess.run(['xdotool',*args],env=env,check=True,capture_output=True,timeout=8)
def controls(s,name=None,label=None):
    return [c for c in s.get('controls',[]) if c.get('mapped') and c.get('sensitive') and c.get('hittable') and ((name and c['name']==name) or (label and c['label']==label))]
def click(name=None,label=None):
    wait(lambda s:bool(controls(s,name,label)), 'Controle '+str(name or label))
    time.sleep(.25)
    c=controls(observed(),name,label)[0]
    input_('mousemove','--window',str(c.get('window',observed()['window'])),str(c['x']+c['width']//2),str(c['y']+c['height']//2),'click','1')

def start(tag):
    global app,log
    state_file.unlink(missing_ok=True)
    log=(root/(tag+'.log')).open('w')
    command=[str(BASE/'build-nativo/inde-especial'),'--arquivo-usabilidade',str(packet),str(state_file)]
    if env.get('P5_NATIVE_DEBUG'):command=['gdb','-q','-batch','-ex','run','-ex','thread apply all bt 25','--args',*command]
    app=subprocess.Popen(command,env=env,stdout=log,stderr=log,start_new_session=True)
    s=wait(lambda s:'window' in s,'Janela')
    input_('windowactivate','--sync',str(s['window']))

def document(title='Uma obra transportável'):
    wait(lambda s:bool(s.get('objects')) or bool(controls(s,label=title)),'Documento disponível')
    time.sleep(.3)
    if not observed().get('objects'):click(label=title)
    wait(lambda s:bool(s.get('objects')) and not s.get('busy'),'Documento composto')

def append(value):
    s=wait(lambda s:any(c.get('hittable') and c['name'].startswith('writing-paragraph-') for c in s.get('controls',[])),'Parágrafo editável')
    names=[c['name'] for c in s['controls'] if c.get('hittable') and c['name'].startswith('writing-paragraph-')]
    click(name=names[-1]);input_('key','ctrl+End');input_('type','--clearmodifiers','--delay','25',value)
    wait(lambda s:s.get('content','').endswith(value),'Texto digitado')

def choose_destination(path,title='Salvar cópia transportável do mesmo projeto',published=True):
    selected=None
    def ready(s):
        nonlocal selected
        found=subprocess.run(['xdotool','search','--all','--onlyvisible','--pid',str(s.get('pid',0)),'--name',title],env=env,capture_output=True,text=True)
        if found.returncode==0 and found.stdout.strip():selected=found.stdout.splitlines()[0];return True
        return False
    wait(ready,'Seletor '+title)
    input_('windowactivate','--sync',selected)
    input_('key','ctrl+l','ctrl+a')
    subprocess.run(['xclip','-selection','clipboard','-in'],input=str(path),text=True,env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
    input_('key','ctrl+v')
    wait(lambda s:any(c.get('value')==str(path) and str(c.get('window'))==str(selected) for c in s.get('controls',[])),'Caminho exato no seletor')
    if title.startswith('Abrir'):
        s=wait(lambda s:any(c.get('name')=='GtkFileChooserEntry' and c.get('mapped') and c.get('hittable') and str(c.get('window'))==str(selected) for c in s.get('controls',[])),'Campo de caminho')
        c=next(c for c in s['controls'] if c.get('name')=='GtkFileChooserEntry' and c.get('mapped') and c.get('hittable') and str(c.get('window'))==str(selected))
        input_('mousemove','--window',str(selected),str(c['x']+c['width']//2),str(c['y']+c['height']//2),'click','1')
        wait(lambda s:any(c.get('value')==str(path) and c.get('focus') and str(c.get('window'))==str(selected) for c in s.get('controls',[])),'Caminho com foco')
        input_('key','--delay','100','Return')
    else:input_('key','Return')
    wait(lambda s:subprocess.run(['xdotool','search','--all','--onlyvisible','--pid',str(s.get('pid',0)),'--name',title],env=env,capture_output=True).returncode!=0,'Seletor fechado')
    if not published:return
    wait(lambda s:path.exists(),'Cópia publicada')
    tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>tick and not s.get('file_task',{}).get('active'),'Operação do arquivo concluída')
    input_('windowactivate','--sync',str(observed()['window']))


def quit_normal():
    input_('key','ctrl+q')
    code=app.wait(timeout=30)
    if code!=0:raise RuntimeError(f'Encerramento normal falhou: {code}')
    if env.get('P5_NATIVE_DEBUG') and 'received signal' in Path(log.name).read_text():raise RuntimeError('Falha capturada pelo depurador; consulte '+log.name)
    log.close()

hold=root/'file-task-hold';entered=root/'file-task-entered'
def gate(phase,title):
    entered.unlink(missing_ok=True);hold.write_text(phase+'\n'+title+'\n')
def held(title):
    return wait(lambda s:s.get('file_task',{}).get('active') and s['file_task'].get('operation')==title and entered.exists(),'Operação reservada: '+title)
def heartbeat():
    tick=observed()['observer_tick']
    s=wait(lambda s:s.get('observer_tick',0)>=tick+4,'Janela responde durante trabalho')
    assert s['file_task']['active']
def settled():
    tick=observed().get('observer_tick',0)
    return wait(lambda s:s.get('observer_tick',0)>tick and not s.get('file_task',{}).get('active'),'Operação concluída',40)
def cancel():
    click(label='Cancelar operação');settled();hold.unlink(missing_ok=True)
def header():
    return next((c['label'] for c in observed().get('controls',[]) if c['name']=='native-project-file' and c.get('mapped')),None)
def open_file(path):
    input_('key','ctrl+alt+o');choose_destination(path,'Abrir arquivo único INDE',False)
def create_file(path):
    input_('key','ctrl+alt+n');choose_destination(path,'Criar arquivo único INDE',False)
def copy_file(path,current=False):
    input_('key','ctrl+alt+s');click(label='Continuar em cópia do estado atual' if current else 'Copiar com o histórico')
    choose_destination(path,published=False)
manager=None
try:
    with (root/'wm.log').open('w') as output:manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=output,stderr=output)
    time.sleep(.8);assert manager.poll() is None
    gate('checking','Abrindo arquivo único');start('combined');held('Abrindo arquivo único');heartbeat();cancel()
    assert header() is None
    open_file(packet);settled();document();assert header()==packet.name
    initial=saved_text()
    append(' Alteracao para testar D2.');input_('key','ctrl+s');settled()
    original=saved_text();assert original[0]==108
    # A confirmed source save and a cancelled subsequent operation are distinct.
    gate('reading','Consultando estados do arquivo');input_('key','ctrl+alt+h');held('Consultando estados do arquivo');heartbeat();cancel()
    assert saved_text()==original and header()==packet.name
    input_('key','ctrl+alt+h')
    s=wait(lambda s:bool(controls(s,name='project-file-state')),'Histórico paginado')
    range_=next(c['label'] for c in s['controls'] if c['name']=='project-file-history-range')
    assert range_.startswith('101–') and range_.endswith(' de 108'),range_
    click(name='project-file-history-older')
    wait(lambda s:any(c['name']=='project-file-history-range' and c.get('label')=='1–100 de 108' for c in s['controls']),'Página anterior')
    click(name='project-file-state');input_('key','Home','Return')
    gate('writing','Retomando estado do arquivo');click(label='Retomar este estado');held('Retomando estado do arquivo');heartbeat();cancel()
    assert saved_text()==original and header()==packet.name
    input_('key','ctrl+alt+h');click(name='project-file-history-older');click(name='project-file-state');input_('key','Home','Return')
    click(label='Retomar este estado');settled();document();assert saved_text()==(original[0]+1,initial[1])
    input_('key','ctrl+alt+h');wait(lambda s:bool(controls(s,name='project-file-state')),'Estado atual selecionado')
    current=saved_text()  # native adoption may have persisted fresh domain metadata
    click(label='Retomar este estado');settled();assert saved_text()==current
    # Full copy, including a cancellable preparation and a sealed publication.
    cancelled=root/'Cancelada.inde';gate('writing','Criando cópia do projeto');copy_file(cancelled)
    held('Criando cópia do projeto');heartbeat();cancel();assert not cancelled.exists() and header()==packet.name
    complete=root/'Completa.inde';gate('committing','Criando cópia do projeto');copy_file(complete)
    held('Criando cópia do projeto');heartbeat();assert not complete.exists()
    wait(lambda s:any(c.get('label')=='Cancelar operação' and c.get('mapped') and not c.get('sensitive') for c in s['controls']),'Publicação sem cancelamento tardio')
    hold.unlink();settled();assert complete.exists() and header()==packet.name
    with sqlite3.connect(f'file:{complete}?mode=ro',uri=True) as a,sqlite3.connect(f'file:{packet}?mode=ro',uri=True) as b:
        assert a.execute('SELECT count(*) FROM states').fetchone()==b.execute('SELECT count(*) FROM states').fetchone()
    gate('reading','Criando cópia do projeto');copy_file(root/'Atual-cancelada.inde',True)
    held('Criando cópia do projeto');heartbeat();cancel();assert header()==packet.name and not (root/'Atual-cancelada.inde').exists()
    fresh=root/'Atual.inde';copy_file(fresh,True);settled();assert header()==fresh.name and saved_text(fresh)[1]==saved_text(packet)[1]
    # A cancelled create keeps the active project. A normal create adopts only
    # after target publication and recovery materialization have completed.
    gate('reading','Criando arquivo único');create_file(root/'Novo-cancelado.inde');held('Criando arquivo único');heartbeat();cancel()
    assert header()==fresh.name and not (root/'Novo-cancelado.inde').exists()
    new=root/'Novo.inde';create_file(new);settled();assert header()==new.name and new.exists()
    gate('checking','Abrindo arquivo único');open_file(complete);held('Abrindo arquivo único');heartbeat();cancel();assert header()==new.name
    open_file(complete);settled();document();assert header()==complete.name
    # Request exit during a read-only listing. Completion must skip presenting
    # the history dialog and go through the normal save/quit barrier.
    gate('reading','Consultando estados do arquivo');input_('key','ctrl+alt+h');held('Consultando estados do arquivo')
    input_('key','ctrl+q');wait(lambda s:s['file_task'].get('exit_requested'),'Saída pendente');heartbeat();hold.unlink()
    assert app.wait(timeout=40)==0;log.close()
    subprocess.run([str(BASE/'build-nativo/inde_project_file'),'verificar',str(complete)],env=env,check=True,capture_output=True)
    assert not any(x in (root/'combined.log').read_text() for x in ['Gtk-CRITICAL','Gdk-CRITICAL','GLib-GObject-CRITICAL'])
    checks=['open_cancel','open_retry','history_cancel','history_pagination','restore_cancel','restore_no_duplicate','copy_cancel','copy_sealed_publication','full_history_preserved','current_copy_cancel','current_copy_adopted','create_cancel','create_adopted','switch_cancel_preserves_binding','switch_adopted','quit_after_history','responsive_loop_all_operations','no_gtk_criticals']
    (root/'resultado.json').write_text(json.dumps(dict.fromkeys(checks,True),indent=2)+'\n')
    print('P5-D2 GTK: combined asynchronous lifecycle, cancellation, pagination, publication and queued quit OK')
finally:
    hold.unlink(missing_ok=True)
    if app and app.poll() is None:
        try:subprocess.run(['import','-window',str(observed().get('window','root')),str(root/'falha.png')],env=env,timeout=5,capture_output=True)
        except (OSError,subprocess.TimeoutExpired):pass
        os.killpg(app.pid,signal.SIGTERM);app.wait(timeout=10)
    if log and not log.closed:log.close()
    if manager and manager.poll() is None:manager.terminate();manager.wait(timeout=10)
