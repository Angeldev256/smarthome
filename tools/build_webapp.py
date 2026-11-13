#!/usr/bin/env python3
"""
Pack the web app (webapp/index.html + style.css + app.js) into ONE gzipped
HTML page and write it as a C byte array at the end of
firmware/smarthome/smarthome.ino (between WEBAPP-BEGIN and WEBAPP-END),
so the whole firmware is a single .ino file.

Run this every time you change anything in /webapp:
    python3 tools/build_webapp.py
"""
import gzip
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent.parent
WEB = ROOT / "webapp"
OUT = ROOT / "firmware" / "smarthome" / "smarthome.ino"

html = (WEB / "index.html").read_text(encoding="utf-8")
css = (WEB / "style.css").read_text(encoding="utf-8")
js = (WEB / "app.js").read_text(encoding="utf-8")

html = html.replace('<link rel="stylesheet" href="style.css">', f"<style>\n{css}\n</style>")
html = html.replace('<script src="app.js"></script>', f"<script>\n{js}\n</script>")
assert "<style>" in html and "smartEstate" in html, "inlining failed"

# Photos are too big for the ESP32's flash page; the gallery hides itself.
html = re.sub(r'\s*<figure><img src="img/[^"]+"[^>]*>.*?</figure>', "", html, flags=re.S)

data = gzip.compress(html.encode("utf-8"), compresslevel=9, mtime=0)

lines = []
for i in range(0, len(data), 20):
    lines.append("  " + ", ".join(f"0x{b:02x}" for b in data[i:i + 20]) + ",")

block = (
    "// WEBAPP-BEGIN\n"
    f"const size_t WEBAPP_HTML_GZ_LEN = {len(data)};\n"
    "const uint8_t WEBAPP_HTML_GZ[] PROGMEM = {\n" + "\n".join(lines) + "\n};\n"
    "// WEBAPP-END"
)
ino = OUT.read_text(encoding="utf-8")
start, end = ino.index("// WEBAPP-BEGIN"), ino.index("// WEBAPP-END") + len("// WEBAPP-END")
OUT.write_text(ino[:start] + block + ino[end:], encoding="utf-8")
print(f"smarthome.ino updated: {len(html)} bytes html -> {len(data)} bytes gzip")
