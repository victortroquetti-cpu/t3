#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test (test/smoke/run.sh).

Tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele) e stubs nos enderecos
que o .asi usa: CInput::AddCommand guarda o nome e o callback do comando, CChat::AddEntry conta as
mensagens e guarda o texto, CGame::SetCursorMode guarda o modo. O fake_gta.exe le esses campos.

Uso: make_fake_samp.py <saida/samp.dll>
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000

# Campos lidos pelo fake_gta.exe (RVA).
DATA = 0x300000
CMD_NAME, CMD_PROC, CHAT_COUNT, CURSOR_MODE, CHAT_TEXT, OBJECT = (DATA + 4 * i for i in range(6))

# Enderecos do SA-MP 0.3.7 R1 (os mesmos de src/main.cpp).
ENTRY = 0x31DF13
ADD_COMMAND = 0x65AD0
ADD_ENTRY = 0x64010
SET_CURSOR = 0x9BD30
UNLOCK_CAM = 0x9BC10
POINTERS = (0x21A0E4, 0x21A0E8, 0x21A0F8, 0x21A10C)  # CChat, CInput, info, misc


def d(value):
    return struct.pack('<I', value)


def main():
    img = bytearray(SIZE)

    def put(rva, data):
        img[rva:rva + len(data)] = data

    put(ENTRY, b'\xB8' + d(1) + b'\xC2\x0C\x00')  # DllMain: mov eax, 1; ret 0Ch
    # AddCommand(this, name, proc): mov eax,[esp+4]; mov [name],eax; mov eax,[esp+8]; mov [proc],eax; ret 8
    put(ADD_COMMAND, b'\x8B\x44\x24\x04\xA3' + d(BASE + CMD_NAME) + b'\x8B\x44\x24\x08\xA3' + d(BASE + CMD_PROC) +
        b'\xC2\x08\x00')
    # AddEntry(this, type, text, prefix, color, prefixColor): guarda o texto, conta; ret 14h
    put(ADD_ENTRY, b'\x8B\x44\x24\x08\xA3' + d(BASE + CHAT_TEXT) + b'\xFF\x05' + d(BASE + CHAT_COUNT) + b'\xC2\x14\x00')
    # SetCursorMode(this, mode, hide): guarda o modo; ret 8
    put(SET_CURSOR, b'\x8B\x44\x24\x04\xA3' + d(BASE + CURSOR_MODE) + b'\xC2\x08\x00')
    put(UNLOCK_CAM, b'\xC3')
    for rva in POINTERS:
        put(rva, d(BASE + OBJECT))

    headers = bytearray(0x400)
    headers[0:2] = b'MZ'
    struct.pack_into('<I', headers, 0x3C, 0x80)
    # i386, 1 secao, sem relocacoes | executavel | 32 bits | DLL
    struct.pack_into('<4sHHIIIHH', headers, 0x80, b'PE\0\0', 0x14C, 1, 0, 0, 0, 0xE0, 0x2103)
    optional = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', 0x10B, 1, 0, SIZE - 0x1000, 0, 0, ENTRY, 0x1000, 0x1000,
                           BASE, 0x1000, 0x200, 4, 0, 0, 0, 4, 0, 0, SIZE, 0x400, 0, 2, 0, 0x100000, 0x1000,
                           0x100000, 0x1000, 0, 16) + bytes(16 * 8)
    headers[0x98:0x98 + len(optional)] = optional
    # .text RWX de 0x1000 ate o fim da imagem
    section = struct.pack('<8sIIIIIIHHI', b'.text', SIZE - 0x1000, 0x1000, SIZE - 0x1000, 0x400, 0, 0, 0, 0,
                          0xE0000060)
    headers[0x178:0x178 + len(section)] = section

    with open(sys.argv[1], 'wb') as out:
        out.write(headers)
        out.write(img[0x1000:])


if __name__ == '__main__':
    main()
