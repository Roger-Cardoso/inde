"""Record the agent's selected visual inspection, not automatic or human acceptance.

Run only after the calling agent has actually inspected the twelve selected images.
Functional GTK evidence is separate; a scripted stage name alone is not visual proof.
"""
from pathlib import Path
from hashlib import sha256
import argparse
import json

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--report",required=True,type=Path)
parser.add_argument("--output",required=True,type=Path)
args=parser.parse_args()
assert not args.output.exists()
r=json.loads(args.report.read_text())
root=Path(__file__).resolve().parents[2]
assert r["complete"] and r["scope"]=="temporal-context-ui"
assert sha256((root/"build/inde").read_bytes()).hexdigest()==r["binary_sha256"]
selected=(
 ("atlas-light","context-date-picker","Explicit scene choice, readable controls and footer"),
 ("atlas-light","context-perspective-date","Explicit reference role, no inferred presence"),
 ("atlas-dark","context-perspective-date","Dark counterpart, visible selected role and action"),
 ("atlas-light","context-shared-impact","Named final clock fields, impact list and visible save footer"),
 ("atlas-dark","context-shared-impact","Dark final clock fields, impact list and visible save footer"),
 ("empty-light-restart","context-restart-warning","Persisted interval and 12:30 precision, nonblocking warning and exception"),
 ("empty-dark","context-date-picker","Empty fixture, clear choice and optional note"),
 ("atlas-dark","context-interval-warning","Interval and precise sources explained, exception acknowledged"),
 ("empty-light","context-date-picker","Light empty counterpart and compact modal"),
 ("empty-dark-restart","context-restart-warning","Dark persisted interval counterpart and return action"),
 ("atlas-dark","context-interval-edit","Top fields and footer, overflow is scrollable content not clipped controls"),
 ("atlas-light","context-interval-edit","Light top counterpart, contrast and footer"))
frames=[]
for case,stage,note in selected:
    name=f"reforma-temporal-{case}-{stage}-{r['run']}.png"
    candidates=[Path(p) for p in r["screenshots"] if Path(p).name==name]
    assert len(candidates)==1
    p=candidates[0]
    frames.append({"path":str(p),"sha256":sha256(p.read_bytes()).hexdigest(),"assessment":note})
result={"reviewed_by_agent":True,"human_acceptance":"pending","binary_sha256":r["binary_sha256"],
        "visual_source_report":{"path":str(args.report.resolve()),"sha256":sha256(args.report.read_bytes()).hexdigest()},
        "screenshots":frames,"selected_count":len(frames),
        "excluded_mistimed_stage_names":["atlas-light/context-advisory","atlas-dark/context-advisory"],
        "limits":["Selected visual inspection only, not every captured frame", "Stage names are not proof of visual contents; mistimed advisory frames excluded", "No HiDPI, screen reader or prolonged writing-session certification"]}
args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
print("RECORDED",args.output,len(frames))
