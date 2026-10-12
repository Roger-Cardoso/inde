"""Temporal production UI: empty/Atlas, light/dark, restart, isolated snapshots.

Not the full reform matrix, visual human acceptance, or whole-app sanitizer coverage.
"""
from pathlib import Path
from datetime import datetime
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import re
import select
import sqlite3
import subprocess
import time
from reform_snapshots import snapshot

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--asan", action="store_true")
parser.add_argument("--templates", action="store_true")
parser.add_argument("--context", action="store_true")
parser.add_argument("--graphic", action="store_true")
parser.add_argument("--deletion", action="store_true")
parser.add_argument("--workflow", action="store_true")
parser.add_argument("--assistant", action="store_true")
parser.add_argument("--builder", action="store_true")
parser.add_argument("--alternatives", action="store_true")
parser.add_argument("--access", action="store_true")
parser.add_argument("--realization", action="store_true")
parser.add_argument("--debugger", action="store_true", help="Diagnostic only: trap fatal GTK warnings under gdb in one disposable case; never delivery evidence")
parser.add_argument("--case", choices=("atlas-light", "atlas-dark", "empty-light", "empty-dark"),
                    help="Bound the run to one fixture/theme plus restart; merge complete case reports for the full matrix")
args = parser.parse_args()
assert not args.debugger or args.case
assert sum((args.templates,args.context,args.graphic,args.deletion,args.workflow,args.assistant,args.builder,args.alternatives,args.access,args.realization)) <= 1
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
run_id = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
qa = root / "qa" / ("temporal-" + run_id)
logs = root / "logs" / ("temporal-" + run_id)
logs.mkdir(parents=True)
qa.mkdir(parents=True)
xvfb = Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb")
driver = root / "build" / (("inde_ui_temporal_workflow_smoke" if args.workflow else "inde_ui_deletion_smoke" if args.deletion else "inde_ui_calendar_graphic_smoke" if args.graphic else "inde_ui_temporal_context_smoke" if args.context else "inde_ui_temporal_templates_smoke" if args.templates else "inde_ui_temporal_system_smoke") + ("_asan" if args.asan else ""))
if args.assistant:
    driver = root / "build" / ("inde_ui_removal_assistant_smoke" + ("_asan" if args.asan else ""))
if args.builder:
    driver = root / "build" / ("inde_ui_narrative_builder_smoke" + ("_asan" if args.asan else ""))
if args.alternatives:
    driver = root / "build" / ("inde_ui_narrative_alternatives_smoke" + ("_asan" if args.asan else ""))
if args.access:
    driver = root / "build" / ("inde_ui_narrative_access_smoke" + ("_asan" if args.asan else ""))
if args.realization:
    driver = root / "build" / ("inde_ui_narrative_realization_smoke" + ("_asan" if args.asan else ""))
report = {"scope": "temporal-workflow-ui" if args.workflow else "calendar-map-deletion-ui" if args.deletion else "calendar-graphic-ui" if args.graphic else "temporal-context-ui" if args.context else "temporal-templates-ui" if args.templates else "temporal-first-cut-ui", "run": run_id, "complete": False,
          "human_acceptance": "pending", "focal_sanitizers": args.asan,
          "checks": {}, "screenshots": [], "driver_sha256": sha256(driver.read_bytes()).hexdigest(),
          "binary_sha256": sha256((root / "build/inde").read_bytes()).hexdigest()}
if args.assistant:
    report["scope"] = "removal-assistant-ui"
if args.builder:
    report["scope"] = "narrative-builder-production-ui"
if args.alternatives:
    report["scope"] = "narrative-alternatives-production-ui"
if args.access:
    report["scope"] = "narrative-access-production-ui"
if args.realization:
    report["scope"] = "narrative-realization-production-ui"
report["selected_cases"] = [args.case] if args.case else ["atlas-light", "atlas-dark", "empty-light", "empty-dark"]

