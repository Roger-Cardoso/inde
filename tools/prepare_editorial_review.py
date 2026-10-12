"""Seal editorial review independently, only after final-code QA and a clean local commit."""
from hashlib import sha256
import json
from pathlib import Path
import shutil
import subprocess
from reform_snapshots import snapshot

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"
BASE = "1c5286fe12edd91014aef41d4c07352f31ddddce"
PREFIX = "reforma-revisao"

def digest(path):
    return sha256(path.read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(["git", *args], cwd=SOURCE, text=True).strip()

def main():
    report_path = DELIVERY / (PREFIX + "-verificacao-2026-10-07.json")
    dark_path = DELIVERY / (PREFIX + "-tema-escuro-2026-10-07.json")
    report, dark = json.loads(report_path.read_text()), json.loads(dark_path.read_text())
    assert report["stage"] == "editorial" and report["complete"] and report["origins_unchanged"]
    assert sum(n.startswith(("atlas-", "empty-")) for n in report["checks"]) == 28
    assert dark["stage"] == "editorial" and dark["origins_unchanged"]
    assert sum(n.startswith(("atlas-", "empty-")) for n in dark["checks"]) == 12
    assert all(c["passed"] for r in (report, dark) for c in r["checks"].values())
    assert report["checks"]["export-readback"]["passed"]
    assert report["checks"]["focal-sanitizers"]["passed"]
    assert report["checks"]["editorial-focal-sanitizers"]["passed"]
    for key, binary in (("binary", "inde"), ("laboratory_binary", "inde_text_blocks_lab")):
        assert digest(ROOT / "build" / binary) == report[key]["sha256"] == dark[key]["sha256"]
    ctest = ROOT / "logs/ctest-revisao.log"
    assert "100% tests passed" in ctest.read_text() and "out of 18" in ctest.read_text()
    assert not git("status", "--porcelain") and git("rev-parse", "HEAD^") == BASE
    acceptance_path = DELIVERY / "reforma-revisao-aceite-anterior-2026-10-07.json"
    acceptance = json.loads(acceptance_path.read_text())
    assert acceptance["human_acceptance"] == "accepted" and acceptance["next_implementation_authorized"]
    assert BASE in {a["commit"] for a in acceptance["accepted_deliveries"]}
    prior_path = DELIVERY / "reforma-continuidade-entrega-2026-10-07.json"
    prior = json.loads(prior_path.read_text())
    for name in ("reforma-entrega-2026-10-07.json", "reforma-organizacao-entrega-2026-10-07.json", "reforma-continuidade-entrega-2026-10-07.json"):
        old = json.loads((DELIVERY / name).read_text())
        assert all(digest(Path(a["path"])) == a["sha256"] for a in old["binaries"].values())
    manifest_path = DELIVERY / (PREFIX + "-entrega-2026-10-07.json")
    binaries = (("inde", "inde", "inde-reforma-revisao-2026-10-07"),
                ("laboratory", "inde_text_blocks_lab", "inde-laboratorio-revisao-2026-10-07"))
    reviews = (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
               ("empty", "Projeto vazio — sem exemplos.inde"))
    assert not manifest_path.exists()
    assert all(not (DELIVERY / name).exists() for _, _, name in binaries)
    assert all(not (ROOT / "review-revisao" / name).exists() for _, name in reviews)
    manifest = {"technical_verification_passed": True, "human_acceptance": "pending",
                "whole_reform_complete": False, "published": False, "stage": "editorial_review",
                "source": str(SOURCE), "branch": git("branch", "--show-current"), "commit": git("rev-parse", "HEAD"),
                "predecessor_commit": BASE, "predecessor_delivery": str(prior_path),
                "accepted_predecessors": acceptance["accepted_deliveries"],
                "human_acceptance_record": {"path": str(acceptance_path), "sha256": digest(acceptance_path)},
                "verification": str(report_path), "supplemental_dark_verification": str(dark_path),
                "binaries": {}, "review_copies": [], "source_artifacts": {},
                "schema": 19, "semantic_payload": 2, "checkpoint_payload": 4, "clipboard_payload": 2,
                "preservation_checkpoint": prior["preservation_checkpoint"],
                "limits": ["Comparison is textual readonly, not merge or immutable history", "Visual comparison: 2 MiB combined text, no truncation", "Suggestion fields: 4096 UTF-8 bytes each", "Independent revision does not copy anchors/bindings/semantics", "Whole-work output and persistent composition remain pending", "Focal sanitizers only, Release dependencies, LSan disabled"]}
    for name in ("inde-reforma-revisao-2026-10-07.bundle", "inde-reforma-revisao-2026-10-07.patch"):
        path = DELIVERY / name
        assert path.is_file()
        manifest["source_artifacts"][name] = {"path": str(path), "sha256": digest(path)}
    for key, source, name in binaries:
        path = DELIVERY / name
        shutil.copy2(ROOT / "build" / source, path);path.chmod(0o750)
        manifest["binaries"][key] = {"path": str(path), "sha256": digest(path)}
    for key, name in reviews:
        origin = ROOT / "review-continuidade" / name
        target = ROOT / "review-revisao" / name
        review = snapshot(origin, target)
        (target / "EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
        subprocess.run([str(ROOT / "build/inde_prepare_editorial_review_copy"),str(target)],check=True)
        review["key"] = key;manifest["review_copies"].append(review)
    shutil.copy2(ctest, DELIVERY / (PREFIX + "-testes-nucleo-2026-10-07.log"))
    shutil.copy2(SOURCE / "docs/reform-editorial-review-2026-10-07.md", DELIVERY / (PREFIX + "-implementacao-2026-10-07.md"))
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))

if __name__ == "__main__": main()
