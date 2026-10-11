#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test do Trok Chat (test/smoke/run.sh).

Tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele), os ponteiros do CChat, CInput, CDialog e
do info nos enderecos do R1 e stubs nas funcoes que o .asi usa ou desvia:
  CChat::AddEntry  guarda o tipo e conta       CChat::Draw / CChat::Render  contam
  CInput::Send     copia o texto e conta       CInput::AddCommand           guarda o nome e o callback
  CInput::Open / CInput::Close ligam e desligam o m_bEnabled (+0x14E0) e contam
O CChat ja vem com tres mensagens (o .asi copia as que ja estavam no chat). O fake_gta.exe le e escreve esses campos.

Uso: make_fake_samp.py <saida/samp.dll> [--outra-versao]
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000

# Campos lidos pelo fake_gta.exe (RVA): DATA + 4 * indice.
DATA = 0x300000
(CMD_NAME, CMD_PROC, ADD_COUNT, DRAW_COUNT, RENDER_COUNT, SEND_COUNT, LAST_TYPE, OPEN_COUNT,
 CLOSE_COUNT) = (DATA + 4 * i for i in range(9))
SEND_BUF = DATA + 0x100

CHAT, INPUT, DIALOG, INFO = 0x301000, 0x308000, 0x30A000, 0x30B000

# Enderecos do SA-MP 0.3.7 R1 (os mesmos de src/samp.cpp).
ENTRY_R1 = 0x31DF13
ENTRY_OTHER = 0xCC5F0  # outra versao: o .asi tem que ficar de fora
P_CHAT, P_INPUT, P_DIALOG, P_INFO = 0x21A0E4, 0x21A0E8, 0x21A0B8, 0x21A0F8
ADD_ENTRY, DRAW, RENDER, SEND, ADD_COMMAND = 0x64010, 0x64230, 0x63D70, 0x65C60, 0x65AD0
INPUT_OPEN, INPUT_CLOSE, INPUT_ENABLED = 0x657E0, 0x658E0, 0x14E0

# CChat (R1)
CHAT_ENTRIES, ENTRY_SIZE = 0x132, 0xFC


def d(value):
    return struct.pack('<I', value)


def main():
    other = '--outra-versao' in sys.argv[2:]
    entry = ENTRY_OTHER if other else ENTRY_R1
    img = bytearray(SIZE)

    def put(rva, data):
        img[rva:rva + len(data)] = data

    put(entry, b'\xB8' + d(1) + b'\xC2\x0C\x00')  # DllMain: mov eax, 1; ret 0Ch
    # AddEntry(this, type, text, prefix, color, prefixColor): mov eax,[esp+4]; mov [tipo],eax; inc [conta]; ret 14h
    put(ADD_ENTRY, b'\x8B\x44\x24\x04\xA3' + d(BASE + LAST_TYPE) + b'\xFF\x05' + d(BASE + ADD_COUNT) + b'\xC2\x14\x00')
    put(DRAW, b'\xFF\x05' + d(BASE + DRAW_COUNT) + b'\xC3')  # inc [conta]; ret
    put(RENDER, b'\xFF\x05' + d(BASE + RENDER_COUNT) + b'\xC3')
    # Send(this, text): push esi; push edi; mov esi,[esp+0Ch]; mov edi,buf; copia ate o zero; pop edi; pop esi;
    # inc [conta]; ret 4
    put(SEND, b'\x56\x57\x8B\x74\x24\x0C\xBF' + d(BASE + SEND_BUF) + b'\xAC\xAA\x84\xC0\x75\xFA\x5F\x5E\xFF\x05' +
        d(BASE + SEND_COUNT) + b'\xC2\x04\x00')
    # AddCommand(this, name, proc): mov eax,[esp+4]; mov [nome],eax; mov eax,[esp+8]; mov [proc],eax; ret 8
    put(ADD_COMMAND, b'\x8B\x44\x24\x04\xA3' + d(BASE + CMD_NAME) + b'\x8B\x44\x24\x08\xA3' + d(BASE + CMD_PROC) +
        b'\xC2\x08\x00')

    # Open(this) / Close(this): mov dword [ecx+14E0h], 1 (ou 0); inc [conta]; ret
    put(INPUT_OPEN, b'\xC7\x81' + d(INPUT_ENABLED) + d(1) + b'\xFF\x05' + d(BASE + OPEN_COUNT) + b'\xC3')
    put(INPUT_CLOSE, b'\xC7\x81' + d(INPUT_ENABLED) + d(0) + b'\xFF\x05' + d(BASE + CLOSE_COUNT) + b'\xC3')

    for pointer, obj in ((P_CHAT, CHAT), (P_INPUT, INPUT), (P_DIALOG, DIALOG), (P_INFO, INFO)):
        put(pointer, d(BASE + obj))
    # CChat: /pagesize 10, modo normal (F7), sem hora; as tres ultimas das 100 mensagens dele.
    put(CHAT + 0x0, d(10))
    put(CHAT + 0x8, d(2))
    old = (
        (4, '', 'SA-MP {B9C9BF}0.3.7-R1 {FFFFFF}Started', 0xFFA9C4E4, 0),
        (4, '', 'Connected to {FFFFFF}Servidor de Teste', 0xFFA9C4E4, 0),
        (2, 'Victor_Trok', 'oi, algu\xe9m a\xed? [teste]', 0xFFFFFFFF, 0xFF33CCFF),
    )
    for i, (kind, prefix, text, color, prefix_color) in enumerate(old):
        e = CHAT + CHAT_ENTRIES + (97 + i) * ENTRY_SIZE
        put(e + 0x4, prefix.encode('cp1252'))
        put(e + 0x20, text.encode('cp1252'))
        put(e + 0xF0, d(kind) + d(color) + d(prefix_color))

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
