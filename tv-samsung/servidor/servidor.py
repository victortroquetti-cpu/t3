#!/usr/bin/env python3
"""
Servidor da Minha TV (Samsung série F, 2013, plataforma Orsay).

Roda no PC, na mesma rede da TV, e faz três coisas:

1. Instala o app na TV. Na TV (logada com a conta "develop"), "Iniciar sincronização
   de apps" baixa http://<IP do PC>/widgetlist.xml e depois o MinhaTV.zip, que este
   servidor monta na hora a partir da pasta app/.
2. Entrega clima (Open-Meteo) e filmes/séries em alta (TMDB) para o app. A TV de 2013
   nem sempre consegue HTTPS moderno, então o PC busca e repassa em HTTP simples.
3. Mostra a pré-visualização do app no navegador: http://<IP do PC>/app/

Só usa a biblioteca padrão do Python 3.7+, sem instalar nada.

    python servidor.py              # porta 80 (a TV só procura na porta 80)
    python servidor.py --ip 192.168.0.10
    python servidor.py --gerar      # só gera dist/MinhaTV.zip e dist/widgetlist.xml
"""

import argparse
import io
import json
import os
import re
import shutil
import socket
import ssl
import sys
import threading
import time
import zipfile
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.error import HTTPError, URLError
from urllib.parse import parse_qs, urlencode, urlparse
from urllib.request import Request, urlopen

VERSAO = "1.0.0"

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PASTA_APP = os.path.join(RAIZ, "app")
CONFIG_EXEMPLO = os.path.join(RAIZ, "config.exemplo.json")
CONFIG_LOCAL = os.path.join(RAIZ, "config.json")
PASTA_CACHE = os.path.join(RAIZ, ".cache")
PASTA_DIST = os.path.join(RAIZ, "dist")

WIDGET_ID = "MinhaTV"
WIDGET_TITULO = "Minha TV"
ZIP_NOME = "MinhaTV.zip"

TIPOS = {
    ".html": "text/html; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".xml": "application/xml; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".info": "text/plain; charset=utf-8",
}

TAMANHOS_TMDB = ("w92", "w154", "w185", "w300", "w342", "w500", "w780")
IMAGEM_TMDB = re.compile(r"^/api/img/(w\d+)/([A-Za-z0-9_-]{5,80}\.(?:jpg|png))$")

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(errors="replace")
    sys.stderr.reconfigure(errors="replace")


# ---------------------------------------------------------------- configuração

def carregar_config():
    """Lê config.json (cria a partir do exemplo na primeira vez)."""
    if not os.path.exists(CONFIG_LOCAL):
        shutil.copyfile(CONFIG_EXEMPLO, CONFIG_LOCAL)
        print("Criei o arquivo config.json (cidade, apps e chave do TMDB). Edite quando quiser.")
    try:
        with open(CONFIG_LOCAL, encoding="utf-8") as f:
            cfg = json.load(f)
    except ValueError as erro:
        sys.exit("O config.json tem um erro de digitação: %s\nConfira vírgulas e aspas." % erro)
    with open(CONFIG_EXEMPLO, encoding="utf-8") as f:
        padrao = json.load(f)
    for chave, valor in padrao.items():
        cfg.setdefault(chave, valor)
    cfg["tmdb_chave"] = str(cfg.get("tmdb_chave") or "").strip()
    return cfg


def config_publica(cfg):
    """O que o app da TV pode ver (nunca a chave do TMDB)."""
    apps = []
    for app in cfg.get("apps") or []:
        if isinstance(app, dict) and app.get("nome") and app.get("id"):
            apps.append({"nome": str(app["nome"]), "id": str(app["id"]), "cor": str(app.get("cor") or "")})
    return {
        "cidade": cfg.get("cidade") or "",
        "latitude": float(cfg.get("latitude")),
        "longitude": float(cfg.get("longitude")),
        "apps": apps,
        "filmes": bool(cfg.get("tmdb_chave")),
        "versao": VERSAO,
    }


