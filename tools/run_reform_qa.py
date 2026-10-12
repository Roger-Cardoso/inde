"""Validate the actual production UI on independent snapshots and a private display."""
from pathlib import Path
from datetime import datetime
from hashlib import sha256
import json
import os
import re
import select
import sqlite3
import subprocess
import time
from reform_snapshots import snapshot
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--case", help="Diagnostic subset; does not seal a full verification")
parser.add_argument("--skip-build", action="store_true", help="Use already compiled production objects and drivers")
parser.add_argument("--unique-captures", action="store_true", help="Append this run ID to captures, preserving previously inspected frames")
parser.add_argument("--theme", choices=("light", "dark"), help="Diagnostic initial theme; does not replace the full verification")
parser.add_argument("--operations-asan", action="store_true", help="Use focal operations UI ASan driver; diagnostic only")
parser.add_argument("--composition-asan", action="store_true", help="Use focal composition/context/modal UI ASan drivers; diagnostic only")
parser.add_argument("--worldbuilding-asan", action="store_true", help="Use focal aliases/calendar UI ASan driver; diagnostic only")
parser.add_argument("--operations-debug", action="store_true", help="Capture a diagnostic backtrace for operations UI")
parser.add_argument("--stage", choices=("organization", "continuity", "editorial", "production", "operations", "composition", "worldbuilding"), required=True, help="Deliver this block without overwriting prior evidence")
options = parser.parse_args()

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"
PREVIOUS = Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-interacao-2026-10-04")
XVFB = PREVIOUS.parent / "editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb"
RUN = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
QA = ROOT / "qa" / RUN
LOGS = ROOT / "logs" / RUN
LOGS.mkdir(parents=True)
QA.mkdir(parents=True)
report = {"complete": False, "checks": {}, "screenshots": [], "run": RUN,
          "human_acceptance": "pending", "source": str(SOURCE), "stage": options.stage}
MODES = ("reform", "organization", "explorer", "relations", "editing", "shell", "editor_interaction", "toolbar", "color", "text_workshop")
STAGES=("organization", "continuity", "editorial", "production", "operations", "composition", "worldbuilding")
def at_least(stage):return STAGES.index(options.stage)>=STAGES.index(stage)
if at_least("continuity"): MODES += ("continuity",)
if at_least("editorial"): MODES += ("editorial_review",)
if at_least("production"): MODES += ("production",)
if at_least("operations"): MODES += ("operations",)
if at_least("composition"): MODES += ("composition", "editor_fixes")
if at_least("worldbuilding"): MODES += ("worldbuilding",)
PREFIX = {"organization":"reforma-organizacao", "continuity":"reforma-continuidade", "editorial":"reforma-revisao", "production":"reforma-producao", "operations":"reforma-operacoes", "composition":"reforma-composicao", "worldbuilding":"reforma-mundo"}[options.stage]
DATE = "2026-10-10" if options.stage=="worldbuilding" else ("2026-10-08" if at_least("production") else "2026-10-07")

def rows(project, include_save_time=True):
    dbpath = project / "data/project.sqlite3"
    with sqlite3.connect(dbpath.as_uri() + "?mode=ro", uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
        assert db.execute("PRAGMA foreign_key_check").fetchall() == []
        tables = [v[0] for v in db.execute("SELECT name FROM sqlite_master WHERE type='table'")]
        result = {}
        for table in tables:
            quoted = '"' + table.replace('"', '""') + '"'
            columns = '*'
            if table == "projects" and not include_save_time:
                columns = ','.join('NULL' if v[1] == "updated_at" else '"' + v[1].replace('"', '""') + '"'
                                   for v in db.execute('PRAGMA table_info(' + quoted + ')'))
            result[table] = sorted(repr(v) for v in db.execute('SELECT ' + columns + ' FROM ' + quoted))
        return result

def run(name, command, env, capture=False):
    print("RUN", name, flush=True)
    log = LOGS / (name + ".log")
    start = time.monotonic()
    with log.open("w") as output:
        process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT, env=env)
        taken = set()
        while process.poll() is None:
            if time.monotonic() - start > 180:
                process.kill(); process.wait(); raise RuntimeError("Timeout: " + name)
            if log.stat().st_size > 8 * 1024 * 1024:
                process.kill(); process.wait(); raise RuntimeError("Runaway diagnostic log: " + name)
            if capture:
                for stage in re.findall(r"^SNAPSHOT: (.+)$", log.read_text(), re.M):
                    if stage in taken: continue
                    time.sleep(0.65 if any(m in name for m in ("-organization", "-continuity", "-editorial_review", "-production")) else 0.35)
                    path = DELIVERY / (PREFIX + "-" + name + "-" + stage + ("-" + RUN if options.unique_captures else "") + ".png")
                    subprocess.run(["import", "-window", "root", str(path)], env=env, check=True, timeout=8)
                    report["screenshots"].append(str(path)); taken.add(stage)
            time.sleep(0.035)
        code = process.wait()
    text = log.read_text()
    report["checks"][name] = {"passed": code == 0 and not re.search(r"FAIL|CRITICAL|Theme parser error|Gtk-WARNING", text),
                              "exit_code": code, "seconds": round(time.monotonic() - start, 3), "log": str(log)}
    (LOGS / "progress.json").write_text(json.dumps(report, ensure_ascii=False, indent=2))
    assert report["checks"][name]["passed"], text[-7000:]
    print("PASS", name, flush=True)

