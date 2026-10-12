"""Record explicit agent visual review, never infer human acceptance from renders."""
import argparse
import hashlib
import json
from pathlib import Path

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--confirm-agent-review",action="store_true",required=True)
    args=parser.parse_args()
    assert args.confirm_agent_review
    root=Path(__file__).resolve().parents[2]
    delivery=root.parents[1]/"outputs"
    final=root/"qa-producao-formatos-2026-10-08-04"
    docx_render=root/"qa-producao-formatos-2026-10-08-03"
    artifacts=[]
    pages=[]
    for profile,pdf_pages in (("manuscript",6),("reading",5)):
        for suffix,count,render in (("docx",5,docx_render/profile/"render-docx"),("pdf",pdf_pages,final/profile/"render-pdf")):
            artifact=final/profile/("obra."+suffix)
            if suffix=="docx":assert digest(artifact)==digest(docx_render/profile/"obra.docx"),"Reused render must match exact bytes"
            images=sorted(render.glob("page-*.png"))
            assert len(images)==count
            artifacts.append({"path":str(artifact),"sha256":digest(artifact),"reviewed_pages":count,"render_directory":str(render)})
            pages.extend({"path":str(p),"sha256":digest(p)} for p in images)
    assert len(pages)==21
    target=delivery/"reforma-producao-visual-verificacao-2026-10-08.json"
    assert not target.exists()
    report={"complete":True,"agent_visual_review":True,"human_acceptance":"pending","reviewed_all_final_pages":True,"total_pages":21,"artifacts":artifacts,"pages":pages,
            "findings_corrected":["PDF Pango contexts use 72 DPI for point coordinates instead of 96 DPI","Short ordinary PDF paragraphs kept together rather than split at a page boundary"],
            "scope":"All 21 finite DOCX/PDF fixture pages visually inspected; no whole-product/publisher/a11y certification"}
    target.write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n")
    print(json.dumps({"complete":True,"pages":21,"path":str(target)}))

if __name__=="__main__":main()
