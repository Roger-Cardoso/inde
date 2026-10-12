"""Compare the entire frozen predecessor source, including its untracked files."""
from hashlib import sha256
import json
from pathlib import Path
import subprocess
import tarfile

source = Path(__file__).resolve().parents[1]
root = source.parent
baseline = root.parents[1] / "outputs/inde-fonte-realizacao-continuidade-2026-10-11.tar.gz"
digest = lambda data: sha256(data).hexdigest()
assert digest(baseline.read_bytes()) == "e941c59ae41b5cf3de7637b4c6cca28517b26071983313e53dfe7f8d2745cc6f"
before = {}
with tarfile.open(baseline) as archive:
    for member in archive.getmembers():
        assert member.isfile() and member.name.startswith("inde/")
        name = member.name.removeprefix("inde/")
        assert ".." not in Path(name).parts
        before[name] = digest(archive.extractfile(member).read())
deleted = []
changed = []
for name, original in sorted(before.items()):
    path = source / name
    if not path.is_file():
        deleted.append(name)
        continue
    current = digest(path.read_bytes())
    if current != original:
        assert name.startswith(("src/ui/", "include/inde/ui/", "src/experimental/blocks_lab", "include/inde/experimental/blocks_lab", "tests/ui_")) or name in (
            "CMakeLists.txt", "tools/build_ui_workspace_smoke.py",
            "docs/PROJECT-CONTEXT.md", "docs/implementation-status.md", "docs/development-roadmap.md"
        ), "Unexpected change outside vocabulary scope: " + name
        changed.append(dict(path=name, before_sha256=original, after_sha256=current))
assert not deleted
files = set(filter(None, subprocess.check_output(
    ["git", "ls-files", "-co", "--exclude-standard", "-z"], cwd=source
).decode().split("\0")))
added = sorted(files - before.keys())
assert not any(v.startswith(("src/application/", "src/persistence/", "src/project/")) for v in added)
report = dict(baseline=str(baseline), baseline_sha256=digest(baseline.read_bytes()),
              baseline_files=len(before), changed_files=changed, added_files=added,
              deleted_files=deleted, core_and_persistence_unchanged=True,
              schema=28, covers_nonignored_untracked=True)
path = root / "checkpoint/vocabulary-preservation-final-2026-10-12.json"
path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
print("PASS preservation", len(before), "baseline files;", len(changed), "changed;", len(added), "added;", path)
