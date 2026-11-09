#!/usr/bin/env python3
"""
trok-mcp: servidor MCP para o Claude Code (ou o Codex) testar o Rearmed sozinho.

Dá ao agente ferramentas para abrir o jogo, mandar comandos ao console, ler o
console e os erros, tirar print da tela, consultar o estado da partida e
compilar modelos e mapas.

Sem dependências: só Python 3.8+ da instalação padrão. Fala MCP por stdio
(JSON-RPC, uma mensagem por linha).

Como o agente fala com o jogo:
  - netcon: o jogo é aberto com -netconport 2121 e o servidor conecta nessa porta
    (TCP, texto). Os comandos vão por ali e a saída do console volta pelo mesmo caminho.
  - RCON (alternativa): se TROK_RCON_SENHA estiver definida e o netcon não
    conectar, usa o protocolo RCON do Source (servidor com rcon_password).
  - console.log: o jogo é aberto com -condebug; a leitura dos erros usa esse
    arquivo e funciona mesmo sem netcon.

Configuração por variáveis de ambiente (todas opcionais; o servidor tenta descobrir sozinho):
  TROK_STEAM          pasta da Steam
  TROK_SDK_BASE       pasta do "Source SDK Base 2013 Multiplayer"
  TROK_MOD_DIR        pasta do mod (padrão: <Steam>/steamapps/sourcemods/mod_trok)
  TROK_JOGO_EXE       executável do jogo (padrão: hl2_win64.exe ou hl2.exe do SDK Base)
  TROK_NETCON_PORTA   porta do netcon (padrão 2121)
  TROK_NETCON_SENHA   senha do netcon, se o jogo for aberto com -netconpassword
  TROK_RCON_HOST      host do RCON (padrão 127.0.0.1)
  TROK_RCON_PORTA     porta do RCON (padrão 27015)
  TROK_RCON_SENHA     senha do RCON (rcon_password)
"""

import base64
import collections
import glob
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time

VERSAO = "0.1.0"
PROTOCOLOS = ("2025-06-18", "2025-03-26", "2024-11-05")
STEAM_APPID_SDK = "243750"


def log(msg):
    sys.stderr.write("[trok-mcp] %s\n" % msg)
    sys.stderr.flush()


class ErroTrok(Exception):
    """Erro mostrado ao agente como resultado da ferramenta (isError)."""


# ---------------------------------------------------------------------------
# Caminhos
# ---------------------------------------------------------------------------

def _env(nome, padrao=None):
    v = os.environ.get(nome)
    return v if v not in (None, "") else padrao


def steam_dir():
    env = _env("TROK_STEAM")
    if env:
        return env
    if os.name == "nt":
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
                return winreg.QueryValueEx(k, "SteamPath")[0].replace("/", "\\")
        except OSError:
            pass
        return r"C:\Program Files (x86)\Steam"
    for p in ("~/.local/share/Steam", "~/.steam/steam"):
        p = os.path.expanduser(p)
        if os.path.isdir(p):
            return p
    return os.path.expanduser("~/.local/share/Steam")


def bibliotecas_steam():
    steam = steam_dir()
    libs = [steam]
    vdf = os.path.join(steam, "steamapps", "libraryfolders.vdf")
    try:
        with open(vdf, encoding="utf-8", errors="replace") as f:
            txt = f.read()
        for m in re.finditer(r'"path"\s+"([^"]+)"', txt):
            p = m.group(1).replace("\\\\", "\\")
            if p not in libs:
                libs.append(p)
    except OSError:
        pass
    return libs


def sdk_base_dir():
    env = _env("TROK_SDK_BASE")
    if env:
        return env
    for lib in bibliotecas_steam():
        p = os.path.join(lib, "steamapps", "common", "Source SDK Base 2013 Multiplayer")
        if os.path.isdir(p):
            return p
    return None


def mod_dir():
    return _env("TROK_MOD_DIR", os.path.join(steam_dir(), "steamapps", "sourcemods", "mod_trok"))


def jogo_exe():
    env = _env("TROK_JOGO_EXE")
    if env:
        return env
    base = sdk_base_dir()
    if not base:
        return None
    nomes = ("hl2_win64.exe", "hl2.exe") if os.name == "nt" else ("hl2_linux64", "hl2.sh", "hl2_linux")
    for n in nomes:
        p = os.path.join(base, n)
        if os.path.isfile(p):
            return p
    return None


