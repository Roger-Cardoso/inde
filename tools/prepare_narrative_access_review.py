"""Seal only a technically verified private access correction; never approval."""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import re
import shutil
import sqlite3
import subprocess
import tarfile
from reform_snapshots import snapshot
parser=argparse.ArgumentParser(description=__doc__)
for key in ("ui","ui-asan","builder-regression","alternatives-regression","temporal-regression","editor-regression","color-asan","startup","ctest","build","debug","asan","visual","preservation"):
    parser.add_argument("--"+key,required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent;delivery=root.parents[1]/"outputs"
def digest(p): return sha256(p.read_bytes()).hexdigest()
def has_runtime_diagnostic(text):
    # This exact test-emitted capture label names the nonblocking impact warning
    # shown to the author; it is not a terminal diagnostic. Do not exclude any
    # other line, including a GTK/compiler warning containing the same text.
    return any(line != "SNAPSHOT: alternatives-impact-warning" and
               re.search(r"\bwarning\b|CRITICAL|AddressSanitizer|runtime error:", line, re.I)
               for line in text.splitlines())
assert not has_runtime_diagnostic("SNAPSHOT: alternatives-impact-warning")
assert has_runtime_diagnostic("Gtk-WARNING **: alternatives-impact-warning")
assert has_runtime_diagnostic("runtime error: invalid access")
assert has_runtime_diagnostic("AddressSanitizer: heap-use-after-free")
assert has_runtime_diagnostic("SNAPSHOT: unexpected-warning")
def git(*args): return subprocess.check_output(["git",*args],cwd=source,text=True).strip()
assert git("rev-parse","HEAD")=="995fbb27f8ee9359643a90f0220bc71269ef5e9a"
subprocess.run(["git","diff","--check"],cwd=source,check=True)
binary=digest(root/"build/inde")
for path,scope,driver in ((args.ui,"narrative-access-production-ui","inde_ui_narrative_access_smoke"),(args.ui_asan,"narrative-access-production-ui","inde_ui_narrative_access_smoke_asan"),(args.builder_regression,"narrative-builder-production-ui","inde_ui_narrative_builder_smoke"),(args.alternatives_regression,"narrative-alternatives-production-ui","inde_ui_narrative_alternatives_smoke"),(args.temporal_regression,"temporal-workflow-ui","inde_ui_temporal_workflow_smoke")):
    r=json.loads(path.read_text());assert r["complete"] and r["origins_unchanged"] and r["scope"]==scope
    assert len(r["checks"])==8 and all(c["passed"] for c in r["checks"].values())
    assert r["binary_sha256"]==binary and r["driver_sha256"]==digest(root/"build"/driver)
for p,scope,n in ((args.editor_regression,"access-editor-regressions",8),(args.color_asan,"access-color-focal-asan",4)):
    r=json.loads(p.read_text());assert r["complete"] and r["origins_unchanged"] and r["scope"]==scope and len(r["checks"])==n
    assert r["binary_sha256"]==binary and r["laboratory_sha256"]==digest(root/"build/inde_text_blocks_lab")
    for mode,sha in r["driver_sha256"].items():
        assert sha==digest(root/"build"/("inde_ui_"+mode+"_smoke"+("_asan" if n==4 else "")))
    for c in r["checks"].values():
        assert c["passed"] and c["exit_code"]==0 and not re.search(r"\bwarning\b|CRITICAL|AddressSanitizer|runtime error:",Path(c["log"]).read_text(),re.I)
assert "100% tests passed" in args.ctest.read_text() and "out of 31" in args.ctest.read_text()
audit=json.loads(args.build.read_text())
assert audit["clean_rebuild_completed"] and audit["all_enabled_units_compiled"] and audit["compiler_warnings"]==0 and audit["binary_sha256"]==binary
for p,end in ((args.debug,"PASS focal builder Debug"),(args.asan,"PASS focal builder ASan/UBSan")):
    t=p.read_text();assert "PASS alternatives:" in t and "PASS: pristine onboarding" in t and end in t
    assert not any(s in t for s in ("AddressSanitizer", "runtime error:", "Traceback", "FAIL"))
r=json.loads(args.startup.read_text());assert r["complete"] and r["scope"]=="startup-only"
for name in ("inde","inde_text_blocks_lab"): assert r["checks"][name]["passed"] and not r["checks"][name]["environmental_warnings"] and r["checks"][name]["binary_sha256"]==digest(root/"build"/name)
for p in (args.ui,args.ui_asan,args.builder_regression,args.alternatives_regression,args.temporal_regression):
    for check in json.loads(p.read_text())["checks"].values():
        assert not has_runtime_diagnostic(Path(check["log"]).read_text())
v=json.loads(args.visual.read_text());assert v["reviewed_by_agent"] and v["binary_sha256"]==binary
for frame in v["screenshots"]:assert digest(Path(frame["path"]))==frame["sha256"]
assert json.loads(args.preservation.read_text())["original_source_unchanged"]
previous=sorted(delivery.glob("reforma*-entrega-*.json"))
for path in previous:
    m=json.loads(path.read_text())
    for b in m["binaries"].values(): assert digest(Path(b["path"]))==b["sha256"]
    for a in m.get("source_artifacts",{}).values(): assert digest(Path(a["path"]))==a["sha256"]
manifest=delivery/"reforma-acesso-planejamento-entrega-2026-10-11.json"
review=root/"review-acesso-planejamento";config=root/"review-acesso-planejamento-config"
assert not manifest.exists() and not review.exists() and not config.exists()
evidence=[Path(v) for v in vars(args).values()]+[source/"docs/narrative-access-2026-10-11.md",delivery/"reforma-acesso-planejamento-implementacao-2026-10-11.md",delivery/"inde-acesso-planejamento-abrir.sh"]
assert all(p.is_file() for p in evidence)
for target in ("inde-reforma-acesso-planejamento-2026-10-11", "inde-laboratorio-acesso-planejamento-2026-10-11", "inde-fonte-acesso-planejamento-2026-10-11.tar.gz"):
    assert not (delivery/target).exists(), "Preserve earlier candidate: "+target
result={"terminal_warnings":0,"warning_verification_scope":{"enabled_compilation_units":audit["compilation_units"],"runtime_checks":52,"startup_processes":2,"environment":"Local GTK4/X11 Cairo, private Xvfb, isolated config, light/dark themes","historical_repair_logs_retained":True},"verification_summary":{"ctest_passed":31,"core_debug_drivers":3,"core_asan_drivers":3,"access_release_checks":8,"access_focal_asan_checks":8,"builder_alternatives_temporal_regressions":24,"editor_laboratory_regressions":8,"color_focal_asan_checks":4,"startup_only_processes":2},"stage":"narrative_access_and_interaction","schema":27,"technical_verification_passed":True,"human_acceptance":"pending","previous_human_acceptance":"not_inferred_from_continue","continuation_authorized_by":"User: correct clean-project onboarding, narrative menus/layout and terminal warnings","published":False,"whole_reform_complete":False,"all_accepted_proposals_complete":False,"source":str(source),"branch":git("branch","--show-current"),"base_commit":git("rev-parse","HEAD"),"source_uncommitted":True,"source_status":git("status","--porcelain"),"binaries":{},"source_artifacts":{},"review_copies":[],"previous_frozen_deliveries_preserved":[str(p) for p in previous],"limits":["No schema change; existing empty default narrative/editorial structures preserved without example text or chapters","Eight comparison targets at once; paired results, not a ranking, merge or synchronization of the four orders","Fresh clean Release build and focused runtime logs have no warnings; this is not a guarantee for every untested desktop environment","Debug/ASan core focal: three drivers and ten production units; access UI focal: driver and fifteen units; color UI focal: driver and four units; remaining dependencies Release; LSan disabled","Automated real-mouse Xvfb tests and selected visual inspection are not human acceptance, HiDPI/IME or screen-reader certification","Remaining narrative-builder fronts and last entity-recognition milestone are unchanged; map/timezone expansion remains deferred"]}
for kind,name,target in (("inde","inde","inde-reforma-acesso-planejamento-2026-10-11"),("laboratory","inde_text_blocks_lab","inde-laboratorio-acesso-planejamento-2026-10-11")):
    p=delivery/target;assert not p.exists();shutil.copy2(root/"build"/name,p);p.chmod(0o750);result["binaries"][kind]={"path":str(p),"sha256":digest(p)}
names=sorted(set(n for n in subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0") if n))
archive=delivery/"inde-fonte-acesso-planejamento-2026-10-11.tar.gz";assert not archive.exists()
with tarfile.open(archive,"w:gz") as tar:
    for name in names:
        p=source/name;assert p.is_file() and not p.is_symlink();tar.add(p,arcname="inde/"+name,recursive=False)
result["source_artifacts"]["complete_source"]={"path":str(archive),"sha256":digest(archive),"files":len(names),"includes_nonignored_untracked":True}
def rows(p):
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)] and not db.execute("PRAGMA foreign_key_check").fetchall()
        r={}
        for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            q='"'+t.replace('"','""')+'"';cols='*'
            if t=="projects":cols=','.join('NULL' if c[1]=="updated_at" else '"'+c[1]+'"' for c in db.execute('PRAGMA table_info('+q+')'))
            r[t]=Counter(repr(row) for row in db.execute("SELECT "+cols+" FROM "+q))
        return r
review.mkdir();config.mkdir();(config/"inde").mkdir();(config/"inde/interface.ini").write_text("[appearance]\ntheme=light\n")
env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory")
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    p=review/name
    if key=="atlas":
        origin=root/"review-alternativas-narrativas"/name;before=rows(origin);item=snapshot(origin,p)
        (p/"EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
        subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(p)],env=env,check=True)
        after=rows(p);assert rows(origin)==before
        for t,values in before.items():
            if t in ("change_log","sqlite_sequence") or t.startswith("cartographic_position_index_"):continue
            assert not values-after[t],"Existing rows changed: "+t
        item["origin_unchanged"]=True
    else:
        subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(p)],env=env,check=True);assert not rows(p)["documents"] and not rows(p)["works"]
        item={"path":str(p),"origin":"newly_created_pristine","mutable_review":True}
    (p/"NARRATIVE_ACCESS_REVIEW_COPY").write_text("INDE_NARRATIVE_ACCESS_REVIEW_COPY_2026_10_11\n");item.update(key=key,schema=27);result["review_copies"].append(item)
result["verification_artifacts"]=[{"path":str(p.resolve()),"sha256":digest(p)} for p in evidence]
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n");print("SEALED",manifest)
