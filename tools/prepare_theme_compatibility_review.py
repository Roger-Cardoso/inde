"""Seal the complementary theme correction without overwriting frozen artifacts."""
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

p=argparse.ArgumentParser(description=__doc__)
for name in ("ui", "ui-asan", "ctest", "audit", "focused"):
    p.add_argument("--"+name, required=True, type=Path)
p.add_argument("--visual-frame", action="append", required=True, type=Path,
               help="Agent-inspected frame, not an automatic visual verdict")
a=p.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent;out=root.parents[1]/"outputs"
digest=lambda p:sha256(p.read_bytes()).hexdigest()
git=lambda *args:subprocess.check_output(["git",*args],cwd=source,text=True).strip()
assert git("rev-parse","HEAD")=="995fbb27f8ee9359643a90f0220bc71269ef5e9a"
subprocess.run(["git","diff","--check"],cwd=source,check=True)
main_sha=digest(root/"build/inde");lab_sha=digest(root/"build/inde_text_blocks_lab")
reports=[]
for path,instrumented in ((a.ui,False),(a.ui_asan,True)):
    r=json.loads(path.read_text())
    assert r["complete"] and r["desktop_files_unchanged"] and r["driver_instrumented"]==instrumented
    assert r["binary_sha256"]==main_sha and r["laboratory_sha256"]==lab_sha
    assert r["driver_sha256"]==digest(Path(r["driver"]))
    assert len(r["checks"])==7
    for key,c in r["checks"].items():
        assert c["passed"] and c["warnings"]==0
        assert not re.search(r"WARNING|CRITICAL|runtime error:|ERROR: AddressSanitizer|unhandled exception",Path(c["log"]).read_text())
    assert len(r["negative_controls"])==2
    for key,c in r["negative_controls"].items():
        assert c["passed"] and c["warnings"]>0 and "Theme parser error" in Path(c["log"]).read_text()
    for path,sha in r["theme_inputs"].items():assert digest(Path(path))==sha
    assert "inherited-theme=Orchis-Dark-Compact; base=Adwaita" in Path(r["checks"]["real-desktop"]["log"]).read_text()
    reports.append(r)
assert "100% tests passed out of 32" in a.ctest.read_text()
audit=json.loads(a.audit.read_text())
assert audit["compiler_warnings"]==0 and audit["all_enabled_units_compiled"] and audit["binary_sha256"]==main_sha
assert "no work to do" in subprocess.check_output(["ninja","-n"],cwd=root/"build",text=True)
focused=a.focused.read_text()
assert "PASS policy Debug" in focused and "PASS policy ASan-UBSan" in focused
assert "BUILT focused ASan/UBSan theme driver" in focused
assert not re.search(r"warning:|error:|ERROR: AddressSanitizer|runtime error:",focused)
available=reports[0]["checks"]["inherited-default"]["screenshots"]
assert len(a.visual_frame)==2 and {str(f.resolve()) for f in a.visual_frame}==set(available)
previous=sorted(out.glob("reforma*-entrega-*.json"))
for path in previous:
    m=json.loads(path.read_text())
    for group in ("binaries","source_artifacts"):
        for item in m.get(group,{}).values():assert digest(Path(item["path"]))==item["sha256"]
manifest=out/"reforma-acesso-planejamento-tema-entrega-2026-10-11.json"
names={"inde":"inde-reforma-acesso-planejamento-tema-2026-10-11",
       "laboratory":"inde-laboratorio-acesso-planejamento-tema-2026-10-11"}
archive=out/"inde-fonte-acesso-planejamento-tema-2026-10-11.tar.gz"
empty=root/"review-acesso-planejamento-tema-laboratorio-vazio.inde"
assert not any(path.exists() for path in (manifest,archive,empty,*(out/name for name in names.values())))
old=json.loads((out/"reforma-acesso-planejamento-entrega-2026-10-11.json").read_text())
copies={c["key"]:Path(c["path"]) for c in old["review_copies"]}
for copy in copies.values():
    assert copy.is_dir() and not copy.is_symlink()
    assert (copy/"NARRATIVE_ACCESS_REVIEW_COPY").read_text().strip()=="INDE_NARRATIVE_ACCESS_REVIEW_COPY_2026_10_11"
assert (copies["atlas"]/"TEXT_BLOCKS_LAB_COPY").read_text().strip()=="INDE_TEXT_BLOCKS_SPIKE_COPY_V1"
evidence=[a.ui,a.ui_asan,a.ctest,a.audit,a.focused,
          source/"docs/theme-compatibility-2026-10-11.md",
          out/"reforma-acesso-planejamento-tema-2026-10-11.md",
          out/"inde-acesso-planejamento-tema-abrir.sh",
          out/"reforma-tema-recents-adicionados-2026-10-11.txt",
          Path("/home/roger/.codex/attachments/7c4bbefd-7931-468a-b08a-2e85996f47dd/Texto colado.txt")]