def ferramenta_sdk(nome):
    """Acha vbsp/vvis/vrad/studiomdl no SDK Base (bin/x64 primeiro)."""
    base = sdk_base_dir()
    if not base:
        return None
    ext = ".exe" if os.name == "nt" else ""
    for sub in (os.path.join("bin", "x64"), "bin", os.path.join("bin", "linux64")):
        p = os.path.join(base, sub, nome + ext)
        if os.path.isfile(p):
            return p
    return None


def console_log_path():
    return os.path.join(mod_dir(), "console.log")


# ---------------------------------------------------------------------------
# Netcon (console remoto do motor: -netconport)
# ---------------------------------------------------------------------------

class Netcon:
    def __init__(self):
        self.sock = None
        self.lock = threading.Condition()
        self.linhas = collections.deque(maxlen=20000)
        self.total = 0          # quantas linhas já chegaram (índice absoluto)
        self.parcial = b""
        self.seq = 0

    @property
    def conectado(self):
        return self.sock is not None

    def conectar(self, timeout=2.0):
        if self.sock:
            return True
        porta = int(_env("TROK_NETCON_PORTA", "2121"))
        try:
            s = socket.create_connection(("127.0.0.1", porta), timeout=timeout)
        except OSError:
            return False
        s.settimeout(None)
        senha = _env("TROK_NETCON_SENHA")
        if senha:
            s.sendall(("PASS %s\n" % senha).encode("utf-8"))
        self.sock = s
        threading.Thread(target=self._ler, args=(s,), daemon=True).start()
        log("netcon conectado na porta %d" % porta)
        return True

    def fechar(self):
        s, self.sock = self.sock, None
        if s:
            try:
                s.close()
            except OSError:
                pass

    def _ler(self, s):
        buf = b""
        while True:
            try:
                data = s.recv(65536)
            except OSError:
                data = b""
            if not data:
                break
            buf += data
            partes = buf.split(b"\n")
            buf = partes.pop()
            with self.lock:
                for p in partes:
                    self.linhas.append(p.decode("utf-8", "replace").rstrip("\r"))
                    self.total += 1
                self.lock.notify_all()
        with self.lock:
            if self.sock is s:
                self.sock = None
            self.lock.notify_all()
        log("netcon desconectado")

    def _desde(self, idx):
        """Linhas com índice absoluto >= idx (chamar com o lock)."""
        primeiro = self.total - len(self.linhas)
        inicio = max(0, idx - primeiro)
        return list(self.linhas)[inicio:]

    def enviar(self, texto):
        if not self.sock:
            raise ErroTrok("netcon não está conectado")
        try:
            self.sock.sendall(texto.encode("utf-8"))
        except OSError as e:
            self.fechar()
            raise ErroTrok("falha ao enviar pelo netcon: %s" % e)

    def executar(self, comandos, timeout=5.0, espera_extra=0.0):
        """Manda os comandos entre dois marcadores 'echo' e devolve a saída entre eles."""
        self.seq += 1
        ini = "__trok_ini_%d__" % self.seq
        fim = "__trok_fim_%d__" % self.seq
        with self.lock:
            idx0 = self.total
        corpo = "".join(c.strip() + "\n" for c in comandos if c.strip())
        self.enviar("echo %s\n%secho %s\n" % (ini, corpo, fim))
        limite = time.time() + timeout
        with self.lock:
            while True:
                linhas = self._desde(idx0)
                if any(l.strip() == fim for l in linhas):
                    break
                resto = limite - time.time()
                if resto <= 0 or not self.sock:
                    break
                self.lock.wait(resto)
        if espera_extra > 0:
            time.sleep(espera_extra)
        with self.lock:
            linhas = self._desde(idx0)
        saida, dentro, depois = [], False, []
        for l in linhas:
            s = l.strip()
            if s == ini:
                dentro = True
                continue
            if s == fim:
                dentro = False
                continue
            if s.endswith(ini) or s.endswith(fim):   # eco do comando "] echo ..."
                continue
            (saida if dentro else depois).append(l)
        # O que chegou depois do marcador final (saída atrasada, como a de um 'map') vai junto.
        return saida + depois

    def ultimas(self, n):
        with self.lock:
            return list(self.linhas)[-n:]


# ---------------------------------------------------------------------------
# RCON (protocolo do Source, alternativa ao netcon)
# ---------------------------------------------------------------------------

