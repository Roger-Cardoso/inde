"""Focal ASan/UBSan on Composition authority, codec, storage, engine and output transaction.

Remaining repositories and GTK dependencies are Release; LSan disabled.
"""
from pathlib import Path
import os
import shlex
import subprocess

SOURCE=Path(__file__).resolve().parents[1]
ROOT=SOURCE.parent
OUT=ROOT/"build/asan-composition"
OUT.mkdir(exist_ok=True)
flags=shlex.split(subprocess.check_output(["pkg-config","--cflags","--libs","glib-2.0","pangocairo","uuid","sqlite3"],text=True))
names=("application/composition_service.cpp","persistence/composition_codec.cpp",
       "persistence/composition_store.cpp","persistence/recovery_codec.cpp",
       "persistence/sqlite_writing_repository.cpp","experimental/text_blocks_snapshot.cpp",
       "experimental/text_blocks.cpp","experimental/text_blocks_operations.cpp",
       "experimental/block_layers.cpp","experimental/block_materialization.cpp",
       "persistence/schema_migrator.cpp")
common=["c++","-std=c++20","-O1","-g","-UNDEBUG","-fsanitize=address,undefined","-fno-omit-frame-pointer","-I"+str(SOURCE/"include")]
objects=[]
for name in names:
    target=OUT/(name.replace("/","_")+".o")
    print("BUILD focal",name,flush=True)
    subprocess.run([*common,*flags,"-c",str(SOURCE/"src"/name),"-o",str(target)],check=True)
    objects.append(str(target))
binary=OUT/"inde_composition_tests"
subprocess.run([*common,str(SOURCE/"tests/composition_tests.cpp"),*objects,str(ROOT/"build/libinde_core.a"),*flags,"-pthread","-ldl","-o",str(binary)],check=True)
env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)],env=env,check=True)
print("PASS focal composition ASan/UBSan; 11 changed core units; dependencies Release; LSan disabled; not whole-app certification",flush=True)
