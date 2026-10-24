"""Acesso à internet e à rede de casa, usado pelo servidor e pelos módulos de vídeo."""

import json
import ssl
import threading
import time
from urllib.request import Request, urlopen

VERSAO = "1.1.0"
AGENTE = "MinhaTV/" + VERSAO


def contexto_ssl():
    try:
        import certifi  # quando existir (macOS costuma precisar)
        return ssl.create_default_context(cafile=certifi.where())
    except ImportError:
        return ssl.create_default_context()


SSL = contexto_ssl()


def abrir(url, cabecalhos=None, limite=15, dados=None, metodo=None):
    """Abre a URL e devolve a resposta aberta (para ler aos poucos). Feche depois."""
    corpo = None
    cab = {"User-Agent": AGENTE}
    cab.update(cabecalhos or {})
    if dados is not None:
        corpo = json.dumps(dados).encode("utf-8")
        cab["Content-Type"] = "application/json"
    pedido = Request(url, data=corpo, headers=cab, method=metodo)
    contexto = SSL if url.startswith("https:") else None
    return urlopen(pedido, timeout=limite, context=contexto)


def baixar(url, cabecalhos=None, limite=12, dados=None, metodo=None):
    with abrir(url, cabecalhos, limite, dados, metodo) as resposta:
        return resposta.read()


def baixar_json(url, cabecalhos=None, dados=None, metodo=None):
    cab = {"Accept": "application/json"}
    cab.update(cabecalhos or {})
    corpo = baixar(url, cab, dados=dados, metodo=metodo)
    return json.loads(corpo.decode("utf-8")) if corpo.strip() else None


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

    def limpar(self, prefixo):
        with self.trava:
            for chave in [c for c in self.itens if c.startswith(prefixo)]:
                del self.itens[chave]


CACHE = Cache()