class Rcon:
    AUTH, AUTH_RESP, EXEC, RESP = 3, 2, 2, 0

    def __init__(self):
        self.sock = None
        self.seq = 10

    def _pacote(self, pid, tipo, corpo):
        b = corpo.encode("utf-8") + b"\x00\x00"
        return struct.pack("<iii", len(b) + 8, pid, tipo) + b

    def _recv_n(self, n):
        dados = b""
        while len(dados) < n:
            parte = self.sock.recv(n - len(dados))
            if not parte:
                raise ErroTrok("RCON: conexão fechada")
            dados += parte
        return dados

    def _recv(self):
        (tam,) = struct.unpack("<i", self._recv_n(4))
        dados = self._recv_n(tam)
        pid, tipo = struct.unpack("<ii", dados[:8])
        return pid, tipo, dados[8:-2].decode("utf-8", "replace")

    def conectar(self):
        if self.sock:
            return True
        senha = _env("TROK_RCON_SENHA")
        if not senha:
            return False
        host = _env("TROK_RCON_HOST", "127.0.0.1")
        porta = int(_env("TROK_RCON_PORTA", "27015"))
        try:
            self.sock = socket.create_connection((host, porta), timeout=3)
            self.sock.settimeout(10)
            self.sock.sendall(self._pacote(1, self.AUTH, senha))
            while True:
                pid, tipo, _ = self._recv()
                if tipo == self.AUTH_RESP:
                    if pid == -1:
                        raise ErroTrok("RCON: senha recusada")
                    break
        except (OSError, ErroTrok) as e:
            self.fechar()
            log("RCON falhou: %s" % e)
            return False
        log("RCON conectado em %s:%d" % (host, porta))
        return True

    def fechar(self):
        s, self.sock = self.sock, None
        if s:
            try:
                s.close()
            except OSError:
                pass

    def executar(self, comandos, timeout=5.0, espera_extra=0.0):
        saida = []
        try:
            self.sock.settimeout(timeout)
            for cmd in comandos:
                if not cmd.strip():
                    continue
                self.seq += 2
                pid, marca = self.seq, self.seq + 1
                self.sock.sendall(self._pacote(pid, self.EXEC, cmd.strip()))
                self.sock.sendall(self._pacote(marca, self.RESP, ""))
                while True:
                    rid, _, corpo = self._recv()
                    if rid == marca:
                        break
                    if rid == pid and corpo:
                        saida.extend(corpo.rstrip("\n").split("\n"))
        except (OSError, struct.error) as e:
            self.fechar()
            raise ErroTrok("RCON: %s" % e)
        if espera_extra > 0:
            time.sleep(espera_extra)
        return saida


NETCON = Netcon()
RCON = Rcon()
PROCESSO = {"proc": None}


def canal():
    """Devolve o canal disponível (netcon primeiro, RCON depois) ou erro explicativo."""
    if NETCON.conectar():
        return NETCON
    if RCON.conectar():
        return RCON
    raise ErroTrok(
        "Não consegui falar com o jogo. Abra-o com a ferramenta jogo_abrir (que já passa "
        "-netconport %s) ou adicione '-netconport %s' às opções de inicialização. "
        "Alternativa: defina TROK_RCON_SENHA e rcon_password no jogo."
        % (_env("TROK_NETCON_PORTA", "2121"), _env("TROK_NETCON_PORTA", "2121")))


def rodar(comandos, timeout=5.0, espera_extra=0.0):
    if isinstance(comandos, str):
        comandos = [c for c in re.split(r"[;\n]", comandos)]
    return canal().executar(comandos, timeout=timeout, espera_extra=espera_extra)


# ---------------------------------------------------------------------------
# console.log
# ---------------------------------------------------------------------------

PADRAO_ERRO = re.compile(
    r"(error|erro\b|failed|falh|missing|not found|n[ãa]o encontr|can'?t |cannot|unable|unknown command|"
    r"bad |invalid|couldn'?t|precache|warning|aviso|assert|crash|leak|overflow|stuck)",
    re.IGNORECASE)


def ler_console_log(max_bytes=4_000_000):
    p = console_log_path()
    try:
        with open(p, "rb") as f:
            f.seek(0, os.SEEK_END)
            tam = f.tell()
            f.seek(max(0, tam - max_bytes))
            dados = f.read()
    except OSError:
        return None
    return dados.decode("utf-8", "replace").splitlines()


