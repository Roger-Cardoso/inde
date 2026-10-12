"""Seal the sixth cut after full final-build GTK coverage, focused dark QA, core and visual review."""
from hashlib import sha256
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot

SOURCE=Path(__file__).resolve().parents[1]
ROOT=SOURCE.parent
DELIVERY=ROOT.parents[1]/"outputs"
BASE="0d642709703f860ef135559409dbedfbf15c5185"
PREFIX="reforma-operacoes"
DATE="2026-10-08"
def digest(p):return sha256(p.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git",*args],cwd=SOURCE,text=True).strip()
def load(name):return json.loads((DELIVERY/name).read_text())
def main():
    qa_path=DELIVERY/f"{PREFIX}-verificacao-{DATE}.json"
    dark_path=DELIVERY/f"{PREFIX}-tema-escuro-final-{DATE}.json"
    visual_path=DELIVERY/f"{PREFIX}-visual-verificacao-{DATE}.json"
    preservation_path=DELIVERY/f"{PREFIX}-preservacao-verificacao-{DATE}.json"
    qa,dark,visual,preserved=(json.loads(p.read_text()) for p in (qa_path,dark_path,visual_path,preservation_path))
    assert qa["complete"] and qa["stage"]=="operations" and qa["ui_scenarios"]==36 and qa["origins_unchanged"]
    assert dark["stage"]=="operations" and dark["origins_unchanged"] and dark["theme"]=="dark"
    assert dark["g_debug"]=="fatal-warnings"
    assert all(c["passed"] for r in (qa,dark) for c in r["checks"].values())
    dark_modes=("organization","editor_interaction","color","operations")
    expected_dark={key+"-"+mode+"-dark"+suffix for key in ("atlas","empty") for mode in dark_modes
                   for suffix in (("","-restart") if mode=="operations" else ("",))}
    assert {k for k in dark["checks"] if k.startswith(("atlas-","empty-"))}==expected_dark
    assert preserved["complete"] and preserved["original_source_unchanged"] and preserved["nonignored_files_checked"]==402
    assert visual["complete"] and visual["stage"]=="operations" and visual["binary_sha256"]==qa["binary"]["sha256"]
    assert visual["reviewed_by_agent"] and len(visual["screenshots"])>=8
    for item in visual["screenshots"]:assert digest(Path(item["path"]))==item["sha256"]
    for key,file in (("binary","inde"),("laboratory_binary","inde_text_blocks_lab")):
        assert digest(ROOT/"build"/file)==qa[key]["sha256"]==dark[key]["sha256"]
    for mode,h in qa["driver_sha256"].items():
        assert digest(ROOT/"build"/("inde_ui_"+mode+"_smoke"))==h
        if mode in dark_modes:assert dark["driver_sha256"][mode]==h
    ctest=ROOT/"logs/ctest-operacoes-final-06-2026-10-08.log"
    sanitizer=ROOT/"logs/sanitizers-operacoes-rotulos-final-02-2026-10-08.log"
    assert "100% tests passed" in ctest.read_text() and "out of 20" in ctest.read_text()
    assert "PASS focal operations ASan/UBSan" in sanitizer.read_text()
    ui_asan_path=DELIVERY/f"{PREFIX}-ui-sanitizadores-final-{DATE}.json"
    ui_asan=json.loads(ui_asan_path.read_text())
    assert ui_asan["stage"]=="operations" and ui_asan["operations_ui_focal_sanitizers"] and ui_asan["origins_unchanged"]
    assert ui_asan["binary"]["sha256"]==qa["binary"]["sha256"]
    assert all(c["passed"] for c in ui_asan["checks"].values())
    assert sum(k.startswith(("atlas-","empty-")) for k in ui_asan["checks"])==4
    assert digest(ROOT/"build/inde_ui_operations_smoke_asan")==ui_asan["operations_focal_driver_sha256"]
    assert not git("status","--porcelain") and git("rev-parse","HEAD^")==BASE
    accept_path=DELIVERY/f"{PREFIX}-aceite-anterior-{DATE}.json"
    accept=json.loads(accept_path.read_text())
    assert accept["human_acceptance"]=="accepted" and accept["next_implementation_authorized"]
    assert BASE in {a["commit"] for a in accept["accepted_deliveries"]}
    previous_names=("reforma-entrega-2026-10-07.json","reforma-organizacao-entrega-2026-10-07.json","reforma-continuidade-entrega-2026-10-07.json","reforma-revisao-entrega-2026-10-07.json","reforma-producao-entrega-2026-10-08.json")
    for name in previous_names:
        for artifact in load(name)["binaries"].values():
            assert digest(Path(artifact["path"]))==artifact["sha256"]
    prior_path=DELIVERY/previous_names[-1]
    prior=load(previous_names[-1])
    manifest_path=DELIVERY/f"{PREFIX}-entrega-{DATE}.json"
    binary_names=(("inde","inde",f"inde-reforma-operacoes-{DATE}"),("laboratory","inde_text_blocks_lab",f"inde-laboratorio-operacoes-{DATE}"))
    review_names=(("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde"))
    assert not manifest_path.exists() and not (ROOT/"review-operacoes-config").exists()
    assert all(not(DELIVERY/name).exists() for _,_,name in binary_names)
    assert all(not(ROOT/"review-operacoes"/name).exists() for _,name in review_names)
    result={"technical_verification_passed":True,"human_acceptance":"pending","whole_reform_complete":False,"published":False,
            "stage":"project_operations","source":str(SOURCE),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),
            "predecessor_commit":BASE,"predecessor_delivery":str(prior_path),"accepted_predecessors":accept["accepted_deliveries"],
            "binaries":{},"review_copies":[],"source_artifacts":{},"schema":20,"semantic_payload":2,"checkpoint_payload":4,"clipboard_payload":2,
            "human_acceptance_record":{"path":str(accept_path),"sha256":digest(accept_path)},
            "preservation_checkpoint":prior["preservation_checkpoint"],"next_front":"Persistent Composition: authority/recovery contract before schema",
            "limits":["New covered Project transactions only, not universal/retroactive history or text undo",
                      "No automatic Project redo, archival UI or external-file rollback",
                      "8 MiB/changeset, 20000 rows/operation, 128 MiB capture, 256 MiB preview database; fail closed, no truncation",
                      "Copied identity keeps inherited history non-reversible; new operations normal",
                      "Focal ASan/UBSan; remaining dependencies Release; LSan disabled; no whole-GTK certification",
                      "No human validation of IME, assistive technologies, HiDPI or long-session/large-database latency"]}
    result["verification_artifacts"]={p.name:{"path":str(p),"sha256":digest(p)} for p in (qa_path,dark_path,visual_path,preservation_path,ctest,sanitizer,ui_asan_path)}
    for ext in ("bundle","patch"):
        p=DELIVERY/f"inde-reforma-operacoes-{DATE}.{ext}";assert p.is_file()
        result["source_artifacts"][ext]={"path":str(p),"sha256":digest(p)}
    for key,file,name in binary_names:
        p=DELIVERY/name;shutil.copy2(ROOT/"build"/file,p);p.chmod(0o750)
        result["binaries"][key]={"path":str(p),"sha256":digest(p)}
    for key,name in review_names:
        target=ROOT/"review-operacoes"/name
        item=snapshot(ROOT/"review-producao"/name,target)
        subprocess.run([str(ROOT/"build/inde_prepare_editorial_review_copy"),str(target)],check=True)
        with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
            assert db.execute("SELECT MAX(version) FROM schema_migrations").fetchone()[0]==20
            assert db.execute("SELECT count(*) FROM project_operations").fetchone()[0]==0,"No invented old operations"
        (target/"OPERATIONS_REVIEW_COPY").write_text("INDE_OPERATIONS_REVIEW_COPY_2026_10_08\n")
        item["key"]=key;result["review_copies"].append(item)
    config=ROOT/"review-producao-config"
    if config.exists():shutil.copytree(config,ROOT/"review-operacoes-config")
    else:(ROOT/"review-operacoes-config").mkdir()
    doc=DELIVERY/f"{PREFIX}-implementacao-{DATE}.md"
    shutil.copy2(SOURCE/"docs/reform-project-operations-2026-10-08.md",doc)
    result["documentation"]={"path":str(doc),"sha256":digest(doc)}
    launcher=DELIVERY/"inde-operacoes-abrir.sh"
    subprocess.run(["bash","-n",str(launcher)],check=True)
    launcher.chmod(0o750)
    result["launcher"]={"path":str(launcher),"sha256":digest(launcher)}
    manifest_path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n")
    print("SEALED",result["commit"],result["binaries"]["inde"]["sha256"],"human acceptance pending")
if __name__=="__main__":main()
