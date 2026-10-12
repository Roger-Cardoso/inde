"""Seal only a technically verified private alternatives candidate; never approval."""
from pathlib import Path
from hashlib import sha256
from collections import Counter
import argparse
import json
import os
import shutil
import sqlite3
import subprocess
import tarfile
from reform_snapshots import snapshot
parser=argparse.ArgumentParser(description=__doc__)
for key in ("ui","ui-asan","builder-regression","temporal-regression","startup","ctest","debug","asan","visual","preservation"):
    parser.add_argument("--"+key,required=True,type=Path)
args=parser.parse_args()
source=Path(__file__).resolve().parents[1];root=source.parent;delivery=root.parents[1]/"outputs"
def digest(p): return sha256(p.read_bytes()).hexdigest()
def git(*args): return subprocess.check_output(["git",*args],cwd=source,text=True).strip()
assert git("rev-parse","HEAD")=="995fbb27f8ee9359643a90f0220bc71269ef5e9a"
subprocess.run(["git","diff","--check"],cwd=source,check=True)
binary=digest(root/"build/inde")
for path,scope,driver in ((args.ui,"narrative-alternatives-production-ui","inde_ui_narrative_alternatives_smoke"),(args.ui_asan,"narrative-alternatives-production-ui","inde_ui_narrative_alternatives_smoke_asan"),(args.builder_regression,"narrative-builder-production-ui","inde_ui_narrative_builder_smoke"),(args.temporal_regression,"temporal-workflow-ui","inde_ui_temporal_workflow_smoke")):
    r=json.loads(path.read_text());assert r["complete"] and r["origins_unchanged"] and r["scope"]==scope
    assert len(r["checks"])==8 and all(c["passed"] for c in r["checks"].values())
    assert r["binary_sha256"]==binary and r["driver_sha256"]==digest(root/"build"/driver)
assert "100% tests passed" in args.ctest.read_text() and "out of 30" in args.ctest.read_text()
for p,end in ((args.debug,"PASS focal builder Debug"),(args.asan,"PASS focal builder ASan/UBSan")):
    t=p.read_text();assert "PASS alternatives:" in t and end in t
    assert not any(s in t for s in ("AddressSanitizer", "runtime error:", "Traceback", "FAIL"))
r=json.loads(args.startup.read_text());assert r["complete"] and r["scope"]=="startup-only"
for name in ("inde","inde_text_blocks_lab"): assert r["checks"][name]["passed"] and r["checks"][name]["binary_sha256"]==digest(root/"build"/name)
v=json.loads(args.visual.read_text());assert v["reviewed_by_agent"] and v["binary_sha256"]==binary
for frame in v["screenshots"]:assert digest(Path(frame["path"]))==frame["sha256"]
assert json.loads(args.preservation.read_text())["original_source_unchanged"]
previous=sorted(delivery.glob("reforma*-entrega-*.json"))
for path in previous:
    m=json.loads(path.read_text())
    for b in m["binaries"].values(): assert digest(Path(b["path"]))==b["sha256"]
    for a in m.get("source_artifacts",{}).values(): assert digest(Path(a["path"]))==a["sha256"]
manifest=delivery/"reforma-alternativas-narrativas-entrega-2026-10-11.json"
review=root/"review-alternativas-narrativas";config=root/"review-alternativas-narrativas-config"
assert not manifest.exists() and not review.exists() and not config.exists()
evidence=[Path(v) for v in vars(args).values()]+[source/"docs/narrative-alternatives-2026-10-11.md",delivery/"reforma-alternativas-narrativas-implementacao-2026-10-11.md",delivery/"inde-alternativas-narrativas-abrir.sh"]
assert all(p.is_file() for p in evidence)
for target in ("inde-reforma-alternativas-narrativas-2026-10-11", "inde-laboratorio-alternativas-narrativas-2026-10-11", "inde-fonte-alternativas-narrativas-2026-10-11.tar.gz"):
    assert not (delivery/target).exists(), "Preserve earlier candidate: "+target