# ---------------------------------------------------------------- rede

def ip_local():
    """IP deste PC na rede de casa (o que a TV deve usar)."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))  # não envia nada, só escolhe a interface de saída
        return s.getsockname()[0]
    except OSError:
        return socket.gethostbyname(socket.gethostname())
    finally:
        s.close()


def contexto_ssl():
    try:
        import certifi  # quando existir (macOS costuma precisar)
        return ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        return ssl.create_default_context()


SSL = contexto_ssl()


def baixar(url, cabecalhos=None, limite=12):
    pedido = Request(url, headers=dict({"User-Agent": "MinhaTV/" + VERSAO}, **(cabecalhos or {})))
    with urlopen(pedido, timeout=limite, context=SSL) as resposta:
        return resposta.read()


def baixar_json(url, cabecalhos=None):
    return json.loads(baixar(url, dict({"Accept": "application/json"}, **(cabecalhos or {}))).decode("utf-8"))


class Cache:
    """Guarda respostas por um tempo; se a internet falhar, devolve a última que deu certo."""

    def __init__(self):
        self.itens = {}
        self.trava = threading.Lock()

    def obter(self, chave, validade, buscar):
        agora = time.time()
        with self.trava:
            item = self.itens.get(chave)
        if item and agora - item[0] < validade:
            return item[1]
        try:
            valor = buscar()
        except Exception:
            if item:
                return item[1]
            raise
        with self.trava:
            self.itens[chave] = (agora, valor)
        return valor


CACHE = Cache()


# ---------------------------------------------------------------- clima e TMDB

def clima(cfg):
    consulta = urlencode({
        "latitude": cfg["latitude"],
        "longitude": cfg["longitude"],
        "current": "temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day",
        "daily": "weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset",
        "timezone": "auto",
        "forecast_days": 5,
    })
    dados = CACHE.obter("clima:%s,%s" % (cfg["latitude"], cfg["longitude"]), 10 * 60,
                        lambda: baixar_json("https://api.open-meteo.com/v1/forecast?" + consulta))
    return dict(dados, cidade=cfg.get("cidade") or "")


def tmdb(cfg, caminho, parametros):
    chave = cfg["tmdb_chave"]
    parametros = dict(parametros)
    cabecalhos = {}
    if chave.startswith("eyJ"):  # "Token de leitura da API" (v4)
        cabecalhos["Authorization"] = "Bearer " + chave
    else:  # "Chave da API" (v3)
        parametros["api_key"] = chave
    return baixar_json("https://api.themoviedb.org/3" + caminho + "?" + urlencode(parametros), cabecalhos)


def imagem(caminho, tamanho):
    return "/api/img/%s%s" % (tamanho, caminho) if caminho else ""


def em_alta(cfg, tipo):
    def buscar():
        dados = tmdb(cfg, "/trending/%s/week" % tipo, {"language": "pt-BR"})
        itens = []
        for r in dados.get("results") or []:
            if not r.get("poster_path"):
                continue
            itens.append({
                "id": r["id"],
                "tipo": tipo,
                "titulo": r.get("title") or r.get("name") or "",
                "ano": (r.get("release_date") or r.get("first_air_date") or "")[:4],
                "nota": round(float(r.get("vote_average") or 0), 1),
                "sinopse": r.get("overview") or "",
                "poster": imagem(r.get("poster_path"), "w185"),
                "fundo": imagem(r.get("backdrop_path"), "w780"),
            })
        return {"itens": itens}
    return CACHE.obter("alta:" + tipo, 60 * 60, buscar)


def duracao(minutos):
    if not minutos:
        return ""
    h, m = divmod(int(minutos), 60)
    return ("%dh %02dmin" % (h, m)) if h else ("%d min" % m)


def titulo(cfg, tipo, id_titulo):
    def buscar():
        r = tmdb(cfg, "/%s/%s" % (tipo, id_titulo), {"language": "pt-BR", "append_to_response": "watch/providers"})
        regiao = (cfg.get("tmdb_regiao") or "BR").upper()
        provedores = ((r.get("watch/providers") or {}).get("results") or {}).get(regiao) or {}
        onde = [p.get("provider_name") for p in provedores.get("flatrate") or [] if p.get("provider_name")]
        if tipo == "movie":
            tempo = duracao(r.get("runtime"))
        else:
            temporadas = r.get("number_of_seasons") or 0
            tempo = ("%d temporada%s" % (temporadas, "s" if temporadas != 1 else "")) if temporadas else ""
        return {
            "titulo": r.get("title") or r.get("name") or "",
            "ano": (r.get("release_date") or r.get("first_air_date") or "")[:4],
            "nota": round(float(r.get("vote_average") or 0), 1),
            "duracao": tempo,
            "generos": [g.get("name") for g in r.get("genres") or [] if g.get("name")],
            "sinopse": r.get("overview") or "",
            "onde": onde,
            "fundo": imagem(r.get("backdrop_path"), "w780"),
        }
    return CACHE.obter("titulo:%s:%s" % (tipo, id_titulo), 24 * 60 * 60, buscar)


def imagem_tmdb(tamanho, arquivo):
    """Baixa a imagem do TMDB uma vez e guarda no disco (.cache/img)."""
    pasta = os.path.join(PASTA_CACHE, "img")
    destino = os.path.join(pasta, "%s_%s" % (tamanho, arquivo))
    if not os.path.exists(destino):
        dados = baixar("https://image.tmdb.org/t/p/%s/%s" % (tamanho, arquivo), limite=20)
        os.makedirs(pasta, exist_ok=True)
        temporario = destino + ".tmp"
        with open(temporario, "wb") as f:
            f.write(dados)
        os.replace(temporario, destino)
    with open(destino, "rb") as f:
        return f.read()


# ---------------------------------------------------------------- pacote do app

def para_ascii_js(texto):
    """Acentos viram \\uXXXX: a TV lê o arquivo certo seja qual for a codificação que assumir."""
    saida = []
    for c in texto:
        n = ord(c)
        if n < 128:
            saida.append(c)
        elif n <= 0xFFFF:
            saida.append("\\u%04x" % n)
        else:
            n -= 0x10000
            saida.append("\\u%04x\\u%04x" % (0xD800 + (n >> 10), 0xDC00 + (n & 0x3FF)))
    return "".join(saida)


def para_ascii_html(texto):
    return "".join(c if ord(c) < 128 else "&#%d;" % ord(c) for c in texto)


def preparar_arquivo(relativo, dados):
    if relativo.endswith(".js"):
        return para_ascii_js(dados.decode("utf-8")).encode("ascii")
    if relativo.endswith(".html"):
        return para_ascii_html(dados.decode("utf-8")).encode("ascii")
    if relativo.endswith(".css"):  # sem comentários (onde ficam os acentos): só ASCII para a TV
        return re.sub(r"/\*.*?\*/\s*", "", dados.decode("utf-8"), flags=re.S).encode("ascii")
    return dados


def config_js(cfg, servidor):
    publica = dict(config_publica(cfg), servidor=servidor, gerado=time.strftime("%Y-%m-%d %H:%M"))
    texto = "// Gerado pelo servidor do PC. Não edite: mude o config.json e sincronize de novo.\n"
    texto += "var CONFIG = " + json.dumps(publica, ensure_ascii=True, indent=1) + ";\n"
    return para_ascii_js(texto).encode("ascii")


def arquivos_do_app():
    for pasta, subpastas, arquivos in os.walk(PASTA_APP):
        subpastas[:] = sorted(p for p in subpastas if not p.startswith("."))
        for nome in sorted(arquivos):
            if nome.startswith("."):
                continue
            caminho = os.path.join(pasta, nome)
            relativo = os.path.relpath(caminho, PASTA_APP).replace(os.sep, "/")
            if relativo != "js/config.js":
                yield relativo, caminho


def gerar_zip(cfg, servidor):
    """O app empacotado do jeito que a TV instala (config.xml na raiz do zip)."""
    memoria = io.BytesIO()
    data = time.localtime()[:6]
    with zipfile.ZipFile(memoria, "w", zipfile.ZIP_DEFLATED) as z:
        def gravar(relativo, conteudo):
            info = zipfile.ZipInfo(relativo, date_time=data)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, conteudo)
        for relativo, caminho in arquivos_do_app():
            with open(caminho, "rb") as f:
                gravar(relativo, preparar_arquivo(relativo, f.read()))
        gravar("js/config.js", config_js(cfg, servidor))
    return memoria.getvalue()


def widgetlist(tamanho_zip, endereco):
    return (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<rsp stat="ok">\n'
        '<list>\n'
        '<widget id="%s">\n'
        '<title>%s</title>\n'
        '<compression size="%d" type="zip"/>\n'
        '<description>Tela inicial limpa: apps, clima e filmes em alta.</description>\n'
        '<download>%s/%s</download>\n'
        '</widget>\n'
        '</list>\n'
        '</rsp>\n'
    ) % (WIDGET_ID, WIDGET_TITULO, tamanho_zip, endereco, ZIP_NOME)


# ---------------------------------------------------------------- HTTP

class Servidor(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, endereco, ip, porta, verboso):
        ThreadingHTTPServer.__init__(self, endereco, Pedido)
        self.ip = ip
        self.porta = porta
        self.verboso = verboso
        self.trava_zip = threading.Lock()
        self.zip = None

    def endereco_publico(self):
        return "http://%s" % self.ip + ("" if self.porta == 80 else ":%d" % self.porta)


class Pedido(BaseHTTPRequestHandler):
    server_version = "MinhaTV/" + VERSAO

    def log_message(self, formato, *args):
        if self.server.verboso:
            sys.stdout.write("[%s] %s\n" % (time.strftime("%H:%M:%S"), formato % args))

    def avisar(self, texto):
        sys.stdout.write("[%s] %s\n" % (time.strftime("%H:%M:%S"), texto))
        sys.stdout.flush()

    def responder(self, codigo, tipo, corpo, extras=None):
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Access-Control-Allow-Origin", "*")
        for chave, valor in (extras or {}).items():
            self.send_header(chave, valor)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corpo)

    def responder_json(self, dados, codigo=200):
        corpo = json.dumps(dados, ensure_ascii=True).encode("ascii")
        self.responder(codigo, TIPOS[".json"], corpo, {"Cache-Control": "no-cache"})

    def erro_json(self, codigo, mensagem):
        self.responder_json({"erro": mensagem}, codigo)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        url = urlparse(self.path)
        caminho = url.path
        consulta = parse_qs(url.query)
        try:
            if caminho == "/widgetlist.xml":
                self.lista_de_widgets()
            elif caminho == "/" + ZIP_NOME:
                self.pacote()
            elif caminho.startswith("/api/"):
                self.api(caminho, consulta)
            elif caminho == "/app" or caminho == "/app/":
                self.responder(302, "text/plain", b"", {"Location": "/app/index.html"})
            elif caminho.startswith("/app/"):
                self.arquivo_do_app(caminho[len("/app/"):])
            elif caminho == "/":
                self.inicio()
            else:
                self.responder(404, "text/plain; charset=utf-8", "Não encontrado".encode("utf-8"))
        except (BrokenPipeError, ConnectionResetError):
            pass

    # ---- instalação na TV

    def montar_zip(self):
        cfg = carregar_config()
        with self.server.trava_zip:
            self.server.zip = gerar_zip(cfg, self.server.endereco_publico())
            return self.server.zip

    def lista_de_widgets(self):
        dados = self.montar_zip()
        if self.command == "GET":
            self.avisar("A TV pediu a lista de apps (%s). Sincronização começou." % self.client_address[0])
        xml = widgetlist(len(dados), self.server.endereco_publico()).encode("utf-8")
        self.responder(200, TIPOS[".xml"], xml, {"Cache-Control": "no-cache"})

    def pacote(self):
        with self.server.trava_zip:
            dados = self.server.zip
        if dados is None:
            dados = self.montar_zip()
        if self.command == "GET":
            self.avisar("A TV baixou o app (%d KB). Veja a TV: ela instala sozinha." % (len(dados) // 1024))
        self.responder(200, "application/zip", dados, {"Cache-Control": "no-cache"})

    # ---- pré-visualização no navegador

    def arquivo_do_app(self, relativo):
        if relativo == "js/config.js":
            self.responder(200, TIPOS[".js"], config_js(carregar_config(), ""), {"Cache-Control": "no-cache"})
            return
        caminho = os.path.realpath(os.path.join(PASTA_APP, relativo))
        if not caminho.startswith(os.path.realpath(PASTA_APP) + os.sep) or not os.path.isfile(caminho):
            self.responder(404, "text/plain; charset=utf-8", "Não encontrado".encode("utf-8"))
            return
        with open(caminho, "rb") as f:
            dados = preparar_arquivo(relativo, f.read())
        tipo = TIPOS.get(os.path.splitext(caminho)[1].lower(), "application/octet-stream")
        self.responder(200, tipo, dados, {"Cache-Control": "no-cache"})

    def inicio(self):
        endereco = self.server.endereco_publico()
        html = """<!DOCTYPE html><html lang="pt-BR"><head><meta charset="utf-8"><title>Minha TV</title>