def resumir_erros(linhas, limite=80):
    contagem = collections.OrderedDict()
    for l in linhas:
        s = l.strip()
        if not s or s.startswith("__trok_") or s.startswith("] echo __trok_"):
            continue
        if PADRAO_ERRO.search(s):
            chave = re.sub(r"\d+(\.\d+)?", "#", s)[:200]
            if chave in contagem:
                contagem[chave][1] += 1
            else:
                contagem[chave] = [s, 1]
    itens = list(contagem.values())
    texto = ["%s%s" % (s, "  (x%d)" % n if n > 1 else "") for s, n in itens[:limite]]
    if len(itens) > limite:
        texto.append("... e mais %d tipos de aviso" % (len(itens) - limite))
    return texto


# ---------------------------------------------------------------------------
# Ferramentas
# ---------------------------------------------------------------------------

FERRAMENTAS = []


def ferramenta(nome, descricao, propriedades=None, obrigatorias=()):
    def deco(fn):
        FERRAMENTAS.append({
            "name": nome,
            "description": descricao,
            "inputSchema": {
                "type": "object",
                "properties": propriedades or {},
                "required": list(obrigatorias),
            },
            "_fn": fn,
        })
        return fn
    return deco


@ferramenta(
    "diagnostico",
    "Mostra os caminhos detectados (Steam, SDK Base, mod, executável, ferramentas de compilação), "
    "se o jogo está aberto e se o netcon/RCON conectam. Use primeiro quando algo não funcionar.")
def t_diagnostico():
    linhas = [
        "trok-mcp %s, Python %s, %s" % (VERSAO, sys.version.split()[0], sys.platform),
        "Steam:        %s" % steam_dir(),
        "SDK Base:     %s" % (sdk_base_dir() or "NÃO ACHEI (instale 'Source SDK Base 2013 Multiplayer' ou defina TROK_SDK_BASE)"),
        "Mod:          %s %s" % (mod_dir(), "" if os.path.isfile(os.path.join(mod_dir(), "gameinfo.txt")) else "(sem gameinfo.txt! copie o mod para sourcemods ou defina TROK_MOD_DIR)"),
        "Executável:   %s" % (jogo_exe() or "não achei; jogo_abrir usará steam -applaunch %s" % STEAM_APPID_SDK),
    ]
    for f in ("studiomdl", "vbsp", "vvis", "vrad"):
        linhas.append("%-13s %s" % (f + ":", ferramenta_sdk(f) or "não achei"))
    log_p = console_log_path()
    linhas.append("console.log:  %s %s" % (log_p, "(existe)" if os.path.isfile(log_p) else "(ainda não existe; o jogo cria com -condebug)"))
    proc = PROCESSO["proc"]
    linhas.append("Processo:     %s" % ("rodando (pid %d)" % proc.pid if proc and proc.poll() is None else "não aberto por mim"))
    linhas.append("Netcon:       %s" % ("conectado" if NETCON.conectar(timeout=1) else "não conecta na porta %s" % _env("TROK_NETCON_PORTA", "2121")))
    if _env("TROK_RCON_SENHA"):
        linhas.append("RCON:         %s" % ("conectado" if RCON.conectar() else "não conecta"))
    return "\n".join(linhas)


@ferramenta(
    "jogo_abrir",
    "Abre o Rearmed em janela, com -dev -console -condebug e o netcon ligado. O console.log anterior "
    "vira console_anterior.log, então 'erros' mostra só esta sessão. Opcional: já carregar um mapa.",
    {
        "mapa": {"type": "string", "description": "mapa para carregar ao abrir (ex.: de_dust2). Vazio = menu."},
        "largura": {"type": "integer", "description": "largura da janela (padrão 1600)"},
        "altura": {"type": "integer", "description": "altura da janela (padrão 900)"},
        "extra": {"type": "string", "description": "opções extras de inicialização, ex.: '-nosound +sv_cheats 1'"},
        "esperar": {"type": "integer", "description": "segundos para esperar o netcon responder (padrão 90)"},
    })
