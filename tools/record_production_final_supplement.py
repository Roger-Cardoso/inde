"""Preserve the completed strict continuity/production supplement separately."""
from hashlib import sha256
import json
from pathlib import Path


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    source = Path(__file__).resolve().parents[1]
    root = source.parent
    delivery = root.parents[1] / "outputs"
    expected = {f"{key}-{mode}{restart}" for key in ("atlas", "empty")
                for mode in ("continuity", "production") for restart in ("", "-restart")}
    reports = []
    for path in (root / "logs").glob("*/diagnostic.json"):
        report = json.loads(path.read_text())
        cases = {key for key in report["checks"] if key.startswith(("atlas-", "empty-"))}
        if report.get("stage") != "production" or cases != expected:
            continue
        if not report.get("origins_unchanged") or not all(v["passed"] for v in report["checks"].values()):
            continue
        if report["binary"]["sha256"] != digest(root / "build/inde"):
            continue
        if report["laboratory_binary"]["sha256"] != digest(root / "build/inde_text_blocks_lab"):
            continue
        reports.append((path, report))
    assert reports, "No completed final-code eight-case supplement"
    path, report = max(reports, key=lambda item: item[1]["run"])
    report["supplement_complete"] = True
    report["scope"] = "Eight strict continuity/production GTK cases, including restarts; separate from the previously completed 32-case full suite. Interrupted full repetition is not counted as complete."
    report["source_diagnostic"] = str(path)
    report["source_diagnostic_sha256"] = digest(path)
    report["continuity_driver_sha256"] = digest(root / "build/inde_ui_continuity_smoke")
    target = delivery / "reforma-producao-retomada-verificacao-2026-10-08.json"
    assert not target.exists()
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"passed": True, "cases": 8, "run": report["run"], "path": str(target)}))


if __name__ == "__main__":
    main()
