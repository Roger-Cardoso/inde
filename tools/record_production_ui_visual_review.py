"""Record an explicit agent review of every page of the actual UI-produced PDFs."""
import argparse
from hashlib import sha256
import json
from pathlib import Path


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--confirm-agent-review", action="store_true", required=True)
    parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    root = source.parent
    delivery = root.parents[1] / "outputs"
    evidence = delivery / "reforma-producao-ui-formatos-verificacao-2026-10-08.json"
    formats = json.loads(evidence.read_text())
    assert formats["complete"]
    assert digest(root / "build/inde") == formats["binary"]["sha256"]
    artifacts = []
    for key in ("atlas", "empty"):
        assert formats["checks"][key]["passed"]
        pdf = Path(formats["checks"][key]["folder"]) / "obra.pdf"
        pages = sorted((root / "qa-producao-ui-visual-2026-10-08" / key).glob("page-*.png"))
        assert [p.name for p in pages] == ["page-1.png", "page-2.png"]
        artifacts.append({"path": str(pdf), "sha256": digest(pdf), "pages": [
            {"path": str(page), "sha256": digest(page), "agent_visually_reviewed": True}
            for page in pages]})
    target = delivery / "reforma-producao-ui-visual-verificacao-2026-10-08.json"
    assert not target.exists()
    result = {"complete": True, "human_acceptance": "pending",
              "reviewed_all_final_pages": True, "reviewed_page_count": 4,
              "binary": formats["binary"], "source_evidence": str(evidence),
              "source_evidence_sha256": digest(evidence), "artifacts": artifacts,
              "observations": "All four pages inspected: readable accents and emoji, correct source order and inline styles, no clipping or overlap. This is visual QA, not PDF/UA or universal-reader certification."}
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"passed": True, "pages": 4, "path": str(target)}))


if __name__ == "__main__":
    main()