def t_jogo_abrir(mapa="", largura=1600, altura=900, extra="", esperar=90):
    proc = PROCESSO["proc"]
    if proc and proc.poll() is None:
        raise ErroTrok("O jogo já está aberto (pid %d). Use jogo_fechar antes, se quiser reabrir." % proc.pid)
    if NETCON.conectar(timeout=1):
        return "O jogo já está aberto e o netcon respondeu. Nada a fazer."
    md = mod_dir()
    if not os.path.isfile(os.path.join(md, "gameinfo.txt")):
        raise ErroTrok("Não achei %s. Instale o mod (LEIA-ME.txt) ou defina TROK_MOD_DIR." % os.path.join(md, "gameinfo.txt"))

    log_p = console_log_path()
    if os.path.isfile(log_p):
        try:
            shutil.move(log_p, os.path.join(md, "console_anterior.log"))
        except OSError:
            pass

    porta = _env("TROK_NETCON_PORTA", "2121")
    args = ["-game", md, "-dev", "-console", "-novid", "-condebug", "-windowed", "-noborder",
            "-w", str(largura), "-h", str(altura), "-netconport", porta,
            "+engine_no_focus_sleep", "0"]
    senha = _env("TROK_NETCON_SENHA")
    if senha:
        args += ["-netconpassword", senha]
    if extra:
        args += extra.split()
    if mapa:
        args += ["+map", mapa]

    exe = jogo_exe()
    if exe:
        cmd = [exe] + args
        cwd = os.path.dirname(exe)
    else:
        steam_exe = os.path.join(steam_dir(), "steam.exe" if os.name == "nt" else "steam")
        if not os.path.isfile(steam_exe):
            steam_exe = "steam"
        cmd = [steam_exe, "-applaunch", STEAM_APPID_SDK] + args
        cwd = None
    try:
        PROCESSO["proc"] = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as e:
        raise ErroTrok("Falha ao abrir: %s\nComando: %s" % (e, " ".join(cmd)))

    limite = time.time() + esperar
    while time.time() < limite:
        if NETCON.conectar(timeout=1):
            return ("Jogo aberto e netcon conectado.\nComando: %s\n"
                    "Dica: use 'console' ou 'erros' para ver o que o jogo disse ao carregar." % " ".join(cmd))
        p = PROCESSO["proc"]
        if exe and p.poll() is not None:
            break
        time.sleep(1)
    msg = "O jogo foi chamado, mas o netcon não respondeu em %ds.\nComando: %s\n" % (esperar, " ".join(cmd))
    p = PROCESSO["proc"]
    if exe and p.poll() is not None:
        msg += "O processo terminou com código %s (travou ou fechou ao abrir).\n" % p.returncode
    linhas = ler_console_log()
    if linhas:
        msg += "Últimas linhas do console.log:\n" + "\n".join(linhas[-40:])
    else:
        msg += ("Sem console.log ainda. Se o jogo abriu, talvez este SDK não aceite -netconport: "
                "nesse caso use RCON (TROK_RCON_SENHA) e leia os erros pelo console.log.")
    raise ErroTrok(msg)


@ferramenta(
    "jogo_fechar",
    "Fecha o jogo (manda 'quit'; se não fechar em 10 s, encerra o processo que eu abri).")
def t_jogo_fechar():
    if NETCON.conectar(timeout=1):
        try:
            NETCON.enviar("quit\n")
        except ErroTrok:
            pass
    proc = PROCESSO["proc"]
    if proc:
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
    NETCON.fechar()
    RCON.fechar()
    PROCESSO["proc"] = None
    return "Jogo fechado."


@ferramenta(
    "comando",
    "Executa comandos no console do jogo e devolve a saída. Vários comandos separados por ';' ou por linha. "
    "Ex.: 'trok_modo surto; map de_dust2', 'hl2mp_bot_add', 'nav_generate', 'trok_bot_estilos_info'.",
    {
        "comandos": {"type": "string", "description": "comandos do console"},
        "espera": {"type": "number", "description": "segundos extras para juntar saída atrasada (ex.: 15 depois de 'map'). Padrão 0."},
        "timeout": {"type": "number", "description": "tempo máximo esperando o fim da saída (padrão 5 s)"},
    },
    ["comandos"])
def t_comando(comandos, espera=0, timeout=5):
    saida = rodar(comandos, timeout=float(timeout), espera_extra=float(espera))
    return "\n".join(saida) if saida else "(sem saída)"


@ferramenta(
    "console",
    "Mostra as últimas linhas do console (pelo netcon, se conectado; senão pelo console.log).",
    {
        "linhas": {"type": "integer", "description": "quantas linhas (padrão 80)"},
        "filtro": {"type": "string", "description": "expressão regular para filtrar (opcional)"},
    })
def t_console(linhas=80, filtro=""):
    fonte = NETCON.ultimas(5000) if NETCON.conectar(timeout=1) else (ler_console_log() or [])
    fonte = [l for l in fonte if not l.strip().startswith("__trok_") and "echo __trok_" not in l]
    if filtro:
        try:
            rx = re.compile(filtro, re.IGNORECASE)
        except re.error as e:
            raise ErroTrok("filtro inválido: %s" % e)
        fonte = [l for l in fonte if rx.search(l)]
    if not fonte:
        return "(console vazio ou ainda sem console.log)"
    return "\n".join(fonte[-int(linhas):])


