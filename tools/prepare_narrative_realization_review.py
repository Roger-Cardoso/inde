"""Seal a verified private realization/continuity candidate, never a release.

Uses fresh SQLite snapshots of the predecessor's mutable review projects.
Originals, earlier frozen binaries and their manifests must remain intact.
"""
from pathlib import Path
from hashlib import sha256
import argparse
import json
import os
import re
import shutil
import sqlite3
import subprocess
import tarfile
from reform_snapshots import snapshot

p=argparse.ArgumentParser(description=__doc__)
for name in ("ui","ui-asan","ctest","debug","asan","startup","audit","visual","desktop-before","runner-completion"):
    p.add_argument("--"+name,required=True,type=Path)
p.add_argument("--regression",action="append",required=True,type=Path)
a=p.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent;out=root.parents[1]/"outputs"
digest=lambda path:sha256(path.read_bytes()).hexdigest()
git=lambda *args:subprocess.check_output(["git",*args],cwd=source,text=True).strip()
assert git("rev-parse","HEAD")=="995fbb27f8ee9359643a90f0220bc71269ef5e9a"
subprocess.run(["git","diff","--check"],cwd=source,check=True)
assert "no work to do" in subprocess.check_output(["ninja","-n"],cwd=root/"build",text=True)
main_sha=digest(root/"build/inde");lab_sha=digest(root/"build/inde_text_blocks_lab")
runner=json.loads(a.runner_completion.read_text())
assert runner["scope"]=="narrative-realization-runner-completion" and runner["binary_sha256"]==main_sha
assert {v["scope"] for v in runner["selected_commands"]}=={"core","ctest","ui","ui-asan-atlas","ui-asan-empty","builder-alternatives","access-atlas","access-empty","workflow","startup"}
assert all(v["exit_code"]==0 for v in [*runner["selected_commands"],*runner["release_builds"]])
assert "100% tests passed out of 33" in a.ctest.read_text()
for path,sanitized in ((a.debug,False),(a.asan,True)):
    text=path.read_text()
    assert all(token in text for token in ("PASS realization:","PASS alternatives:","PASS builder:","PASS: pristine onboarding","PASS focal builder"))
    assert ("ASan/UBSan" in text)==sanitized
    assert not re.search(r"warning:|error:|runtime error:|ERROR: AddressSanitizer|Assertion|SIGABRT",text)
for path,sanitized in ((a.ui,False),(a.ui_asan,True)):
    report=json.loads(path.read_text())
    assert report["complete"] and report["origins_unchanged"] and report["focal_sanitizers"]==sanitized
    assert report["scope"]=="narrative-realization-production-ui" and report["binary_sha256"]==main_sha
    assert digest(root/"build"/("inde_ui_narrative_realization_smoke"+("_asan" if sanitized else "")))==report["driver_sha256"]
    assert len(report["checks"])==8 and all(v["passed"] for v in report["checks"].values())
    for value in report["checks"].values():
        assert not re.search(r"FAIL|WARNING|CRITICAL|runtime error:|ERROR: AddressSanitizer",Path(value["log"]).read_text())
regression_checks=0
expected_regressions={
    "narrative-builder-production-ui":"inde_ui_narrative_builder_smoke",
    "narrative-alternatives-production-ui":"inde_ui_narrative_alternatives_smoke",
    "narrative-access-production-ui":"inde_ui_narrative_access_smoke",
    "temporal-workflow-ui":"inde_ui_temporal_workflow_smoke",
}
seen_regressions=set()
for path in a.regression:
    report=json.loads(path.read_text())
    assert report["complete"] and report["origins_unchanged"] and report["binary_sha256"]==main_sha
    scope=report["scope"]
    assert scope in expected_regressions and scope not in seen_regressions
    seen_regressions.add(scope)
    driver=expected_regressions[scope]+("_asan" if report["focal_sanitizers"] else "")
    assert digest(root/"build"/driver)==report["driver_sha256"]
    assert len(report["checks"])==8 and all(v["passed"] for v in report["checks"].values())
    regression_checks+=len(report["checks"])
assert seen_regressions==set(expected_regressions)
audit=json.loads(a.audit.read_text())
assert audit["clean_rebuild_completed"] and audit["all_enabled_units_compiled"] and audit["compiler_warnings"]==0 and audit["binary_sha256"]==main_sha
startup=json.loads(a.startup.read_text())
assert startup["complete"] and len(startup["checks"])==2
for name,sha in (("inde",main_sha),("inde_text_blocks_lab",lab_sha)):
    assert startup["checks"][name]["passed"] and startup["checks"][name]["binary_sha256"]==sha
