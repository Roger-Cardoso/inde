import json,os,re,signal,subprocess,time
from pathlib import Path
base=Path.cwd();root=base/'dados/p5b-launcher-20261126';root.mkdir(exist_ok=False)
env=dict(os.environ,GSK_RENDERER='cairo',GTK_A11Y='none',GTK_USE_PORTAL='0',G_DEBUG='fatal-criticals',GDK_BACKEND='x11',GTK_THEME='Adwaita',XDG_CONFIG_HOME=str(root/'config'),XDG_STATE_HOME=str(root/'state'),XDG_DATA_HOME=str(root/'data'),XDG_CACHE_HOME=str(root/'cache'))
manager=process=None
try:
    with (root/'wm.log').open('w') as wm,(root/'launcher.log').open('w') as log:
        manager=subprocess.Popen(['xfwm4','--compositor=off'],env=env,stdout=wm,stderr=wm);time.sleep(.8)
        assert manager.poll() is None
        process=subprocess.Popen(['bash','scripts/demos/Abrir-demo-arquivo-unico-nativo.sh'],env=env,stdout=log,stderr=log,start_new_session=True)
        deadline=time.monotonic()+180;packet=window=None
        while time.monotonic()<deadline:
            assert process.poll() is None,'launcher ended early'
            found=re.search(r'^Arquivo fictício: (.+)$',(root/'launcher.log').read_text(),re.M)
            if found:
                packet=Path(found[1]);r=subprocess.run(['xdotool','search','--onlyvisible','--name','INDE'],env=env,capture_output=True,text=True)
                if r.returncode==0 and r.stdout.strip():window=r.stdout.splitlines()[-1];break
            time.sleep(.2)
        assert packet and packet.is_file() and window
        subprocess.run(['xdotool','windowactivate','--sync',window],env=env,check=True);time.sleep(1)
        subprocess.run(['xdotool','key','--clearmodifiers','ctrl+s'],env=env,check=True);time.sleep(.8)
        subprocess.run(['xdotool','key','--clearmodifiers','ctrl+q'],env=env,check=True)
        assert process.wait(timeout=40)==0
        result=json.loads(subprocess.check_output(['build-nativo/inde_project_file','verificar',str(packet)],text=True))
        assert result['valid']
        assert not any(x in (root/'launcher.log').read_text() for x in ['Gtk-CRITICAL','Gdk-CRITICAL','GLib-GObject-CRITICAL'])
        (root/'resultado.json').write_text(json.dumps(dict(native_launcher=True,synthetic_file=str(packet),direct_file=True,keyboard_save=True,keyboard_quit=True,verified_single_file=True),indent=2)+'\n')
        print('P5-B launcher: normal native application, direct file, Save/Quit and integrity OK')
finally:
    if process and process.poll() is None:os.killpg(process.pid,signal.SIGTERM);process.wait(timeout=10)
    if manager and manager.poll() is None:manager.terminate();manager.wait(timeout=10)
