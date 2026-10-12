"""Seal the organization block without replacing the first reform delivery."""
from hashlib import sha256
from pathlib import Path
import json
import shutil
import subprocess
from reform_snapshots import snapshot

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"
PREFIX = "reforma-organizacao"
BASE = "6c3bc27858fb8f7237d269390232ac921288b92b"
MARKER = "INDE_ORGANIZATION_REVIEW_COPY_2026_10_07\n"


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(["git", *args], cwd=SOURCE, text=True).strip()


def main():
    report = DELIVERY / (PREFIX + "-verificacao-2026-10-07.json")
    dark_report = DELIVERY / (PREFIX + "-tema-escuro-2026-10-07.json")
    verified, dark = json.loads(report.read_text()), json.loads(dark_report.read_text())
    assert verified["stage"] == "organization" and verified["complete"] and verified["origins_unchanged"]
    assert sum(name.startswith(("atlas-", "empty-")) for name in verified["checks"]) == 20
    assert all(check["passed"] for check in verified["checks"].values())
    assert dark["stage"] == "organization" and dark["origins_unchanged"] and len(dark["checks"]) == 6
    assert all(check["passed"] for check in dark["checks"].values())
    for key, binary in (("binary", "inde"), ("laboratory_binary", "inde_text_blocks_lab")):
        assert digest(ROOT / "build" / binary) == verified[key]["sha256"] == dark[key]["sha256"]
    ctest = ROOT / "logs/ctest-organizacao.log"
    assert "100% tests passed" in ctest.read_text() and "out of 16" in ctest.read_text()
    assert not git("status", "--porcelain"), "Seal only a clean committed source"
    assert git("rev-parse", "HEAD^") == BASE
    previous_path = DELIVERY / "reforma-entrega-2026-10-07.json"
    previous = json.loads(previous_path.read_text())
    for binary in previous["binaries"].values():
        assert digest(Path(binary["path"])) == binary["sha256"], "First delivery changed"
    binaries = (("inde", "inde", "inde-reforma-organizacao-2026-10-07"),
                ("laboratory", "inde_text_blocks_lab", "inde-laboratorio-organizacao-2026-10-07"))
    reviews = (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
               ("empty", "Projeto vazio — sem exemplos.inde"))
    target_manifest = DELIVERY / (PREFIX + "-entrega-2026-10-07.json")
    assert not target_manifest.exists()
    assert all(not (DELIVERY / name).exists() for _, _, name in binaries)
    assert all(not (ROOT / "review-organizacao" / name).exists() for _, name in reviews)
    manifest = {"technical_verification_passed": True, "human_acceptance": "pending",
                "whole_reform_complete": False, "published": False, "stage": "organization",
                "source": str(SOURCE), "branch": git("branch", "--show-current"), "commit": git("rev-parse", "HEAD"),
                "predecessor_commit": BASE, "predecessor_delivery": str(previous_path),
                "verification": str(report), "supplemental_dark_verification": str(dark_report),
                "binaries": {}, "review_copies": [], "source_artifacts": {},
                "schema": 18, "semantic_payload": 2, "checkpoint_payload": 4, "clipboard_payload": 2,
                "preservation_checkpoint": previous["preservation_checkpoint"]}
    for name in ("inde-reforma-organizacao-2026-10-07.bundle", "inde-reforma-organizacao-2026-10-07.patch"):
        path = DELIVERY / name
        assert path.is_file()
        manifest["source_artifacts"][name] = {"path": str(path), "sha256": digest(path)}
    for key, binary, name in binaries:
        path = DELIVERY / name
        shutil.copy2(ROOT / "build" / binary, path); path.chmod(0o750)
        manifest["binaries"][key] = {"path": str(path), "sha256": digest(path)}
    for key, name in reviews:
        origin = ROOT / "review" / name
        path = ROOT / "review-organizacao" / name
        review = snapshot(origin, path)
        (path / "ORGANIZATION_REVIEW_COPY").write_text(MARKER)
        review["key"] = key
        manifest["review_copies"].append(review)
    shutil.copy2(ctest, DELIVERY / (PREFIX + "-testes-nucleo-2026-10-07.log"))
    shutil.copy2(SOURCE / "docs/reform-organization-2026-10-07.md", DELIVERY / (PREFIX + "-implementacao-2026-10-07.md"))
    target_manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
