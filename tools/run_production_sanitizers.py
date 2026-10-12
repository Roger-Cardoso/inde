"""Focal ASan/UBSan on production, packaging, PDF and tests; core dependencies Release."""
from pathlib import Path
import os
import shlex
import subprocess

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
OUT = ROOT / "build/asan-production"
OUT.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "pangocairo", "uuid", "sqlite3"], text=True))
binary = OUT / "inde_work_production_tests"
command = ["c++", "-std=c++20", "-O1", "-g", "-UNDEBUG", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
           "-I" + str(SOURCE / "include"), str(SOURCE / "tests/work_production_tests.cpp"),
           *(str(SOURCE / "src/application" / name) for name in ("work_production.cpp", "production_packages.cpp", "production_pdf.cpp")),
           str(ROOT / "build/libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)]
print("BUILD focal production ASan/UBSan; dependencies Release", flush=True)
subprocess.run(command, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)], env=env, check=True)
print("PASS focal production ASan/UBSan; LSan disabled; not whole-app/GTK certification", flush=True)
