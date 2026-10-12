"""Freeze continuous contextual-time phase candidate after proportional evidence; never overwrite."""
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
for key in ("ui", "ui-asan", "visual"):
    parser.add_argument("--" + key, required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
base = "61525492724b843de3406bffca27884dd83ba824"
prefix = "fluxo-temporal-2026-10-10"
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
assert not git("status", "--porcelain") and git("rev-parse", "HEAD^") == base
binary = digest(root / "build/inde")
for path, focal in ((args.ui, False), (args.ui_asan, True)):
    report = json.loads(path.read_text())
    assert report["complete"] and report["origins_unchanged"]
    assert report["scope"] == "temporal-workflow-ui" and report["focal_sanitizers"] == focal
    assert report["binary_sha256"] == binary and len(report["checks"]) == 8
    assert all(v["passed"] for v in report["checks"].values())
    assert report["driver_sha256"] == digest(root / "build" / ("inde_ui_temporal_workflow_smoke" + ("_asan" if focal else "")))
visual = json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"] == binary and len(visual["screenshots"]) >= 4
for frame in visual["screenshots"]:
    assert digest(Path(frame["path"])) == frame["sha256"]
ctest = root / "logs/ctest-temporal-workflow-final-2026-10-10.log"
core = root / "logs/core-temporal-workflow-asan-2026-10-10.log"
assert "100% tests passed" in ctest.read_text() and "out of 27" in ctest.read_text()
assert "PASS focal temporal_workflow ASan/UBSan" in core.read_text()
assert not any(v in core.read_text() for v in ("AddressSanitizer", "runtime error:", "FAIL"))
previous = sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous) == 13
for path in previous:
    for artifact in json.loads(path.read_text())["binaries"].values():
        assert digest(Path(artifact["path"])) == artifact["sha256"]
manifest = delivery / ("reforma-" + prefix.replace("-2026-10-10", "") + "-entrega-2026-10-10.json")
assert not manifest.exists()
for flag in ("context", "deletion"):
    path = root / ("logs/temporal-workflow-regression-" + flag + "-2026-10-10.json")
    regression = json.loads(path.read_text())
    assert regression["complete"] and regression["origins_unchanged"] and len(regression["checks"]) == 8
    assert all(v["passed"] for v in regression["checks"].values())
    assert regression["binary_sha256"] == binary

result = {
    "stage": "temporal_workflow_continuous_phase", "technical_verification_passed": True,
    "human_acceptance": "pending", "published": False,
    "whole_reform_complete": False, "announced_five_fronts_implemented": True,
    "entity_recognition_started": False, "entity_recognition_order": "last",
    "source": str(source), "branch": git("branch", "--show-current"),
    "commit": git("rev-parse", "HEAD"), "predecessor_commit": base,
    "predecessor_human_acceptance": "confirmed_in_current_chat", "schema": 25,
    "binaries": {}, "review_copies": [], "source_artifacts": {},
    "previous_frozen_deliveries_preserved": [str(p) for p in previous],
    "limits": [
        "Text undo removes the binding, retains the reusable reference; project-history reversal is separate",
        "Partial dates and unknown time are not completed silently; roles/meanings are declared by the author",
        "Monthly regime filters refer to the selected presentation, not existence of story facts",
        "Unaligned/ambiguous/approximate/season references are not assigned to days",
        "Monthly source limits: 500 points/events, partial counts explicit; at most 500 references per axis",
        "Navigation is a disposable local preference, not a selected date or authorial database state",
        "Compound project-history capture retains its existing 8 MiB/20000 rows conflict guards",
        "Core ASan/UBSan focal: 21 core units and test; dependencies Release, LSan disabled",
        "UI ASan/UBSan focal: seven UI units and driver; dependencies Release, LSan disabled",
        "Selected visual inspection and GTK fixtures are not human acceptance, IME/HiDPI/screen-reader certification",
        "Empty-project GTK fixture preserves prior QA records; core test starts in a freshly created project",
        "No map reform, time zones, recurrence or entity recognition in this phase"
    ]
}
for kind, name in (("inde", "inde"), ("laboratory", "inde_text_blocks_lab")):
    target = delivery / ("inde-" + ("reforma" if kind == "inde" else "laboratorio") + "-" + prefix)
    assert not target.exists()
    shutil.copy2(root / "build" / name, target)
    target.chmod(0o750)
    result["binaries"][kind] = {"path": str(target), "sha256": digest(target)}
config = root / "review-fluxo-temporal-config"
assert not config.exists()
shutil.copytree(root / "review-exclusoes-tempo-mapa-config", config)
for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
                  ("empty", "Projeto vazio — sem exemplos.inde")):
    origin = root / "review-exclusoes-tempo-mapa" / name
    before = rows(origin)
    target = root / "review-fluxo-temporal" / name
    item = snapshot(origin, target)
    assert rows(origin) == before and rows(target) == before
    (target / "TEMPORAL_WORKFLOW_REVIEW_COPY").write_text("INDE_TEMPORAL_WORKFLOW_REVIEW_COPY_2026_10_10\n")
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
evidence = (args.ui, args.ui_asan, args.visual, ctest, core,
            root / "logs/temporal-workflow-regression-context-2026-10-10.json",
            root / "logs/temporal-workflow-regression-deletion-2026-10-10.json",
            root / "logs/build-temporal-workflow-sealed-2026-10-10.log",
            source / "docs/temporal-workflow-phase-2026-10-10.md",
            delivery / "reforma-fluxo-temporal-relatorio-2026-10-10.md",
            delivery / "inde-fluxo-temporal-abrir.sh")
result["verification_artifacts"] = [{"path": str(p.resolve()), "sha256": digest(p)}
                                  for p in evidence]
manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("SEALED", manifest)
