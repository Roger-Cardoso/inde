"""Require the exact 46-case GTK regression or 20-case dark matrix from completed shards."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
DELIVERY=ROOT.parents[1]/"outputs"
MODES=("reform","organization","explorer","relations","editing","shell","editor_interaction","toolbar","color","text_workshop","continuity","editorial_review","production","operations","composition","editor_fixes","worldbuilding")
RESTART=("continuity","editorial_review","production","operations","composition","worldbuilding")
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("shards",nargs="+",type=Path)
parser.add_argument("--theme",choices=("dark",))
args=parser.parse_args()
if args.theme:
    MODES=("organization","editor_interaction","color","operations","composition","editor_fixes","worldbuilding")
expected={key+"-"+mode+("-"+args.theme if args.theme else "")+suffix for key in ("atlas","empty") for mode in MODES for suffix in (("","-restart") if mode in RESTART else ("",))}
result={"complete":False,"stage":"worldbuilding","theme":args.theme,"checks":{},"screenshots":[],"shards":[],"human_acceptance":"pending"}
for path in args.shards:
    r=json.loads(path.read_text())
    assert r["stage"]=="worldbuilding" and r["origins_unchanged"] and r["theme"]==args.theme
    assert not r["worldbuilding_ui_focal_sanitizers"]
    assert r["g_debug"]=="fatal-warnings"
    for name,c in r["checks"].items():
        assert c["passed"] and Path(c["log"]).is_file()
        if name in expected:
            assert name not in result["checks"],"Duplicate coverage"
            result["checks"][name]=c
    for field in ("binary","laboratory_binary","driver_sha256"):
        if field in result:assert result[field]==r[field],"Build changed between shards"
        else:result[field]=r[field]
    result["screenshots"]+=r["screenshots"]
    result["shards"].append({"path":str(path),"sha256":sha256(path.read_bytes()).hexdigest()})
assert set(result["checks"])==expected,expected-set(result["checks"])
for key in ("binary","laboratory_binary"):
    assert sha256(Path(result[key]["path"]).read_bytes()).hexdigest()==result[key]["sha256"]
for mode,h in result["driver_sha256"].items():
    assert sha256((ROOT/"build"/("inde_ui_"+mode+"_smoke")).read_bytes()).hexdigest()==h
result.update(complete=True,origins_unchanged=True,build_type="Release",ui_scenarios=len(expected),g_debug="fatal-warnings",scope="Completed disjoint disposable-fixture shards with exact union of all required scenarios; interrupted runs and subsets never accepted as full verification")
destination=DELIVERY/("reforma-mundo-tema-escuro-final-2026-10-10.json" if args.theme else "reforma-mundo-verificacao-2026-10-10.json")
assert not destination.exists()
destination.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("PASS",len(expected),"actual GTK scenarios, final binaries/drivers identical")
