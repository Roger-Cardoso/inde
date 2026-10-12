"""Bounded startup of the actual main and laboratory binaries, not UX proof."""
from datetime import datetime
from hashlib import sha256
from pathlib import Path
import json
import os
import re
import select
import sqlite3
import subprocess
from reform_snapshots import snapshot

source = Path(__file__).resolve().parents[1]
root = source.parent
run = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
qa = root / "qa" / ("builder-startup-" + run)
logs = root / "logs" / ("builder-startup-" + run)
qa.mkdir(parents=True); logs.mkdir(parents=True)
env = dict(os.environ, GSETTINGS_BACKEND="memory", GTK_IM_MODULE="simple",
           GDK_BACKEND="x11", GSK_RENDERER="cairo")
launcher = root.parents[1] / "outputs/inde-construtor-narrativo-abrir.sh"
env.update(dict(re.findall(r"^export (INDE_\w+)='([^']+)'", launcher.read_text(), re.M)))
xvfb = "/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-etapa1/recuperacao/dependencies/xvfb/usr/bin/Xvfb"
reader, writer = os.pipe()
report = {"scope": "startup-only", "run": run, "complete": False,
          "human_acceptance": "pending", "checks": {},
          "limits": ["Six-second process startup only; does not prove editing, navigation or human usability"]}
with (logs / "xvfb.log").open("w") as log:
    server = subprocess.Popen([xvfb, "-displayfd", str(writer), "-screen", "0", "1280x900x24", "-nolisten", "tcp"],
                              stdout=log, stderr=subprocess.STDOUT, pass_fds=(writer,))
    os.close(writer)
    try:
        assert select.select([reader], [], [], 15)[0]
        display = os.read(reader, 64).decode().strip()
        assert display.isdigit() and server.poll() is None
        env["DISPLAY"] = ":" + display
        origin = root / "review-auxiliar-exclusao/Atlas de Véspera — Projeto Integral.inde"
        for name in ("inde", "inde_text_blocks_lab"):
            binary = root / "build" / name
            target = qa / (name + ".inde")
            snapshot(origin, target)
            scoped = dict(env, XDG_CONFIG_HOME=str(qa / (name + "-config")))
            # The laboratory deliberately refuses migration; prepare only this copy.
            (target / "EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
            subprocess.run([str(root / "build/inde_prepare_editorial_review_copy"), str(target)],
                           env=scoped, check=True)
            output = logs / (name + ".log")
            with output.open("w") as stream:
                child = subprocess.Popen([str(binary), str(target)], env=scoped, stdout=stream, stderr=subprocess.STDOUT)
                try:
                    code = child.wait(timeout=6)
                    raise AssertionError(f"{name} exited prematurely: {code}")
                except subprocess.TimeoutExpired:
                    child.terminate(); child.wait(timeout=10)
                finally:
                    if child.poll() is None:
                        child.kill(); child.wait()
            text = output.read_text()
            assert not re.search(r"CRITICAL|Gtk-WARNING|AddressSanitizer|runtime error:|unhandled exception", text), text
            with sqlite3.connect((target / "data/project.sqlite3").as_uri() + "?mode=ro", uri=True) as db:
                assert db.execute("PRAGMA integrity_check").fetchall() == [("ok",)]
                assert not db.execute("PRAGMA foreign_key_check").fetchall()
            report["checks"][name] = {"passed": True, "duration_seconds": 6,
                "shutdown": "deliberate SIGTERM after startup interval", "log": str(output),
                "binary_sha256": sha256(binary.read_bytes()).hexdigest(),
                "environmental_warnings": [line for line in text.splitlines() if "warning" in line.lower()]}
    finally:
        os.close(reader); server.terminate(); server.wait(timeout=10)
report["complete"] = True
path = logs / "report.json"
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print("PASS startup-only", path)
