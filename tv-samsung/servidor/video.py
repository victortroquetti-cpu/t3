"""
Canais ao vivo e repasse de vídeo pelo PC.

Os canais vêm do config.json: a lista "canais" e as listas M3U de "canais_listas".
Quem escolhe as fontes é o dono da TV; o projeto não traz nenhuma.

A TV de 2013 nem sempre abre HTTPS moderno. Por isso o PC busca a playlist HLS (.m3u8),
troca cada endereço dentro dela por um endereço do próprio PC (/api/rel/...) e entrega
tudo em HTTP simples. Só passam pelo repasse os servidores que vieram da configuração
ou de uma playlist já repassada.
"""

import base64
import hashlib
import re
import threading
from urllib.parse import urljoin, urlparse

import rede

EXTENSOES = ("ts", "aac", "mp3", "mp4", "m4s", "m4a", "vtt", "key", "bin")
URI_ATRIBUTO = re.compile(r'URI="([^"]+)"')


class Repasse:
    def __init__(self):
        self.hosts = set()
        self.trava = threading.Lock()

    def permitir(self, url):
        host = urlparse(url).netloc.lower()
        if host:
            with self.trava:
                self.hosts.add(host)

    def permitido(self, url):
        partes = urlparse(url)
        with self.trava:
            return partes.scheme in ("http", "https") and partes.netloc.lower() in self.hosts


REPASSE = Repasse()


def codificar(url):
    return base64.urlsafe_b64encode(url.encode("utf-8")).decode("ascii").rstrip("=")


def decodificar(texto):
    texto += "=" * (-len(texto) % 4)
    return base64.urlsafe_b64decode(texto.encode("ascii")).decode("utf-8")


def e_hls(url):
    return ".m3u8" in urlparse(url).path.lower()


def caminho_repasse(url, playlist):
    """Endereço no PC que entrega a URL original para a TV."""
    REPASSE.permitir(url)
    if playlist:
        extensao = "m3u8"
    else:
        final = urlparse(url).path.rsplit(".", 1)
        extensao = final[1].lower() if len(final) == 2 and final[1].lower() in EXTENSOES else "ts"
    return "/api/rel/%s.%s" % (codificar(url), extensao)


def reescrever(texto, base):
    """Troca os endereços de uma playlist HLS pelos do repasse."""
    saida = []
    proxima_e_playlist = False
    for linha in texto.splitlines():
        limpa = linha.strip()
        if not limpa:
            continue
        if limpa.startswith("#"):
            if limpa.startswith("#EXT-X-STREAM-INF"):
                proxima_e_playlist = True
            if 'URI="' in limpa:
                lista = limpa.startswith(("#EXT-X-MEDIA", "#EXT-X-I-FRAME-STREAM-INF"))
                limpa = URI_ATRIBUTO.sub(
                    lambda m: 'URI="%s"' % caminho_repasse(urljoin(base, m.group(1)), lista), limpa)
            saida.append(limpa)
            continue
        absoluta = urljoin(base, limpa)
        saida.append(caminho_repasse(absoluta, proxima_e_playlist or e_hls(absoluta)))
        proxima_e_playlist = False
    return "\n".join(saida) + "\n"


def ler_m3u(texto):
    """Canais de uma lista M3U (#EXTINF:-1 ...,Nome  /  URL)."""
    canais = []
    nome = None
    for linha in texto.splitlines():
        linha = linha.strip()
        if linha.startswith("#EXTINF"):
            nome = linha.split(",", 1)[1].strip() if "," in linha else "Canal"
        elif linha and not linha.startswith("#") and nome:
            canais.append({"nome": nome, "url": linha})
            nome = None
    return canais


def lista_de_canais(cfg):
    canais = []
    for canal in cfg.get("canais") or []:
        if isinstance(canal, dict) and canal.get("nome") and canal.get("url"):
            canais.append({"nome": str(canal["nome"]), "url": str(canal["url"]).strip()})
    for endereco in cfg.get("canais_listas") or []:
        try:
            texto = rede.CACHE.obter(
                "m3u:" + endereco, 6 * 60 * 60,
                lambda endereco=endereco: rede.baixar(endereco, limite=20).decode("utf-8", "replace"))
            canais.extend(ler_m3u(texto)[:300])
        except Exception:
            pass  # uma lista fora do ar não derruba as outras
    vistos = set()
    unicos = []
    for canal in canais:
        canal["id"] = hashlib.sha1(canal["url"].encode("utf-8")).hexdigest()[:12]
        if canal["id"] not in vistos:
            vistos.add(canal["id"])
            unicos.append(canal)
    return unicos


def endereco_para_tv(url, pelo_pc):
    """O que a TV abre: o repasse do PC (padrão) ou a URL original."""
    hls = e_hls(url)
    if not pelo_pc:
        return {"url": url, "hls": hls}
    return {"url": caminho_repasse(url, hls), "hls": hls}
