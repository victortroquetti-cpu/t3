#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test do Trok Hitmarker (test/smoke/run.sh).

Tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele) e stubs nos enderecos que o .asi usa ou
desvia: os do dano (SendGiveDamage e SendTakeDamage contam e guardam o id), a mensagem de morte e o bullet sync dos
outros jogadores (contam), a cor do
jogador (le o campo +0x1F8 do CRemotePlayer falso), o nick local, o AddCommand, o AddEntry e o cursor. As estruturas do
SA-MP (CNetGame, pools, jogadores) o fake_gta.exe monta na memoria dele e liga nos ponteiros do samp.dll.

Uso: make_fake_samp.py <saida/samp.dll> [r3]   (r3: ponto de entrada do 0.3.7 R3, o .asi tem que recusar)
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000

# Campos lidos e escritos pelo fake_gta.exe (RVA): DATA + 4 * indice.
DATA = 0x300000
(CMD_NAME, CMD_PROC, CHAT_COUNT, CURSOR_MODE, GIVE_COUNT, GIVE_ID, TAKE_COUNT, TAKE_ID, DEATH_COUNT, BULLET_COUNT,
 OBJECT) = (DATA + 4 * i for i in range(11))
LOCAL_NAME = DATA + 0x100

# Enderecos do SA-MP 0.3.7 R1 (os mesmos de src/main.cpp e src/samp.cpp).
ENTRY_R1 = 0x31DF13
ENTRY_R3 = 0xCC4D0
ADD_COMMAND, ADD_ENTRY, SET_CURSOR, UNLOCK_CAM = 0x65AD0, 0x64010, 0x9BD30, 0x9BC10
SEND_GIVE, SEND_TAKE, DEATH_ADD, COLOR_ARGB, LOCAL_NAME_FN = 0x6770, 0x6660, 0x66A10, 0x12A00, 0x13CD0
REMOTE_BULLET = 0x12BE0  # CRemotePlayer::Update(BulletData*)
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
    put(ADD_ENTRY, inc(CHAT_COUNT) + b'\xC2\x14\x00')  # AddEntry: conta; ret 14h
    # SetCursorMode(this, mode, hide): guarda o modo; ret 8
    put(SET_CURSOR, b'\x8B\x44\x24\x04\xA3' + d(BASE + CURSOR_MODE) + b'\xC2\x08\x00')
    put(UNLOCK_CAM, b'\xC3')
    # SendGiveDamage / SendTakeDamage(this, id, damage, weapon, part): conta, guarda o id; ret 10h
    put(SEND_GIVE, inc(GIVE_COUNT) + b'\x8B\x44\x24\x04\xA3' + d(BASE + GIVE_ID) + b'\xC2\x10\x00')
    put(SEND_TAKE, inc(TAKE_COUNT) + b'\x8B\x44\x24\x04\xA3' + d(BASE + TAKE_ID) + b'\xC2\x10\x00')
    # CDeathWindow::AddMessage(this, killer, victim, kc, vc, weapon): conta; ret 14h
    put(DEATH_ADD, inc(DEATH_COUNT) + b'\xC2\x14\x00')
    put(REMOTE_BULLET, inc(BULLET_COUNT) + b'\xC2\x04\x00')  # bullet sync de outro jogador: conta; ret 4
    put(COLOR_ARGB, b'\x8B\x81' + d(0x1F8) + b'\xC3')  # mov eax, [ecx+1F8h]; ret
    put(LOCAL_NAME_FN, b'\xB8' + d(BASE + LOCAL_NAME) + b'\xC3')  # mov eax, nick; ret
    put(LOCAL_NAME, b'Victor_Trok\0')
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
