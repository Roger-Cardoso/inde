"""Freeze a private, uncommitted builder candidate and independent review copies.

Requires evidence from these exact binaries. Preserves earlier deliveries.
Source archive includes tracked and nonignored untracked files, not just git diff.
"""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import tarfile
from reform_snapshots import snapshot
parser = argparse.ArgumentParser(description=__doc__)
for key in ("ui", "ui-asan", "ctest", "debug", "asan", "visual", "regression", "startup"):
    parser.add_argument("--" + key, required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
prefix = "construtor-narrativo-2026-10-11"
base = "995fbb27f8ee9359643a90f0220bc71269ef5e9a"
def digest(path): return sha256(path.read_bytes()).hexdigest()
def git(*cmd): return subprocess.check_output(["git", *cmd], cwd=source, text=True).strip()
def rows(path):
    with sqlite3.connect((path / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        result = {}
        for (table,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            quoted = '"' + table.replace('"', '""') + '"'
            columns = '*'
            if table == "projects":
                columns = ','.join('NULL' if c[1] == "updated_at" else '"' + c[1] + '"' for c in db.execute('PRAGMA table_info(' + quoted + ')'))
            result[table] = Counter(repr(r) for r in db.execute('SELECT ' + columns + ' FROM ' + quoted))
        return result
assert git("rev-parse", "HEAD") == base
subprocess.run(["git", "diff", "--check"], cwd=source, check=True)
binary = digest(root / "build/inde")
for path, focal in ((args.ui, False), (args.ui_asan, True)):
    r = json.loads(path.read_text())
    assert r["complete"] and r["origins_unchanged"] and r["scope"] == "narrative-builder-production-ui"
    assert len(r["checks"]) == 8 and all(c["passed"] for c in r["checks"].values())
    assert r["binary_sha256"] == binary and r["focal_sanitizers"] == focal
    assert r["driver_sha256"] == digest(root / "build" / ("inde_ui_narrative_builder_smoke" + ("_asan" if focal else "")))
r = json.loads(args.regression.read_text())
assert r["complete"] and r["origins_unchanged"] and r["scope"] == "temporal-workflow-ui"
assert r["binary_sha256"] == binary and len(r["checks"]) == 8 and all(c["passed"] for c in r["checks"].values())
assert "100% tests passed" in args.ctest.read_text() and "out of 29" in args.ctest.read_text()
startup = json.loads(args.startup.read_text())
assert startup["complete"] and startup["scope"] == "startup-only"
for name in ("inde", "inde_text_blocks_lab"):
    assert startup["checks"][name]["passed"]
    assert startup["checks"][name]["binary_sha256"] == digest(root / "build" / name)
assert "PASS focal builder Debug" in args.debug.read_text()
assert "PASS focal builder ASan/UBSan" in args.asan.read_text()
assert not any(x in args.asan.read_text() for x in ("AddressSanitizer", "runtime error:", "FAIL"))
visual = json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"] == binary
for frame in visual["screenshots"]: assert digest(Path(frame["path"])) == frame["sha256"]
previous = sorted(delivery.glob("reforma*-entrega-*.json"))
for path in previous:
    for item in json.loads(path.read_text())["binaries"].values(): assert digest(Path(item["path"])) == item["sha256"]
preservation = delivery / "reforma-construtor-preservacao-2026-10-11.json"
assert json.loads(preservation.read_text())["original_source_unchanged"]
manifest = delivery / "reforma-construtor-narrativo-entrega-2026-10-11.json"
review = root / "review-construtor-narrativo"
config = root / "review-construtor-narrativo-config"
assert not manifest.exists() and not review.exists() and not config.exists()
result = {"stage": "narrative_builder_foundation", "technical_verification_passed": True,
    "human_acceptance": "pending", "published": False, "whole_reform_complete": False,
    "all_accepted_proposals_complete": False, "source": str(source), "branch": git("branch", "--show-current"),
    "base_commit": base, "source_uncommitted": True, "source_status": git("status", "--porcelain"), "schema": 26,
    "binaries": {}, "review_copies": [], "source_artifacts": {},
    "previous_frozen_deliveries_preserved": [str(p) for p in previous],
    "limits": ["First operational cut and initial Document realization, not the entire narrative builder",
      "Four order authorities independent; writing queue is optional, not editing history",
      "Legacy precedes links remain unclassified and are not chronological constraints",
      "No inference of entity presence, travel, canonical relationship changes or whole-text unit identity",
      "Contextual derivation blocked until complete transfer; reusable model is only a skeleton",
      "Per-line movements, anchored realizations, impact, comparison, knowledge/revelations and advanced lenses pending",
      "Core Debug and ASan/UBSan focal: test plus six units; remaining dependencies Release; LSan disabled",
      "UI ASan/UBSan focal: driver plus seven units; remaining production objects Release; LSan disabled",
      "Selected visual review and automated flow are not human, accessibility, HiDPI or IME certification",
      "No map reform/timezones or automatic entity recognition; entity recognition last"]}
for kind, name in (("inde", "inde"), ("laboratory", "inde_text_blocks_lab")):
    target = delivery / ("inde-" + ("reforma-" if kind == "inde" else "laboratorio-") + prefix)
    assert not target.exists()
    shutil.copy2(root / "build" / name, target); target.chmod(0o750)
    result["binaries"][kind] = {"path": str(target), "sha256": digest(target)}
names = sorted(set(n for n in subprocess.check_output(["git", "ls-files", "-co", "--exclude-standard", "-z"], cwd=source).decode().split("\0") if n))
archive = delivery / ("inde-fonte-" + prefix + ".tar.gz")
assert not archive.exists()
with tarfile.open(archive, "w:gz") as tar:
    for name in names:
        path = source / name
        assert path.is_file() and not path.is_symlink()
        tar.add(path, arcname="inde/" + name, recursive=False)
result["source_artifacts"]["complete_source"] = {"path": str(archive), "sha256": digest(archive), "files": len(names), "includes_nonignored_untracked": True}
review.mkdir(); config.mkdir()
(config / "inde").mkdir()
(config / "inde/interface.ini").write_text("[appearance]\ntheme=light\n")
env = dict(os.environ, XDG_CONFIG_HOME=str(config), GSETTINGS_BACKEND="memory")
for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"), ("empty", "Projeto vazio — sem exemplos.inde")):
    target = review / name
    if key == "atlas":
        origin = root / "review-auxiliar-exclusao" / name
        before = rows(origin); item = snapshot(origin, target)
        (target / "EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
        subprocess.run([str(root / "build/inde_prepare_editorial_review_copy"), str(target)], env=env, check=True)
        after = rows(target); assert rows(origin) == before
        for table, values in before.items():
            if table in ("change_log", "sqlite_sequence") or table.startswith("cartographic_position_index_"): continue
            assert not values - after[table], "Existing rows changed: " + table
        item["origin_unchanged"] = True
    else:
        subprocess.run([str(root / "build/inde_text_blocks_empty_fixture"), str(target)], env=env, check=True)
        assert not rows(target)["documents"] and not rows(target)["works"]
        item = {"path": str(target), "origin": "newly_created_pristine", "mutable_review": True}
    (target / "NARRATIVE_BUILDER_REVIEW_COPY").write_text("INDE_NARRATIVE_BUILDER_REVIEW_COPY_2026_10_11\n")
    item.update(key=key, schema=26); result["review_copies"].append(item)
evidence = [args.ui, args.ui_asan, args.ctest, args.debug, args.asan, args.visual, args.regression, args.startup, preservation,
    source / "docs/narrative-builder-contract-2026-10-11.md", source / "docs/narrative-builder-increment-2026-10-11.md",
    delivery / "reforma-construtor-narrativo-implementacao-2026-10-11.md", delivery / "inde-construtor-narrativo-abrir.sh"]
result["verification_artifacts"] = [{"path": str(p.resolve()), "sha256": digest(p)} for p in evidence]
manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("SEALED", manifest)
