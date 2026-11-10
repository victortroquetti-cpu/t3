#!/usr/bin/env python3
"""
ca_extrator: lê os pacotes .rez (LithTech) de uma instalação do Combat Arms do próprio jogador.

Regra do projeto (CLAUDE.md, regra 2): assets do CA são só provisório pessoal. Este script **não traz
nenhum arquivo do jogo**; ele lê a instalação de quem roda e extrai para
game/mod_trok/custom/ca_local/ (no .gitignore). Pacotes criptografados são pulados.

Formato REZ escrito do zero a partir da descrição pública do RezMgr (ver o projeto MIT K4rian/gorez):
  cabeçalho de 168 bytes: assinatura (127), versão, posição e tamanho do diretório raiz, ...
  entrada: tipo (u32: 0 = arquivo, 1 = pasta)
    arquivo: pos, tam, hora, id, extensão invertida (4 bytes), nº de chaves, nome\\0, comentário\\0
    pasta:   pos, tam, hora, nome\\0  (pos/tam apontam para as entradas da pasta)

Uso:
  python ca_extrator.py listar  <pasta_do_CA ou arquivo.rez>        resumo por pacote e por tipo
  python ca_extrator.py extrair <pasta_do_CA> [--tipos wav,dtx] [--pacotes SND,GUNS_SND_AR] [--saida DIR]
"""

import argparse
import collections
import os
import struct
import sys

ASSINATURA = b"RezMgr Version 1"
TAM_CABECALHO = 168


class ErroRez(Exception):
    pass


def ler_cstr(dados, pos):
    fim = dados.index(b"\x00", pos)
    return dados[pos:fim].decode("latin-1"), fim + 1


class Rez:
    """Um pacote .rez aberto (só leitura)."""

    def __init__(self, caminho):
        self.caminho = caminho
        self.arquivos = []  # (caminho_interno, pos, tam)
        self.f = open(caminho, "rb")
        self.tamanho = os.fstat(self.f.fileno()).st_size
        cab = self.f.read(TAM_CABECALHO)
        if ASSINATURA not in cab[:64]:
            raise ErroRez("sem assinatura RezMgr (pacote criptografado ou não é REZ)")
        # REZ padrão: CR LF antes do texto e campos binários depois dele. Versões do CA alteram esses
        # bytes e codificam a posição do índice: é proteção, e o projeto não a contorna (CLAUDE.md, regra 2).
        if cab[:2] != b"\r\n":
            raise ErroRez("cabeçalho alterado pelo jogo (pacote protegido): não é aberto por regra do projeto")
        (self.versao, self.raiz_pos, self.raiz_tam) = struct.unpack_from("<III", cab, 127)
        if self.raiz_pos + self.raiz_tam > self.tamanho:
            raise ErroRez("diretório fora do arquivo")
        self._ler_pasta(self.raiz_pos, self.raiz_tam, "", 0)

    def _ler_pasta(self, pos, tam, prefixo, nivel):
        if nivel > 32:
            raise ErroRez("pastas demais (arquivo corrompido?)")
        self.f.seek(pos)
        dados = self.f.read(tam)
        i = 0
        while i + 4 <= len(dados):
            (tipo,) = struct.unpack_from("<I", dados, i)
            i += 4
            if tipo == 0:
                p, t, _hora, _id = struct.unpack_from("<IIII", dados, i)
                ext = dados[i + 16:i + 20].rstrip(b"\x00")[::-1].decode("latin-1")
                (nchaves,) = struct.unpack_from("<I", dados, i + 20)
                i += 24
                nome, i = ler_cstr(dados, i)
                _coment, i = ler_cstr(dados, i)
                i += 4 * nchaves
                if t > 0:
                    nome_completo = prefixo + nome + ("." + ext.lower() if ext else "")
                    self.arquivos.append((nome_completo, p, t))
            elif tipo == 1:
                p, t, _hora = struct.unpack_from("<III", dados, i)
                i += 12
                nome, i = ler_cstr(dados, i)
                if t > 0:
                    atual = self.f.tell()
                    self._ler_pasta(p, t, prefixo + nome + "/", nivel + 1)
                    self.f.seek(atual)
            else:
                raise ErroRez("entrada de tipo desconhecido (%d)" % tipo)

    def ler(self, pos, tam):
        self.f.seek(pos)
        return self.f.read(tam)

    def fechar(self):
        self.f.close()


