"""Merge four complete bounded temporal QA reports, never partial runs."""
from pathlib import Path
from hashlib import sha256
import argparse
import json

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("reports", nargs=4, type=Path)
args = parser.parse_args()
assert not args.output.exists(), "Do not overwrite an earlier evidence record"
parts = [json.loads(p.read_text()) for p in args.reports]
expected = {"atlas-light", "atlas-dark", "empty-light", "empty-dark"}
assert {p["selected_cases"][0] for p in parts} == expected
first = parts[0]
checks = {}
screenshots = []
for part in parts:
    assert part["complete"] and part["origins_unchanged"] and len(part["selected_cases"]) == 1
    assert len(part["checks"]) == 2
    for key in ("scope", "focal_sanitizers", "binary_sha256", "driver_sha256"):
        assert part[key] == first[key], "Different candidate, driver or mode"
    case = part["selected_cases"][0] + ("-asan" if part["focal_sanitizers"] else "")
    assert set(part["checks"]) == {case, case + "-restart"}
    for key, check in part["checks"].items():
        assert key not in checks and check["passed"] and check["exit_code"] == 0
        checks[key] = check
    screenshots.extend(part["screenshots"])
source = Path(__file__).resolve().parents[1]
build = source.parent / "build"
assert sha256((build / "inde").read_bytes()).hexdigest() == first["binary_sha256"]
drivers = {
    "removal-assistant-ui": "inde_ui_removal_assistant_smoke",
    "temporal-workflow-ui": "inde_ui_temporal_workflow_smoke",
    "narrative-builder-production-ui": "inde_ui_narrative_builder_smoke",
    "narrative-alternatives-production-ui": "inde_ui_narrative_alternatives_smoke",
    "narrative-access-production-ui": "inde_ui_narrative_access_smoke",
    "narrative-realization-production-ui": "inde_ui_narrative_realization_smoke",
}
assert first["scope"] in drivers
driver = build / (drivers[first["scope"]] + ("_asan" if first["focal_sanitizers"] else ""))
assert sha256(driver.read_bytes()).hexdigest() == first["driver_sha256"]
result = dict(first, complete=True, checks=checks, screenshots=screenshots,
              run="bounded-complete-cases", selected_cases=sorted(expected),
              parts=[{"path": str(p.resolve()), "sha256": sha256(p.read_bytes()).hexdigest()} for p in args.reports])
args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("MERGED_COMPLETE", args.output, len(checks), "checks")
