"""Reproduce inherited desktop CSS failure, then check process-local theme isolation.

Never changes desktop settings or opens a live authorial project. --desktop-probe
only initializes GTK/settings on the real display; it creates no visible window.
"""
from datetime import datetime
from hashlib import sha256
from pathlib import Path
import argparse
import json
import os
import re
import select
import sqlite3
import subprocess
from reform_snapshots import snapshot

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--desktop-probe", action="store_true")
parser.add_argument("--driver", type=Path, help="Optional focused instrumented theme driver")
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
delivery = root.parents[1] / "outputs"
run = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
qa = root / "qa" / ("theme-compatibility-" + run)
logs = root / "logs" / ("theme-compatibility-" + run)
qa.mkdir(parents=True); logs.mkdir(parents=True)
binary = root / "build/inde"
lab = root / "build/inde_text_blocks_lab"
probe = args.driver.resolve() if args.driver else root / "build/inde_ui_theme_compatibility_smoke"
theme = Path("/home/roger/.themes/Orchis-Dark-Compact/gtk-4.0/gtk.css")
desktop_settings = Path("/home/roger/.config/xfce4/xfconf/xfce-perchannel-xml/xsettings.xml")
digest = lambda p: sha256(p.read_bytes()).hexdigest()
observed_files = [theme, theme.with_name("gtk-dark.css"), desktop_settings]
recents = Path("/home/roger/.config/inde/recent-projects")
if recents.exists():
    observed_files.append(recents)
before = {str(p): digest(p) for p in observed_files}
report = dict(scope="inherited-theme-compatibility", run=run, complete=False,
              binary_sha256=digest(binary), laboratory_sha256=digest(lab),
              driver=str(probe), driver_sha256=digest(probe), checks={}, negative_controls={},
              driver_instrumented=bool(args.driver),
              theme_inputs=before, human_acceptance="pending",
              limits=["Startup is bounded, not editing/long-session proof",
                      "Desktop probe initializes settings without a visible window",
                      "No change to desktop CSS/settings; explicit GTK_THEME is preserved",
                      "Private Xvfb inherits Orchis through settings.ini, not a global XSettings mutation"])
def config(key, inherited=True):
    p = qa / (key + "-config"); p.mkdir()
    if inherited:
        (p / "gtk-4.0").mkdir()
        (p / "gtk-4.0/settings.ini").write_text(
            "[Settings]\ngtk-theme-name=Orchis-Dark-Compact\n"
            "gtk-application-prefer-dark-theme=false\ngtk-font-name=Sans 11\n"
            "gtk-icon-theme-name=Adwaita\n")
    return p
def run_probe(key, env, frames=False, expected_warning=False):
    output = logs / (key + ".log")
    cmd = [str(probe), "render", str(logs / (key + "-frames"))] if frames else [str(probe), "--inspect-desktop"]
    with output.open("w") as log:
        child = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=20)
    text = output.read_text()
    warnings = [line for line in text.splitlines() if re.search(r"WARNING|CRITICAL|runtime error:|AddressSanitizer", line)]
    assert child.returncode == 0 and "PASS:" in text, text
    if expected_warning:
        assert warnings and "Theme parser error" in text and "base=Orchis-Dark-Compact" in text
        report["negative_controls"][key] = dict(passed=True, log=str(output), warnings=len(warnings),
                                               reason="Deliberately explicit incompatible override remains observable")
    else:
        assert not warnings, text
        report["checks"][key] = dict(passed=True, log=str(output), warnings=0,
                                    screenshots=[str(p) for p in sorted((logs / (key + "-frames")).glob("*.png"))])
    print("PASS", key, flush=True)

