"""Seal the second temporal candidate in fresh review copies, never overwriting earlier deliveries."""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--ui",required=True,type=Path)
parser.add_argument("--ui-asan",required=True,type=Path)
parser.add_argument("--regression",required=True,type=Path)
parser.add_argument("--visual",required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1]
root=source.parent
delivery=root.parents[1]/"outputs"
prefix="temporal-templates-2026-10-10"
base="3f7eae22ff6e230d4ef4030d99d05e75e15d5f23"
def digest(p):return sha256(p.read_bytes()).hexdigest()
def git(*cmd):return subprocess.check_output(["git",*cmd],cwd=source,text=True).strip()
def rows(p):
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        return {t:Counter(repr(r) for r in db.execute('SELECT * FROM "'+t.replace('"','""')+'"')) for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}

assert not git("status","--porcelain") and git("rev-parse","HEAD^")==base
binary_hash=digest(root/"build/inde")
for path,focal,scope in ((args.ui,False,"temporal-templates-ui"),(args.ui_asan,True,"temporal-templates-ui"),(args.regression,False,"temporal-first-cut-ui")):
    report=json.loads(path.read_text())
    assert report["complete"] and report["scope"]==scope and report["origins_unchanged"]
    assert report["focal_sanitizers"]==focal and len(report["checks"])==8 and all(v["passed"] for v in report["checks"].values())
    assert report["binary_sha256"]==binary_hash
    driver_name="inde_ui_temporal_system_smoke" if scope=="temporal-first-cut-ui" else "inde_ui_temporal_templates_smoke"
    assert digest(root/"build"/(driver_name+("_asan" if focal else "")))==report["driver_sha256"]
visual=json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"]==binary_hash
for f in visual["screenshots"]:assert digest(Path(f["path"]))==f["sha256"]
ctest=root/"logs/ctest-temporal-templates-final-2026-10-10.log"
asan=root/"logs/temporal-templates-core-asan-final-2026-10-10.log"
preservation=delivery/("reforma-"+prefix+"-preservacao.json")
assert "100% tests passed" in ctest.read_text() and "out of 24" in ctest.read_text()
assert "PASS focal temporal_templates ASan/UBSan" in asan.read_text()
assert json.loads(preservation.read_text())["original_source_unchanged"]
previous=sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous)==9
for p in previous:
    for item in json.loads(p.read_text())["binaries"].values():assert digest(Path(item["path"]))==item["sha256"]
manifest=delivery/"reforma-temporal-templates-entrega-2026-10-10.json"
assert not manifest.exists() and not (root/"review-temporal-templates").exists()
result={"stage":"temporal_templates_and_bindings","technical_verification_passed":True,"human_acceptance":"pending",
        "published":False,"whole_temporal_system_complete":False,"whole_reform_complete":False,
        "source":str(source),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),
        "predecessor_commit":base,"predecessor_human_acceptance":"confirmed_by_user","schema":24,
        "binaries":{},"review_copies":[],"source_artifacts":{},"previous_frozen_deliveries_preserved":[str(p) for p in previous],
        "limits":["Chinese ICU calculated model, associated Gregorian year 1901-2100; not universal historical/observational calendar",
                  "Hebrew calculated, Islamic civil tabular and Ethiopic Amete Mihret: own-era years 1-9999; no sunset/religious observance",
                  "Ancient Egyptian civil model has no assumed terrestrial epoch or regnal era",
                  "Maya Haab cycles anchored by GMT 584283; Long Count/Tzolkin derived display, not independent input or alternative correlations",
                  "ICU rules pinned to saved version; engine upgrades require future explicit migration",
                  "No inferred date semantics, graph materialization, point creation or narrative reordering",
                  "Target search up to 100 matches, ASCII casefold; 500 references/axis and 500 attachments/reference or target",
                  "Regular clock remains a configurable convention; no timezone or historical unequal-hour model",
                  "Monthly view still base-axis lens; contextual object shortcuts, intervals and uncertainty remain proposals",
                  "Focal ASan/UBSan: 8 core units, 4 UI units and drivers; dependencies Release; LSan disabled",
                  "GTK tests and selected visual review do not certify human acceptance, whole app, HiDPI, assistive technology or long writing sessions"]}
for kind,name in (("inde","inde"),("laboratory","inde_text_blocks_lab")):
    target=delivery/("inde-"+("reforma" if kind=="inde" else "laboratorio")+"-"+prefix)
    assert not target.exists();shutil.copy2(root/"build"/name,target);target.chmod(0o750)
    result["binaries"][kind]={"path":str(target),"sha256":digest(target)}
config=root/"review-temporal-templates-config"
assert not config.exists();shutil.copytree(root/"review-temporal-config",config)
env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory")
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    origin=root/"review-temporal"/name;before=rows(origin)
    target=root/"review-temporal-templates"/name;item=snapshot(origin,target)
    subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(target)],check=True,env=env)
    assert rows(origin)==before
    after=rows(target)
    for table,values in before.items():assert not values-after[table],"Migration altered existing rows: "+table
    with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()[0]==24
        assert db.execute("SELECT count(*) FROM temporal_attachments").fetchone()[0]==0
    (target/"TEMPORAL_TEMPLATES_REVIEW_COPY").write_text("INDE_TEMPORAL_TEMPLATES_REVIEW_COPY_2026_10_10\n")
    item.update(key=key,schema=24,authorial_rows_preserved=True,new_attachment_rows=0)
    result["review_copies"].append(item)
bundle=delivery/("inde-reforma-"+prefix+".bundle")
patch=delivery/("inde-reforma-"+prefix+".patch")
assert not bundle.exists() and not patch.exists()
subprocess.run(["git","bundle","create",str(bundle),"HEAD"],cwd=source,check=True)
subprocess.run(["git","bundle","verify",str(bundle)],cwd=source,check=True)
patch.write_bytes(subprocess.check_output(["git","diff","--binary",base,"HEAD"],cwd=source))
for p in (bundle,patch):result["source_artifacts"][p.suffix[1:]]={"path":str(p),"sha256":digest(p)}
evidence=(args.ui,args.ui_asan,args.regression,args.visual,ctest,asan,preservation,delivery/"aceite-temporal-2026-10-10.json")
result["verification_artifacts"]=[{"path":str(p.resolve()),"sha256":digest(p)} for p in evidence]
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("SEALED",manifest)
