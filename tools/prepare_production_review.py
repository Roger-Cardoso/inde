"""Seal this production cut only after final-code QA, visual QA and a clean local commit."""
from hashlib import sha256
import json
from pathlib import Path
import shutil
import sqlite3
import subprocess
from reform_snapshots import snapshot

SOURCE = Path(__file__).resolve().parents[1]
ROOT = SOURCE.parent
DELIVERY = ROOT.parents[1] / "outputs"
BASE = "ace99635cb84db45cb401758664f6c17ff83bf29"
PREFIX = "reforma-producao"
DATE = "2026-10-08"

def digest(path): return sha256(path.read_bytes()).hexdigest()
def git(*args): return subprocess.check_output(["git", *args], cwd=SOURCE, text=True).strip()

def main():
    report_path = DELIVERY / f"{PREFIX}-verificacao-{DATE}.json"
    dark_path = DELIVERY / f"{PREFIX}-tema-escuro-{DATE}.json"
    formats_path = DELIVERY / f"{PREFIX}-formatos-verificacao-{DATE}.json"
    visual_path = DELIVERY / f"{PREFIX}-visual-verificacao-{DATE}.json"
    ui_formats_path = DELIVERY / f"{PREFIX}-ui-formatos-verificacao-{DATE}.json"
    ui_visual_path = DELIVERY / f"{PREFIX}-ui-visual-verificacao-{DATE}.json"
    final_supplement_path = DELIVERY / f"{PREFIX}-retomada-verificacao-{DATE}.json"
    preservation_path = DELIVERY / f"{PREFIX}-preservacao-verificacao-{DATE}.json"
    report, dark, formats, visual = (json.loads(p.read_text()) for p in (report_path,dark_path,formats_path,visual_path))
    assert report["stage"] == "production" and report["complete"] and report["origins_unchanged"]
    assert sum(n.startswith(("atlas-", "empty-")) for n in report["checks"]) == 32
    assert dark["stage"] == "production" and dark["origins_unchanged"]
    assert sum(n.startswith(("atlas-", "empty-")) for n in dark["checks"]) == 16
    assert all(c["passed"] for r in (report,dark) for c in r["checks"].values())
    supplement=json.loads(final_supplement_path.read_text())
    assert supplement["stage"]=="production" and supplement["supplement_complete"] and supplement["origins_unchanged"]
    assert sum(n.startswith(("atlas-","empty-")) for n in supplement["checks"])==8
    assert all(c["passed"] for c in supplement["checks"].values())
    assert supplement["binary"]["sha256"]==report["binary"]["sha256"]
    assert supplement["laboratory_binary"]["sha256"]==report["laboratory_binary"]["sha256"]
    assert supplement["continuity_driver_sha256"]==digest(ROOT/"build/inde_ui_continuity_smoke")
    assert all(report["checks"][name]["passed"] for name in ("export-readback","focal-sanitizers","editorial-focal-sanitizers","production-focal-sanitizers"))
    assert formats["complete"] and visual["complete"] and visual["reviewed_all_final_pages"]
    ui_formats=json.loads(ui_formats_path.read_text())
    assert ui_formats["complete"] and all(c["passed"] for c in ui_formats["checks"].values())
    assert ui_formats["binary"]["sha256"]==report["binary"]["sha256"]
    ui_visual=json.loads(ui_visual_path.read_text())
    assert ui_visual["complete"] and ui_visual["reviewed_all_final_pages"] and ui_visual["reviewed_page_count"]==4
    assert ui_visual["binary"]["sha256"]==report["binary"]["sha256"]
    assert digest(ui_formats_path)==ui_visual["source_evidence_sha256"]
    for item in ui_visual["artifacts"]:
        assert digest(Path(item["path"]))==item["sha256"]
        for page in item["pages"]:
            assert page["agent_visually_reviewed"] and digest(Path(page["path"]))==page["sha256"]
    preservation=json.loads(preservation_path.read_text())
    assert preservation["complete"] and preservation["original_source_unchanged"] and preservation["nonignored_files_checked"]==402
    assert all(c["passed"] for c in formats["checks"].values())
    assert digest(ROOT / "build/inde_work_production_tests") == formats["test_binary_sha256"]
    for item in visual["artifacts"]: assert digest(Path(item["path"])) == item["sha256"]
    for key,binary in (("binary","inde"),("laboratory_binary","inde_text_blocks_lab")):
        assert digest(ROOT / "build" / binary) == report[key]["sha256"] == dark[key]["sha256"]
    ctest = ROOT / "logs/ctest-producao-final-03.log"
    assert "100% tests passed" in ctest.read_text() and "out of 19" in ctest.read_text()
    assert not git("status","--porcelain") and git("rev-parse","HEAD^") == BASE
    acceptance_path = DELIVERY / f"{PREFIX}-aceite-anterior-{DATE}.json"
    acceptance = json.loads(acceptance_path.read_text())
    assert acceptance["human_acceptance"] == "accepted" and acceptance["next_implementation_authorized"]
    assert BASE in {a["commit"] for a in acceptance["accepted_deliveries"]}
    prior_path = DELIVERY / "reforma-revisao-entrega-2026-10-07.json"
    prior = json.loads(prior_path.read_text())
    for name in ("reforma-entrega-2026-10-07.json","reforma-organizacao-entrega-2026-10-07.json","reforma-continuidade-entrega-2026-10-07.json","reforma-revisao-entrega-2026-10-07.json"):
        old=json.loads((DELIVERY/name).read_text())
        assert all(digest(Path(a["path"]))==a["sha256"] for a in old["binaries"].values())
    original=Path(prior["preservation_checkpoint"]["original_candidate"])
    assert subprocess.check_output(["git","rev-parse","HEAD"],cwd=original,text=True).strip()==prior["preservation_checkpoint"]["original_base"]
    # Same entry-count method as the historical preservation manifest; an
    # untracked directory is one entry, not a claim about its file count.
    statuses=subprocess.check_output(["git","status","--porcelain"],cwd=original,text=True).splitlines()
    assert len(statuses)==244 and sum(s.startswith("??") for s in statuses)==204
    assert digest(Path(prior["preservation_checkpoint"]["path"]))==prior["preservation_checkpoint"]["sha256"]
    manifest_path=DELIVERY/f"{PREFIX}-entrega-{DATE}.json"
    binaries=(("inde","inde",f"inde-reforma-producao-{DATE}"),("laboratory","inde_text_blocks_lab",f"inde-laboratorio-producao-{DATE}"))
    reviews=(("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde"))
    assert not manifest_path.exists() and not (ROOT/"review-producao-config").exists()
    assert all(not(DELIVERY/name).exists() for _,_,name in binaries)
    assert all(not(ROOT/"review-producao"/name).exists() for _,name in reviews)
    manifest={"technical_verification_passed":True,"human_acceptance":"pending","whole_reform_complete":False,"published":False,"stage":"work_production",
              "source":str(SOURCE),"branch":git("branch","--show-current"),"commit":git("rev-parse","HEAD"),"predecessor_commit":BASE,"predecessor_delivery":str(prior_path),
              "accepted_predecessors":acceptance["accepted_deliveries"],"human_acceptance_record":{"path":str(acceptance_path),"sha256":digest(acceptance_path)},
              "verification":str(report_path),"supplemental_dark_verification":str(dark_path),"formats_verification":str(formats_path),"visual_verification":str(visual_path),
              "strict_continuity_production_supplement":str(final_supplement_path),
              "ui_formats_verification":str(ui_formats_path),"ui_visual_verification":str(ui_visual_path),"original_preservation_verification":str(preservation_path),
              "binaries":{},"review_copies":[],"source_artifacts":{},"schema":19,"semantic_payload":2,"checkpoint_payload":4,"clipboard_payload":2,
              "preservation_checkpoint":prior["preservation_checkpoint"],"next_front":"Project operation impact and reversal, not text undo",
              "limits":["Derived snapshot, not publication/backup/persistent Composition", "UI: 1000 units, 100 selected per unit, 500 candidates per unit, 10000 Documents", "Selected text 8 MiB; saved selection 24 KiB; ZIP/PDF 128 MiB; no truncation", "Four inline styles retained; other styles substituted and reported", "No PDF/A, PDF/UA, commercial print layout, images, footnotes or roundtrip import", "Focal sanitizers only; Release dependencies; LSan disabled"]}
    manifest["verification_artifacts"] = {
        path.name: {"path": str(path), "sha256": digest(path)}
        for path in (report_path, dark_path, formats_path, visual_path, ui_formats_path,
                     ui_visual_path, final_supplement_path, preservation_path)}
    for name in (f"inde-reforma-producao-{DATE}.bundle",f"inde-reforma-producao-{DATE}.patch"):
        path=DELIVERY/name; assert path.is_file()
        manifest["source_artifacts"][name]={"path":str(path),"sha256":digest(path)}
    for key,source,name in binaries:
        path=DELIVERY/name;shutil.copy2(ROOT/"build"/source,path);path.chmod(0o750)
        manifest["binaries"][key]={"path":str(path),"sha256":digest(path)}
    for key,name in reviews:
        origin=ROOT/"review-revisao"/name;target=ROOT/"review-producao"/name
        review=snapshot(origin,target)
        with sqlite3.connect((target/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
            assert db.execute("SELECT MAX(version) FROM schema_migrations").fetchone()[0]==19
        (target/"PRODUCTION_REVIEW_COPY").write_text("INDE_PRODUCTION_REVIEW_COPY_2026_10_08\n")
        review["key"]=key;manifest["review_copies"].append(review)
    config=ROOT/"review-revisao-config"
    if config.exists():shutil.copytree(config,ROOT/"review-producao-config")
    else:(ROOT/"review-producao-config").mkdir()
    shutil.copy2(ctest,DELIVERY/f"{PREFIX}-testes-nucleo-{DATE}.log")
    shutil.copy2(SOURCE/"docs/reform-work-production-2026-10-08.md",DELIVERY/f"{PREFIX}-implementacao-{DATE}.md")
    documentation=DELIVERY/f"{PREFIX}-implementacao-{DATE}.md"
    manifest["documentation"]={"path":str(documentation),"sha256":digest(documentation)}
    manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps(manifest,ensure_ascii=False,indent=2))

if __name__=="__main__":main()
