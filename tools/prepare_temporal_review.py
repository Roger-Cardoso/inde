"""Seal the first temporal adaptation cut; preserve predecessors and fresh review copies."""
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
parser.add_argument("--release-report",required=True,type=Path)
parser.add_argument("--asan-report",required=True,type=Path)
parser.add_argument("--visual-report",required=True,type=Path)
parser.add_argument("--regression-report",required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1]
root=source.parent
delivery=root.parents[1]/"outputs"
base="69cfcaf585678abac49f66cdef381bf377e416e8"
def digest(p): return sha256(p.read_bytes()).hexdigest()
def git(*cmd): return subprocess.check_output(["git",*cmd],cwd=source,text=True).strip()
def rows(p):
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        return {name:Counter(repr(row) for row in db.execute('SELECT * FROM "'+name.replace('"','""')+'"'))
                for (name,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}

reports=[json.loads(p.read_text()) for p in (args.release_report,args.asan_report,args.visual_report,args.regression_report)]
release,asan,visual,regression=reports
binary_hash=digest(root/"build/inde")
for r,focal in ((release,False),(asan,True)):
    assert r["complete"] and r["scope"]=="temporal-first-cut-ui" and r["origins_unchanged"]
    assert r["focal_sanitizers"]==focal and len(r["checks"])==8 and all(c["passed"] for c in r["checks"].values())
    assert r["binary_sha256"]==binary_hash
    assert r["driver_sha256"]==digest(root/"build"/("inde_ui_temporal_system_smoke"+("_asan" if focal else "")))
assert visual["complete"] and visual["reviewed_by_agent"] and visual["binary_sha256"]==binary_hash
for item in visual["screenshots"]: assert digest(Path(item["path"]))==item["sha256"]
assert regression["origins_unchanged"] and all(c["passed"] for c in regression["checks"].values())
assert regression["binary"]["sha256"]==binary_hash
ctest=root/"logs/ctest-temporal-final-2026-10-10.log"
core_asan=root/"logs/temporal-core-asan-final2-2026-10-10.log"
preservation=delivery/"reforma-temporal-preservacao-2026-10-10.json"
assert "100% tests passed out of 23" in ctest.read_text()
assert "PASS focal temporal_system ASan/UBSan" in core_asan.read_text()
assert json.loads(preservation.read_text())["original_source_unchanged"]
assert not git("status","--porcelain") and git("rev-parse","HEAD^")==base
previous=sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous)==8
for p in previous:
    for item in json.loads(p.read_text())["binaries"].values(): assert digest(Path(item["path"]))==item["sha256"]
manifest=delivery/"reforma-temporal-entrega-2026-10-10.json"
assert not manifest.exists() and not (root/"review-temporal").exists()
result={"technical_verification_passed":True,"human_acceptance":"pending","published":False,
        "whole_temporal_system_complete":False,"whole_reform_complete":False,"stage":"temporal_first_cut",
        "source":str(source),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),
        "predecessor_commit":base,"predecessor_eighth_block_human_verdict":"rejected",
        "schema":23,"binaries":{},"review_copies":[],"source_artifacts":{},
        "previous_frozen_deliveries_preserved":[str(p) for p in previous],
        "limits":["Gregorian and Julian are proleptic, astronomical year numbering; no regional cutovers presumed",
                  "Custom engine still one leap day per simple cycle; broader rules, eras, historical/religious presets pending",
                  "Regular clock only: 1-8 subdivisions, no presumed UTC or civil zone; month/year not fixed durations",
                  "Named date/time references not yet linked to events, travel, documents or automatic chronology sorting",
                  "64 immutable calendar versions/project; 32 months, 60 days/month, 16 weekdays; +-1000000 years",
                  "128 adoptions/axis; 500 named temporal references/axis; bounded queries, no truncation of stored data",
                  "Map graphical/technical reform, time-variable maps and real world-map templates are future work",
                  "Focal sanitizers only, 7 core and 4 UI units; dependencies Release; LSan disabled",
                  "Automated UI and agent visual review are not human acceptance, assistive technology or whole-app certification"]}
evidence=(*[p.resolve() for p in (args.release_report,args.asan_report,args.visual_report,args.regression_report)],ctest,core_asan,preservation)
result["verification_artifacts"]=[{"path":str(p),"sha256":digest(p)} for p in evidence]
for kind,name in (("inde","inde"),("laboratory","inde_text_blocks_lab")):
    target=delivery/("inde-"+("reforma" if kind=="inde" else "laboratorio")+"-temporal-2026-10-10")
    assert not target.exists(); shutil.copy2(root/"build"/name,target); target.chmod(0o750)
    result["binaries"][kind]={"path":str(target),"sha256":digest(target)}
config=root/"review-temporal-config"
assert not config.exists()
shutil.copytree(root/"review-mundo-config",config)
env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory")
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    origin=root/"review-mundo"/name; before=rows(origin)
    target=root/"review-temporal"/name; item=snapshot(origin,target)
    marker=target/"EDITORIAL_REVIEW_COPY"
    assert marker.read_text().strip()=="INDE_EDITORIAL_REVIEW_COPY_2026_10_07"
    subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(target)],check=True,env=env)
    assert rows(origin)==before
    after=rows(target)
    for table,values in before.items(): assert not values-after[table],"Migration altered existing row: "+table
    with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()[0]==23
        for table in ("calendar_revisions","calendar_adoptions","temporal_references"):
            assert db.execute("SELECT count(*) FROM "+table).fetchone()[0]==0
    (target/"TEMPORAL_REVIEW_COPY").write_text("INDE_TEMPORAL_REVIEW_COPY_2026_10_10\n")
    item.update(key=key,schema=23,existing_rows_preserved=True,prior_review_unchanged=True)
    result["review_copies"].append(item)
bundle=delivery/"inde-reforma-temporal-2026-10-10.bundle"
patch=delivery/"inde-reforma-temporal-2026-10-10.patch"
assert not bundle.exists() and not patch.exists()
subprocess.run(["git","bundle","create",str(bundle),"HEAD"],cwd=source,check=True)
subprocess.run(["git","diff",base,"HEAD","--output="+str(patch)],cwd=source,check=True)
result["source_artifacts"]={p.suffix[1:]:{"path":str(p),"sha256":digest(p)} for p in (bundle,patch)}
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("TEMPORAL_REVIEW_SEALED",manifest)
