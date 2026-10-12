"""Freeze the verified vocabulary candidate into new artifacts; never overwrite.

Requires current build and exact-hash evidence. Human acceptance stays pending.
"""
import argparse
from hashlib import sha256
import json
import os
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import tarfile
from reform_snapshots import snapshot
parser=argparse.ArgumentParser(description=__doc__)
for name in ("ui","ui-asan","ctest","debug","asan","build","startup","visual","preservation","migration-tests"):
    parser.add_argument("--"+name,required=True,type=Path)
parser.add_argument("--regression",action="append",required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent;out=root.parents[1]/"outputs"
digest=lambda p:sha256(p.read_bytes()).hexdigest()
git=lambda *v:subprocess.check_output(["git",*v],cwd=source,text=True).strip()
assert git("rev-parse","HEAD")=="995fbb27f8ee9359643a90f0220bc71269ef5e9a"
subprocess.run(["git","diff","--check"],cwd=source,check=True)
assert "no work to do" in subprocess.check_output(["ninja","-n"],cwd=root/"build",text=True)
main_sha=digest(root/"build/inde");lab_sha=digest(root/"build/inde_text_blocks_lab")
assert "100% tests passed out of 34" in args.ctest.read_text()
assert "Ran 3 tests" in args.migration_tests.read_text() and "\nOK\n" in args.migration_tests.read_text()
assert not re.search(r"warning:|error:|FAILED:|killed",args.build.read_text(),re.I)
for path,phrase in ((args.debug,"PASS vocabulary focal Debug"),(args.asan,"PASS vocabulary focal ASan/UBSan")):
    text=path.read_text();assert phrase in text
    assert not re.search(r"warning:|error:|Assertion|AddressSanitizer:|runtime error:",text)
reports=[]
for path,sanitized in ((args.ui,False),(args.ui_asan,True)):
    r=json.loads(path.read_text());assert r["complete"] and r["binary_sha256"]==main_sha and r["focal_sanitizers"]==sanitized
    assert len(r["checks"])==4 and all(v["passed"] and v["original_unchanged"] for v in r["checks"].values())
    assert r["driver_sha256"]==digest(root/"build"/("inde_ui_author_vocabulary_smoke"+("_asan" if sanitized else "")))
    reports.append(r)
regressions=[]
for p in args.regression:
    r=json.loads(p.read_text());assert r["complete"] and r["binary_sha256"]==main_sha
    if r["scope"]=="narrative-realization-production-ui":
        assert len(r["checks"])==8 and r["origins_unchanged"]
        assert r["driver_sha256"]==digest(root/"build/inde_ui_narrative_realization_smoke")
    elif r["scope"]=="temporal-workflow-ui":
        assert len(r["checks"])==8 and r["origins_unchanged"]
        assert r["driver_sha256"]==digest(root/"build/inde_ui_temporal_workflow_smoke")
    elif r["scope"]=="editor-selection-dictionary-inline":
        assert len(r["checks"])==4 and all(v["original_unchanged"] for v in r["checks"].values())
        assert r["driver_sha256"]==digest(root/"build/inde_ui_editor_fixes_smoke")
    else:
        raise AssertionError("Unexpected regression scope: "+r["scope"])
    assert all(v["passed"] for v in r["checks"].values());regressions.append(r)
startup=json.loads(args.startup.read_text());assert startup["complete"] and len(startup["checks"])==2
assert startup["checks"]["inde"]["binary_sha256"]==main_sha and startup["checks"]["inde_text_blocks_lab"]["binary_sha256"]==lab_sha
visual=json.loads(args.visual.read_text());assert visual["reviewed_by_agent"] and visual["binary_sha256"]==main_sha
assert len(visual["screenshots"])>=4
for v in visual["screenshots"]:assert digest(Path(v["path"]))==v["sha256"]
preservation=json.loads(args.preservation.read_text());assert preservation["core_and_persistence_unchanged"] and not preservation["deleted_files"]
previous=out/"reforma-realizacao-continuidade-entrega-2026-10-11.json"
old=json.loads(previous.read_text())
for group in ("binaries","source_artifacts"):
    for item in old[group].values():assert digest(Path(item["path"]))==item["sha256"]
review=root/"review-vocabulario-autoral";config=root/"review-vocabulario-autoral-config"
manifest=out/"reforma-vocabulario-autoral-entrega-2026-10-12.json"
archive=out/"inde-fonte-vocabulario-autoral-2026-10-12.tar.gz"
names=dict(inde="inde-reforma-vocabulario-autoral-2026-10-12",laboratory="inde-laboratorio-vocabulario-autoral-2026-10-12")
assert not any(p.exists() for p in (review,config,manifest,archive,*(out/v for v in names.values())))
review.mkdir();copies=[]
atlas=review/"Atlas de Véspera — Projeto Integral.inde"
copies.append(snapshot(Path(old["launch_targets"]["atlas"]["project"]),atlas))
empty=review/"Projeto vazio — sem exemplos.inde"
subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(empty)],env=dict(os.environ,XDG_CONFIG_HOME=str(config)),check=True)
lab_empty=review/"Laboratório vazio.inde"
subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(lab_empty)],env=dict(os.environ,XDG_CONFIG_HOME=str(config)),check=True)
for p in (atlas,empty,lab_empty):
    (p/"AUTHOR_VOCABULARY_REVIEW_COPY").write_text("INDE_AUTHOR_VOCABULARY_REVIEW_COPY_2026_10_12\n")
    (p/"TEXT_BLOCKS_LAB_COPY").write_text("INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n")
    (p/"TEXT_WORKSHOP_COPY").write_text("INDE_TEXT_WORKSHOP_COPY_V1\n")
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)] and not db.execute("PRAGMA foreign_key_check").fetchall()
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()==(28,)
        if p!=atlas:
            for table in ("documents","works","intellectual_properties"):
                assert db.execute("SELECT count(*) FROM "+table).fetchone()==(0,)
    if p!=atlas:copies.append(dict(path=str(p),origin="new synthetic pristine fixture",mutable_review=True))