@ferramenta(
    "erros",
    "Lista os erros e avisos desta sessão (console.log), agrupados e contados: som, modelo ou material "
    "que falta, comando desconhecido, falha de script, etc.",
    {"limite": {"type": "integer", "description": "máximo de tipos de aviso (padrão 80)"}})
def t_erros(limite=80):
    linhas = ler_console_log()
    if linhas is None:
        linhas = NETCON.ultimas(20000) if NETCON.conectar(timeout=1) else []
        if not linhas:
            raise ErroTrok("Sem console.log em %s. O jogo precisa ser aberto com -condebug (jogo_abrir já faz isso)." % console_log_path())
    res = resumir_erros(linhas, int(limite))
    return "\n".join(res) if res else "Nenhum erro ou aviso encontrado (%d linhas lidas)." % len(linhas)


@ferramenta(
    "print_tela",
    "Tira um print do jogo e devolve a imagem (para conferir HUD, menu, placar, mira, etc.). "
    "Esconde o console antes.",
    {
        "qualidade": {"type": "integer", "description": "qualidade do JPEG, 1 a 100 (padrão 85)"},
        "manter_console": {"type": "boolean", "description": "não esconder o console (padrão false)"},
    })
def t_print_tela(qualidade=85, manter_console=False):
    pasta = os.path.join(mod_dir(), "screenshots")
    antes = set(glob.glob(os.path.join(pasta, "*.jpg")))
    t0 = time.time()
    cmds = []
    if not manter_console:
        cmds.append("hideconsole")
    cmds += ["jpeg_quality %d" % max(1, min(100, int(qualidade))), "jpeg"]
    rodar(cmds)
    arq = None
    limite = time.time() + 8
    while time.time() < limite:
        novos = [p for p in glob.glob(os.path.join(pasta, "*.jpg"))
                 if p not in antes or os.path.getmtime(p) >= t0 - 1]
        if novos:
            arq = max(novos, key=os.path.getmtime)
            tam1 = os.path.getsize(arq)
            time.sleep(0.3)
            if os.path.getsize(arq) == tam1 and tam1 > 0:
                break
        time.sleep(0.3)
    if not arq:
        raise ErroTrok("O print não apareceu em %s. Confira se o jogo não está minimizado." % pasta)
    with open(arq, "rb") as f:
        dados = base64.b64encode(f.read()).decode("ascii")
    return [
        {"type": "image", "data": dados, "mimeType": "image/jpeg"},
        {"type": "text", "text": "Print salvo em %s" % arq},
    ]


@ferramenta(
    "estado",
    "Resumo da partida: mapa e jogadores (status), modo, regras e a lista de bots com nível e estilo.")
def t_estado():
    cmds = ["status", "trok_modo", "mp_teamplay", "mp_fraglimit", "mp_timelimit",
            "hl2mp_bot_quota", "hl2mp_bot_difficulty", "trok_bot_estilos_info"]
    saida = rodar(cmds, timeout=6)
    return "\n".join(saida) if saida else "(sem saída: o jogo está no menu?)"


@ferramenta(
    "cvar",
    "Lê ou muda uma ConVar. Sem 'valor', só lê. Com 'valor', muda e lê de volta para confirmar.",
    {
        "nome": {"type": "string", "description": "nome da ConVar, ex.: trok_recuo_fixo"},
        "valor": {"type": "string", "description": "novo valor (opcional)"},
    },
    ["nome"])
def t_cvar(nome, valor=None):
    if not re.match(r"^[A-Za-z0-9_\.]+$", nome):
        raise ErroTrok("nome de ConVar inválido")
    cmds = []
    if valor is not None and str(valor) != "":
        cmds.append('%s "%s"' % (nome, str(valor).replace('"', "")))
    cmds.append(nome)
    saida = rodar(cmds)
    return "\n".join(saida) if saida else "(sem resposta: a ConVar existe?)"


@ferramenta(
    "partida_teste",
    "Começa uma partida com bots, igual ao botão INICIAR do menu: modo, mapa, nível e bots por time.",
    {
        "modo": {"type": "string", "enum": ["equipe", "solo", "surto"], "description": "modo (padrão equipe)"},
        "mapa": {"type": "string", "description": "mapa (padrão de_dust2)"},
        "bots_por_time": {"type": "integer", "description": "bots por time, 1 a 16 (padrão 4)"},
        "nivel": {"type": "integer", "description": "nível dos bots, 0 a 3 (padrão 1)"},
        "espera": {"type": "number", "description": "segundos esperando o mapa carregar (padrão 20)"},
    })
