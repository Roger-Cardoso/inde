"""Promote only the exact sixteen passing supplemental dark cases, not a full QA claim."""
from hashlib import sha256
import json
from pathlib import Path

root=Path(__file__).resolve().parents[2]
delivery=root.parents[1]/"outputs"
expected={key+"-"+mode+"-dark" for key in ("atlas","empty") for mode in ("production","editorial_review","continuity","editor_interaction","text_workshop")}
expected|={key+"-"+mode+"-dark-restart" for key in ("atlas","empty") for mode in ("production","editorial_review","continuity")}
candidates=[]
for path in sorted((root/"logs").glob("*/diagnostic.json")):
    report=json.loads(path.read_text())
    cases={name for name in report.get("checks",{}) if name.startswith(("atlas-","empty-"))}
    if report.get("stage")=="production" and cases==expected and report.get("origins_unchanged") and all(c["passed"] for c in report["checks"].values()):candidates.append((path,report))
assert candidates
path,report=candidates[-1]
for key,name in (("binary","inde"),("laboratory_binary","inde_text_blocks_lab")):
    assert sha256((root/"build"/name).read_bytes()).hexdigest()==report[key]["sha256"],"Dark evidence must use exact final binaries"
report["supplemental_verification_complete"]=True
report["scope"]="Sixteen focused initial-dark GTK cases, not the full thirty-two-case suite"
report["diagnostic_origin"]=str(path)
target=delivery/"reforma-producao-tema-escuro-2026-10-08.json"
assert not target.exists()
target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
print(json.dumps({"passed":True,"cases":16,"run":report["run"],"path":str(target)}))
