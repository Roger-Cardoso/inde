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

BASE=Path.cwd()
sys.path.insert(0,str(BASE))
from ambiente import native_environment
root=Path(sys.argv[1]).absolute();root.mkdir(parents=True,exist_ok=False)
env=native_environment(root/'profile',BASE)
env.update(GDK_BACKEND='x11',GSK_RENDERER='cairo',GTK_A11Y='none',GTK_USE_PORTAL='0',
           G_DEBUG='fatal-criticals',P5_TRANSPORT_BLOCKS='1',XDG_STATE_HOME=str(root/'state'),
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
    wait(ready,'Seletor '+title);input_('windowactivate','--sync',selected)
    input_('key','ctrl+l','ctrl+a')
    subprocess.run(['xclip','-selection','clipboard','-in'],input=str(path),text=True,env=env,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=5)
    input_('key','ctrl+v');wait(lambda s:any(str(c.get('value','')).rstrip('/')==str(path) and str(c.get('window'))==str(selected) for c in s.get('controls',[])),'Caminho escolhido')
    if title.startswith('Abrir'):
        current=wait(lambda s:any(c.get('name')=='GtkFileChooserEntry' and c.get('hittable') and str(c.get('window'))==str(selected) for c in s['controls']),'Campo do seletor')
        c=next(c for c in current['controls'] if c.get('name')=='GtkFileChooserEntry' and c.get('hittable') and str(c.get('window'))==str(selected))
        input_('mousemove','--window',selected,str(c['x']+c['width']//2),str(c['y']+c['height']//2),'click','1')
        wait(lambda s:any(c.get('focus') and str(c.get('window'))==str(selected) and str(c.get('value','')).rstrip('/')==str(path) for c in s['controls']),'Caminho com foco')
    input_('key','Return');time.sleep(.4)
    if title=='Abrir projeto INDE':
        for c in observed().get('controls',[]):
            if c.get('label')=='Abrir' and str(c.get('window'))==str(selected) and c.get('hittable'):
                input_('mousemove','--window',selected,str(c['x']+c['width']//2),str(c['y']+c['height']//2),'click','1');break
    wait(lambda s:subprocess.run(['xdotool','search','--all','--onlyvisible','--pid',str(s.get('pid',0)),'--name',title],env=env,capture_output=True).returncode!=0,'Seletor fechado')
    input_('windowactivate','--sync',str(observed()['window']))


def quit_normal():
    input_('key','ctrl+q')
    code=app.wait(timeout=30)
    if code!=0:raise RuntimeError(f'Encerramento normal falhou: {code}')
    if env.get('P5_NATIVE_DEBUG') and 'received signal' in Path(log.name).read_text():raise RuntimeError('Falha capturada pelo depurador; consulte '+log.name)
    log.close()

hold=root/'file-task-hold';entered=root/'file-task-entered'
def gate():entered.unlink(missing_ok=True);hold.write_text('reading')
def held():
    wait(lambda s:s.get('file_task',{}).get('active') and entered.exists(),'Confirmação antes da troca')
    tick=observed()['observer_tick'];wait(lambda s:s.get('observer_tick',0)>=tick+4,'Janela responde antes da troca')
def settled():
    tick=observed().get('observer_tick',0)
    wait(lambda s:s.get('observer_tick',0)>tick and not s.get('file_task',{}).get('active'),'Troca concluída')
def header():return next((c['label'] for c in observed().get('controls',[]) if c['name']=='native-project-file' and c.get('mapped')),None)
def cancel():click(label='Cancelar salvamento');settled();hold.unlink(missing_ok=True)
def open_folder():input_('key','ctrl+o');choose_destination(root/'fixture/Original-sintetico.inde','Abrir projeto INDE',False)
def new_folder(path):input_('key','ctrl+n');choose_destination(path,'Criar projeto INDE',False)
manager=None
try:
    with (root/'wm.log').open('w') as out:manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=out,stderr=out)
    time.sleep(.8);assert manager.poll() is None
    start('legacy-transitions');document();initial=saved_text();append(' Troca para pasta.')
    changed=observed()['content'];gate();open_folder();held();cancel()
    assert header()==packet.name and saved_text()==initial and observed()['content']==changed
    open_folder();settled();assert header() is None and saved_text()[1]==changed
    input_('key','ctrl+alt+o');choose_destination(packet,'Abrir arquivo único INDE',False);settled();document()
    path=root/'Pasta-nova.inde';gate();new_folder(path);held();cancel()
    assert header()==packet.name and not path.exists()
    new_folder(path);settled();wait(lambda s:path.is_dir() and header() is None,'Pasta nova ativa')
    quit_normal()
    (root/'resultado.json').write_text(json.dumps(dict(open_folder_cancel_preserves_file=True,open_folder_confirms_source=True,new_folder_cancel_preserves_binding=True,new_folder_confirms_source=True,responsive_source_confirmation=True,normal_quit=True),indent=2)+'\n')
    print('P5-D2 legacy: folder open/create source confirmation, cancellation and normal exit OK')
finally:
    hold.unlink(missing_ok=True)
    if app and app.poll() is None:
        try:subprocess.run(['import','-window',str(observed().get('window','root')),str(root/'falha.png')],env=env,timeout=5,capture_output=True)
        except (OSError,subprocess.TimeoutExpired):pass
        os.killpg(app.pid,signal.SIGTERM);app.wait(timeout=10)
    if log and not log.closed:log.close()
    if manager and manager.poll() is None:manager.terminate();manager.wait(timeout=10)
