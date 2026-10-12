"""Merge four complete isolated workflow cases, preserving all parent evidence."""
import argparse
from datetime import datetime
from hashlib import sha256
import json
from pathlib import Path
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("reports",nargs=4,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent
digest=lambda path:sha256(path.read_bytes()).hexdigest()
parents=[json.loads(p.read_text()) for p in args.reports]
checks={};screenshots=[];cases=set()
for r in parents:
    assert r["complete"] and r["scope"]=="temporal-workflow-ui" and r["origins_unchanged"]
    assert not r["focal_sanitizers"] and len(r["selected_cases"])==1
    case=r["selected_cases"][0]
    assert case not in cases and set(r["checks"])=={case,case+"-restart"}
    assert all(v["passed"] for v in r["checks"].values())
    assert r["binary_sha256"]==digest(root/"build/inde")
    assert r["driver_sha256"]==digest(root/"build/inde_ui_temporal_workflow_smoke")
    cases.add(case);checks.update(r["checks"]);screenshots.extend(r["screenshots"])
assert cases=={"atlas-light","atlas-dark","empty-light","empty-dark"}
merged=dict(parents[0],run="merged-"+datetime.now().strftime("%Y%m%d-%H%M%S-%f"),
    checks=checks,screenshots=screenshots,selected_cases=sorted(cases),
    merged_independent_cases=True,
    parent_reports=[dict(path=str(p.resolve()),sha256=digest(p)) for p in args.reports],
    limits=["Four independently isolated fixture/theme runs, each with restart; not human acceptance"])
path=root/"logs"/("temporal-"+merged["run"])/"report.json"
assert not path.exists()
path.parent.mkdir()
path.write_text(json.dumps(merged,ensure_ascii=False,indent=2)+"\n")
print("PASS complete merged workflow matrix",path)
