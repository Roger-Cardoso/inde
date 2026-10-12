"""Seal the text temporal foundation only after verified evidence. Never overwrite a delivery."""
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

parser = argparse.ArgumentParser(description=__doc__)
for key in ("ui", "ui-asan", "templates", "regression", "visual"):
    parser.add_argument("--" + key, required=True, type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
prefix = "temporal-contexto-2026-10-10"
base = "a8a75a7c2f44f7e2c44ccb7b6431a4675e2079bc"
def digest(p): return sha256(p.read_bytes()).hexdigest()
def git(*cmd): return subprocess.check_output(["git", *cmd], cwd=source, text=True).strip()
def rows(p):
    with sqlite3.connect((p / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert not db.execute("PRAGMA foreign_key_check").fetchall()
        return {t: Counter(repr(r) for r in db.execute('SELECT * FROM "' + t.replace('"', '""') + '"'))
                for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}

assert not git("status", "--porcelain") and git("rev-parse", "HEAD^") == base
binary_hash = digest(root / "build/inde")
reports = ((args.ui, False, "temporal-context-ui", "context"),
           (args.ui_asan, True, "temporal-context-ui", "context"),
           (args.templates, False, "temporal-templates-ui", "templates"),
           (args.regression, False, "temporal-first-cut-ui", "system"))
for path, focal, scope, driver in reports:
    r = json.loads(path.read_text())
    assert r["complete"] and r["scope"] == scope and r["origins_unchanged"]
    assert r["focal_sanitizers"] == focal and len(r["checks"]) == 8 and all(v["passed"] for v in r["checks"].values())
    assert r["binary_sha256"] == binary_hash
    assert digest(root / "build" / ("inde_ui_temporal_" + driver + "_smoke" + ("_asan" if focal else ""))) == r["driver_sha256"]
visual = json.loads(args.visual.read_text())
assert visual["reviewed_by_agent"] and visual["binary_sha256"] == binary_hash
for f in visual["screenshots"]: assert digest(Path(f["path"])) == f["sha256"]
ctest = root / "logs/ctest-temporal-context-final-2026-10-10.log"
asan = root / "logs/temporal-context-core-asan-final-2026-10-10.log"
preservation = delivery / ("reforma-" + prefix + "-preservacao-final.json")
assert "100% tests passed" in ctest.read_text() and "out of 25" in ctest.read_text()
assert "PASS focal temporal_context ASan/UBSan" in asan.read_text()
assert json.loads(preservation.read_text())["original_source_unchanged"]
previous = sorted(delivery.glob("reforma*-entrega-*.json"))
assert len(previous) == 10
for p in previous:
    for item in json.loads(p.read_text())["binaries"].values(): assert digest(Path(item["path"])) == item["sha256"]
manifest = delivery / "reforma-temporal-contexto-entrega-2026-10-10.json"
assert not manifest.exists() and not (root / "review-temporal-contexto").exists()
result = {"stage": "temporal_text_context_foundation", "technical_verification_passed": True,
          "human_acceptance": "pending", "published": False, "whole_temporal_system_complete": False,
          "whole_reform_complete": False, "all_accepted_proposals_complete": False,
          "entity_recognition_started": False, "entity_recognition_order": "last",
          "source": str(source), "branch": git("branch", "--show-current"), "commit": git("rev-parse", "HEAD"),
          "predecessor_commit": base, "predecessor_human_acceptance": "confirmed_by_user", "schema": 25,
          "binaries": {}, "review_copies": [], "source_artifacts": {},
          "previous_frozen_deliveries_preserved": [str(p) for p in previous],
          "limits": ["Foundation increment, not all eight authorized proposals",
                     "On-demand conservative before-birth warnings; presence, birth and scene date must be explicit",
                     "No blocking narrative conflicts, no automatic entity recognition or links",
                     "No winning overlapping chronology, implicit presence or approximate numerical margin",
                     "32 dates/annotation, 500 references/axis, 500 text uses/reference; explicit refusals",
                     "Warning consultation capped at 200 with visible limit notice",
                     "Native clipboard and independent editorial revisions still do not copy the semantic graph",
                     "Named regular clock units, including interval end; no timezone or sunset model",
                     "Contextual shortcuts, history/read order, adoption monthly lens, eras/evidence/correlations, recurrence, day boundary/maps and final entity recognition remain authorized follow-ups",
                     "ASan/UBSan focal: 16 core units and five UI units; other dependencies Release; LSan disabled",
                     "Block distribution tested in Release, not instrumented by this focal core suite",
                     "GTK matrix and selected visual inspection are not human acceptance or whole-app accessibility/HiDPI certification"]}
for kind, name in (("inde", "inde"), ("laboratory", "inde_text_blocks_lab")):
    target = delivery / ("inde-" + ("reforma" if kind == "inde" else "laboratorio") + "-" + prefix)
    assert not target.exists()
    shutil.copy2(root / "build" / name, target); target.chmod(0o750)
    result["binaries"][kind] = {"path": str(target), "sha256": digest(target)}
config = root / "review-temporal-contexto-config"
assert not config.exists(); shutil.copytree(root / "review-temporal-templates-config", config)
env = dict(os.environ, XDG_CONFIG_HOME=str(config), GSETTINGS_BACKEND="memory")
for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"), ("empty", "Projeto vazio — sem exemplos.inde")):
    origin = root / "review-temporal-templates" / name
    before = rows(origin)
    target = root / "review-temporal-contexto" / name
    item = snapshot(origin, target)
    subprocess.run([str(root / "build/inde_prepare_editorial_review_copy"), str(target)], check=True, env=env)
    assert rows(origin) == before
    after = rows(target)
    for table, values in before.items(): assert not values - after[table], "Migration altered rows: " + table
    with sqlite3.connect((target / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("SELECT max(version) FROM schema_migrations").fetchone()[0] == 25
        assert db.execute("SELECT count(*) FROM semantic_date_bindings").fetchone()[0] == 0
    (target / "TEMPORAL_CONTEXT_REVIEW_COPY").write_text("INDE_TEMPORAL_CONTEXT_REVIEW_COPY_2026_10_10\n")
    item.update(key=key, schema=25, authorial_rows_preserved=True, new_text_date_rows=0)
    result["review_copies"].append(item)
bundle = delivery / ("inde-reforma-" + prefix + ".bundle")
patch = delivery / ("inde-reforma-" + prefix + ".patch")
assert not bundle.exists() and not patch.exists()
subprocess.run(["git", "bundle", "create", str(bundle), "HEAD"], cwd=source, check=True)
subprocess.run(["git", "bundle", "verify", str(bundle)], cwd=source, check=True)
patch.write_bytes(subprocess.check_output(["git", "diff", "--binary", base, "HEAD"], cwd=source))
for p in (bundle, patch): result["source_artifacts"][p.suffix[1:]] = {"path": str(p), "sha256": digest(p)}
evidence = (args.ui, args.ui_asan, args.templates, args.regression, args.visual, ctest, asan, preservation,
            delivery / "aceite-temporal-templates-2026-10-10.json")
result["verification_artifacts"] = [{"path": str(p.resolve()), "sha256": digest(p)} for p in evidence]
manifest.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("SEALED", manifest)
