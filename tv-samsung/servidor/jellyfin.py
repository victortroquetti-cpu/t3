"""
Jellyfin: os filmes e séries do PC na TV.

O servidor entra no Jellyfin com o usuário e a senha do config.json, como qualquer app
do Jellyfin faria, e pede o vídeo já em H.264 + AAC em HLS, que a TV de 2013 toca. O
Jellyfin converte o que for preciso. A posição de "continuar assistindo" volta para o
Jellyfin quando o vídeo para.
"""

import hashlib
import threading
import uuid
from urllib.error import HTTPError
from urllib.parse import urlencode

import rede

CLIENTE = "Minha TV"
DISPOSITIVO = "TV Samsung"
TICKS_POR_MS = 10000
CAMPOS = "Limit=24&Fields=Overview&EnableUserData=true&ImageTypeLimit=1"
ID_VALIDO = "0123456789abcdef-"


def id_valido(texto):
    return 0 < len(texto) <= 40 and all(c in ID_VALIDO for c in texto.lower())


def lista(resposta):
    if isinstance(resposta, dict):
        return resposta.get("Items") or []
    return resposta or []


class Jellyfin:
    def __init__(self):
        self.trava = threading.Lock()
        self.login = None
        self.token = None
        self.usuario_id = None

    # ---- conexão

    @staticmethod
    def dados(cfg):
        j = cfg.get("jellyfin") or {}
        return (str(j.get("endereco") or "").rstrip("/"), str(j.get("usuario") or ""), str(j.get("senha") or ""))

    @classmethod
    def configurado(cls, cfg):
        endereco, usuario, _ = cls.dados(cfg)
        return bool(endereco and usuario)

    @staticmethod
    def dispositivo(usuario):
        return "minhatv-" + hashlib.sha1(usuario.encode("utf-8")).hexdigest()[:12]

    def cabecalhos(self, usuario, token=None):
        valor = 'MediaBrowser Client="%s", Device="%s", DeviceId="%s", Version="%s"' % (
            CLIENTE, DISPOSITIVO, self.dispositivo(usuario), rede.VERSAO)
        if token:
            valor += ', Token="%s"' % token
        cab = {"Authorization": valor, "X-Emby-Authorization": valor}
        if token:
            cab["X-Emby-Token"] = token
        return cab

    def entrar(self, cfg):
        endereco, usuario, senha = self.dados(cfg)
        resposta = rede.baixar_json(endereco + "/Users/AuthenticateByName", self.cabecalhos(usuario),
                                    dados={"Username": usuario, "Pw": senha}, metodo="POST")
        with self.trava:
            self.login = (endereco, usuario, senha)
            self.token = resposta["AccessToken"]
            self.usuario_id = resposta["User"]["Id"]

    def pedir(self, cfg, caminho, dados=None, metodo=None, json=True):
        endereco, usuario, senha = self.dados(cfg)
        for tentativa in (1, 2):
            with self.trava:
                precisa_entrar = self.token is None or self.login != (endereco, usuario, senha)
            if precisa_entrar:
                self.entrar(cfg)
            url = endereco + caminho.replace("{uid}", self.usuario_id)
            cab = self.cabecalhos(usuario, self.token)
            try:
                if json:
                    return rede.baixar_json(url, cab, dados=dados, metodo=metodo)
                return rede.baixar(url, cab, dados=dados, metodo=metodo)
            except HTTPError as erro:
                if erro.code == 401 and tentativa == 1:
                    with self.trava:
                        self.token = None  # sessão vencida: entra de novo
                    continue
                raise

    def pedir_um_destes(self, cfg, caminhos):
        """O Jellyfin 10.9 trocou alguns endereços: tenta o antigo e o novo."""
        erro_final = None
        for caminho in caminhos:
            try:
                return self.pedir(cfg, caminho)
            except HTTPError as erro:
                if erro.code != 404:
                    raise
                erro_final = erro
        raise erro_final

    # ---- conteúdo

    @staticmethod
    def resumir(item):
        episodio = item.get("Type") == "Episode"
        if episodio:
            titulo = item.get("SeriesName") or item.get("Name") or ""
            subtitulo = "T%s E%s · %s" % (item.get("ParentIndexNumber", "?"), item.get("IndexNumber", "?"),
                                              item.get("Name") or "")
            poster = item.get("SeriesId") if item.get("SeriesPrimaryImageTag") else item.get("Id")
        else:
            titulo = item.get("Name") or ""
            subtitulo = ""
            poster = item.get("Id")
        fundo = item.get("Id") if item.get("BackdropImageTags") else item.get("ParentBackdropItemId")
        return {
            "id": item.get("Id"),
            "tipo": "jellyfin",
            "titulo": titulo,
            "subtitulo": subtitulo,
            "ano": str(item.get("ProductionYear") or ""),
            "sinopse": item.get("Overview") or "",
            "duracao_ms": int((item.get("RunTimeTicks") or 0) / TICKS_POR_MS),
            "posicao_ms": int(((item.get("UserData") or {}).get("PlaybackPositionTicks") or 0) / TICKS_POR_MS),
            "poster": ("/api/jf/img/%s" % poster) if poster else "",
            "fundo": ("/api/jf/fundo/%s" % fundo) if fundo else "",
        }

    def inicio(self, cfg):
        def buscar():
            continuar = self.pedir_um_destes(cfg, [
                "/Users/{uid}/Items/Resume?MediaTypes=Video&" + CAMPOS,
                "/UserItems/Resume?userId={uid}&MediaTypes=Video&" + CAMPOS])
            proximos = self.pedir(cfg, "/Shows/NextUp?UserId={uid}&" + CAMPOS)
            filmes = self.pedir_um_destes(cfg, [
                "/Users/{uid}/Items/Latest?IncludeItemTypes=Movie&" + CAMPOS,
                "/Items/Latest?userId={uid}&IncludeItemTypes=Movie&" + CAMPOS])
            return {
                "continuar": [self.resumir(i) for i in lista(continuar) if i.get("Id")],
                "proximos": [self.resumir(i) for i in lista(proximos) if i.get("Id")],
                "filmes": [self.resumir(i) for i in lista(filmes) if i.get("Id")],
            }
        return rede.CACHE.obter("jf:inicio", 60, buscar)

    def imagem(self, cfg, item_id, fundo):
        if fundo:
            caminho = "/Items/%s/Images/Backdrop/0?maxWidth=780&quality=80" % item_id
        else:
            caminho = "/Items/%s/Images/Primary?maxHeight=330&quality=85" % item_id
        return rede.CACHE.obter("jfimg:%s:%s" % (item_id, fundo), 60 * 60,
                                lambda: self.pedir(cfg, caminho, json=False))

    def tocar(self, cfg, item_id, do_inicio):
        """Endereço HLS do Jellyfin (H.264 + AAC, até 1080p) e de onde começar."""
        item = self.pedir_um_destes(cfg, ["/Users/{uid}/Items/%s" % item_id, "/Items/%s?userId={uid}" % item_id])
        fontes = item.get("MediaSources") or [{"Id": item_id}]
        fonte = fontes[0].get("Id") or item_id
        posicao = 0 if do_inicio else int((item.get("UserData") or {}).get("PlaybackPositionTicks") or 0)
        sessao = uuid.uuid4().hex
        endereco, usuario, _ = self.dados(cfg)
        parametros = {
            "MediaSourceId": fonte,
            "DeviceId": self.dispositivo(usuario),
            "PlaySessionId": sessao,
            "api_key": self.token,
            "VideoCodec": "h264",
            "AudioCodec": "aac,mp3",
            "MaxAudioChannels": 2,
            "TranscodingMaxAudioChannels": 2,
            "MaxStreamingBitrate": 10000000,
            "MaxWidth": 1920,
            "MaxHeight": 1080,
            "SegmentContainer": "ts",
            "BreakOnNonKeyFrames": "True",
            "h264-profile": "high,main,baseline",
            "h264-level": "41",
            "StartTimeTicks": posicao,
        }
        resumo = self.resumir(item)
        return {
            "url": endereco + "/Videos/%s/master.m3u8?%s" % (item_id, urlencode(parametros)),
            "fonte": fonte,
            "sessao": sessao,
            "inicio_ms": int(posicao / TICKS_POR_MS),
            "duracao_ms": resumo["duracao_ms"],
            "titulo": resumo["titulo"],
            "subtitulo": resumo["subtitulo"],
        }

    def relatar(self, cfg, evento, item_id, fonte, sessao, posicao_ms, pausado):
        caminhos = {"inicio": "/Sessions/Playing", "progresso": "/Sessions/Playing/Progress",
                    "fim": "/Sessions/Playing/Stopped"}
        corpo = {"ItemId": item_id, "MediaSourceId": fonte, "PlaySessionId": sessao,
                 "PositionTicks": int(posicao_ms) * TICKS_POR_MS, "IsPaused": bool(pausado),
                 "CanSeek": True, "PlayMethod": "Transcode"}
        self.pedir(cfg, caminhos[evento], dados=corpo, metodo="POST", json=False)
        if evento == "fim":
            _, usuario, _ = self.dados(cfg)
            try:  # o Jellyfin para de converter o vídeo
                self.pedir(cfg, "/Videos/ActiveEncodings?" + urlencode(
                    {"DeviceId": self.dispositivo(usuario), "PlaySessionId": sessao}), metodo="DELETE", json=False)
            except Exception:
                pass
            rede.CACHE.limpar("jf:inicio")


JELLYFIN = Jellyfin()
