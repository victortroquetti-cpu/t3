#!/usr/bin/env python3
"""Converte os textos UTF-8 de um .lua para escapes decimais (\\195\\167...), como no Trok Kill List.

O arquivo-fonte fica ASCII puro e o mimgui recebe UTF-8. So mexe dentro de strings ('...' e "...");
falha se encontrar acento fora de string (comentario ou codigo).
Uso: python3 tools/lua_ascii.py "lua/Trok UI Showcase.lua"
"""
import sys

path = sys.argv[1]
src = open(path, 'rb').read()
out = bytearray()
i, quote = 0, None
while i < len(src):
    c = src[i]
    if quote is None:
        if src[i:i + 2] == b'--':  # comentario ate o fim da linha
            end = src.find(b'\n', i)
            end = len(src) if end < 0 else end
            chunk = src[i:end]
            if any(b > 127 for b in chunk):
                line = src[:i].count(b'\n') + 1
                sys.exit(f'acento em comentario na linha {line}')
            out += chunk
            i = end
            continue
        if c in (0x27, 0x22):
            quote = c
        elif c > 127:
            line = src[:i].count(b'\n') + 1
            sys.exit(f'acento fora de string na linha {line}')
        out.append(c)
    else:
        if c == 0x5C:  # barra invertida: copia o escape inteiro
            out += src[i:i + 2]
            i += 2
            continue
        if c == quote:
            quote = None
            out.append(c)
        elif c > 127:
            out += b'\\%d' % c
        else:
            out.append(c)
    i += 1
open(path, 'wb').write(bytes(out))
print(f'{path}: ASCII ({len(out)} bytes)')