launcher = delivery / "inde-acesso-planejamento-abrir.sh"
catalogs = dict(re.findall(r"^export (INDE_\w+)='([^']+)'", launcher.read_text(), re.M))
env = dict(os.environ, GSETTINGS_BACKEND="memory", GTK_IM_MODULE="simple",
           GDK_BACKEND="x11", GSK_RENDERER="cairo",
           ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1", **catalogs)
env.pop("GTK_THEME", None)
reader, writer = os.pipe()
xvfb = "/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb"
with (logs / "xvfb.log").open("w") as log:
    server = subprocess.Popen([xvfb, "-displayfd", str(writer), "-screen", "0",
                               "1280x900x24", "-nolisten", "tcp"],
                              stdout=log, stderr=subprocess.STDOUT, pass_fds=(writer,))
    os.close(writer)
    try:
        assert select.select([reader], [], [], 15)[0]
        display = os.read(reader, 64).decode().strip()
        assert display.isdigit() and server.poll() is None
        env["DISPLAY"] = ":" + display
        empty = qa / "pristine.inde"
        fixture_env = dict(env, XDG_CONFIG_HOME=str(config("fixture")))
        subprocess.run([str(root / "build/inde_text_blocks_empty_fixture"), str(empty)],
                       env=fixture_env, check=True)
        # Negative control uses the frozen executable reported by the user.
        old = delivery / "inde-reforma-acesso-planejamento-2026-10-11"
        old_manifest = json.loads((delivery / "reforma-acesso-planejamento-entrega-2026-10-11.json").read_text())
        assert digest(old) == old_manifest["binaries"]["inde"]["sha256"]
        scoped = dict(env, XDG_CONFIG_HOME=str(config("old-inherited")))
        scoped.pop("G_DEBUG", None)
        output = logs / "old-inherited.log"
        with output.open("w") as stream:
            child = subprocess.Popen([str(old), str(empty)], env=scoped,
                                     stdout=stream, stderr=subprocess.STDOUT)
            try:
                child.wait(timeout=5)
                raise AssertionError("Old binary exited before reproduction completed")
            except subprocess.TimeoutExpired:
                child.terminate(); child.wait(timeout=10)
            finally:
                if child.poll() is None: child.kill(); child.wait()
        text = output.read_text()
        assert "Theme parser error" in text and "Unknown name of pseudo-class" in text
        assert 'No property named "--accent-bg-color"' in text and "Expected a valid color" in text
        report["negative_controls"]["old-inherited"] = dict(passed=True, log=str(output),
            warnings=text.count("Gtk-WARNING"), binary_sha256=digest(old),
            reason="Frozen previous executable reproduces the reported inherited-theme warnings")
        print("PASS old-inherited reproduction", flush=True)
        for key, override in (("inherited-default", None), ("explicit-compatible", "Adwaita:dark")):
            scoped = dict(env, G_DEBUG="fatal-warnings", XDG_CONFIG_HOME=str(config(key)))
            if override: scoped["GTK_THEME"] = override
            run_probe(key, scoped, frames=True)
        scoped = dict(env, GTK_THEME="Orchis-Dark-Compact", XDG_CONFIG_HOME=str(config("explicit-incompatible")))
        scoped.pop("G_DEBUG", None)
        run_probe("explicit-incompatible", scoped, expected_warning=True)

        atlas = qa / "atlas.inde"
        origin = root / "review-alternativas-narrativas/Atlas de Véspera — Projeto Integral.inde"
        snapshot(origin, atlas)
        for name, executable in (("main", binary), ("laboratory", lab)):
            for project, fixture in (("empty", empty), ("atlas", atlas)):
                key = name + "-" + project
                target = qa / (key + ".inde")
                snapshot(fixture, target)
                (target / "EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
                if name == "laboratory":
                    (target / "TEXT_BLOCKS_LAB_COPY").write_text("INDE_TEXT_BLOCKS_SPIKE_COPY_V1\n")
                    (target / "TEXT_WORKSHOP_COPY").write_text("INDE_TEXT_WORKSHOP_COPY_V1\n")
                scoped = dict(env, G_DEBUG="fatal-warnings", XDG_CONFIG_HOME=str(config(key)))
                subprocess.run([str(root / "build/inde_prepare_editorial_review_copy"), str(target)],
                               env=scoped, check=True, stdout=subprocess.DEVNULL)
                output = logs / (key + ".log")
                with output.open("w") as stream:
                    child = subprocess.Popen([str(executable), str(target)], env=scoped,
                                             stdout=stream, stderr=subprocess.STDOUT)
                    try:
                        code = child.wait(timeout=6)
                        raise AssertionError(f"{key} exited prematurely: {code}")
                    except subprocess.TimeoutExpired:
                        child.terminate(); child.wait(timeout=10)
                    finally:
                        if child.poll() is None: child.kill(); child.wait()
                text = output.read_text()
                assert not re.search(r"WARNING|CRITICAL|runtime error:|AddressSanitizer|FAIL", text), text
                with sqlite3.connect((target / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
                    assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
                    assert not db.execute("PRAGMA foreign_key_check").fetchall()
                report["checks"][key] = dict(passed=True, warnings=0, log=str(output),
                    duration_seconds=6, shutdown="deliberate SIGTERM after startup interval",
                    binary_sha256=digest(executable), project_integrity=True)
                print("PASS", key, flush=True)
    finally:
        os.close(reader); server.terminate(); server.wait(timeout=10)
if args.desktop_probe:
    # Preserve real desktop display/theme/input settings. No window or project.
    scoped = dict(os.environ, GSETTINGS_BACKEND="memory", GSK_RENDERER="cairo",
                  G_DEBUG="fatal-warnings", XDG_CONFIG_HOME=str(config("real-desktop", False)),
                  ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                  UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    scoped.pop("GTK_THEME", None)
    run_probe("real-desktop", scoped)
report["desktop_files_unchanged"] = all(digest(Path(p)) == sha for p, sha in before.items())
assert report["desktop_files_unchanged"]
report["complete"] = True
path = logs / "report.json"
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print("THEME_COMPATIBILITY_QA_COMPLETE", path)
