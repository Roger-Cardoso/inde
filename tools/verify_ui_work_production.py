"""Read native GUI-produced packages back against their SQLite source snapshots, including emoji."""
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import xml.etree.ElementTree as ET
import zipfile
import pdfplumber
from pypdf import PdfReader

root=Path(__file__).resolve().parents[2]
delivery=root.parents[1]/"outputs"
qa=json.loads((delivery/"reforma-producao-verificacao-2026-10-08.json").read_text())
assert qa["complete"] and qa["stage"]=="production"
assert hashlib.sha256((root/"build/inde").read_bytes()).hexdigest()==qa["binary"]["sha256"]
checks={}
compact=lambda s:re.sub(r"\s","",s)
for key,name in (("atlas","Atlas de Véspera — Projeto Integral.inde"),("empty","Projeto vazio — sem exemplos.inde")):
    assert qa["checks"][key+"-production"]["passed"]
    fixture=root/"qa"/qa["run"]/key/"production"/name
    folders=list((fixture.parent/"production-output").iterdir())
    assert len(folders)==1
    folder=folders[0];report=json.loads((folder/"relatorio.json").read_text())
    assert report["complete"] and report["readback_verified"]
    bodies=[]
    with sqlite3.connect((fixture/"data/project.sqlite3").as_uri()+"?mode=ro",uri=True) as db:
        for unit in report["units"]:
            for item in unit["documents"]:
                text=db.execute("SELECT content FROM documents WHERE id=?",(item["id"],)).fetchone()[0]
                assert hashlib.sha256(text.encode()).hexdigest()==item["body_sha256"]
                bodies.append(text)
    assert len(bodies)==2 and "🙂" in bodies[1]
    text=(folder/"obra.txt").read_bytes().decode()
    assert all(b in text for b in bodies) and text.index(bodies[0])<text.index(bodies[1])
    with zipfile.ZipFile(folder/"obra.docx") as z:
        assert z.testzip() is None
        parsed=ET.fromstring(z.read("word/document.xml"))
        docx=compact("".join(e.text or "" for e in parsed.iter("{http://schemas.openxmlformats.org/wordprocessingml/2006/main}t")))
    with zipfile.ZipFile(folder/"obra.epub") as z:
        assert z.testzip() is None
        epub=compact("".join("".join(ET.fromstring(z.read(n)).itertext()) for n in sorted(z.namelist()) if re.fullmatch(r"EPUB/unit\d+\.xhtml",n)))
    # Content-stream order, not geometric line grouping: fallback emoji fonts
    # have different glyph bounds and can be placed on another inferred line
    # by coordinate-based extractors despite correct drawing/Unicode order.
    pdf_text=compact("".join(p.extract_text() or "" for p in PdfReader(folder/"obra.pdf").pages))
    for label,output in (("DOCX",docx),("EPUB",epub),("PDF",pdf_text)):
        expected=[compact(b) for b in bodies]
        assert all(b in output for b in expected),f"Unicode content missing in {label}: expected={expected!r}, observed={output!r}"
        assert output.index(expected[0])<output.index(expected[1]),"GUI order lost"
    for item in report["outputs"]:
        assert hashlib.sha256((folder/item["file"]).read_bytes()).hexdigest()==item["sha256"]
    checks[key]={"passed":True,"source_body_hashes":True,"exact_txt_bodies":True,"docx_epub_pdf_unicode_emoji_order":True,"folder":str(folder)}
output=delivery/"reforma-producao-ui-formatos-verificacao-2026-10-08.json"
assert not output.exists()
output.write_text(json.dumps({"complete":True,"human_acceptance":"pending","binary":qa["binary"],"checks":checks,"scope":"Actual two GTK-generated snapshots; SQLite source read-only, exact TXT and normalized professional-format text including emoji", "pdf_extraction":"pypdf content-stream order; whitespace normalized. Geometric extraction can group fallback emoji on another inferred line; not a tagged/a11y PDF claim"},ensure_ascii=False,indent=2)+"\n")
print(json.dumps(checks,ensure_ascii=False))
