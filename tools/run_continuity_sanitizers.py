"""Focal ASan/UBSan for the new preferences and export units (not the whole GTK app)."""
from pathlib import Path
import os
import shlex
import subprocess

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
OUT = ROOT / "build/asan-continuity"
OUT.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "uuid", "sqlite3"], text=True))
binary = OUT / "inde_continuity_production_tests"
command = ["c++", "-std=c++20", "-O1", "-g", "-UNDEBUG", "-fsanitize=address,undefined",
           "-fno-omit-frame-pointer", "-I" + str(SOURCE / "include"),
           *[str(SOURCE / path) for path in ("tests/continuity_production_tests.cpp",
             "src/application/document_export.cpp", "src/ui/saved_workspaces.cpp", "src/ui/ui_preferences.cpp")],
           str(ROOT / "build/libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)]
print("BUILD focal ASan/UBSan: new export, saved workspaces, preferences, inline context parser and unit test; core dependencies remain Release", flush=True)
subprocess.run(command, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1",
           G_DEBUG="fatal-criticals") # Negative malformed-preferences fixture intentionally emits a warning; GTK UI keeps fatal-warnings.
subprocess.run([str(binary)], env=env, check=True)
print("PASS focal ASan/UBSan; LSan disabled; no whole-app or GTK certification", flush=True)
