"""Private Xvfb/profile/snapshots only. Automated task probes are not human acceptance."""
import argparse, os, re, select, subprocess, time, json, sqlite3, hashlib
from collections import Counter
from datetime import datetime
from pathlib import Path
from reform_snapshots import snapshot
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--mode',choices=('release','debug','asan'),default='release')
p.add_argument('--case',choices=('atlas-light','atlas-dark','empty-light','empty-dark'))
p.add_argument('--lab',action='store_true')
p.add_argument('--gdb',action='store_true',help='Diagnostic backtrace only; never seals a passing matrix')
a=p.parse_args();source=Path(__file__).resolve().parents[1];root=source.parent
tag=datetime.now().strftime('%Y%m%d-%H%M%S-%f');qa=root/'qa'/('keyboard-'+a.mode+'-'+tag);logs=root/'logs'/qa.name
qa.mkdir(parents=True);logs.mkdir(parents=True)
assert not a.lab or a.mode=='release','Laboratory probe is Release, not focal instrumentation'
driver=root/'build'/('inde_ui_keyboard_lab_smoke' if a.lab else 'inde_ui_keyboard_smoke' if a.mode=='release' else 'inde_ui_keyboard_'+a.mode)
digest=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
report=dict(complete=False,human_acceptance='pending',configuration=a.mode,driver_sha256=digest(driver),binary_sha256=digest(root/'build/inde'),cases={},screenshots=[],limits=['Private GTK task probes, not a human writer session','No IBus, screen-reader, HiDPI, Wayland or window-manager certification','Debug/ASan instrument eleven routing/UI units and driver only; dependencies Release, LSan disabled'])
report.update(laboratory=a.lab,laboratory_binary_sha256=digest(root/'build/inde_text_blocks_lab'))
def rows(project):
    with sqlite3.connect((project/'data/project.sqlite3').as_uri()+'?mode=ro',uri=True) as db:
        assert db.execute('PRAGMA integrity_check').fetchall()==[('ok',)]
        assert not db.execute('PRAGMA foreign_key_check').fetchall()
        result={}
        for (table,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'"):
            quoted='"'+table.replace('"','""')+'"'
            cols='*' if table!='projects' else ','.join('NULL' if col[1]=='updated_at' else '"'+col[1]+'"' for col in db.execute('PRAGMA table_info('+quoted+')'))
            result[table]=Counter(repr(row) for row in db.execute('SELECT '+cols+' FROM '+quoted))
        return result
env=dict(os.environ,GSETTINGS_BACKEND='memory',G_DEBUG='fatal-warnings',GDK_BACKEND='x11',GTK_IM_MODULE='simple',GSK_RENDERER='cairo',INDE_CLIPBOARD_TEST_ISOLATED='1')
launcher=root.parents[1]/'outputs/inde-realizacao-continuidade-abrir.sh'
env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'",launcher.read_text(),re.M)))
if a.mode=='asan':env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
xvfb='/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb'
r,w=os.pipe()
with (logs/'xvfb.log').open('w') as out:
    server=subprocess.Popen([xvfb,'-displayfd',str(w),'-screen','0','1280x900x24','-nolisten','tcp'],stdout=out,stderr=subprocess.STDOUT,pass_fds=(w,));os.close(w)
    try:
        assert select.select([r],[],[],15)[0];display=os.read(r,64).decode().strip();assert display.isdigit();env['DISPLAY']=':'+display
        subprocess.run(['setxkbmap','-layout','br','-model','abnt2'],env=env,check=True)
        report['keyboard_layout']='br, model abnt2 on private Xvfb; native XTest delivery, not a physical-device certification'
        for kind,name in (('atlas','Atlas de Véspera — Projeto Integral.inde'),('empty','Projeto vazio — sem exemplos.inde')):
            origin=root/'review-vocabulario-autoral'/name
            if kind=='empty':
                origin=qa/'pristine.inde';subprocess.run([str(root/'build/inde_text_blocks_empty_fixture'),str(origin)],env=dict(env,XDG_CONFIG_HOME=str(qa/'seed-config')),check=True)
            before=rows(origin)
            for theme in ('light','dark'):
                case=kind+'-'+theme
                if a.case and a.case!=case:continue
                target=qa/case/name;snapshot(origin,target)
                if a.lab:(target/'TEXT_BLOCKS_LAB_COPY').write_text('INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n')
                scoped=dict(env,XDG_CONFIG_HOME=str(qa/('config-'+case)))
                ack=qa/('capture-ack-'+case);ack.mkdir();scoped['INDE_UI_CAPTURE_ACK_DIR']=str(ack)
                config=Path(scoped['XDG_CONFIG_HOME'])/'inde';config.mkdir(parents=True);(config/'interface.ini').write_text('[appearance]\ntheme='+theme+'\n')
                log=logs/(case+'.log');start=time.monotonic();captured=set();print('RUN',a.mode,case,flush=True)
                with log.open('w') as stream:
                    command=[str(driver),str(target),'--allow-fixture-writes']
                    if a.gdb:command=['gdb','--batch','-ex','run','-ex','bt','--args',*command]
                    child=subprocess.Popen(command,env=scoped,stdout=stream,stderr=subprocess.STDOUT)
                    while child.poll() is None:
                        if time.monotonic()-start>120:child.kill();child.wait();raise RuntimeError('Timeout '+case)
                        for stage in re.findall(r'^SNAPSHOT: (.+)$',log.read_text(errors='replace'),re.M):
                            if stage in captured:continue
                            frame=logs/(case+'-'+stage+'.png');subprocess.run(['import','-window','root',str(frame)],env=scoped,check=True,timeout=8)
                            (ack/stage).write_text('captured\n')
                            report['screenshots'].append(str(frame));captured.add(stage)
                        time.sleep(.12)
                report['cases'][case]=dict(exit_code=child.returncode,passed=False,log=str(log))
                text=log.read_text();assert child.returncode==0,text[-5000:]
                assert not a.gdb,'Diagnostic gdb output is not certification'
                assert 'PASS keyboard GTK' in text
                assert not re.search(r'Gtk-(?:WARNING|CRITICAL)|GLib-(?:WARNING|CRITICAL)|AddressSanitizer:|runtime error:|FAIL keyboard',text),text[-5000:]
                after=rows(target)
                for table,values in before.items():
                    if table=='sqlite_sequence':continue # Operational AUTOINCREMENT counter, not an author record.
                    assert not (values-after.get(table,Counter())), 'Prior author rows changed: '+table
                assert rows(origin)==before,'Origin changed'
                commands=[]
                for id,key,label,group in re.findall(r'^COMMAND: ([^|\n]+)\|([^|\n]*)\|([^|\n]+)\|([^\n]+)$',text,re.M):commands.append(dict(id=id,accelerator=key,label=label,context=group))
                assert len({c['id'] for c in commands})==len(commands)
                report['cases'][case]=dict(passed=True,prior_rows_preserved=True,preservation_scope='All prior rows retained except operational sqlite_sequence counters and projects.updated_at; origin unchanged in full',integrity='ok',foreign_keys='ok',commands=len(commands),log=str(log),seconds=round(time.monotonic()-start,2))
                (logs/(case+'-commands.json')).write_text(json.dumps(commands,ensure_ascii=False,indent=2)+'\n')
                print('PASS',case,len(commands),'registered commands',flush=True)
        report['complete']=True
    finally:
        if 'child' in locals() and child.poll() is None:
            child.terminate();child.wait(timeout=10)
        server.terminate();server.wait(timeout=10);os.close(r)
        (logs/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        print(logs/'report.json',flush=True)
