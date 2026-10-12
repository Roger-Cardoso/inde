"""Seal eighth private cut after final-build evidence; never overwrite earlier artifacts."""
from hashlib import sha256
from collections import Counter
import json
import os
from pathlib import Path
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot
SOURCE=Path(__file__).resolve().parents[1]
ROOT=SOURCE.parent
DELIVERY=ROOT.parents[1]/"outputs"
BASE="354e58ac2729d5624f6a422d7b170b483f50b388"
PREFIX="reforma-mundo"
DATE="2026-10-10"
def digest(p):return sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=SOURCE,text=True).strip()
def rows(project):
    with sqlite3.connect((project/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert db.execute("PRAGMA foreign_key_check").fetchall()==[]
        result={}
        for (name,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            quoted='"'+name.replace('"','""')+'"'
            result[name]=Counter(repr(r) for r in db.execute("SELECT * FROM "+quoted))
        return result
def main():
    evidence={name:DELIVERY/f"{PREFIX}-{name}-{DATE}.json" for name in ("verificacao","tema-escuro-final","visual-verificacao","preservacao-verificacao","ui-sanitizadores-final")}
    qa,dark,visual,preserved,asan=(json.loads(evidence[n].read_text()) for n in evidence)
    assert qa["complete"] and qa["stage"]=="worldbuilding" and qa["ui_scenarios"]==46 and qa["origins_unchanged"]
    assert dark["stage"]=="worldbuilding" and dark["theme"]=="dark" and dark["origins_unchanged"] and dark["ui_scenarios"]==20
    assert asan["stage"]=="worldbuilding" and asan["worldbuilding_ui_focal_sanitizers"] and asan["origins_unchanged"] and asan["ui_scenarios"]==4
    assert all(c["passed"] for r in (qa,dark,asan) for c in r["checks"].values())
    assert all(r["g_debug"]=="fatal-warnings" for r in (qa,dark,asan))
    assert preserved["complete"] and preserved["original_source_unchanged"] and preserved["nonignored_files_checked"]==402
    assert visual["complete"] and visual["reviewed_by_agent"] and visual["stage"]=="worldbuilding" and len(visual["screenshots"])>=12
    assert visual["binary_sha256"]==qa["binary"]["sha256"]
    for item in visual["screenshots"]:assert digest(Path(item["path"]))==item["sha256"]
    for field,file in (("binary","inde"),("laboratory_binary","inde_text_blocks_lab")):
        assert digest(ROOT/"build"/file)==qa[field]["sha256"]==dark[field]["sha256"]==asan[field]["sha256"]
    for mode,h in qa["driver_sha256"].items():assert digest(ROOT/"build"/("inde_ui_"+mode+"_smoke"))==h
    assert digest(ROOT/"build/inde_ui_worldbuilding_smoke_asan")==asan["worldbuilding_focal_driver_sha256"]
    ctest=ROOT/"logs/ctest-mundo-final-2026-10-10.log"
    sanitizer=ROOT/"logs/sanitizers-mundo-final-2026-10-10.log"
    assert "100% tests passed" in ctest.read_text() and "out of 22" in ctest.read_text()
    assert "PASS focal worldbuilding ASan/UBSan" in sanitizer.read_text()
    assert not git("status","--porcelain") and git("rev-parse","HEAD^")==BASE
    accept_path=DELIVERY/f"{PREFIX}-aceite-anterior-{DATE}.json"
    accept=json.loads(accept_path.read_text())
    assert accept["human_acceptance"]=="accepted" and accept["accepted_commit"]==BASE and accept["next_implementation_authorized"]
    previous=("reforma-entrega-2026-10-07.json","reforma-organizacao-entrega-2026-10-07.json","reforma-continuidade-entrega-2026-10-07.json","reforma-revisao-entrega-2026-10-07.json","reforma-producao-entrega-2026-10-08.json","reforma-operacoes-entrega-2026-10-08.json","reforma-composicao-entrega-2026-10-08.json")
    for name in previous:
        for artifact in json.loads((DELIVERY/name).read_text())["binaries"].values():assert digest(Path(artifact["path"]))==artifact["sha256"]
    prior=json.loads((DELIVERY/previous[-1]).read_text())
    manifest_path=DELIVERY/f"{PREFIX}-entrega-{DATE}.json"
    assert not manifest_path.exists() and not (ROOT/"review-mundo").exists() and not (ROOT/"review-mundo-config").exists()
    result={"technical_verification_passed":True,"human_acceptance":"pending","whole_reform_complete":False,"published":False,"stage":"aliases_fictional_calendars",
            "source":str(SOURCE),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),"predecessor_commit":BASE,
            "predecessor_delivery":str(DELIVERY/previous[-1]),"binaries":{},"review_copies":[],"source_artifacts":{},"schema":22,
            "semantic_payload":2,"checkpoint_payload":4,"clipboard_payload":2,"preservation_checkpoint":prior["preservation_checkpoint"],
            "human_acceptance_record":{"path":str(accept_path),"sha256":digest(accept_path)},"previous_frozen_deliveries_preserved":list(previous),
            "next_front":"Situated knowledge and additional saved views; new human acceptance required",
            "limits":["Aliases are descriptive names, not semantic bindings, automatic replacements or knowledge/temporal scopes",
                      "64 aliases/entity; 64 immutable calendars/project; 32 months, 60 days/month, 16 weekday names; one leap day; signed years including zero; +-1000000 years",
                      "Not Gregorian/astronomical, multiple eras, movable feasts or multiple leap rules; no definitive calendar deletion UI",
                      "Monthly display up to 500 points and 500 events with real counts and explicit partial-display warning; database not truncated",
                      "Focal ASan/UBSan: 7 core and 4 UI units plus drivers; other dependencies Release; LSan disabled; not whole-app certification",
                      "No human IME, assistive-technology, HiDPI, large-base or long-session certification"]}
    result["verification_artifacts"]={p.name:{"path":str(p),"sha256":digest(p)} for p in (*evidence.values(),ctest,sanitizer)}
    for ext in ("bundle","patch"):
        p=DELIVERY/f"inde-reforma-mundo-{DATE}.{ext}";assert p.is_file();result["source_artifacts"][ext]={"path":str(p),"sha256":digest(p)}
    shutil.copytree(ROOT/"review-composicao-config",ROOT/"review-mundo-config")
    review_env=dict(os.environ,XDG_CONFIG_HOME=str(ROOT/"review-mundo-config"),GSETTINGS_BACKEND="memory")
    for key,file,name in (("inde","inde",f"inde-reforma-mundo-{DATE}"),("laboratory","inde_text_blocks_lab",f"inde-laboratorio-mundo-{DATE}")):
        target=DELIVERY/name;assert not target.exists();shutil.copy2(ROOT/"build"/file,target);target.chmod(0o750)
        result["binaries"][key]={"path":str(target),"sha256":digest(target)}
    for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
        origin=ROOT/"review-composicao"/name
        original_rows=rows(origin)
        target=ROOT/"review-mundo"/name;item=snapshot(origin,target)
        subprocess.run([str(ROOT/"build/inde_prepare_editorial_review_copy"),str(target)],check=True,env=review_env)
        assert rows(origin)==original_rows,"Accepted review copy changed"
        migrated_rows=rows(target)
        for table,before in original_rows.items():
            assert not (before-migrated_rows[table]),"Existing row lost or altered during migration: "+table
        with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
            assert db.execute("SELECT MAX(version) FROM schema_migrations").fetchone()[0]==22
            for table in ("entity_aliases","fictional_calendars","axis_calendars"):
                assert db.execute("SELECT count(*) FROM "+table).fetchone()[0]==0
        (target/"WORLD_REVIEW_COPY").write_text("INDE_WORLD_REVIEW_COPY_2026_10_10\n")
        item.update(key=key,schema=22,integrity_check="ok",foreign_key_check=[],existing_rows_preserved=True,previous_review_rows_unchanged=True)
        result["review_copies"].append(item)
    result["documentation"]={}
    for file,target_name in (("reform-worldbuilding-2026-10-10.md",f"{PREFIX}-implementacao-{DATE}.md"),("worldbuilding-authority-contract-2026-10-10.md",f"{PREFIX}-contrato-{DATE}.md")):
        p=DELIVERY/target_name;assert not p.exists();shutil.copy2(SOURCE/"docs"/file,p);result["documentation"][target_name]={"path":str(p),"sha256":digest(p)}
    launcher=DELIVERY/"inde-mundo-abrir.sh";subprocess.run(["bash","-n",str(launcher)],check=True);launcher.chmod(0o750)
    result["launcher"]={"path":str(launcher),"sha256":digest(launcher)}
    manifest_path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
    print("SEALED",result["commit"],result["binaries"]["inde"]["sha256"],"human acceptance pending")
if __name__=="__main__":main()
