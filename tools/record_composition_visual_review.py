"""Record the explicit final frames inspected by the agent, not a human UX approval."""
from hashlib import sha256
import json
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"

def digest(path):
    return sha256(path.read_bytes()).hexdigest()

def main():
    target = DELIVERY / "reforma-composicao-visual-verificacao-2026-10-08.json"
    assert not target.exists(), "Never overwrite a previous inspection"
    dark = json.loads((DELIVERY / "reforma-composicao-tema-escuro-final-2026-10-08.json").read_text())
    binary_hash = digest(ROOT / "build/inde")
    assert binary_hash == dark["binary"]["sha256"]
    frames = []
    groups = (
        ("atlas-editor_fixes", "light", ("context-selection", "dictionary-selection", "inline-bottom")),
        ("empty-editor_fixes-dark", "dark", ("context-selection", "dictionary-selection", "inline-bottom")),
        ("atlas-composition", "light", ("composition-output",)),
        ("atlas-composition-restart", "light", ("composition-recovered-draft",)),
        ("atlas-composition-dark", "dark", ("composition-order", "composition-draft", "composition-origin", "composition-reading", "composition-output")),
        ("atlas-composition-dark-restart", "dark", ("composition-recovered-draft",)),
    )
    for group, theme, names in groups:
        for name in names:
            path = DELIVERY / f"reforma-composicao-{group}-{name}.png"
            assert path.is_file()
            frames.append({"path": str(path), "sha256": digest(path), "theme": theme, "view": name})
    assert len(frames) == 14
    result = {
        "complete": True, "reviewed_by_agent": True, "stage": "composition",
        "human_acceptance": "pending", "binary_sha256": binary_hash,
        "screenshots": frames,
        "qa_runs": {"inspected_light": str(ROOT / "logs/20261008-192926"), "dark": str(ROOT / "logs/20261008-193236")},
        "light_run_status": "Interrupted by the environment after 41 passed scenarios; the inspected Atlas scenes passed. Full verification is sealed separately from two completed disjoint shards.",
        "excluded_transition_frames": "The two early inline-top captures were inspected but exclude the not-yet-painted footer and are not certified as final visual evidence. Settled inline-bottom frames and actual widget bounds verify reachability.",
        "observations": [
            "Selection remains highlighted above the context menu; no oversized painted backing surface.",
            "Dictionary opens with the selected query and a readable fixed footer; empty exact-query results are stated, not replaced by a guessed sense.",
            "Inline models scroll independently of the close footer; the last management command is wholly reachable. Long labels ellipsize, with full tooltip and accessible label.",
            "Composition uses one exclusive task page, readable in light and dark themes, with explicit draft/apply and frozen-origin authority.",
            "Output acknowledgement is visible and unselected; warning contents scroll inside their bounded area.",
            "Actual process restart exposes a recovered draft and explicit Apply/Discard choices.",
            "Reading and output concatenate authoritative block text exactly, without silently injecting separators; these are declared textual, not WYSIWYG previews.",
        ],
        "limits": "Visual review of private Xvfb captures at a 1000x720 app viewport; not physical-display/HiDPI, IME, assistive-technology or human usability certification. Black outside the app is the unused Xvfb desktop.",
    }
    target.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print("RECORDED", len(frames), "explicitly inspected final frames", binary_hash)

if __name__ == "__main__":
    main()