visual=json.loads(a.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"]==main_sha and len(visual["screenshots"])>=4
for item in visual["screenshots"]:assert digest(Path(item["path"]))==item["sha256"]
desktop=json.loads(a.desktop_before.read_text())
for name,sha in desktop.items():
    path=Path(name)
    assert (digest(path) if path.is_file() else None)==sha,"Desktop configuration changed: "+name
previous=sorted(out.glob("reforma*-entrega-*.json"))
for path in previous:
    m=json.loads(path.read_text())
    for group in ("binaries","source_artifacts"):
        for item in m.get(group,{}).values():assert digest(Path(item["path"]))==item["sha256"]
manifest=out/"reforma-realizacao-continuidade-entrega-2026-10-11.json"
archive=out/"inde-fonte-realizacao-continuidade-2026-10-11.tar.gz"
names={"inde":"inde-reforma-realizacao-continuidade-2026-10-11","laboratory":"inde-laboratorio-realizacao-continuidade-2026-10-11"}
review=root/"review-realizacao-continuidade";config=root/"review-realizacao-continuidade-config"
assert not any(path.exists() for path in (manifest,archive,review,config,*(out/name for name in names.values())))
predecessor=out/"reforma-acesso-planejamento-tema-entrega-2026-10-11.json"
old=json.loads(predecessor.read_text())
origins={key:Path(old["launch_targets"][key]["project"]) for key in ("atlas","vazio")}
copies=[]
for key,origin in origins.items():
    target=review/origin.name
    entry=snapshot(origin,target);entry["key"]=key;copies.append(entry)
    (target/"NARRATIVE_REALIZATION_REVIEW_COPY").write_text("INDE_NARRATIVE_REALIZATION_REVIEW_COPY_2026_10_11\n")
    (target/"EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
    subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(target)],
        env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory"),check=True)
    with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)] and not db.execute("PRAGMA foreign_key_check").fetchall()
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()==(28,)
empty=review/"Laboratório vazio.inde"
subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(empty)],
    env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory"),check=True)
(empty/"TEXT_BLOCKS_LAB_COPY").write_text("INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n")
(empty/"TEXT_WORKSHOP_COPY").write_text("INDE_TEXT_WORKSHOP_COPY_V1\n")
evidence=[a.ui,a.ui_asan,a.ctest,a.debug,a.asan,a.startup,a.audit,a.visual,a.desktop_before,a.runner_completion,*a.regression,
    source/"docs/narrative-realization-continuity-2026-10-11.md",
    out/"reforma-realizacao-continuidade-2026-10-11.md",
    out/"inde-realizacao-continuidade-abrir.sh"]
preservation=out/"reforma-realizacao-preservacao-2026-10-11.json"
assert json.loads(preservation.read_text())["original_source_unchanged"]
evidence.append(preservation)
assert all(path.is_file() for path in evidence)
for name,sha in desktop.items():
    path=Path(name)
    assert (digest(path) if path.is_file() else None)==sha,"Review preparation touched desktop configuration: "+name
result=dict(stage="narrative_realization_continuity",schema=28,technical_verification_passed=True,
    predecessor=str(predecessor),predecessor_human_acceptance="confirmed explicitly in this chat",
    human_acceptance="pending",published=False,whole_reform_complete=False,source=str(source),
    branch=git("branch","--show-current"),base_commit=git("rev-parse","HEAD"),source_uncommitted=True,
    source_status=git("status","--porcelain"),previous_frozen_deliveries_preserved=[str(v) for v in previous],
    desktop_files_unchanged=True,review_copies=copies,binaries={},source_artifacts={},launch_targets={},
    verification=dict(ctest_passed=33,final_compiler_warnings=0,enabled_compilation_units=audit["compilation_units"],
        core_focal_units=14,core_focal_drivers=4,ui_focal_units=20,ui_checks=16,regression_checks=regression_checks,
        startup_only_checks=2,remaining_dependencies="Release",leak_sanitizer=False),
    regression_environment="Legacy GTK drivers run with explicit GTK_THEME=Adwaita; new realization driver and actual executable startup use the production compatibility bridge",
    limits=["No whole-program sanitizer coverage or leak certification",
        "Legacy whole-Document associations have no retroactive textual fingerprint/review; choose a range/occurrence",
        "Editorial precedence requires a unique public sequence, not inferred from associations",
        "Presence overlaps are possibilities; different axes/calendars require explicit equivalence",
        "Independent occurrence starts outside reading; author can explicitly show/replace occurrences",
        "Bounded complete queries refuse excess instead of silently truncating",
        "Screenshots and automated tests are not human acceptance, full accessibility or long-session evidence",
        "Review snapshots capture committed contents only, not live unsaved buffers",
        "No commit, push, publication or automatic next phase"],
    verification_artifacts=[dict(path=str(v.resolve()),sha256=digest(v)) for v in evidence],
    agent_visual_review=visual)
for kind,name in names.items():
    destination=out/name
    shutil.copy2(root/"build"/("inde" if kind=="inde" else "inde_text_blocks_lab"),destination)
    destination.chmod(0o750);result["binaries"][kind]=dict(path=str(destination),sha256=digest(destination))
files=sorted(set(filter(None,subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0"))))
with tarfile.open(archive,"w:gz") as tar:
    for name in files:
        path=source/name;assert path.is_file() and not path.is_symlink()
        tar.add(path,arcname="inde/"+name,recursive=False)
result["source_artifacts"]["complete_source"]=dict(path=str(archive),sha256=digest(archive),files=len(files),includes_nonignored_untracked=True)
for key in ("atlas","vazio"):
    target=next(Path(v["path"]) for v in copies if v["key"]==key)
    result["launch_targets"][key]=dict(project=str(target),binary="inde",marker="NARRATIVE_REALIZATION_REVIEW_COPY",marker_value="INDE_NARRATIVE_REALIZATION_REVIEW_COPY_2026_10_11")
atlas=Path(result["launch_targets"]["atlas"]["project"])
assert (atlas/"TEXT_BLOCKS_LAB_COPY").read_text().strip()=="INDE_TEXT_BLOCKS_SPIKE_COPY_V1"
result["launch_targets"]["laboratorio-atlas"]=dict(result["launch_targets"]["atlas"],binary="laboratory")
result["launch_targets"]["laboratorio-vazio"]=dict(project=str(empty),binary="laboratory",marker="TEXT_BLOCKS_LAB_COPY",marker_value="INDE_TEXT_BLOCKS_SPIKE_COPY_V1")
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("SEALED",manifest)