def rows(project):
    with sqlite3.connect((project / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert db.execute("PRAGMA foreign_key_check").fetchall() == []
        # R-tree pages can be reorganized by a remove/restore with identical
        # canonical positions. Verify logical content, not internal node bytes.
        if db.execute("SELECT 1 FROM sqlite_master WHERE name='cartographic_position_index'").fetchone():
            assert db.execute("SELECT rtreecheck('cartographic_position_index')").fetchone() == ("ok",)
            indexed = db.execute("SELECT id,min_lon,max_lon,min_lat,max_lat FROM cartographic_position_index ORDER BY id").fetchall()
            positions = db.execute("SELECT id,longitude_e6,longitude_e6,latitude_e6,latitude_e6 FROM cartographic_positions ORDER BY id").fetchall()
            assert indexed == positions, "Spatial index differs from canonical positions"
        result = {}
        for (table,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'"):
            quoted = '"' + table.replace('"', '""') + '"'
            columns = '*'
            if table == "projects":
                columns = ','.join('NULL' if v[1] == "updated_at" else '"' + v[1].replace('"', '""') + '"'
                                   for v in db.execute('PRAGMA table_info(' + quoted + ')'))
            result[table] = sorted(repr(v) for v in db.execute('SELECT ' + columns + ' FROM ' + quoted))
        return result

def execute(name, target, mode, env):
    print("RUN", name, flush=True)
    log = logs / (name + ".log")
    start = time.monotonic()
    captured = set()
    with log.open("w") as out:
        command = [str(driver), str(target), mode]
        if args.debugger:
            command = ["gdb", "--batch", "-ex", "run", "-ex", "bt 40", "--args", *command]
        child = subprocess.Popen(command, env=env, stdout=out, stderr=subprocess.STDOUT)
        while child.poll() is None:
            if time.monotonic() - start > 120:
                child.kill(); child.wait(); raise RuntimeError("Timeout " + name)
            for stage in re.findall(r"^SNAPSHOT: (.+)$", log.read_text(), re.M):
                if stage in captured: continue
                time.sleep(0.35)
                frame = delivery / ("reforma-temporal-" + name + "-" + stage + "-" + run_id + ".png")
                subprocess.run(["import", "-window", "root", str(frame)], env=env, check=True, timeout=8)
                report["screenshots"].append(str(frame)); captured.add(stage)
            time.sleep(0.035)
        code = child.wait()
    text = log.read_text()
    passed = code == 0 and "PASS temporal" in text and not re.search(r"FAIL|CRITICAL|Gtk-WARNING|AddressSanitizer|runtime error:", text)
    report["checks"][name] = {"passed": passed, "exit_code": code, "log": str(log),
                              "seconds": round(time.monotonic() - start, 3)}
    (logs / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2))
    assert passed, text[-8000:]
    print("PASS", name, flush=True)

env = dict(os.environ, GSETTINGS_BACKEND="memory", G_DEBUG="fatal-warnings", GDK_BACKEND="x11",
           GTK_IM_MODULE="simple", INDE_CLIPBOARD_TEST_ISOLATED="1", GSK_RENDERER="cairo")
if args.asan:
    env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
launcher = Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/outputs/editor-interacao-abrir-revisao.sh")
env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'", launcher.read_text(), re.M)))
reader, writer = os.pipe()
with (logs / "xvfb.log").open("w") as log:
    server = subprocess.Popen([str(xvfb), "-displayfd", str(writer), "-screen", "0", "1280x900x24", "-nolisten", "tcp"],
                              stdout=log, stderr=subprocess.STDOUT, pass_fds=(writer,))
    os.close(writer)
    try:
        assert select.select([reader], [], [], 15)[0], "Display timeout"
        display = os.read(reader, 64).decode().strip()
        assert display.isdigit() and server.poll() is None, "Xvfb could not create a private display; inspect xvfb.log"
        env["DISPLAY"] = ":" + display
        for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"), ("empty", "Projeto vazio — sem exemplos.inde")):
            if args.case and not args.case.startswith(key + "-"):
                continue
            origin = root / ("review-exclusoes-tempo-mapa" if args.workflow else "review-calendario-grafico" if args.deletion else "review-temporal-contexto" if args.graphic else "review-temporal-templates" if args.context else "review-temporal" if args.templates else "review-mundo") / name
            if args.assistant:
                origin = root / "review-fluxo-temporal" / name
            if args.builder or args.alternatives or args.access or args.realization:
                origin = root / "review-auxiliar-exclusao" / name
                if key == "empty":
                    origin = qa / "pristine-base.inde"
                    subprocess.run([str(root / "build/inde_text_blocks_empty_fixture"), str(origin)], env=dict(env, XDG_CONFIG_HOME=str(qa / "seed-config")), check=True)
            before = rows(origin)
            for theme in ("light", "dark"):
                if args.case and args.case != key + "-" + theme:
                    continue
                title = key + "-" + theme + ("-asan" if args.asan else "")
                target = qa / title / name
                snapshot(origin, target)
                scoped = dict(env, XDG_CONFIG_HOME=str(qa / "config" / title))
                config = Path(scoped["XDG_CONFIG_HOME"]) / "inde"
                config.mkdir(parents=True)
                (config / "interface.ini").write_text("[appearance]\ntheme=" + theme + "\n")
                execute(title, target, "--allow-fixture-writes", scoped)
                execute(title + "-restart", target, "--verify-restart", scoped)
                assert rows(origin) == before, "Original review modified"
                after = rows(target)
                for table, values in before.items():
                    if table in ("change_log", "sqlite_sequence") or table.startswith("cartographic_position_index_"): continue
                    assert not Counter(values) - Counter(after[table]), "Existing data changed: " + table
        report["origins_unchanged"] = True
    finally:
        os.close(reader); server.terminate(); server.wait(timeout=10)
assert sha256(driver.read_bytes()).hexdigest() == report["driver_sha256"]
assert sha256((root / "build/inde").read_bytes()).hexdigest() == report["binary_sha256"]
report["complete"] = True
(logs / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print("TEMPORAL_QA_COMPLETE", logs / "report.json", flush=True)
