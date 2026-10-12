"""Compare against the accepted vocabulary source, including untracked files."""
import hashlib,json,subprocess,tarfile
from pathlib import Path
source=Path(__file__).resolve().parents[1];out=source.parent.parents[1]/'outputs'
archive=out/'inde-fonte-vocabulario-autoral-2026-10-12.tar.gz'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
expected={
 'inde-reforma-vocabulario-autoral-2026-10-12':'808c6f985f98db9e62c1cadad641d59074e8db8669c821c41c6fdadee8bb6d7f',
 'inde-laboratorio-vocabulario-autoral-2026-10-12':'89c365e31d12921a5cd9c7a606821aae045578ace1b01675769f84dff518ba15',
 'inde-fonte-vocabulario-autoral-2026-10-12.tar.gz':'6b3a288be8eba839e98ed81f14029114438857f54c2ff00a5628e5c6da596df0'}
for file,sha in expected.items():assert digest(out/file)==sha,'Accepted sealed delivery changed: '+file
changed=[];deleted=[];checked=0
with tarfile.open(archive,'r:gz') as tar:
    for member in tar.getmembers():
        if not member.isfile():continue
        rel=Path(member.name).relative_to('inde');path=source/rel;checked+=1
        if not path.is_file():deleted.append(str(rel));continue
        if hashlib.sha256(tar.extractfile(member).read()).hexdigest()!=digest(path):changed.append(str(rel))
assert not deleted,deleted
immutable=('src/application/','src/project/','src/persistence/','include/inde/application/','include/inde/project/','include/inde/persistence/')
assert not any(p.startswith(immutable) for p in changed),'Core/data/schema modifications detected'
report=dict(passed=True,accepted_deliveries_preserved=True,core_persistence_schema_unchanged=True,baseline_files=checked,changed_files=changed,deleted_files=deleted)
subprocess.run(['git','diff','--check'],cwd=source,check=True)
print(json.dumps(report,ensure_ascii=False,indent=2))
