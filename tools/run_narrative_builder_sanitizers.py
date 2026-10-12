"""Focal Debug / ASan+UBSan for builder, schema, Save As and usage/undo.

Remaining core/dependencies are Release; not whole-program sanitizer coverage.
"""
from pathlib import Path
import argparse
import os
import shlex
import subprocess
import hashlib
import json
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--debug", action="store_true")
parser.add_argument("--prepare-only",action="store_true")
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
build = source.parent / "build"
out = build / ("debug-narrative-builder" if args.debug else "asan-narrative-builder")
out.mkdir(exist_ok=True)
flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "glib-2.0", "pangocairo", "uuid", "sqlite3", "icu-i18n"], text=True))
instrument = [] if args.debug else ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
units = ["application/narrative_builder.cpp", "application/narrative_builder_alternatives.cpp", "application/narrative_realizations.cpp", "application/narrative_continuity.cpp", "application/structure_model_service.cpp", "application/project_service.cpp", "persistence/sqlite_catalog_repository.cpp", "persistence/schema_migrator.cpp", "persistence/sqlite_database.cpp", "persistence/project_database_repository.cpp", "persistence/project_operation_store.cpp", "persistence/sqlite_structure_model_repository.cpp", "persistence/sqlite_writing_repository.cpp", "persistence/composition_store.cpp"]
objects = []
headers=hashlib.sha256()
for header in sorted([*source.joinpath("include").rglob("*.hpp"), *source.joinpath("src").rglob("*.hpp")]):
    headers.update(str(header.relative_to(source)).encode());headers.update(header.read_bytes())
for index, unit in enumerate(units):
    obj = out / (str(index) + ".o")
    command=["c++", "-std=c++20", "-O0", "-g1", "-UNDEBUG", *instrument, "-I" + str(source / "include"), *flags, "-c", str(source / "src" / unit), "-o", str(obj)]
    stamp=out/(str(index)+".json")
    signature=hashlib.sha256(headers.digest()+json.dumps(command).encode()+(source/"src"/unit).read_bytes()).hexdigest()
    if not obj.exists() or not stamp.exists() or json.loads(stamp.read_text())["signature"]!=signature:
        print("Compile focal",unit,flush=True)
        subprocess.run(command,check=True)
        stamp.write_text(json.dumps({"signature":signature})+"\n")
    else:print("Reuse verified focal",unit,flush=True)
    objects.append(str(obj))
if args.prepare_only:
    print("PREPARED focal core objects only; linking and tests still pending",flush=True)
    raise SystemExit(0)
for test in ("narrative_builder","narrative_alternatives","narrative_realization","work_setup"):
    binary = out / ("inde_"+test+"_tests")
    subprocess.run(["c++", "-std=c++20", "-O0", "-g1", "-UNDEBUG", *instrument, "-I" + str(source / "include"), str(source / ("tests/"+test+"_tests.cpp")), *objects, str(build / "libinde_core.a"), *flags, "-pthread", "-ldl", "-o", str(binary)], check=True)
    subprocess.run([str(binary)], env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"), check=True)
print("PASS focal builder " + ("Debug" if args.debug else "ASan/UBSan; LSan disabled") + "; remaining dependencies Release")
