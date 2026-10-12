"""Focal ASan/UBSan: worldbuilding engine/service, search, schema, Save As and reversal.

Seven core units plus the test are instrumented. Other dependencies Release; LSan disabled.
"""
from pathlib import Path
import os
import shlex
import subprocess
import argparse

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--test",choices=("temporal_workflow","worldbuilding","temporal_system","temporal_templates","temporal_context"),default="worldbuilding")
parser.add_argument("--reuse-current-units",action="store_true",help="Reuse this tool's existing focal objects only when newer than source and all INDE headers")
args=parser.parse_args()

SOURCE=Path(__file__).resolve().parents[1]
ROOT=SOURCE.parent
OUT=ROOT/"build/asan-worldbuilding"
OUT.mkdir(exist_ok=True)
flags=shlex.split(subprocess.check_output(["pkg-config","--cflags","--libs","glib-2.0","pangocairo","uuid","sqlite3","icu-i18n"],text=True))
names=("project/worldbuilding.cpp","application/worldbuilding_service.cpp",
       "persistence/narrative_query_support.cpp","application/planning_service.cpp",
       "persistence/schema_migrator.cpp","persistence/project_database_repository.cpp",
       "persistence/project_operation_store.cpp","project/calendar_engines.cpp")
if args.test in ("temporal_context","temporal_workflow"):
    names += ("persistence/semantic_codec.cpp", "persistence/semantic_store.cpp",
              "project/semantic_registry.cpp", "project/semantic_validation.cpp",
              "application/text_semantic_service.cpp", "application/document_edit_session.cpp",
              "application/semantic_anchor_engine.cpp", "application/editorial_review.cpp")
if args.test=="temporal_workflow":
    names += ("persistence/sqlite_writing_repository.cpp", "application/editor_entity_creation.cpp",
              "persistence/calendar_navigation_store.cpp", "application/document_save_coordinator.cpp",
              "persistence/document_recovery_store.cpp")
common=["c++","-std=c++20","-O1","-g","-UNDEBUG","-fsanitize=address,undefined","-fno-omit-frame-pointer","-I"+str(SOURCE/"include")]
if args.test in ("temporal_context","temporal_workflow"):
    common=["-O0" if v=="-O1" else "-g1" if v=="-g" else v for v in common]
objects=[]
headers_mtime=max(p.stat().st_mtime_ns for folder in (SOURCE/"include",SOURCE/"src") for p in folder.rglob("*.hpp"))
for name in names:
    target=OUT/(name.replace("/","_")+".o")
    current=args.reuse_current_units and target.is_file() and not target.is_symlink() and target.stat().st_mtime_ns>=max(headers_mtime,(SOURCE/"src"/name).stat().st_mtime_ns)
    print("REUSE focal" if current else "BUILD focal",name,flush=True)
    if not current: subprocess.run([*common,*flags,"-c",str(SOURCE/"src"/name),"-o",str(target)],check=True)
    objects.append(str(target))
binary=OUT/("inde_"+args.test+"_tests")
subprocess.run([*common,str(SOURCE/("tests/"+args.test+"_tests.cpp")),*objects,str(ROOT/"build/libinde_core.a"),*flags,"-pthread","-ldl","-o",str(binary)],check=True)
env=dict(os.environ,ASAN_OPTIONS="detect_leaks=0:halt_on_error=1",UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
subprocess.run([str(binary)],env=env,check=True)
print("PASS focal "+args.test+" ASan/UBSan; "+str(len(names))+" core units; dependencies Release; LSan disabled; not whole-app certification",flush=True)
