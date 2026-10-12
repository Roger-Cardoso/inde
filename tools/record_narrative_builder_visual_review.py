"""Record the agent's selected visual inspection, never human approval.

Run only after actually viewing the supplied frames. Does not evaluate pixels.
"""
from pathlib import Path
from hashlib import sha256
import argparse
import json
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--ui", required=True, type=Path)
parser.add_argument("--frame", action="append", required=True, type=Path)
parser.add_argument("--editor-ui", type=Path)
parser.add_argument("--editor-frame", action="append", default=[], type=Path)
parser.add_argument("--output", required=True, type=Path)
args = parser.parse_args()
report = json.loads(args.ui.read_text())
assert report["complete"] and report["scope"] in (
    "narrative-builder-production-ui", "narrative-alternatives-production-ui", "narrative-access-production-ui",
    "narrative-realization-production-ui")
assert len(args.frame) >= 4 and not args.output.exists()
source = Path(__file__).resolve().parents[1]
assert sha256((source.parent / "build/inde").read_bytes()).hexdigest() == report["binary_sha256"]
allowed = set(report["screenshots"])
if args.editor_ui:
    editor = json.loads(args.editor_ui.read_text())
    assert editor["complete"] and editor["scope"] == "access-editor-regressions"
    assert editor["binary_sha256"] == report["binary_sha256"]
    allowed.update(editor["screenshots"])
assert not args.editor_frame or args.editor_ui
frames = []
for path in [*args.frame, *args.editor_frame]:
    assert str(path.resolve()) in allowed
    frames.append({"path": str(path.resolve()), "sha256": sha256(path.read_bytes()).hexdigest()})
observations = [
      "Selected light writing and dark unit/creation/restart frames inspected",
      "Text and primary/secondary actions legible at observed 1200x760 window; no additional permanent side panel",
      "Creation modal has visible field and footer actions; unit details use body scrolling rather than footer overlap",
      "Restart frame keeps the working lens and expanded Document section; writing has the explicit planning-return action",
      "One hover tooltip is visible in the restart capture; it is not a modal or a permanent extra layer"]
if report["scope"] == "narrative-alternatives-production-ui":
    observations = [
      "Selected light/dark derivation, movement preview, comparison and line/restart frames inspected",
      "Sharing acknowledgement and derivation policy appear in the modal body with separate visible footer actions",
      "Comparison replaces the current working area, uses collapsed evidence and does not add a permanent side panel",
      "Narrative and line movement previews explicitly preserve other orders; warnings are advisory",
      "Text and actions legible in selected 1200x760 captures; detailed content uses scrolling",
      "Hover tooltips from the stationary QA pointer appear in selected modal/restart captures; not permanent panels or extra modal layers"]
if report["scope"] == "narrative-access-production-ui":
    observations = [
      "Selected empty-project setup, library, comparison and full-width detail frames inspected in light and dark themes",
      "First-work setup explains the universe and keeps invalid input, title fields and footer actions visible",
      "Reusable models appear only in the optional context panel; per-item actions replace the global toolbar clutter",
      "Comparison has a named source and selected target count; paired results occupy the working area",
      "Units, lines and links occupy alternate full-width sections without draggable dividers",
      "Compact layout and restarted view keep the chosen section; inspection is not human acceptance"]
    if args.editor_frame:
        observations += [
          "Selected light palette and dark HSV color frames inspected; controls and footer remain visible",
          "Color preview uses the actual editor paper theme; a low-contrast author-chosen color remains visibly low contrast, never silently changed",
          "Hover tooltips in selected frames belong to the stationary QA pointer; not permanent panels"]
if report["scope"]=="narrative-realization-production-ui":
    observations=[
      "Selected native light/dark unit, long Document selection and continuity frames inspected",
      "Realizations remain in an expandable unit section (expanded for QA); source/review actions use an overflow menu",
      "Continuity replaces the working area, with collapsed evidence, filtering and pagination; no permanent new side panel",
      "Document selection and return action are legible in inspected captures; automated assertions separately verify cursor/scroll",
      "Long occurrence selection/scroll and return are tested by assertions, not certified by a Document screenshot",
      "Conference starts compact, with technical identity and independent-prose options collapsed; the independence request reveals the explicit confirmation with visible footer actions",
      "Selected dark continuity/exception frames identify units by name and keep advisory evidence and author decisions separate",
      "Content extending below the working area's fold remains scrollable; these captures do not certify every control at every screen size",
      "Inspection is limited to supplied frames, not human acceptance or exhaustive accessibility certification"]
result = {"reviewed_by_agent": True, "human_acceptance": "pending", "binary_sha256": report["binary_sha256"],
    "screenshots": frames, "observations": observations,
    "limits": ["Selected visual inspection, not exhaustive UX, WCAG contrast measurement or human acceptance",
      "No certification of HiDPI, screen reader, IBus/IME, narrow screens or one-hour real writing session",
      "Automated data assertions, not screenshots, establish saving/order preservation"]}
args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print(args.output)
