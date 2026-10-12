"""Read-only compare every non-ignored candidate source file with the preservation archive."""
import hashlib
import json
from pathlib import Path
import subprocess
import tarfile
import argparse

source=Path("/home/roger/Documents/Codex/2026-10-02/analise-o-chat/work/editor-interacao-2026-10-04/inde")
root=Path(__file__).resolve().parents[2]
delivery=root.parents[1]/"outputs"
archive=delivery/"inde-candidato-antes-da-reforma-2026-10-07.tar.gz"
names=subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0")
names=sorted(set(n for n in names if n))
checked=0
with tarfile.open(archive,"r:gz") as tar:
    for name in names:
        member=tar.getmember("inde/"+name)
        path=source/name
        assert member.isfile() and path.is_file() and not path.is_symlink(),name
        with path.open("rb") as current,tar.extractfile(member) as saved:
            assert hashlib.file_digest(current,"sha256").digest()==hashlib.file_digest(saved,"sha256").digest(),name
        checked+=1
original_bin=source.parent/"artifacts/inde-editor-projeto"
assert hashlib.sha256(original_bin.read_bytes()).hexdigest()=="27e9f18bfe132b16a4320413739b6064711f4e374ef006d72a7ae8a789ade2bb"
assert subprocess.check_output(["git","rev-parse","HEAD"],cwd=source,text=True).strip()=="f155bd866d32c9326e5853a2882e6adbfca6ba23"
entries=subprocess.check_output(["git","status","--porcelain"],cwd=source,text=True).splitlines()
assert len(entries)==244 and sum(e.startswith("??") for e in entries)==204
report={"complete":True,"original_source_unchanged":True,"nonignored_files_checked":checked,"source":str(source),"historical_status_entries":{"tracked_changes":40,"untracked_entries":204,"total_entries":244},"original_binary_sha256":hashlib.sha256(original_bin.read_bytes()).hexdigest(),"checkpoint_sha256":hashlib.sha256(archive.read_bytes()).hexdigest(),"scope":"Byte comparison of all nonignored source files; status untracked directories count as entries, not individual files"}
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output",type=Path,default=delivery/"reforma-producao-preservacao-verificacao-2026-10-08.json")
path=parser.parse_args().output
assert not path.exists()
path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
print(json.dumps(report,ensure_ascii=False))