def t_partida_teste(modo="equipe", mapa="de_dust2", bots_por_time=4, nivel=1, espera=20):
    if modo not in ("equipe", "solo", "surto"):
        raise ErroTrok("modo deve ser equipe, solo ou surto")
    if not re.match(r"^[A-Za-z0-9_\-]+$", mapa):
        raise ErroTrok("nome de mapa inválido")
    n = max(1, min(16, int(bots_por_time)))
    cmds = ["disconnect", "progress_enable", "trok_modo %s" % modo,
            "hl2mp_bot_difficulty %d" % max(0, min(3, int(nivel))),
            "hl2mp_bot_quota %d" % (n * 2), "hl2mp_bot_quota_mode fill",
            "maxplayers %d" % min(32, n * 2 + 2), "map %s" % mapa]
    saida = rodar(cmds, timeout=10, espera_extra=float(espera))
    erros = resumir_erros(saida, 30)
    texto = "Pedi: %s\n" % "; ".join(cmds)
    texto += "Avisos ao carregar:\n" + "\n".join(erros) if erros else "Nenhum aviso ao carregar."
    texto += "\nUse 'estado' para conferir os bots e 'print_tela' para ver a tela."
    return texto


def _rodar_processo(cmd, cwd=None, timeout=1800):
    try:
        r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=timeout)
    except subprocess.TimeoutExpired:
        raise ErroTrok("Demorou mais de %d s: %s" % (timeout, " ".join(cmd)))
    except OSError as e:
        raise ErroTrok("Falha ao rodar %s: %s" % (cmd[0], e))
    return r.returncode, r.stdout.decode("utf-8", "replace").splitlines()


def _resumo_saida(nome, codigo, linhas, cauda=25):
    importantes = [l for l in linhas if re.search(r"(error|erro|warning|leak|failed|can'?t|missing|too many)", l, re.I)]
    partes = ["== %s (código %d) ==" % (nome, codigo)]
    if importantes:
        partes.append("Avisos e erros:")
        partes += importantes[:60]
    partes.append("Fim da saída:")
    partes += linhas[-cauda:]
    return "\n".join(partes)


@ferramenta(
    "modelo_compilar",
    "Compila um .qc com o studiomdl do SDK Base, para a pasta do mod. Use depois de exportar SMD/DMX do "
    "Blender (Blender Source Tools).",
    {"qc": {"type": "string", "description": "caminho do arquivo .qc"}},
    ["qc"])
def t_modelo_compilar(qc):
    exe = ferramenta_sdk("studiomdl")
    if not exe:
        raise ErroTrok("Não achei o studiomdl no SDK Base (bin/x64 ou bin). Defina TROK_SDK_BASE.")
    qc = os.path.abspath(qc)
    if not os.path.isfile(qc):
        raise ErroTrok("Arquivo não existe: %s" % qc)
    codigo, linhas = _rodar_processo([exe, "-game", mod_dir(), "-nop4", "-verbose", qc],
                                     cwd=os.path.dirname(qc), timeout=600)
    return _resumo_saida("studiomdl", codigo, linhas)


@ferramenta(
    "mapa_compilar",
    "Compila um .vmf (vbsp, vvis, vrad) e copia o .bsp para maps/ do mod. 'rapido' usa -fast no vvis e no "
    "vrad (para testar); sem ele, compila com qualidade final. Avisa se o mapa vazar (leak).",
    {
        "vmf": {"type": "string", "description": "caminho do .vmf"},
        "rapido": {"type": "boolean", "description": "compilação rápida (padrão true)"},
        "copiar": {"type": "boolean", "description": "copiar o .bsp para a pasta maps do mod (padrão true)"},
    },
    ["vmf"])
