"""Independent HTML/TXT/readback verification against a generated core fixture."""
import argparse
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess

class ReadingParser(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.inside = False
        self.parts = []
        self.mains = 0

    def handle_starttag(self, tag, attrs):
        assert tag in {"html", "head", "meta", "title", "style", "body", "h1", "main", "span"}, tag
        assert not any(key.lower().startswith("on") for key, _ in attrs)
        if tag == "main":
            assert dict(attrs).get("id") == "document-text"
            self.inside = True
            self.mains += 1

    def handle_endtag(self, tag):
        if tag == "main":
            self.inside = False

    def handle_data(self, data):
        if self.inside:
            self.parts.append(data)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-binary", required=True, type=Path)
    args = parser.parse_args()
    result = subprocess.run([str(args.test_binary)], check=True, capture_output=True, text=True)
    print(result.stdout.strip())
    folder = Path(re.search(r'fixture="(.+)"', result.stdout)[1])
    report = json.loads((folder / "relatorio.json").read_text())
    text = (folder / "documento.txt").read_bytes()
    html = (folder / "documento.html").read_bytes()
    assert report["complete"] and report["readback_verified"]
    assert hashlib.sha256(text).hexdigest() == report["text_sha256"]
    assert hashlib.sha256(html).hexdigest() == report["html_sha256"]
    reader = ReadingParser()
    reader.feed(html.decode("utf-8"))
    reader.close()
    assert reader.mains == 1 and not reader.inside
    assert "".join(reader.parts).encode("utf-8") == text
    print(json.dumps({"passed": True, "fixture": str(folder), "text_bytes": len(text),
                      "html_parser_exact_text_readback": True, "hashes_verified": True}))

if __name__ == "__main__":
    main()
