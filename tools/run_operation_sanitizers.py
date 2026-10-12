"""Focal ASan/UBSan: session capture, inversion, validation, transactions and migration.

Other repositories/services and GTK dependencies remain Release; LSan disabled.
"""
from pathlib import Path
import os
import shlex
import subprocess

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
OUT = ROOT / "build/asan-operations"
OUT.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "pangocairo", "uuid", "sqlite3"], text=True))
binary = OUT / "inde_project_operation_tests"
command = ["c++", "-std=c++20", "-O1", "-g", "-UNDEBUG", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
           "-I" + str(SOURCE / "include"), str(SOURCE / "tests/project_operation_tests.cpp"),
           *(str(SOURCE / "src/persistence" / name) for name in ("sqlite_database.cpp", "project_operation_store.cpp", "schema_migrator.cpp")),
           str(ROOT / "build/libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)]
print("BUILD focal operations ASan/UBSan; dependencies Release", flush=True)
subprocess.run(command, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)], env=env, check=True)
print("PASS focal operations ASan/UBSan; LSan disabled; not whole-app/GTK certification", flush=True)
