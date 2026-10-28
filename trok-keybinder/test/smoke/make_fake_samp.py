#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test do Trok KeyBinder (test/smoke/run.sh).

Tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele) e stubs nos enderecos que o .asi usa.
O fake_gta.exe troca os que precisam de logica (CInput::Send, CChat::AddEntry, CGame::SetCursorMode) por funcoes
dele e monta as estruturas do SA-MP na memoria dele.

Uso: make_fake_samp.py <saida/samp.dll> [r3]   (r3: ponto de entrada do 0.3.7 R3, o .asi tem que recusar)
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000

DATA = 0x300000
CMD_NAME, CMD_PROC, OBSTACLE_CALLS, NO_OBSTACLES = (DATA + 4 * i for i in range(4))
OBJECT = DATA + 0x100  # CChat, CInput e CGame de mentira (o CInput le +0x14E0, o CGame +0x55)

ENTRY_R1 = 0x31DF13
ENTRY_R3 = 0xCC4D0
ADD_COMMAND = 0x65AD0
ADD_ENTRY = 0x64010
SEND = 0x65C60
SET_CURSOR = 0x9BD30
UNLOCK_CAM = 0x9BC10
HAS_NO_OBSTACLES = 0x67530
POINTERS = (0x21A0E4, 0x21A0E8, 0x21A10C)  # CChat, CInput, CGame


def d(value):
    return struct.pack('<I', value)


def inc(rva):
    return b'\xFF\x05' + d(BASE + rva)  # inc dword [BASE + rva]


def main():
    entry = ENTRY_R3 if len(sys.argv) > 2 and sys.argv[2] == 'r3' else ENTRY_R1
    img = bytearray(SIZE)

    def put(rva, data):
        img[rva:rva + len(data)] = data

    put(entry, b'\xB8' + d(1) + b'\xC2\x0C\x00')  # DllMain: mov eax, 1; ret 0Ch
    # AddCommand(this, name, proc): guarda os dois; ret 8
    put(ADD_COMMAND, b'\x8B\x44\x24\x04\xA3' + d(BASE + CMD_NAME) + b'\x8B\x44\x24\x08\xA3' + d(BASE + CMD_PROC) +
        b'\xC2\x08\x00')
    # Os tres abaixo o fake_gta.exe troca; ate la: ret com os argumentos certos (e 5+ bytes para o MinHook).
    put(ADD_ENTRY, b'\x90' * 8 + b'\xC2\x14\x00')
    put(SEND, b'\x90' * 8 + b'\xC2\x04\x00')
    put(SET_CURSOR, b'\x90' * 8 + b'\xC2\x08\x00')
    put(UNLOCK_CAM, b'\xC3')
    # CLabel::HasNoObstacles(CVector): conta e devolve o campo NO_OBSTACLES; ret 0Ch
    put(HAS_NO_OBSTACLES, inc(OBSTACLE_CALLS) + b'\xA1' + d(BASE + NO_OBSTACLES) + b'\xC2\x0C\x00')
    for rva in POINTERS:
        put(rva, d(BASE + OBJECT))

    headers = bytearray(0x400)
    headers[0:2] = b'MZ'
    struct.pack_into('<I', headers, 0x3C, 0x80)
    # i386, 1 secao, sem relocacoes | executavel | 32 bits | DLL
    struct.pack_into('<4sHHIIIHH', headers, 0x80, b'PE\0\0', 0x14C, 1, 0, 0, 0, 0xE0, 0x2103)
    optional = struct.pack('<HBBIIIIIIIIIHHHHHHIIIIHHIIIIII', 0x10B, 1, 0, SIZE - 0x1000, 0, 0, entry, 0x1000, 0x1000,
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