def t_mapa_compilar(vmf, rapido=True, copiar=True):
    vmf = os.path.abspath(vmf)
    if not os.path.isfile(vmf):
        raise ErroTrok("Arquivo não existe: %s" % vmf)
    ferr = {n: ferramenta_sdk(n) for n in ("vbsp", "vvis", "vrad")}
    falta = [n for n, p in ferr.items() if not p]
    if falta:
        raise ErroTrok("Não achei %s no SDK Base. Defina TROK_SDK_BASE." % ", ".join(falta))
    base = os.path.splitext(vmf)[0]
    bsp = base + ".bsp"
    md = mod_dir()
    partes = []
    etapas = [
        ("vbsp", [ferr["vbsp"], "-game", md, base]),
        ("vvis", [ferr["vvis"]] + (["-fast"] if rapido else []) + ["-game", md, base]),
        ("vrad", [ferr["vrad"]] + (["-fast", "-bounce", "2"] if rapido else ["-final", "-StaticPropLighting", "-StaticPropPolys"])
         + ["-game", md, base]),
    ]
    for nome, cmd in etapas:
        codigo, linhas = _rodar_processo(cmd, cwd=os.path.dirname(vmf), timeout=3600)
        partes.append(_resumo_saida(nome, codigo, linhas, cauda=12))
        if nome == "vbsp" and any("LEAK" in l.upper() for l in linhas):
            partes.append("O MAPA VAZOU (leak): ache o buraco com o arquivo .lin no Hammer (Map > Load Pointfile).")
            break
        if codigo != 0:
            break
    if copiar and os.path.isfile(bsp):
        destino = os.path.join(md, "maps")
        os.makedirs(destino, exist_ok=True)
        shutil.copy2(bsp, destino)
        partes.append("Copiado para %s" % os.path.join(destino, os.path.basename(bsp)))
    return "\n\n".join(partes)


# ---------------------------------------------------------------------------
# MCP por stdio
# ---------------------------------------------------------------------------

INSTRUCOES = (
    "Ferramentas para testar o Rearmed (mod de Source SDK 2013 MP). Fluxo típico: diagnostico -> "
    "jogo_abrir -> partida_teste -> estado / print_tela / erros -> ajustar com cvar ou comando. "
    "Números de jogabilidade ficam em cfg e scripts do mod, não no código. Os comandos do mod começam com trok_.")


def escrever(msg):
    sys.stdout.buffer.write(json.dumps(msg, ensure_ascii=False).encode("utf-8") + b"\n")
    sys.stdout.buffer.flush()


def tratar(msg):
    metodo = msg.get("method")
    mid = msg.get("id")
    params = msg.get("params") or {}
    if mid is None:
        return None  # notificação
    if metodo == "initialize":
        pedido = params.get("protocolVersion")
        versao = pedido if pedido in PROTOCOLOS else PROTOCOLOS[0]
        return {"protocolVersion": versao,
                "capabilities": {"tools": {"listChanged": False}},
                "serverInfo": {"name": "trok", "version": VERSAO},
                "instructions": INSTRUCOES}
    if metodo == "ping":
        return {}
    if metodo == "tools/list":
        return {"tools": [{k: v for k, v in f.items() if k != "_fn"} for f in FERRAMENTAS]}
    if metodo == "tools/call":
        nome = params.get("name")
        args = params.get("arguments") or {}
        f = next((f for f in FERRAMENTAS if f["name"] == nome), None)
        if not f:
            raise LookupError("ferramenta desconhecida: %s" % nome)
        try:
            res = f["_fn"](**args)
            conteudo = res if isinstance(res, list) else [{"type": "text", "text": str(res)}]
            return {"content": conteudo, "isError": False}
        except ErroTrok as e:
            return {"content": [{"type": "text", "text": str(e)}], "isError": True}
        except TypeError as e:
            return {"content": [{"type": "text", "text": "argumentos inválidos: %s" % e}], "isError": True}
        except Exception as e:  # nunca derrubar o servidor por causa de uma ferramenta
            log("erro em %s: %r" % (nome, e))
            return {"content": [{"type": "text", "text": "erro interno: %r" % e}], "isError": True}
    raise LookupError("método não suportado: %s" % metodo)


def main():
    log("trok-mcp %s pronto (mod: %s)" % (VERSAO, mod_dir()))
    while True:
        linha = sys.stdin.buffer.readline()
        if not linha:
            break
        linha = linha.strip()
        if not linha:
            continue
        try:
            msg = json.loads(linha.decode("utf-8"))
        except ValueError:
            escrever({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": "JSON inválido"}})
            continue
        mid = msg.get("id")
        try:
            res = tratar(msg)
            if mid is not None:
                escrever({"jsonrpc": "2.0", "id": mid, "result": res})
        except LookupError as e:
            if mid is not None:
                escrever({"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": str(e)}})
        except Exception as e:
            if mid is not None:
                escrever({"jsonrpc": "2.0", "id": mid, "error": {"code": -32603, "message": repr(e)}})


if __name__ == "__main__":
    main()
