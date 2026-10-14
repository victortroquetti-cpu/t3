#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test do Trok Tags (test/smoke/run.sh).

Tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele) e stubs nos enderecos que o
.asi usa. Os stubs das funcoes de desenho so contam as chamadas (o .asi as desvia; a conta mostra se a
original foi chamada) e o CLabelPool::Draw anota o m_bNotEmpty dos 4 primeiros textos 3D no momento do
desenho. As estruturas do SA-MP (CNetGame, pools, baloes...) o fake_gta.exe monta na memoria dele e liga
nos ponteiros do samp.dll.

Uso: make_fake_samp.py <saida/samp.dll> [r3]   (r3: ponto de entrada do 0.3.7 R3, o .asi tem que recusar)
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000

# Campos lidos e escritos pelo fake_gta.exe (RVA).
DATA = 0x300000
CMD_NAME, CMD_PROC, CHAT_COUNT, CURSOR_MODE, CHAT_TEXT, OBJECT = (DATA + 4 * i for i in range(6))
COUNTERS = DATA + 0x2000  # depois do OBJECT (o CInput le +0x14E0 dele)
LABELS_DRAWN, BARS_DRAWN, POOL_DRAWS, OBSTACLE_CALLS, NO_OBSTACLES, UPDATE_PLAYERS = (COUNTERS + 4 * i for i in range(6))
SEEN = COUNTERS + 0x40  # m_bNotEmpty[0..3] vistos dentro do CLabelPool::Draw

# Enderecos do SA-MP 0.3.7 R1 (os mesmos de src/main.cpp e src/samp.cpp).
ENTRY_R1 = 0x31DF13
ENTRY_R3 = 0xCC4D0
ADD_COMMAND = 0x65AD0
ADD_ENTRY = 0x64010
SET_CURSOR = 0x9BD30
UNLOCK_CAM = 0x9BC10
DRAW_LABEL = 0x686C0
DRAW_HEALTH_BAR = 0x689C0
LABEL_POOL_DRAW = 0x1340
HAS_NO_OBSTACLES = 0x67530
UPDATE_PLAYERS_FN = 0x8A10
POINTERS = (0x21A0E4, 0x21A0E8, 0x21A10C)  # CChat, CInput, misc (o resto o fake_gta.exe liga)


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
    # AddCommand(this, name, proc): mov eax,[esp+4]; mov [name],eax; mov eax,[esp+8]; mov [proc],eax; ret 8
    put(ADD_COMMAND, b'\x8B\x44\x24\x04\xA3' + d(BASE + CMD_NAME) + b'\x8B\x44\x24\x08\xA3' + d(BASE + CMD_PROC) +
        b'\xC2\x08\x00')
    # AddEntry(this, type, text, prefix, color, prefixColor): guarda o texto, conta; ret 14h
    put(ADD_ENTRY, b'\x8B\x44\x24\x08\xA3' + d(BASE + CHAT_TEXT) + inc(CHAT_COUNT) + b'\xC2\x14\x00')
    # SetCursorMode(this, mode, hide): guarda o modo; ret 8
    put(SET_CURSOR, b'\x8B\x44\x24\x04\xA3' + d(BASE + CURSOR_MODE) + b'\xC2\x08\x00')
    put(UNLOCK_CAM, b'\xC3')
    # CPlayerTags::DrawLabel(pos, text, color, dist, drawStatus, status): conta; ret 18h
    put(DRAW_LABEL, inc(LABELS_DRAWN) + b'\xC2\x18\x00')
    # CPlayerTags::DrawHealthBar(pos, health, armour, dist): conta; ret 10h
    put(DRAW_HEALTH_BAR, inc(BARS_DRAWN) + b'\xC2\x10\x00')
    # CLabelPool::Draw(): anota m_bNotEmpty[0..3] (this + 0xE800 + 4i) e conta; ret
    code = b''
    for i in range(4):
        code += b'\x8B\x81' + d(0xE800 + 4 * i) + b'\xA3' + d(BASE + SEEN + 4 * i)  # mov eax,[ecx+..]; mov [..],eax
    put(LABEL_POOL_DRAW, code + inc(POOL_DRAWS) + b'\xC3')
    # CLabel::HasNoObstacles(CVector): conta e devolve o campo NO_OBSTACLES; ret 0Ch
    put(HAS_NO_OBSTACLES, inc(OBSTACLE_CALLS) + b'\xA1' + d(BASE + NO_OBSTACLES) + b'\xC2\x0C\x00')
    # CNetGame::UpdatePlayers(): conta; ret
    put(UPDATE_PLAYERS_FN, inc(UPDATE_PLAYERS) + b'\xC3')
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
