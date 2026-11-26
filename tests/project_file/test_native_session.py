"""Real X11 keyboard/mouse against the normal native file lifecycle.
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
           G_DEBUG='fatal-criticals',XDG_STATE_HOME=str(root/'state'),
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

def choose_destination(path,title='Salvar cópia transportável do mesmo projeto'):
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
    wait(lambda s:path.exists(),'Cópia publicada')
    input_('windowactivate','--sync',str(observed()['window']))


def quit_normal():
    input_('key','ctrl+q')
    code=app.wait(timeout=30)
    if code!=0:raise RuntimeError(f'Encerramento normal falhou: {code}')
    if env.get('P5_NATIVE_DEBUG') and 'received signal' in Path(log.name).read_text():raise RuntimeError('Falha capturada pelo depurador; consulte '+log.name)
    log.close()

manager=None
try:
    with (root/'wm.log').open('w') as output:
        manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=output,stderr=output)
    time.sleep(.8)
    if manager.poll() is not None:raise RuntimeError('Gerenciador privado não iniciou')
    initial_seq,initial=saved_text()
    start('save');document();append(' Mudanca confirmada.')
    # Native text autosave may persist the workspace; the .inde changes only at
    # the project confirmation barrier, never on each keystroke.
    time.sleep(.8)
    assert saved_text()==(initial_seq,initial)
    input_('key','ctrl+s')
    wait(lambda s:saved_text()[1].endswith(' Mudanca confirmada.'),'Salvar atingiu o arquivo único')
    confirmation_tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>confirmation_tick and not s.get('file_task',{}).get('active'),'Confirmação concluída')
    with sqlite3.connect(f'file:{packet}?mode=ro',uri=True) as db:saved_seq=db.execute('SELECT seq FROM head').fetchone()[0]
    full_copy=root/'Com-historico.inde'
    input_('key','ctrl+alt+s');click(label='Copiar com o histórico');choose_destination(full_copy)
    assert saved_text(full_copy)[1]==saved_text()[1]
    with sqlite3.connect(f'file:{full_copy}?mode=ro',uri=True) as copied, sqlite3.connect(f'file:{packet}?mode=ro',uri=True) as original:
        assert copied.execute('SELECT count(*) FROM states').fetchone()==original.execute('SELECT count(*) FROM states').fetchone()
    # Real history control, selecting the first nonempty state by keyboard.
    input_('key','ctrl+alt+h')
    click(name='project-file-state');input_('key','Home','Return')
    click(label='Retomar este estado')
    wait(lambda s:saved_text()[1]==initial,'Estado anterior confirmado')
    quit_normal()
    with sqlite3.connect(f'file:{packet}?mode=ro',uri=True) as db:
        assert db.execute('SELECT count(*) FROM states WHERE seq=?',(saved_seq,)).fetchone()[0]==1
    # Reopening the same .inde must show its contents and its real path in recents.
    recent=(root/'profile/config/inde/recent-projects')
    # The profile layout is selected by ambiente; locate its convenience list.
    found=list((root/'profile').rglob('recent-projects'))
    assert found and str(packet) in found[0].read_text()
    assert 'work-' not in found[0].read_text()
    start('crash');document();append(' Recuperar esta escrita.')
    wait(lambda s:not s.get('busy'),'Gravação nativa do rascunho')
    time.sleep(1.0)
    os.killpg(app.pid,signal.SIGKILL);app.wait(timeout=10);log.close()
    assert saved_text()[1]==initial
    start('recover')
    click(label='Retomar o trabalho preservado');document()
    wait(lambda s:s.get('content','').endswith(' Recuperar esta escrita.'),'Trabalho recuperado')
    input_('key','ctrl+s')
    wait(lambda s:saved_text()[1].endswith(' Recuperar esta escrita.'),'Recuperação salva no arquivo')
    confirmation_tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>confirmation_tick and not s.get('file_task',{}).get('active'),'Confirmação concluída')
    # Finalization failure must offer a normal exit retaining a durable workspace.
    append(' Saida com recuperacao.')
    os.chmod(packet.parent,0o500)
    input_('key','ctrl+q')
    click(label='Encerrar preservando o trabalho')
    assert app.wait(timeout=30)==0
    log.close();os.chmod(packet.parent,0o700)
    start('after-failed-close')
    click(label='Retomar o trabalho preservado');document()
    wait(lambda s:s.get('content','').endswith(' Saida com recuperacao.'),'Saída falha preservou escrita')
    old_packet=packet;old_saved=saved_text()[1]
    fresh=root/'Estado-atual.inde'
    input_('key','ctrl+alt+s');click(label='Continuar em cópia do estado atual');choose_destination(fresh)
    wait(lambda s:any(c['name']=='native-project-file' and c.get('mapped') and c['label']==fresh.name for c in s.get('controls',[])) and bool(s.get('objects')) and s.get('content','').endswith(' Saida com recuperacao.'),'Cópia atual ativa')
    assert saved_text(old_packet)[1]==old_saved
    packet=fresh
    quit_normal()
    assert saved_text()[1].endswith(' Saida com recuperacao.')
    start('new-native-file')
    new_file=root/'Novo.inde'
    input_('key','ctrl+alt+n');choose_destination(new_file,'Criar arquivo único INDE')
    wait(lambda s:any(c['name']=='native-project-file' and c.get('mapped') and c['label']==new_file.name for c in s.get('controls',[])),'Novo arquivo ativo')
    packet=new_file
    click(label='Novo Documento')
    s=wait(lambda s:any(c.get('role')=='entry' and c.get('hittable') for c in s['controls']),'Nome do Documento')
    c=next(c for c in s['controls'] if c.get('role')=='entry' and c.get('hittable'))
    click(name=c['name']);input_('type','--clearmodifiers','Documento no arquivo unico');click(label='Criar e escrever')
    wait(lambda s:bool(s.get('objects')) and s.get('content')=='','Documento novo')
    append('Primeira escrita em arquivo.')
    input_('key','ctrl+s')
    wait(lambda s:saved_text(title='Documento no arquivo unico')[1]=='Primeira escrita em arquivo.','Novo arquivo salvo')
    confirmation_tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>confirmation_tick and not s.get('file_task',{}).get('active'),'Confirmação concluída')
    input_('key','ctrl+alt+o');choose_destination(new_file,'Abrir arquivo único INDE')
    wait(lambda s:not any(c.get('mapped') and c.get('window') and str(c['window'])!=str(s.get('window')) and c.get('role')=='entry' for c in s.get('controls',[])),'Seletor de abertura fechado')
    quit_normal()
    start('reserve-and-open-saved');document('Documento no arquivo unico');append(' Rascunho reservado.')
    time.sleep(1.0)
    os.killpg(app.pid,signal.SIGKILL);app.wait(timeout=10);log.close()
    start('open-saved');click(label='Abrir o estado salvo');document('Documento no arquivo unico')
    wait(lambda s:s.get('content')=='Primeira escrita em arquivo.','Abrir salvo manteve conteúdo confirmado')
    reserved=False
    for candidate in (root/'state').rglob('work-*.inde/data/project.sqlite3'):
        with sqlite3.connect(f'file:{candidate}?mode=ro',uri=True) as db:
            row=db.execute("SELECT content FROM documents WHERE title='Documento no arquivo unico'").fetchone()
            if row and row[0].endswith(' Rascunho reservado.'):reserved=True
    assert reserved, 'Abrir o salvo apagou a cópia com trabalho pendente'
    quit_normal()
    for p in root.glob('*.log'):
        assert not any(x in p.read_text() for x in ['Gtk-CRITICAL','Gdk-CRITICAL','GLib-GObject-CRITICAL']),p
    (root/'resultado.json').write_text(json.dumps(dict(native_single_file=True,native_create=True,native_open_dialog=True,open_saved_preserves_pending_work=True,explicit_save=True,no_per_keystroke_capture=True,
        history_restore=True,later_history_preserved=True,full_history_copy=True,current_state_copy=True,recents_real_file=True,crash_recovery=True,
        failed_file_save_can_quit=True,preserved_work_reopened=True),indent=2)+'\n')
    print('P5-B GTK: real Save, history restore, recents, crash recovery and failure-safe normal exit OK')
finally:
    os.chmod(packet.parent,0o700)
    if app and app.poll() is None:
        os.killpg(app.pid,signal.SIGTERM);app.wait(timeout=10)
    if log and not log.closed:log.close()
    if manager and manager.poll() is None:
        manager.terminate();manager.wait(timeout=10)
