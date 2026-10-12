"""Focal ASan/UBSan on new editorial core and its test; dependencies remain Release."""
from pathlib import Path
import os
import shlex
import subprocess

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
OUT = ROOT / "build/asan-editorial"
OUT.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "uuid", "sqlite3"], text=True))
binary = OUT / "inde_editorial_review_tests"
command = ["c++", "-std=c++20", "-O1", "-g", "-UNDEBUG", "-fsanitize=address,undefined",
           "-fno-omit-frame-pointer", "-I" + str(SOURCE / "include"),
           str(SOURCE / "tests/editorial_review_tests.cpp"), str(SOURCE / "src/application/editorial_review.cpp"),
           str(ROOT / "build/libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)]
print("BUILD focal ASan/UBSan: editorial decisions, suggestions, exact diff and test; dependencies Release", flush=True)
subprocess.run(command, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)], env=env, check=True)
print("PASS focal ASan/UBSan; LSan disabled; not a whole-app/GTK certification", flush=True)
