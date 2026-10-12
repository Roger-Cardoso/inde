"""Freeze tested local executables and prepare fresh, non-overwritten review copies."""
from pathlib import Path
from hashlib import sha256
import json
import shutil
import subprocess
from reform_snapshots import snapshot

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"
PREVIOUS = Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-interacao-2026-10-04")
REPORT = DELIVERY / "reforma-verificacao-2026-10-07.json"
DARK_REPORT = DELIVERY / "reforma-verificacao-tema-escuro-2026-10-07.json"
CANDIDATE_COMMIT = "993c4e5a96c140a8b7c4578fabdd671a630bc97f"
MARKER = "INDE_REFORM_REVIEW_COPY_2026_10_07\n"


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    verified = json.loads(REPORT.read_text())
    assert verified["complete"] and verified["origins_unchanged"]
    assert all(check["passed"] for check in verified["checks"].values())
    assert sum(name.startswith(("atlas-", "empty-")) for name in verified["checks"]) == 14
    assert digest(ROOT / "build/inde") == verified["binary"]["sha256"]
    assert digest(ROOT / "build/inde_text_blocks_lab") == verified["laboratory_binary"]["sha256"]
    dark = json.loads(DARK_REPORT.read_text())
    assert dark["origins_unchanged"] and len(dark["checks"]) == 4
    assert all(check["passed"] for check in dark["checks"].values())
    assert dark["binary"]["sha256"] == verified["binary"]["sha256"]
    assert dark["laboratory_binary"]["sha256"] == verified["laboratory_binary"]["sha256"]
    ctest = ROOT / "logs/ctest-reforma.log"
    assert "100% tests passed" in ctest.read_text() and "out of 16" in ctest.read_text()
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=SOURCE, text=True)
    assert not status.strip(), "Commit the preserved candidate/reform before sealing sources"
    manifest = {"technical_verification_passed": True, "human_acceptance": "pending",
                "whole_reform_complete": False, "published": False, "source": str(SOURCE),
                "branch": subprocess.check_output(["git", "branch", "--show-current"], cwd=SOURCE, text=True).strip(),
                "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True).strip(),
                "binaries": {}, "review_copies": [], "verification": str(REPORT),
                "supplemental_dark_verification": str(DARK_REPORT),
                "preserved_candidate_commit": CANDIDATE_COMMIT, "source_artifacts": {},
                "schema": 18, "semantic_payload": 2, "checkpoint_payload": 4, "clipboard_payload": 2}
    assert subprocess.check_output(["git", "rev-parse", "HEAD^"], cwd=SOURCE, text=True).strip() == CANDIDATE_COMMIT
    for name in ("inde-reforma-2026-10-07.bundle", "inde-reforma-2026-10-07.patch"):
        artifact = DELIVERY / name
        assert artifact.is_file()
        manifest["source_artifacts"][name] = {"path": str(artifact), "sha256": digest(artifact)}
    for key, built, delivered in (("inde", "inde", "inde-reforma-2026-10-07"),
                                  ("laboratory", "inde_text_blocks_lab", "inde-laboratorio-reforma-2026-10-07")):
        target = DELIVERY / delivered
        assert not target.exists(), "Do not overwrite a delivered executable"
        shutil.copy2(ROOT / "build" / built, target)
        target.chmod(0o750)
        manifest["binaries"][key] = {"path": str(target), "sha256": digest(target)}
    for key, name in (("atlas", "Atlas de Véspera — Projeto Integral.inde"),
                      ("empty", "Projeto vazio — sem exemplos.inde")):
        copy = ROOT / "review" / name
        item = snapshot(PREVIOUS / "review" / name, copy)
        (copy / "REFORM_REVIEW_COPY").write_text(MARKER)
        item["key"] = key
        manifest["review_copies"].append(item)
    checkpoint = ROOT / "checkpoint/candidato-editor-antes-da-reforma.tar.gz"
    target_checkpoint = DELIVERY / "inde-candidato-antes-da-reforma-2026-10-07.tar.gz"
    assert not target_checkpoint.exists()
    shutil.copy2(checkpoint, target_checkpoint)
    manifest["preservation_checkpoint"] = {"path": str(target_checkpoint), "sha256": digest(target_checkpoint),
        "original_candidate": str(PREVIOUS / "inde"), "original_base": "f155bd866d32c9326e5853a2882e6adbfca6ba23",
        "original_tracked_changes": 40, "original_untracked": 204, "original_total": 244}
    shutil.copy2(ctest, DELIVERY / "reforma-testes-nucleo-2026-10-07.log")
    shutil.copy2(SOURCE / "docs/reform-2026-10-07.md", DELIVERY / "reforma-implementacao-2026-10-07.md")
    (DELIVERY / "reforma-entrega-2026-10-07.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(manifest, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
