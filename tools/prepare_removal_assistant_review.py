"""Freeze the deletion-guide corrective candidate; preserve every earlier delivery."""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot

parser = argparse.ArgumentParser(description=__doc__)
for key in ("ui", "ui-asan", "visual", "regression-deletion", "regression-workflow"):
    parser.add_argument("--" + key, required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
base = "9dadf11dcb0a64e35e91719ea1547f93d3e4c523"
prefix = "auxiliar-exclusao-2026-10-10"

def digest(p):
    return sha256(p.read_bytes()).hexdigest()

def git(*command):
    return subprocess.check_output(["git", *command], cwd=source, text=True).strip()

def rows(p):
    with sqlite3.connect((p / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()[0] == 25
        return {t: Counter(repr(r) for r in db.execute('SELECT * FROM "' + t.replace('"', '""') + '"'))
                for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}

assert not git("status", "--porcelain")
subprocess.run(["git", "merge-base", "--is-ancestor", base, "HEAD"], cwd=source, check=True)
binary = digest(root / "build/inde")
for path, focal in ((args.ui, False), (args.ui_asan, True)):
    report = json.loads(path.read_text())
    assert report["complete"] and report["origins_unchanged"]
    assert report["scope"] == "removal-assistant-ui" and report["focal_sanitizers"] == focal
    assert report["binary_sha256"] == binary and len(report["checks"]) == 8
    assert all(v["passed"] for v in report["checks"].values())
    assert report["driver_sha256"] == digest(root / "build" / ("inde_ui_removal_assistant_smoke" + ("_asan" if focal else "")))
for path, scope in ((args.regression_deletion, "calendar-map-deletion-ui"),
                    (args.regression_workflow, "temporal-workflow-ui")):
    report = json.loads(path.read_text())
    assert report["scope"] == scope and report["complete"] and report["origins_unchanged"]
    assert report["binary_sha256"] == binary and len(report["checks"]) == 8
    assert all(v["passed"] for v in report["checks"].values())
visual = json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"] == binary and len(visual["screenshots"]) >= 4
for frame in visual["screenshots"]:
    assert digest(Path(frame["path"])) == frame["sha256"]
ctest = root / "logs/ctest-removal-assistant-final-2026-10-10.log"
debug = root / "logs/core-removal-assistant-debug-final2-2026-10-10.log"
core = root / "logs/core-removal-assistant-asan-final-2026-10-10.log"
assert "100% tests passed" in ctest.read_text() and "out of 28" in ctest.read_text()
assert "PASS focal removal assistant Debug" in debug.read_text()
assert "PASS focal removal assistant ASan/UBSan" in core.read_text()
assert not any(v in core.read_text() for v in ("AddressSanitizer", "runtime error:", "FAIL"))
previous = sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous) == 14
for path in previous:
    for artifact in json.loads(path.read_text())["binaries"].values():
        assert digest(Path(artifact["path"])) == artifact["sha256"]
manifest = delivery / "reforma-auxiliar-exclusao-entrega-2026-10-10.json"
assert not manifest.exists()
result = {
    "stage": "removal_assistant_corrective_phase", "technical_verification_passed": True,
    "human_acceptance": "pending", "published": False, "whole_reform_complete": False,
    "entity_recognition_started": False, "entity_recognition_order": "last",
    "source": str(source), "branch": git("branch", "--show-current"),
    "commit": git("rev-parse", "HEAD"), "predecessor_commit": base,
    "parent_commit": git("rev-parse", "HEAD^"),
    "increment_commits": git("rev-list", "--reverse", base + "..HEAD").splitlines(), "schema": 25,
    "removal_categories": 24, "binaries": {}, "review_copies": [], "source_artifacts": {},
    "previous_frozen_deliveries_preserved": [str(p) for p in previous],
    "limits": [
        "No automatic unlinking, date conversion, cascade cleanup or trash purge",
        "Axes detach only after explicit LIFO review of adoption history; authorial dates remain",
        "Semantic marks and anchors stay in writing undo, not generic SQL deletion",
        "Legacy structural trash without a corresponding journal operation has explanation, no safe individual restore shortcut",
        "Usage inspection fails closed above 20000 metadata items; future modules need owner/navigation tests",
        "Core Debug and ASan/UBSan focal: test plus three current core units; dependencies Release",
        "UI ASan/UBSan focal: driver plus seven UI units; dependencies Release; LSan disabled",
        "GTK fixture called empty retains prior QA records; core test starts in a newly created project",
        "Selected visual inspection is not human approval or HiDPI/IME/screen-reader certification",
        "No map reform, time zones, entity recognition or public release in this increment"
    ]
}
for kind, name in (("inde", "inde"), ("laboratory", "inde_text_blocks_lab")):
    target = delivery / ("inde-" + ("reforma" if kind == "inde" else "laboratorio") + "-" + prefix)
    assert not target.exists()
    shutil.copy2(root / "build" / name, target)
    target.chmod(0o750)
    result["binaries"][kind] = {"path": str(target), "sha256": digest(target)}
config = root / "review-auxiliar-exclusao-config"
assert not config.exists()
shutil.copytree(root / "review-fluxo-temporal-config", config)
for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
                  ("empty", "Projeto vazio — sem exemplos.inde")):
    origin = root / "review-fluxo-temporal" / name
    before = rows(origin)
    target = root / "review-auxiliar-exclusao" / name
    item = snapshot(origin, target)
    assert rows(origin) == before and rows(target) == before
    (target / "REMOVAL_ASSISTANT_REVIEW_COPY").write_text("INDE_REMOVAL_ASSISTANT_REVIEW_COPY_2026_10_10\n")
    item.update(key=key, schema=25, authorial_rows_preserved=True)
    result["review_copies"].append(item)
bundle = delivery / ("inde-reforma-" + prefix + ".bundle")
patch = delivery / ("inde-reforma-" + prefix + ".patch")
assert not bundle.exists() and not patch.exists()
subprocess.run(["git", "bundle", "create", str(bundle), "HEAD"], cwd=source, check=True)
subprocess.run(["git", "bundle", "verify", str(bundle)], cwd=source, check=True)
patch.write_bytes(subprocess.check_output(["git", "diff", "--binary", base, "HEAD"], cwd=source))
for p in (bundle, patch):
    result["source_artifacts"][p.suffix[1:]] = {"path": str(p), "sha256": digest(p)}
evidence = (args.ui, args.ui_asan, args.visual, args.regression_deletion, args.regression_workflow,
            ctest, core, debug, root / "logs/build-removal-assistant-final-sealed-2026-10-10.log",
            source / "docs/removal-assistant-2026-10-10.md",
            delivery / "reforma-auxiliar-exclusao-relatorio-2026-10-10.md",
            delivery / "inde-auxiliar-exclusao-abrir.sh")
result["verification_artifacts"] = [{"path": str(p.resolve()), "sha256": digest(p)} for p in evidence]
manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("SEALED", manifest)
