"""Bounded color/laboratory regression for this correction, not full reform QA."""
from pathlib import Path
from datetime import datetime
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import re
import select
import sqlite3
import subprocess
import time
from reform_snapshots import snapshot

p=argparse.ArgumentParser(description=__doc__)
p.add_argument("--color-asan",action="store_true")
a=p.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent
delivery=root.parents[1]/"outputs"
run=datetime.now().strftime("%Y%m%d-%H%M%S-%f")
qa=root/"qa"/("access-editor-"+run);logs=root/"logs"/("access-editor-"+run)
qa.mkdir(parents=True);logs.mkdir(parents=True)
def digest(path):return sha256(path.read_bytes()).hexdigest()
def rows(path):
    with sqlite3.connect((path/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        result={}
        for (table,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            q='"'+table.replace('"','""')+'"';cols="*"
            if table=="projects":
                cols=",".join("NULL" if c[1]=="updated_at" else '"'+c[1]+'"' for c in db.execute("PRAGMA table_info("+q+")"))
            result[table]=Counter(repr(row) for row in db.execute("SELECT "+cols+" FROM "+q))
        return result
modes=("color",) if a.color_asan else ("color","text_workshop")
drivers={mode:root/"build"/("inde_ui_"+mode+"_smoke"+("_asan" if a.color_asan else "")) for mode in modes}
report=dict(scope="access-color-focal-asan" if a.color_asan else "access-editor-regressions",
    complete=False,human_acceptance="pending",checks={},screenshots=[],
    binary_sha256=digest(root/"build/inde"),laboratory_sha256=digest(root/"build/inde_text_blocks_lab"),
    driver_sha256={mode:digest(driver) for mode,driver in drivers.items()},
    limits=["Laboratory uses prepared disposable copies with source documents; pristine onboarding is covered by the separate access matrix",
            "Focal color sanitizers cover four UI units and driver; dependencies Release, LSan disabled"] if a.color_asan else
           ["Focused editor/color and laboratory regression, not the full reform matrix"])
env=dict(os.environ,GSETTINGS_BACKEND="memory",G_DEBUG="fatal-warnings",GDK_BACKEND="x11",
    GTK_IM_MODULE="simple",INDE_CLIPBOARD_TEST_ISOLATED="1",GSK_RENDERER="cairo")
launcher=Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/outputs/editor-interacao-abrir-revisao.sh")
env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'",launcher.read_text(),re.M)))
if a.color_asan:env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
xvfb="/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb"
reader,writer=os.pipe()
with (logs/"xvfb.log").open("w") as output:
    server=subprocess.Popen([xvfb,"-displayfd",str(writer),"-screen","0","1280x900x24","-nolisten","tcp"],stdout=output,stderr=subprocess.STDOUT,pass_fds=(writer,))
    os.close(writer)
    try:
        assert select.select([reader],[],[],15)[0]
        display=os.read(reader,64).decode().strip()
        assert display.isdigit() and server.poll() is None
        env["DISPLAY"]=":"+display
        original=Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-interacao-2026-10-04/review")
        for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
            origin=original/name;before=rows(origin)
            for theme in ("light","dark"):
                for mode in modes:
                    case=key+"-"+mode+"-"+theme
                    target=qa/case/name;snapshot(origin,target)
                    scoped=dict(env,XDG_CONFIG_HOME=str(qa/("config-"+case)))
                    config=Path(scoped["XDG_CONFIG_HOME"])/"inde";config.mkdir(parents=True)
                    (config/"interface.ini").write_text("[appearance]\ntheme="+theme+"\n")
                    (target/"EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
                    subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(target)],env=scoped,check=True)
                    log=logs/(case+".log");captured=set();start=time.monotonic()
                    command=[str(drivers[mode]),str(target)]
                    if mode=="color":command.append("--allow-fixture-writes")
                    print("RUN",case,flush=True)
                    with log.open("w") as stream:
                        child=subprocess.Popen(command,env=scoped,stdout=stream,stderr=subprocess.STDOUT)
                        while child.poll() is None:
                            if time.monotonic()-start>120:
                                child.kill();child.wait();raise RuntimeError("Timeout "+case)
                            for stage in re.findall(r"^SNAPSHOT: (.+)$",log.read_text(),re.M):
                                if stage in captured:continue
                                time.sleep(.35)
                                frame=delivery/("reforma-acesso-editor-"+case+"-"+stage+"-"+run+".png")
                                subprocess.run(["import","-window","root",str(frame)],env=scoped,check=True,timeout=8)
                                captured.add(stage);report["screenshots"].append(str(frame))
                            time.sleep(.035)
                        code=child.wait()
                    text=log.read_text()
                    passed=code==0 and "PASS:" in text and not re.search(r"FAIL|\bwarning\b|CRITICAL|AddressSanitizer|runtime error:",text,re.I)
                    report["checks"][case]=dict(passed=passed,exit_code=code,log=str(log),seconds=round(time.monotonic()-start,3))
                    (logs/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
                    assert passed,text[-8000:]
                    after=rows(target)
                    for table,values in before.items():
                        if table in ("change_log","sqlite_sequence") or table.startswith("cartographic_position_index_"):continue
                        assert not values-after[table],"Existing rows changed: "+table
                    assert rows(origin)==before
                    print("PASS",case,flush=True)
        report["origins_unchanged"]=True
    finally:
        os.close(reader);server.terminate();server.wait(timeout=10)
assert len(report["checks"])==len(modes)*4
assert digest(root/"build/inde")==report["binary_sha256"]
assert digest(root/"build/inde_text_blocks_lab")==report["laboratory_sha256"]
for mode,driver in drivers.items():assert digest(driver)==report["driver_sha256"][mode]
report["complete"]=True
path=logs/"report.json";path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
print("PASS ACCESS EDITOR",path,flush=True)