result={"stage":"narrative_builder_alternatives","schema":27,"technical_verification_passed":True,"human_acceptance":"pending","previous_human_acceptance":"not_inferred_from_continue","continuation_authorized_by":"User: prossiga","published":False,"whole_reform_complete":False,"all_accepted_proposals_complete":False,"source":str(source),"branch":git("branch","--show-current"),"base_commit":git("rev-parse","HEAD"),"source_uncommitted":True,"source_status":git("status","--porcelain"),"binaries":{},"source_artifacts":{},"review_copies":[],"previous_frozen_deliveries_preserved":[str(p) for p in previous],"limits":["Planning contexts copied; external Documents/entities/relations/time references shared with explicit acknowledgement","Comparison by lineage; no matching by titles, prose diff, merge, canonical promotion or literary score","Movement warnings inspect direct declared neighborhood and precedes/depends only in chosen narrative/writing authority; not transitive consequences, chronological validation or editorial ordering","40 differences/page, 4000 total; provenance bounded at 4 MiB; oversized comparisons refuse, not truncate","Debug/ASan core focal: drivers and eight units; UI focal: driver and nine units; remaining dependencies Release; LSan disabled","Selected visual inspection and automated QA are not human UX, HiDPI, IME or accessibility certification","Anchored realization/composition reconciliation, knowledge/revelations and advanced lenses remain pending; entity recognition last; maps/timezones deferred"]}
for kind,name,target in (("inde","inde","inde-reforma-alternativas-narrativas-2026-10-11"),("laboratory","inde_text_blocks_lab","inde-laboratorio-alternativas-narrativas-2026-10-11")):
    p=delivery/target;assert not p.exists();shutil.copy2(root/"build"/name,p);p.chmod(0o750);result["binaries"][kind]={"path":str(p),"sha256":digest(p)}
names=sorted(set(n for n in subprocess.check_output(["git","ls-files","-co","--exclude-standard","-z"],cwd=source).decode().split("\0") if n))
archive=delivery/"inde-fonte-alternativas-narrativas-2026-10-11.tar.gz";assert not archive.exists()
with tarfile.open(archive,"w:gz") as tar:
    for name in names:
        p=source/name;assert p.is_file() and not p.is_symlink();tar.add(p,arcname="inde/"+name,recursive=False)
result["source_artifacts"]["complete_source"]={"path":str(archive),"sha256":digest(archive),"files":len(names),"includes_nonignored_untracked":True}
def rows(p):
    with sqlite3.connect((p/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)] and not db.execute("PRAGMA foreign_key_check").fetchall()
        r={}
        for (t,) in db.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall():
            q='"'+t.replace('"','""')+'"';cols='*'
            if t=="projects":cols=','.join('NULL' if c[1]=="updated_at" else '"'+c[1]+'"' for c in db.execute('PRAGMA table_info('+q+')'))
            r[t]=Counter(repr(row) for row in db.execute("SELECT "+cols+" FROM "+q))
        return r
review.mkdir();config.mkdir();(config/"inde").mkdir();(config/"inde/interface.ini").write_text("[appearance]\ntheme=light\n")
env=dict(os.environ,XDG_CONFIG_HOME=str(config),GSETTINGS_BACKEND="memory")
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    p=review/name
    if key=="atlas":
        origin=root/"review-construtor-narrativo"/name;before=rows(origin);item=snapshot(origin,p)
        (p/"EDITORIAL_REVIEW_COPY").write_text("INDE_EDITORIAL_REVIEW_COPY_2026_10_07\n")
        subprocess.run([str(root/"build/inde_prepare_editorial_review_copy"),str(p)],env=env,check=True)
        after=rows(p);assert rows(origin)==before
        for t,values in before.items():
            if t in ("change_log","sqlite_sequence") or t.startswith("cartographic_position_index_"):continue
            assert not values-after[t],"Existing rows changed: "+t
        item["origin_unchanged"]=True
    else:
        subprocess.run([str(root/"build/inde_text_blocks_empty_fixture"),str(p)],env=env,check=True);assert not rows(p)["documents"] and not rows(p)["works"]
        item={"path":str(p),"origin":"newly_created_pristine","mutable_review":True}
    (p/"NARRATIVE_ALTERNATIVES_REVIEW_COPY").write_text("INDE_NARRATIVE_ALTERNATIVES_REVIEW_COPY_2026_10_11\n");item.update(key=key,schema=27);result["review_copies"].append(item)
result["verification_artifacts"]=[{"path":str(p.resolve()),"sha256":digest(p)} for p in evidence]
manifest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n");print("SEALED",manifest)
