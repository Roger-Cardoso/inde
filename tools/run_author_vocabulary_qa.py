"""Isolated vocabulary/form QA. Snapshots never overwrite review or author data.

Automatic task probes are not human usability or full accessibility acceptance.
"""
import argparse
from collections import Counter
from datetime import datetime
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import select
import sqlite3
import subprocess
import time
from reform_snapshots import snapshot

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--asan",action="store_true")
parser.add_argument("--editor-fixes",action="store_true")
parser.add_argument("--case",choices=("atlas-light","atlas-dark","empty-light","empty-dark"))
args=parser.parse_args()
assert not (args.asan and args.editor_fixes)
source=Path(__file__).resolve().parents[1];root=source.parent
run=datetime.now().strftime("%Y%m%d-%H%M%S-%f")
qa=root/"qa"/("vocabulary-"+run);logs=root/"logs"/("vocabulary-"+run)
qa.mkdir(parents=True);logs.mkdir(parents=True)
driver=root/"build"/("inde_ui_editor_fixes_smoke" if args.editor_fixes else "inde_ui_author_vocabulary_smoke"+("_asan" if args.asan else ""))
report=dict(scope="editor-selection-dictionary-inline" if args.editor_fixes else "vocabulary-task-probes",run=run,complete=False,human_acceptance="pending",
    focal_sanitizers=args.asan,driver_sha256=sha256(driver.read_bytes()).hexdigest(),
    binary_sha256=sha256((root/"build/inde").read_bytes()).hexdigest(),checks={},screenshots=[],
    limits=["GTK task probes on private Xvfb, not observed writer testing",
            "Not a screen-reader, IBus, HiDPI or long-session certification",
            "Focal sanitizers instrument seven affected UI units and driver; dependencies Release, LSan disabled"])

def rows(project):
    with sqlite3.connect((project/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        result={}
        for (table,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            quoted='"'+table.replace('"','""')+'"';cols="*"
            if table=="projects":
                cols=",".join("NULL" if c[1]=="updated_at" else '"'+c[1]+'"' for c in db.execute("PRAGMA table_info("+quoted+")"))
            result[table]=Counter(repr(row) for row in db.execute("SELECT "+cols+" FROM "+quoted))
        return result

env=dict(os.environ,GSETTINGS_BACKEND="memory",G_DEBUG="fatal-warnings",GDK_BACKEND="x11",
    GTK_IM_MODULE="simple",INDE_CLIPBOARD_TEST_ISOLATED="1",GSK_RENDERER="cairo")
launcher=root.parents[1]/"outputs/inde-realizacao-continuidade-abrir.sh"
env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'",launcher.read_text(),re.M)))
if args.asan:env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
if args.editor_fixes:
    env["GTK_THEME"]="Adwaita"
    report["limits"]=["Legacy editor probe uses compatible Adwaita GTK base; application light/dark styles still tested",
                      "Not screen-reader, IBus, HiDPI or human acceptance"]
xvfb="/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb"
reader,writer=os.pipe()
with (logs/"xvfb.log").open("w") as out:
    server=subprocess.Popen([xvfb,"-displayfd",str(writer),"-screen","0","1280x900x24","-nolisten","tcp"],stdout=out,stderr=subprocess.STDOUT,pass_fds=(writer,))
    os.close(writer)
    try:
        assert select.select([reader],[],[],15)[0]
        display=os.read(reader,64).decode().strip();assert display.isdigit() and server.poll() is None
        env["DISPLAY"]=":"+display
        for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
            if args.case and not args.case.startswith(key+"-"):continue
            origin=root/"review-realizacao-continuidade"/name
            if key=="empty":
                origin=qa/"pristine-base.inde"
                subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(origin)],
                    env=dict(env,XDG_CONFIG_HOME=str(qa/"seed-config")),check=True)
                with sqlite3.connect(origin/"data/project.sqlite3") as db:
                    for table in ("works","intellectual_properties","documents"):
                        assert db.execute("SELECT count(*) FROM "+table).fetchone()==(0,)
            before=rows(origin)
            for theme in ("light","dark"):
                case=key+"-"+theme
                if args.case and case!=args.case:continue
                target=qa/case/name;snapshot(origin,target)
                scoped=dict(env,XDG_CONFIG_HOME=str(qa/("config-"+case)))
                config=Path(scoped["XDG_CONFIG_HOME"])/"inde";config.mkdir(parents=True)
                (config/"interface.ini").write_text("[appearance]\ntheme="+theme+"\n")
                path=logs/(case+".log");start=time.monotonic();captured=set()
                print("RUN",case,flush=True)
                with path.open("w") as stream:
                    child=subprocess.Popen([str(driver),str(target),"--allow-fixture-writes"],env=scoped,stdout=stream,stderr=subprocess.STDOUT)
                    while child.poll() is None:
                        if time.monotonic()-start>100:
                            child.kill();child.wait();raise RuntimeError("Timeout "+case)
                        for stage in re.findall(r"^SNAPSHOT: (.+)$",path.read_text(),re.M):
                            if stage in captured:continue
                            frame=logs/(case+"-"+stage+".png")
                            subprocess.run(["import","-window","root",str(frame)],env=scoped,check=True,timeout=8)
                            captured.add(stage);report["screenshots"].append(str(frame))
                        time.sleep(.04)
                    code=child.wait()
                text=path.read_text()
                success_phrase="PASS editor fixes:" if args.editor_fixes else "PASS:"
                passed=code==0 and success_phrase in text and not re.search(r"FAIL|CRITICAL|warning|AddressSanitizer|runtime error:",text,re.I)
                report["checks"][case]=dict(passed=passed,exit_code=code,log=str(path),seconds=round(time.monotonic()-start,3),original_unchanged=rows(origin)==before)
                after=rows(target)
                for table,values in before.items():
                    if table in ("change_log","sqlite_sequence") or table.startswith("cartographic_position_index_"):continue
                    assert not values-after[table],"Existing author rows changed: "+table
                (logs/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
                assert passed,text[-7000:]
                assert rows(origin)==before
                print("PASS",case,flush=True)
    finally:
        os.close(reader);server.terminate();server.wait(timeout=10)
report["complete"]=True
(logs/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
print("PASS vocabulary QA",logs/"report.json")
