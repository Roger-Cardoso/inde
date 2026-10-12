"""Focal Debug or ASan/UBSan of vocabulary helpers, validation and JSON codec.

Three core units plus the new helper/test are instrumented. Remaining core and
dependencies are Release. This is not a full Debug or sanitizer build.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--debug",action="store_true")
args=parser.parse_args()
source=Path(__file__).resolve().parents[1];build=source.parent/"build"
out=build/("debug-author-vocabulary" if args.debug else "asan-author-vocabulary")
out.mkdir(exist_ok=True)
flags=shlex.split(subprocess.check_output(["pkg-config","--cflags","--libs","glib-2.0","pangocairo","uuid","sqlite3","icu-i18n"],text=True))
instrument=[] if args.debug else ["-fsanitize=address,undefined","-fno-omit-frame-pointer"]
common=["c++","-std=c++20","-O0","-g1","-UNDEBUG","-Wall","-Wextra","-Wpedantic",*instrument,"-I"+str(source/"include")]
units=("project/semantic_registry.cpp","project/semantic_validation.cpp","persistence/semantic_codec.cpp")
objects=[]
for unit in units:
    target=out/(Path(unit).name+".o")
    subprocess.run([*common,*flags,"-c",str(source/"src"/unit),"-o",str(target)],check=True)
    objects.append(str(target))
binary=out/"inde_author_vocabulary_tests"
command=[*common,str(source/"tests/author_vocabulary_tests.cpp"),*objects,str(build/"libinde_core.a"),*flags,"-pthread","-ldl","-o",str(binary)]
subprocess.run(command,check=True)
subprocess.run([str(binary)],env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"),check=True)
report=dict(passed=True,configuration="focal Debug" if args.debug else "focal ASan/UBSan",units=list(units),
    driver_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),command=command,
    limits="New helper/driver and three core units; remaining dependencies Release. LSan disabled for sanitizer run.")
(out/"report.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
print("PASS vocabulary",report["configuration"],out/"report.json")
