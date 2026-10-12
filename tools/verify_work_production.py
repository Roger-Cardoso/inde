"""Independent ZIP/XML/HTML/PDF readback and EPUBCheck of both native production profiles."""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile
import pdfplumber

W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
X = "{http://www.w3.org/1999/xhtml}"
O = "{http://www.idpf.org/2007/opf}"

def paragraphs(text):
    return text.replace("\r\n", "\n").replace("\r", "\n").split("\n")

def heading(unit):
    return (unit["designator"] + " " if unit["designator"] else "") + unit["title"]

class HTML(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.inside = False
        self.values = []
        self.part = None
    def handle_starttag(self, tag, attrs):
        assert not any(k.lower().startswith("on") for k, v in attrs)
        assert tag not in {"script", "iframe", "img", "audio", "video"}
        if tag == "body": self.inside = True
        if self.inside and (tag == "p" or re.fullmatch("h[1-6]", tag)): self.part = []
    def handle_endtag(self, tag):
        if self.inside and (tag == "p" or re.fullmatch("h[1-6]", tag)):
            self.values.append("".join(self.part)); self.part = None
        if tag == "body": self.inside = False
    def handle_data(self, data):
        if self.part is not None: self.part.append(data)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-binary", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--epubcheck", required=True, type=Path)
    parser.add_argument("--verify-existing", action="store_true", help="Recheck an existing fixture only if it came from the exact current test binary")
    args = parser.parse_args()
    if args.verify_existing:
        prior = json.loads((args.output / "verification.json").read_text())
        assert prior["test_binary_sha256"] == hashlib.sha256(args.test_binary.read_bytes()).hexdigest()
    else:
        assert not args.output.exists(), "Use a fresh output, never overwrite earlier evidence"
        result = subprocess.run([str(args.test_binary.resolve()), "--emit", str(args.output.resolve())], text=True, capture_output=True, check=True)
        print(result.stdout, end="")
    expected = json.loads((args.output / "expected.json").read_text())
    body = []
    for unit in expected["units"]:
        if unit["heading"]: body.append(heading(unit))
        for text in unit["texts"]: body += paragraphs(text)
    checks = {}
    for profile in ("manuscript", "reading"):
        folder = args.output / profile
        report = json.loads((folder / "relatorio.json").read_text())
        assert report["complete"] and report["readback_verified"] and report["profile"] == profile
        for item in report["outputs"]:
            data = (folder / item["file"]).read_bytes()
            assert len(data) == item["bytes"] and hashlib.sha256(data).hexdigest() == item["sha256"]
        assert (folder / "obra.txt").read_bytes() == expected["preview"].encode()
        html = HTML(); html.feed((folder / "obra.html").read_text()); html.close()
        assert html.values == [expected["title"], expected["subtitle"], expected["author"], *body], html.values[:8]
        with zipfile.ZipFile(folder / "obra.docx") as z:
            assert z.testzip() is None and len(z.namelist()) == len(set(z.namelist()))
            for name in z.namelist(): ET.fromstring(z.read(name))
            doc = ET.fromstring(z.read("word/document.xml"))
            actual = []
            for p in doc.find(W + "body").findall(W + "p"):
                if p.find(".//" + W + "br") is not None: continue
                actual.append("".join(e.text or "" if e.tag == W + "t" else "\t" for e in p.iter() if e.tag in {W + "t", W + "tab"}))
            assert actual == [expected["title"], expected["subtitle"], expected["author"], *body]
            assert doc.find(".//" + W + "b") is not None and doc.find(".//" + W + "i") is not None
            styles = ET.fromstring(z.read("word/styles.xml"))
            spacing = styles.find(".//" + W + "pPrDefault/" + W + "pPr/" + W + "spacing")
            assert spacing.attrib[W + "line"] == ("480" if profile == "manuscript" else "360")
            assert styles.find(".//" + W + "rPrDefault/" + W + "rPr/" + W + "sz").attrib[W + "val"] == "24"
            assert doc.find(".//" + W + "pgSz").attrib == {W+"w":"12240",W+"h":"15840"}
        with zipfile.ZipFile(folder / "obra.epub") as z:
            assert z.testzip() is None and z.infolist()[0].filename == "mimetype" and z.infolist()[0].compress_type == zipfile.ZIP_STORED
            assert z.read("mimetype") == b"application/epub+zip"
            for name in z.namelist():
                if name.endswith((".xml", ".opf", ".xhtml")): ET.fromstring(z.read(name))
            opf = ET.fromstring(z.read("EPUB/package.opf"))
            manifest = {e.attrib["id"]:e.attrib["href"] for e in opf.find(O + "manifest")}
            for href in manifest.values(): assert "EPUB/"+href in z.namelist()
            spine = [manifest[e.attrib["idref"]] for e in opf.find(O + "spine")]
            assert spine == ["title.xhtml", *[f"unit{i+1}.xhtml" for i in range(len(expected["units"]))]]
            epub_body = []
            for name in spine[1:]:
                xml = ET.fromstring(z.read("EPUB/"+name))
                for el in xml.find(X + "body").iter():
                    if el.tag in {X+"p", *[X+f"h{i}" for i in range(1,7)]}: epub_body.append("".join(el.itertext()))
            assert epub_body == body
            nav=ET.fromstring(z.read("EPUB/nav.xhtml"))
            assert [a.attrib["href"] for a in nav.iter(X+"a")] == spine
            assert expected["excluded"] not in "".join(z.read("EPUB/"+n).decode() for n in spine)
        validation = subprocess.run(["java", "-jar", str(args.epubcheck.resolve()), str(folder / "obra.epub")], capture_output=True, text=True)
        (folder / "epubcheck.log").write_text(validation.stdout + validation.stderr)
        assert validation.returncode == 0 and "0 warnings" in validation.stdout, validation.stdout + validation.stderr
        with pdfplumber.open(folder / "obra.pdf") as pdf:
            page_text = []
            for index,page in enumerate(pdf.pages):
                assert page.width == 612 and page.height == 792
                chars = [c for c in page.chars if c["top"] < 735]
                assert all(69 <= c["x0"] <= 541 and c["x1"] <= 544 and 60 <= c["top"] and c["bottom"] <= 724 for c in chars), "PDF clipping/margins"
                content = page.crop((60, 60, 550, 730)).extract_text() or ""
                page_text.append(content)
            joined = re.sub(r"\s", "", "".join(page_text))
            wanted = re.sub(r"\s", "", "".join([expected["title"],expected["subtitle"],expected["author"],*body]))
            assert joined == wanted, "PDF text lost/reordered"
            sizes = [c["size"] for p in pdf.pages[1:] for c in p.chars if c["top"] < 735]
            assert sum(abs(s-12)<0.1 for s in sizes) > len(sizes)*0.85, "Body font not 12pt"
            checks[profile] = {"passed":True,"pdf_pages":len(pdf.pages),"exact_txt":True,"docx_xml_text_styles_tabs":True,"epub_spine_text_nav":True,"html_exact_paragraphs":True,"pdf_text_margins_12pt":True,"hash_readback":True,"epubcheck":str(folder/"epubcheck.log")}
    assert checks["manuscript"]["pdf_pages"] >= checks["reading"]["pdf_pages"]
    evidence = {"complete":True,"human_acceptance":"pending","checks":checks,"test_binary_sha256":hashlib.sha256(args.test_binary.read_bytes()).hexdigest(),"fixture":str(args.output.resolve()),"epubcheck_jar_sha256":hashlib.sha256(args.epubcheck.read_bytes()).hexdigest(),"scope":"Independent finite fixtures, not universal publisher/render/a11y certification"}
    (args.output / "verification.json").write_text(json.dumps(evidence, ensure_ascii=False, indent=2)+"\n")
    print(json.dumps(evidence,ensure_ascii=False))

if __name__ == "__main__": main()
