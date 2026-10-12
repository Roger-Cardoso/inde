"""Seal the first graphic-calendar increment without approving the whole review."""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot

parser=argparse.ArgumentParser(description=__doc__)
for name in ("graphic","asan","context","system","visual"):
    parser.add_argument("--"+name,required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1]
root=source.parent
delivery=root.parents[1]/"outputs"
prefix="calendario-grafico-2026-10-10"
base="a0d2849afac0fcb6297e537b161760da9b8c9473"
def digest(p): return sha256(p.read_bytes()).hexdigest()
def git(*cmd): return subprocess.check_output(["git",*cmd],cwd=source,text=True).strip()
def rows(p):
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()[0]==25
        return {t:Counter(repr(r) for r in db.execute('SELECT * FROM "'+t.replace('"','""')+'"'))
                for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
assert not git("status","--porcelain") and git("rev-parse","HEAD^")==base
binary=digest(root/"build/inde")
for path,scope,focal in ((args.graphic,"calendar-graphic-ui",False),(args.asan,"calendar-graphic-ui",True),
                         (args.context,"temporal-context-ui",False),(args.system,"temporal-first-cut-ui",False)):
    r=json.loads(path.read_text())
    assert r["complete"] and r["origins_unchanged"] and r["scope"]==scope
    assert r["focal_sanitizers"]==focal and r["binary_sha256"]==binary
    assert len(r["checks"])==8 and all(v["passed"] for v in r["checks"].values())
visual=json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"]==binary and len(visual["screenshots"])>=6
for frame in visual["screenshots"]: assert digest(Path(frame["path"]))==frame["sha256"]
ctest=root/"logs/ctest-calendar-graphic-final-2026-10-10.log"
core=root/"logs/calendar-core-asan-2026-10-10.log"
preservation=delivery/"reforma-calendario-grafico-preservacao-2026-10-10.json"
assert "100% tests passed" in ctest.read_text() and "out of 26" in ctest.read_text()
assert "PASS calendar projection: 13793 cells" in core.read_text()
assert json.loads(preservation.read_text())["original_source_unchanged"]
previous=sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous)==11
for path in previous:
    for item in json.loads(path.read_text())["binaries"].values(): assert digest(Path(item["path"]))==item["sha256"]
manifest=delivery/"reforma-calendario-grafico-entrega-2026-10-10.json"
assert not manifest.exists()
result={"stage":"calendar_graphic_first_increment","technical_verification_passed":True,
        "human_acceptance":"pending","review_accepted_by_user":True,"published":False,
        "whole_review_implemented":False,"whole_temporal_system_complete":False,"whole_reform_complete":False,
        "entity_recognition_started":False,"entity_recognition_order":"last",
        "source":str(source),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),
        "predecessor_commit":base,"predecessor_human_acceptance":"pending","schema":25,
        "binaries":{},"review_copies":[],"source_artifacts":{},
        "previous_frozen_deliveries_preserved":[str(p) for p in previous],
        "limits":["First increment: shared picker, replaced date forms, monthly conversion lenses and reference search",
                  "Creating and binding a NEW reference inside writing still needs atomic service/history/recovery integration",
                  "Contextual shortcuts on all target details and compact transient writing surface remain authorized follow-ups",
                  "Monthly consultation shows points/events, not all authorial references and intervals; regime explained, not per-cell cut markers",
                  "Own week omissions/resets, sunset, zones/maps, eras and recurrence not implemented",
                  "Initial viewport without context is first supported positive year, never an authorial selection",
                  "Focal ASan/UBSan: three core units and three UI units; dependencies Release, LSan disabled",
                  "Selected agent visual inspection and GTK tests are not human acceptance, complete accessibility certification or measured writer task improvement"]}
for kind,name in (("inde","inde"),("laboratory","inde_text_blocks_lab")):
    target=delivery/("inde-"+("reforma" if kind=="inde" else "laboratorio")+"-"+prefix)
    assert not target.exists();shutil.copy2(root/"build"/name,target);target.chmod(0o750)
    result["binaries"][kind]={"path":str(target),"sha256":digest(target)}
config=root/"review-calendario-grafico-config"
assert not config.exists();shutil.copytree(root/"review-temporal-contexto-config",config)
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    origin=root/"review-temporal-contexto"/name;before=rows(origin)
    target=root/"review-calendario-grafico"/name;item=snapshot(origin,target)
    assert rows(origin)==before and rows(target)==before
    (target/"CALENDAR_GRAPHIC_REVIEW_COPY").write_text("INDE_CALENDAR_GRAPHIC_REVIEW_COPY_2026_10_10\n")
    item.update(key=key,schema=25,authorial_rows_preserved=True);result["review_copies"].append(item)
bundle=delivery/("inde-reforma-"+prefix+".bundle");patch=delivery/("inde-reforma-"+prefix+".patch")
assert not bundle.exists() and not patch.exists()
subprocess.run(["git","bundle","create",str(bundle),"HEAD"],cwd=source,check=True)
subprocess.run(["git","bundle","verify",str(bundle)],cwd=source,check=True)
patch.write_bytes(subprocess.check_output(["git","diff","--binary",base,"HEAD"],cwd=source))
for p in (bundle,patch): result["source_artifacts"][p.suffix[1:]]={"path":str(p),"sha256":digest(p)}
evidence=(args.graphic,args.asan,args.context,args.system,args.visual,ctest,core,preservation,
          delivery/"aceite-revisao-calendarios-graficos-2026-10-10.json")
result["verification_artifacts"]=[{"path":str(p.resolve()),"sha256":digest(p)} for p in evidence]
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("SEALED",manifest)