def achar_rez(origem):
    if os.path.isfile(origem):
        return [origem]
    achados = []
    for raiz, _pastas, arqs in os.walk(origem):
        for a in arqs:
            if a.lower().endswith(".rez"):
                achados.append(os.path.join(raiz, a))
    return sorted(achados)


def tipo_pelo_conteudo(cab):
    """Reconhece o formato pelos primeiros bytes, para saber se o arquivo está aberto."""
    if cab[:4] == b"RIFF":
        return "wav"
    if cab[:3] == b"ID3" or cab[:2] in (b"\xff\xfb", b"\xff\xf3"):
        return "mp3"
    if cab[:4] == b"DDS ":
        return "dds"
    if cab[:8] == b"\x89PNG\r\n\x1a\n":
        return "png"
    if cab[:2] == b"BM":
        return "bmp"
    if cab[:3] == b"\xff\xd8\xff":
        return "jpg"
    if cab[:4] == b"]\x00\x00\x80" or cab[:3] == b"\x5d\x00\x00":
        return "lzma"
    try:
        txt = cab.decode("utf-8")
        if all(c.isprintable() or c in "\r\n\t" for c in txt[:64]):
            return "texto"
    except UnicodeDecodeError:
        pass
    return "binário"


def cmd_listar(args):
    pacotes = achar_rez(args.origem)
    if not pacotes:
        print("Nenhum .rez em", args.origem)
        return 1
    total_tipos = collections.Counter()
    total_bytes = collections.Counter()
    for caminho in pacotes:
        rel = os.path.relpath(caminho, args.origem) if os.path.isdir(args.origem) else os.path.basename(caminho)
        try:
            rez = Rez(caminho)
        except ErroRez as e:
            print("  PULADO %-40s %s" % (rel, e))
            continue
        tipos = collections.Counter(os.path.splitext(n)[1].lstrip(".") or "(sem)" for n, _p, _t in rez.arquivos)
        for n, _p, t in rez.arquivos:
            ext = os.path.splitext(n)[1].lstrip(".") or "(sem)"
            total_tipos[ext] += 1
            total_bytes[ext] += t
        resumo = ", ".join("%s %d" % kv for kv in tipos.most_common(5))
        print("  %-40s %6d arquivos  %s" % (rel, len(rez.arquivos), resumo))
        rez.fechar()
    print("\nTotal por tipo:")
    for ext, n in total_tipos.most_common():
        print("  %-8s %7d arquivos  %9.1f MB" % (ext, n, total_bytes[ext] / 1e6))
    return 0


def cmd_extrair(args):
    pacotes = achar_rez(args.origem)
    tipos = set(t.strip().lower() for t in args.tipos.split(",")) if args.tipos else None
    filtro = set(p.strip().lower() for p in args.pacotes.split(",")) if args.pacotes else None
    saida = args.saida
    os.makedirs(saida, exist_ok=True)
    n = 0
    for caminho in pacotes:
        nome_pacote = os.path.splitext(os.path.basename(caminho))[0]
        if filtro and nome_pacote.lower() not in filtro:
            continue
        try:
            rez = Rez(caminho)
        except ErroRez as e:
            print("PULADO", caminho, e)
            continue
        for interno, pos, tam in rez.arquivos:
            ext = os.path.splitext(interno)[1].lstrip(".").lower()
            if tipos and ext not in tipos:
                continue
            destino = os.path.normpath(os.path.join(saida, interno.replace("\\", "/")))
            if not destino.startswith(os.path.abspath(saida)) and not os.path.abspath(destino).startswith(os.path.abspath(saida)):
                continue  # nome estranho tentando sair da pasta
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            with open(destino, "wb") as out:
                out.write(rez.ler(pos, tam))
            n += 1
        rez.fechar()
    print("%d arquivo(s) extraído(s) em %s" % (n, saida))
    return 0


def main():
    ap = argparse.ArgumentParser(description="Lê os .rez do Combat Arms instalado (provisório pessoal).")
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("listar")
    a.add_argument("origem")
    a.set_defaults(func=cmd_listar)
    b = sub.add_parser("extrair")
    b.add_argument("origem")
    b.add_argument("--tipos", help="extensões, ex.: wav,dtx")
    b.add_argument("--pacotes", help="nomes dos .rez sem extensão, ex.: SND,GUNS_SND_AR")
    b.add_argument("--saida", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                   "..", "game", "mod_trok", "custom", "ca_local"))
    b.set_defaults(func=cmd_extrair)
    args = ap.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
