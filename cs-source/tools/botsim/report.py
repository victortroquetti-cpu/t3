#!/usr/bin/env python3
"""Monta a página do comparativo (report_template.html + resultado do botsim.py).

Uso: python3 -I report.py [resultado.json] [saida.html]
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "botsim_result.json"
    dst = sys.argv[2] if len(sys.argv) > 2 else "botsim_report.html"
    with open(src, encoding="utf-8") as f:
        data = json.load(f)
    # Os caminhos de exemplo não entram na página (só o mapa de calor).
    for k in ("padrao", "trok"):
        data["rotas"][k].pop("exemplo", None)
    with open(os.path.join(HERE, "report_template.html"), encoding="utf-8") as f:
        html = f.read()
    payload = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    html = html.replace("/*DATA*/null", payload, 1)
    with open(dst, "w", encoding="utf-8") as f:
        f.write(html)
    print("ok:", dst, f"({len(html) // 1024} KB)")


if __name__ == "__main__":
    main()
