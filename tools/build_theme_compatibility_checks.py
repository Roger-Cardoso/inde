"""Debug/policy and focal ASan/UBSan theme checks; GTK dependencies stay Release."""
from pathlib import Path
import json
import os
import shlex
import subprocess

source = Path(__file__).resolve().parents[1]
target = source.parent / "build/focal-theme"
target.mkdir(parents=True, exist_ok=True)
common = ["c++", "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-I" + str(source / "include")]
flags = lambda package, mode: shlex.split(subprocess.check_output(["pkg-config", mode, package], text=True))
def invoke(command):
    print("COMMAND", shlex.join(command), flush=True)
    subprocess.run(command, cwd=source, check=True)
env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
           UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
for mode, extra in (("Debug", ["-O0", "-g1"]), ("ASan-UBSan", ["-O0", "-g1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])):
    binary = target / ("theme-policy-" + mode)
    invoke([*common, *extra, *flags("glib-2.0", "--cflags"),
            "tests/theme_compatibility_tests.cpp", "src/ui/theme_compatibility.cpp",
            *flags("glib-2.0", "--libs"), "-o", str(binary)])
    subprocess.run([str(binary)], env=env, check=True)
    print("PASS policy", mode, flush=True)
extra = ["-O0", "-g1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
units = ["tests/ui_theme_compatibility_smoke.cpp", "src/ui/theme_compatibility.cpp",
         "src/ui/application_theme.cpp", "src/ui/ui_preferences.cpp", "src/ui/rendering_policy.cpp"]
binary = target / "inde_ui_theme_compatibility_smoke_asan"
invoke([*common, *extra, *flags("gtkmm-4.0", "--cflags"), *units,
        *flags("gtkmm-4.0", "--libs"), "-o", str(binary)])
print("BUILT focused ASan/UBSan theme driver", binary, flush=True)
print(json.dumps({"production_units": 4, "test_driver": 1, "dependencies": "Release",
                  "leak_sanitizer": False}), flush=True)