def main():
    env = dict(os.environ, GSETTINGS_BACKEND="memory", G_DEBUG="fatal-warnings" if at_least("operations") else "fatal-criticals", GDK_BACKEND="x11",
               GTK_IM_MODULE="simple", INDE_CLIPBOARD_TEST_ISOLATED="1", GSK_RENDERER="cairo")
    env["XDG_CONFIG_HOME"]=str(QA/"preparation-config")
    if options.worldbuilding_asan:
        assert options.stage=="worldbuilding" and options.case and options.skip_build
        assert not (options.operations_asan or options.composition_asan)
        env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    if options.operations_asan:
        assert options.stage=="operations" and options.case and options.skip_build
        env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    if options.composition_asan:
        assert options.stage=="composition" and options.case and options.skip_build
        env.update(ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    launcher = PREVIOUS.parents[1] / "outputs/editor-interacao-abrir-revisao.sh"
    env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'", launcher.read_text(), re.M)))
    if not options.skip_build:
        if at_least("editorial"):
            run("build-copy-preparation",["cmake","--build",str(ROOT / "build"),"--target","inde_prepare_editorial_review_copy","-j","2"],env)
        run("build-release", ["cmake", "--build", str(ROOT / "build"), "--target", "inde",
            "inde_ui_text_workshop_smoke", "inde_text_blocks_lab", *(["inde_continuity_production_tests"] if options.stage in ("continuity", "editorial", "production") else []), "-j", "2"], env)
        for mode in MODES:
            if mode == "text_workshop": continue
            if options.case and not any(re.search(options.case, key + "-" + mode) for key in ("atlas", "empty")): continue
            run("build-" + mode, ["python3", str(SOURCE / "tools/build_ui_workspace_smoke.py"),
                "--build", str(ROOT / "build"), "--test", mode], env)
    assert "CMAKE_BUILD_TYPE:STRING=Release" in (ROOT / "build/CMakeCache.txt").read_text()
    if at_least("continuity"):
        run("export-readback", ["python3", str(SOURCE / "tools/verify_document_export.py"),
            "--test-binary", str(ROOT / "build/inde_continuity_production_tests")],
            dict(env,G_DEBUG="fatal-criticals")) # Core negative fixture deliberately warns on corrupt preferences.
        if not options.case and not options.theme:
            run("focal-sanitizers", ["python3", str(SOURCE / "tools/run_continuity_sanitizers.py")], env)
    if options.stage in ("editorial", "production") and not options.case and not options.theme:
        run("editorial-focal-sanitizers", ["python3", str(SOURCE / "tools/run_editorial_review_sanitizers.py")], env)
    if options.stage == "production" and not options.case and not options.theme:
        run("production-focal-sanitizers", ["python3", str(SOURCE / "tools/run_production_sanitizers.py")], env)
    starting_binary_hash = sha256((ROOT / "build/inde").read_bytes()).hexdigest()
    starting_lab_hash = sha256((ROOT / "build/inde_text_blocks_lab").read_bytes()).hexdigest()
    reader, writer = os.pipe()
    with (LOGS / "xvfb.log").open("w") as log:
        server = subprocess.Popen([str(XVFB), "-displayfd", str(writer), "-screen", "0", "1280x900x24", "-nolisten", "tcp"],
                                  stdout=log, stderr=subprocess.STDOUT, pass_fds=(writer,))
        os.close(writer)
        try:
            assert select.select([reader], [], [], 15)[0], "Display startup timed out"
            env["DISPLAY"] = ":" + os.read(reader, 64).decode().strip()
            for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
                              ("empty", "Projeto vazio — sem exemplos.inde")):
                origin = PREVIOUS / "review" / name
                original = rows(origin)
                original_authorial = rows(origin, include_save_time=False)
                for mode in MODES:
                    if options.case and not re.search(options.case, key + "-" + mode): continue
                    target = QA / key / mode / name
                    snapshot(origin, target)
                    if at_least("editorial"):
                        (target / "EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
                        subprocess.run([str(ROOT / "build/inde_prepare_editorial_review_copy"), str(target)], env=env, check=True, stdout=subprocess.DEVNULL)
                    scoped = dict(env, XDG_CONFIG_HOME=str(QA / "config" / (key + "-" + mode)))
                    if options.theme:
                        config = Path(scoped["XDG_CONFIG_HOME"]) / "inde"
                        config.mkdir(parents=True)
                        (config / "interface.ini").write_text("[appearance]\ntheme=" + options.theme + "\n")
                    args = [str(ROOT / "build" / ("inde_ui_" + mode + "_smoke")), str(target)]
                    if mode=="operations" and options.operations_asan:
                        args[0]=str(ROOT/"build/inde_ui_operations_smoke_asan")
                    if options.composition_asan and mode in ("composition","editor_fixes"):
                        args[0]=str(ROOT/"build"/("inde_ui_"+mode+"_smoke_asan"))
                    if options.worldbuilding_asan and mode=="worldbuilding":
                        args[0]=str(ROOT/"build/inde_ui_worldbuilding_smoke_asan")
                    if mode == "shell":
                        with sqlite3.connect(target / "data/project.sqlite3") as db:
                            expected = db.execute("SELECT count(*) FROM entities").fetchone()[0]
                        args += [str(expected), "lifecycle", "--allow-fixture-writes"]
                    elif mode != "text_workshop": args += ["--allow-fixture-writes"]
                    if mode=="operations" and options.operations_debug:
                        assert options.case and options.skip_build
                        args=["gdb","--batch","-ex","run","-ex","bt","--args",*args]
                    test_name = key + "-" + mode + ("-" + options.theme if options.theme else "")
                    if mode=="operations" and options.operations_asan:test_name+="-asan"
                    if options.composition_asan and mode in ("composition","editor_fixes"):test_name+="-asan"
                    if options.worldbuilding_asan and mode=="worldbuilding":test_name+="-asan"
                    run(test_name, args, scoped, capture=mode in ("reform", "organization", "continuity", "editorial_review", "production", "operations", "composition", "editor_fixes", "text_workshop", "editor_interaction", "worldbuilding"))
                    if mode in ("continuity", "editorial_review", "production", "operations", "composition", "worldbuilding"):
                        run(test_name + "-restart", [args[0], str(target), "--verify-restart"], scoped, capture=True)
                    assert rows(origin) == original, "Prior review fixture altered"
                    after = rows(target, include_save_time=False)
                    if mode in ("reform", "organization", "continuity", "editorial_review", "production", "operations", "composition", "editor_fixes", "explorer", "relations", "shell", "text_workshop", "worldbuilding"):
                        from collections import Counter
                        for table, before in original_authorial.items():
                            if table in ("change_log", "sqlite_sequence"): continue
                            assert not (Counter(before) - Counter(after[table])), "Existing row changed: " + table
            report["origins_unchanged"] = True
            report["preservation_scope"] = "Original fixtures: all SQLite rows unchanged. Copies: existing rows retained except operational change_log/sqlite_sequence and projects.updated_at updated by Save."
        finally:
            os.close(reader); server.terminate(); server.wait(timeout=10)
    report["binary"] = {"path": str(ROOT / "build/inde"), "sha256": sha256((ROOT / "build/inde").read_bytes()).hexdigest()}
    assert report["binary"]["sha256"] == starting_binary_hash, "Production executable changed during QA"
    assert sha256((ROOT / "build/inde_text_blocks_lab").read_bytes()).hexdigest() == starting_lab_hash
    report["laboratory_binary"] = {"path": str(ROOT / "build/inde_text_blocks_lab"), "sha256": starting_lab_hash}
    report["build_type"] = "Release"
    if at_least("operations"):
        report["driver_sha256"] = {mode: sha256((ROOT / "build" / ("inde_ui_" + mode + "_smoke")).read_bytes()).hexdigest()
                                    for mode in MODES if (ROOT / "build" / ("inde_ui_" + mode + "_smoke")).exists()}
        report["case"] = options.case
        report["theme"] = options.theme
        report["operations_ui_focal_sanitizers"] = options.operations_asan
        report["composition_ui_focal_sanitizers"] = options.composition_asan
        report["worldbuilding_ui_focal_sanitizers"] = options.worldbuilding_asan
        if options.worldbuilding_asan:
            report["worldbuilding_focal_driver_sha256"]=sha256((ROOT/"build/inde_ui_worldbuilding_smoke_asan").read_bytes()).hexdigest()
        if options.composition_asan:
            report["composition_focal_driver_sha256"]={m:sha256((ROOT/"build"/("inde_ui_"+m+"_smoke_asan")).read_bytes()).hexdigest() for m in ("composition","editor_fixes")}
        report["g_debug"] = env["G_DEBUG"]
        if options.operations_asan:
            report["operations_focal_driver_sha256"]=sha256((ROOT/"build/inde_ui_operations_smoke_asan").read_bytes()).hexdigest()
    report["complete"] = not bool(options.case or options.theme or options.operations_asan or options.composition_asan or options.worldbuilding_asan)
    report["ui_scenarios"] = sum(k.startswith(("atlas-","empty-")) for k in report["checks"])
    destination = LOGS / "diagnostic.json" if not report["complete"] else DELIVERY / (PREFIX + "-verificacao-" + DATE + ".json")
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print("QA_COMPLETE", flush=True)

if __name__ == "__main__": main()