result=dict(stage="author_vocabulary",schema=28,technical_verification_passed=True,human_acceptance="pending",
    proposals_human_approval="confirmed explicitly in this chat",previous_block_human_acceptance="confirmed explicitly in this chat",
    published=False,whole_reform_complete=False,source=str(source),branch=git("branch","--show-current"),
    base_commit=git("rev-parse","HEAD"),source_uncommitted=True,source_status=git("status","--porcelain"),
    predecessor=str(previous),previous_frozen_deliveries_preserved=True,review_copies=copies,binaries={},source_artifacts={},launch_targets={},
    verification=dict(ctest_passed=34,migration_guard_tests=3,compiler_warnings_final_build=0,
        core_focal_units=3,ui_focal_units=7,ui_checks=sum(len(r["checks"]) for r in reports),
        regression_checks=sum(len(r["checks"]) for r in regressions),startup_only_checks=2,leak_sanitizer=False),
    limits=["Debug and sanitizers focal; remaining dependencies Release; not whole-program instrumentation",
        "Automated GTK probes and image inspection are not human writer, screen-reader, IBus, HiDPI or long-session acceptance",
        "Unknown diagnostics and historical operation records remain verbatim; author names never globally replaced",
        "No new map/timezone/template world-map implementation or automatic entity identification",
        "Review snapshots capture committed contents, not unsaved live text buffers; workspace state is not a backup",
        "No commit, push, publication or automatic next phase"],
    verification_artifacts=[],agent_visual_review=visual)
evidence=[args.ui,args.ui_asan,args.ctest,args.debug,args.asan,args.build,args.startup,args.visual,args.preservation,args.migration_tests,*args.regression,
    source/"docs/author-vocabulary-2026-10-12.md",out/"reforma-vocabulario-autoral-2026-10-12.md",out/"inde-vocabulario-autoral-abrir.sh"]
for path in evidence:assert path.is_file();result["verification_artifacts"].append(dict(path=str(path.resolve()),sha256=digest(path)))
for kind,name in names.items():
    destination=out/name;shutil.copy2(root/"build"/("inde" if kind=="inde" else "inde_text_blocks_lab"),destination);destination.chmod(0o750)
    result["binaries"][kind]=dict(path=str(destination),sha256=digest(destination))
files=sorted(set(filter(None,subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0"))))
with tarfile.open(archive,"w:gz") as tar:
    for name in files:
        p=source/name;assert p.is_file() and not p.is_symlink();tar.add(p,arcname="inde/"+name,recursive=False)
result["source_artifacts"]["complete_source"]=dict(path=str(archive),sha256=digest(archive),files=len(files),includes_nonignored_untracked=True)
for choice,binary,project in (("atlas","inde",atlas),("vazio","inde",empty),("laboratorio-atlas","laboratory",atlas),("laboratorio-vazio","laboratory",lab_empty)):
    result["launch_targets"][choice]=dict(binary=binary,project=str(project),marker="AUTHOR_VOCABULARY_REVIEW_COPY",marker_value="INDE_AUTHOR_VOCABULARY_REVIEW_COPY_2026_10_12")
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("SEALED private vocabulary candidate",manifest,"; human acceptance pending")
