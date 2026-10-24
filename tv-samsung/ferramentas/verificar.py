#!/usr/bin/env python3
"""
Confere o app antes de mandar para a TV:

- config.xml válido e com os ícones que ele cita;
- JavaScript em ES5 puro (com o acorn, via npx, quando o Node existe; senão, uma busca simples);
- nenhuma API que o navegador de 2013 não tem (.bind, classList, fetch, Promise...);
- o pacote MinhaTV.zip monta, com config.xml na raiz e JS/HTML só em ASCII.

    python ferramentas/verificar.py
"""

import importlib.util
import io
import os
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
APP = os.path.join(RAIZ, "app")

APIS_PROIBIDAS = [
    ".bind(", "classList", "requestAnimationFrame", "fetch(", "Promise", ".includes(", "Object.assign",
    "addEventListener", ".trim()", "Array.from", "toLocale", "localStorage", "JSON.stringify(",
]
SINTAXE_ES6 = [
    (re.compile(r"\b(let|const)\s"), "let/const"),
    (re.compile(r"=>"), "arrow function"),
    (re.compile(r"`"), "crase (template string)"),
    (re.compile(r"\bclass\s+[A-Za-z_$]"), "class"),
    (re.compile(r"\.\.\.[A-Za-z_$\[]"), "spread"),
]

problemas = []


def sem_comentarios_e_textos(codigo):
    codigo = re.sub(r"/\*.*?\*/", " ", codigo, flags=re.S)
    codigo = re.sub(r"//[^\n]*", " ", codigo)
    return re.sub(r"'(?:\\.|[^'\\\n])*'|\"(?:\\.|[^\"\\\n])*\"", "''", codigo)


def checar_config():
    raiz = ET.parse(os.path.join(APP, "config.xml")).getroot()
    for tag in ("ThumbIcon", "BigThumbIcon", "ListIcon", "BigListIcon"):
        caminho = os.path.join(APP, raiz.find(tag).text)
        if not os.path.exists(caminho):
            problemas.append("config.xml cita %s, que não existe" % raiz.find(tag).text)
    if (raiz.find("width").text, raiz.find("height").text) != ("1280", "720"):
        problemas.append("config.xml: a tela do app deve ser 1280x720 (o CSS foi feito para isso)")


def checar_js():
    npx = shutil.which("npx")
    for nome in sorted(os.listdir(os.path.join(APP, "js"))):
        if not nome.endswith(".js"):
            continue
        caminho = os.path.join(APP, "js", nome)
        with open(caminho, encoding="utf-8") as f:
            codigo = f.read()
        limpo = sem_comentarios_e_textos(codigo)
        for regex, nome_sintaxe in SINTAXE_ES6:
            if regex.search(limpo):
                problemas.append("js/%s usa %s (a TV só entende ES5)" % (nome, nome_sintaxe))
        for api in APIS_PROIBIDAS:
            if api in limpo:
                problemas.append("js/%s usa %s, que a TV de 2013 não tem" % (nome, api))
        if npx:
            r = subprocess.run([npx, "--yes", "acorn@8.14.0", "--ecma5", "--silent", caminho],
                               capture_output=True, text=True, shell=(os.name == "nt"))
            if r.returncode != 0:
                problemas.append("js/%s não é ES5 válido: %s" % (nome, (r.stderr or r.stdout).strip()))
    if not npx:
        print("(Node não encontrado: conferi o ES5 só por busca simples.)")


def checar_css():
    with open(os.path.join(APP, "css", "home.css"), encoding="utf-8") as f:
        css = re.sub(r"/\*.*?\*/", " ", f.read(), flags=re.S)
    if any(ord(c) > 127 for c in css):
        problemas.append("css/home.css tem acentos ou símbolos fora dos comentários: use só ASCII no CSS")
    for proibido in ("display: flex", "display:flex", "display: grid", "display:grid", "var(--"):
        if proibido in css:
            problemas.append("css/home.css usa %s, que a TV de 2013 não tem" % proibido)


def checar_pacote():
    spec = importlib.util.spec_from_file_location("servidor", os.path.join(RAIZ, "servidor", "servidor.py"))
    servidor = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(servidor)
    with open(servidor.CONFIG_EXEMPLO, encoding="utf-8") as f:
        import json
        cfg = json.load(f)
    dados = servidor.gerar_zip(dict(cfg, tmdb_chave=""), "http://192.168.0.2")
    with zipfile.ZipFile(io.BytesIO(dados)) as z:
        nomes = z.namelist()
        if "config.xml" not in nomes or "index.html" not in nomes or "js/config.js" not in nomes:
            problemas.append("o zip precisa de config.xml, index.html e js/config.js na raiz")
        for nome in nomes:
            if nome.endswith((".js", ".html", ".css")) and any(b > 127 for b in z.read(nome)):
                problemas.append("%s no zip não está em ASCII" % nome)
    print("Pacote: %d arquivos, %d KB." % (len(nomes), len(dados) // 1024))


def main():
    checar_config()
    checar_js()
    checar_css()
    checar_pacote()
    if problemas:
        print("Problemas:")
        for p in problemas:
            print(" - " + p)
        sys.exit(1)
    print("Tudo certo para a TV.")


if __name__ == "__main__":
    main()
