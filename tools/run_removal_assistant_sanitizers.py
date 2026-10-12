"""Focal core Debug or ASan/UBSan for the deletion guide and calendar cleanup.

Dependencies stay Release; this is not a full Debug or full-app sanitizer build.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--debug", action="store_true")
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
root = source.parent
out = root / "build" / ("debug-removal-assistant" if args.debug else "asan-removal-assistant")
out.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "pangocairo", "uuid", "sqlite3", "icu-i18n"], text=True))
binary = out / "inde_removal_assistant_tests"
instrument = [] if args.debug else ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
command = ["c++", "-std=c++20", "-O0", "-g1", "-UNDEBUG", *instrument,
           "-I" + str(source / "include"), str(source / "tests/removal_assistant_tests.cpp"),
           str(source / "src/persistence/project_operation_store.cpp"),
           str(source / "src/persistence/sqlite_database.cpp"),
           str(source / "src/application/worldbuilding_service.cpp"),
           str(root / "build/libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)]
subprocess.run(command, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)], env=env, check=True)
print("PASS focal removal assistant " + ("Debug" if args.debug else "ASan/UBSan; LSan disabled") + "; dependencies Release")
