"""Freeze calendar/map deletion candidate after proportional evidence; never overwrite."""
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
base = "4eab5cf9cb6b8bf27d130affe27bb7d0e8945deb"
prefix = "exclusoes-tempo-mapa-2026-10-10"
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
    assert report["scope"] == "calendar-map-deletion-ui" and report["focal_sanitizers"] == focal
    assert report["binary_sha256"] == binary and len(report["checks"]) == 8
    assert all(v["passed"] for v in report["checks"].values())
    assert report["driver_sha256"] == digest(root / "build" / ("inde_ui_deletion_smoke" + ("_asan" if focal else "")))
visual = json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"] == binary and len(visual["screenshots"]) >= 4
for frame in visual["screenshots"]:
    assert digest(Path(frame["path"])) == frame["sha256"]
ctest = root / "logs/ctest-removal-final-2026-10-10.log"
core = root / "logs/core-removal-asan-2026-10-10.log"
assert "100% tests passed" in ctest.read_text() and "out of 26" in ctest.read_text()
assert "PASS calendar-map removal:" in core.read_text()
assert not any(v in core.read_text() for v in ("AddressSanitizer", "runtime error:", "FAIL"))
previous = sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous) == 12
for path in previous:
    for artifact in json.loads(path.read_text())["binaries"].values():
        assert digest(Path(artifact["path"])) == artifact["sha256"]
manifest = delivery / ("reforma-" + prefix.replace("-2026-10-10", "") + "-entrega-2026-10-10.json")
assert not manifest.exists()
result = {
    "stage": "calendar_map_deletion", "technical_verification_passed": True,
    "human_acceptance": "pending", "published": False,
    "whole_reform_complete": False, "whole_review_implemented": False,
    "entity_recognition_started": False, "entity_recognition_order": "last",
    "source": str(source), "branch": git("branch", "--show-current"),
    "commit": git("rev-parse", "HEAD"), "predecessor_commit": base,
    "predecessor_human_acceptance": "pending", "schema": 25,
    "binaries": {}, "review_copies": [], "source_artifacts": {},
    "previous_frozen_deliveries_preserved": [str(p) for p in previous],
    "limits": [
        "Deletes one temporal version; axes, adoptions, dates and derived revisions block removal",
        "Map removal includes terrain/positions/locks, preserves Local entities and external exports",
        "Preview and project history use existing limits: 8 MiB/20000 rows; no blind forced deletion",
        "Reversal is project history, not text undo, and can be blocked by later conflicts",
        "Core ASan/UBSan focal: operation store and test; dependencies Release, LSan disabled",
        "UI ASan/UBSan focal: five UI units and driver; other dependencies Release, LSan disabled",
        "Selected visual inspection and GTK fixtures are not human acceptance or full accessibility certification"
    ]
}
for kind, name in (("inde", "inde"), ("laboratory", "inde_text_blocks_lab")):
    target = delivery / ("inde-" + ("reforma" if kind == "inde" else "laboratorio") + "-" + prefix)
    assert not target.exists()
    shutil.copy2(root / "build" / name, target)
    target.chmod(0o750)
    result["binaries"][kind] = {"path": str(target), "sha256": digest(target)}
config = root / "review-exclusoes-tempo-mapa-config"
assert not config.exists()
shutil.copytree(root / "review-calendario-grafico-config", config)
for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
                  ("empty", "Projeto vazio — sem exemplos.inde")):
    origin = root / "review-calendario-grafico" / name
    before = rows(origin)
    target = root / "review-exclusoes-tempo-mapa" / name
    item = snapshot(origin, target)
    assert rows(origin) == before and rows(target) == before
    (target / "CALENDAR_MAP_REMOVAL_REVIEW_COPY").write_text("INDE_CALENDAR_MAP_REMOVAL_REVIEW_COPY_2026_10_10\n")
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
result["verification_artifacts"] = [{"path": str(p.resolve()), "sha256": digest(p)}
                                  for p in (args.ui, args.ui_asan, args.visual, ctest, core)]
manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("SEALED", manifest)

