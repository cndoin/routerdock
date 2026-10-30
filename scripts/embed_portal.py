"""Embed the self-contained portal; no filesystem upload or CDN required."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
html=(root/'firmware/web/index.html').read_text(encoding='utf-8-sig')
assert ')RDHTML"' not in html
(root/'firmware/include/PortalPage.h').write_text('#pragma once\n#include <pgmspace.h>\nstatic const char PORTAL_HTML[] PROGMEM = R"RDHTML('+html+')RDHTML";\n',encoding='utf-8')