assert all(f.is_file() for f in evidence)
result=dict(stage="theme_compatibility_correction",schema=27,technical_verification_passed=True,
            human_acceptance="pending",published=False,whole_reform_complete=False,
            source=str(source),branch=git("branch","--show-current"),base_commit=git("rev-parse","HEAD"),
            source_uncommitted=True,source_status=git("status","--porcelain"),
            predecessor=str(out/"reforma-acesso-planejamento-entrega-2026-10-11.json"),
            verification=dict(ctest_passed=32,enabled_compilation_units=audit["compilation_units"],
                              final_compiler_warnings=0,normal_positive_checks=7,
                              focal_matrix_positive_checks=7,deliberate_negative_controls=4,
                              focal_production_units=4,focal_test_drivers=1,
                              focal_dependencies="Release",leak_sanitizer=False),
            environment_scope="GTK 4.14.5, inherited Orchis CSS in private Xvfb; real desktop settings probe without visible window",
            previous_frozen_deliveries_preserved=[str(f) for f in previous],binaries={},source_artifacts={},
            launch_targets={},verification_artifacts=[],
            agent_visual_review=dict(reviewed=True,human_acceptance=False,
                                     notes="Native light/dark driver frames: controls, selected text and buttons legible; not full-app/accessibility certification",
                                     frames=[dict(path=str(f.resolve()),sha256=digest(f)) for f in a.visual_frame]),
            limits=["Explicit incompatible GTK_THEME intentionally remains observable, not silenced",
                    "GTK_THEME pre-init bridge must be revalidated when GTK changes; not a new end-user setting",
                    "Three probes in focal matrix instrumented; four actual executable startups in that matrix remain Release",
                    "Six-second startup and no-window desktop probe are not long-session/human UX evidence",
                    "Earlier 52 runtime tests belong to predecessor hash, not this corrected executable",
                    "Primary review copies reused without resetting user-saved contents; separate synthetic empty laboratory copy",
                    "No schema/authorial-order change, commit, push or publication"])
for kind,name in names.items():
    dest=out/name;shutil.copy2(root/"build"/("inde" if kind=="inde" else "inde_text_blocks_lab"),dest)
    dest.chmod(0o750);result["binaries"][kind]=dict(path=str(dest),sha256=digest(dest))
files=sorted(set(filter(None,subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0"))))
with tarfile.open(archive,"w:gz") as tar:
    for name in files:
        f=source/name;assert f.is_file() and not f.is_symlink();tar.add(f,arcname="inde/"+name,recursive=False)
result["source_artifacts"]["complete_source"]=dict(path=str(archive),sha256=digest(archive),files=len(files),includes_nonignored_untracked=True)
subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(empty)],
               env=dict(os.environ,XDG_CONFIG_HOME=str(root/"review-acesso-planejamento-config"),
                        GSETTINGS_BACKEND="memory"),check=True)
(empty/"TEXT_BLOCKS_LAB_COPY").write_text("INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n")
(empty/"TEXT_WORKSHOP_COPY").write_text("INDE_TEXT_WORKSHOP_COPY_V1\n")
with sqlite3.connect((empty/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
    assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)] and not db.execute("PRAGMA foreign_key_check").fetchall()
    assert not db.execute("SELECT COUNT(*) FROM works").fetchone()[0] and not db.execute("SELECT COUNT(*) FROM documents").fetchone()[0]
for key,copy,kind,marker,value in (
    ("vazio",copies["empty"],"inde","NARRATIVE_ACCESS_REVIEW_COPY","INDE_NARRATIVE_ACCESS_REVIEW_COPY_2026_10_11"),
    ("atlas",copies["atlas"],"inde","NARRATIVE_ACCESS_REVIEW_COPY","INDE_NARRATIVE_ACCESS_REVIEW_COPY_2026_10_11"),
    ("laboratorio-atlas",copies["atlas"],"laboratory","NARRATIVE_ACCESS_REVIEW_COPY","INDE_NARRATIVE_ACCESS_REVIEW_COPY_2026_10_11"),
    ("laboratorio-vazio",empty,"laboratory","TEXT_BLOCKS_LAB_COPY","INDE_TEXT_BLOCKS_SPIKE_COPY_V1")):
    result["launch_targets"][key]=dict(project=str(copy),binary=kind,marker=marker,marker_value=value)
result["verification_artifacts"]=[dict(path=str(f.resolve()),sha256=digest(f)) for f in evidence]
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("SEALED",manifest)