<style>body{font-family:system-ui,Arial,sans-serif;background:#0b0e13;color:#e8ebf0;max-width:720px;margin:40px auto;padding:0 16px;line-height:1.5}
a{color:#8fb6e8}code{background:#1b212b;padding:2px 6px;border-radius:4px}</style></head><body>
<h1>Minha TV: servidor ligado</h1>
<p>Na TV, use este IP: <code>%s</code></p>
<p><a href="/app/index.html">Abrir a pré-visualização do app</a>. Use as setas, Enter, Backspace (RETURN) e R (vermelho).</p>
<p>Instalação: <code>%s/widgetlist.xml</code></p>
</body></html>""" % (self.server.ip, endereco)
        self.responder(200, TIPOS[".html"], html.encode("utf-8"))

    # ---- dados para o app

    def api(self, caminho, consulta):
        cfg = carregar_config()
        if caminho == "/api/status":
            agora = time.time()
            self.responder_json({
                "ok": True,
                "versao": VERSAO,
                "utc_ms": int(agora * 1000),
                "fuso_min": int(time.localtime(agora).tm_gmtoff // 60),
                "filmes": bool(cfg["tmdb_chave"]),
            })
            return
        if caminho == "/api/config":
            self.responder_json(config_publica(cfg))
            return
        try:
            if caminho == "/api/clima":
                self.responder_json(clima(cfg))
            elif caminho == "/api/em-alta":
                tipo = (consulta.get("tipo") or [""])[0]
                if tipo not in ("movie", "tv"):
                    self.erro_json(400, "tipo deve ser movie ou tv")
                elif not cfg["tmdb_chave"]:
                    self.erro_json(503, "sem chave do TMDB no config.json")
                else:
                    self.responder_json(em_alta(cfg, tipo))
            elif caminho == "/api/titulo":
                tipo = (consulta.get("tipo") or [""])[0]
                id_titulo = (consulta.get("id") or [""])[0]
                if tipo not in ("movie", "tv") or not id_titulo.isdigit():
                    self.erro_json(400, "pedido inválido")
                elif not cfg["tmdb_chave"]:
                    self.erro_json(503, "sem chave do TMDB no config.json")
                else:
                    self.responder_json(titulo(cfg, tipo, id_titulo))
            elif IMAGEM_TMDB.match(caminho):
                tamanho, arquivo = IMAGEM_TMDB.match(caminho).groups()
                if tamanho not in TAMANHOS_TMDB:
                    self.erro_json(400, "tamanho inválido")
                    return
                tipo = TIPOS[".png"] if arquivo.endswith(".png") else TIPOS[".jpg"]
                self.responder(200, tipo, imagem_tmdb(tamanho, arquivo), {"Cache-Control": "max-age=86400"})
            else:
                self.erro_json(404, "não encontrado")
        except HTTPError as erro:
            if erro.code == 401:
                self.erro_json(502, "chave do TMDB recusada")
            else:
                self.erro_json(502, "o serviço respondeu %d" % erro.code)
        except (URLError, socket.timeout, OSError, ValueError) as erro:
            self.avisar("Falha ao buscar %s: %s" % (caminho, erro))
            self.erro_json(502, "sem internet no PC")


# ---------------------------------------------------------------- início

def explicar_porta_ocupada(porta, erro):
    print("\nNão consegui usar a porta %d: %s" % (porta, erro))
    if porta == 80:
        print("- Outro programa já usa a porta 80 (Skype antigo, IIS, outro servidor). Feche-o e tente de novo.")
        print("- No Linux, rode com sudo (portas abaixo de 1024 pedem administrador).")
    sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="Servidor da Minha TV (Samsung 2013).")
    parser.add_argument("--porta", type=int, default=80, help="porta HTTP (a TV só sincroniza pela 80)")
    parser.add_argument("--ip", help="IP deste PC na rede (descoberto sozinho se omitido)")
    parser.add_argument("--gerar", action="store_true", help="só gera dist/MinhaTV.zip e dist/widgetlist.xml")
    parser.add_argument("--verboso", action="store_true", help="mostra todos os pedidos")
    args = parser.parse_args()

    ip = args.ip or ip_local()
    cfg = carregar_config()

    if args.gerar:
        endereco = "http://%s" % ip + ("" if args.porta == 80 else ":%d" % args.porta)
        dados = gerar_zip(cfg, endereco)
        os.makedirs(PASTA_DIST, exist_ok=True)
        with open(os.path.join(PASTA_DIST, ZIP_NOME), "wb") as f:
            f.write(dados)
        with open(os.path.join(PASTA_DIST, "widgetlist.xml"), "w", encoding="utf-8") as f:
            f.write(widgetlist(len(dados), endereco))
        print("Gerado: dist/%s (%d KB) e dist/widgetlist.xml" % (ZIP_NOME, len(dados) // 1024))
        return

    try:
        servidor = Servidor(("0.0.0.0", args.porta), ip, args.porta, args.verboso)
    except PermissionError as erro:
        explicar_porta_ocupada(args.porta, erro)
    except OSError as erro:
        explicar_porta_ocupada(args.porta, erro)

    linha = "=" * 64
    print(linha)
    print(" Minha TV: servidor ligado (versão %s)" % VERSAO)
    print(linha)
    print(" IP deste PC ........ %s" % ip)
    print(" Pré-visualização ... %s/app/" % servidor.endereco_publico())
    print(" Cidade do clima .... %s" % (cfg.get("cidade") or "?"))
    print(" Filmes em alta ..... %s" % ("ligado" if cfg["tmdb_chave"] else "desligado (falta a chave do TMDB no config.json)"))
    print(linha)
    print(" Na TV (logada como 'develop'):")
    print("   Smart Hub > Apps > Mais Apps > Opções > Configuração de IP > %s" % ip)
    print("   Opções > Iniciar sincronização de apps")
    print(" Deixe esta janela aberta: o clima e os filmes da TV passam por aqui.")
    print(" Para desligar: Ctrl+C")
    print(linha)
    if ip.startswith("127.") or ip.startswith("169.254."):
        print(" ATENÇÃO: o IP %s não é da rede de casa. Confira a conexão ou use --ip." % ip)
    sys.stdout.flush()

    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        print("\nServidor desligado.")


if __name__ == "__main__":
    main()
