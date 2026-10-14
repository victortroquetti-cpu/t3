#!/usr/bin/env python3
"""Gera um samp.dll de mentira para o smoke test do Trok Headmove (test/smoke/run.sh).

So tem o ponto de entrada do SA-MP 0.3.7 R1 (o .asi reconhece a versao por ele) e uma imagem RWX do tamanho do samp.dll:
o fake_gta.exe poe nela, pelos enderecos do R1, os saltos para as funcoes de mentira dele (os dois ProcessHead, o
AimStuff::GetAim e o CChat::AddEntry) e os ponteiros (a mira da camera e o chat).

Uso: make_fake_samp.py <saida/samp.dll> [r3]   (r3: ponto de entrada do 0.3.7 R3, o .asi tem que recusar)
"""
import struct
import sys

BASE = 0x30000000  # sem relocacoes: o Wine carrega na base preferida
SIZE = 0x320000
ENTRY_R1 = 0x31DF13
ENTRY_R3 = 0xCC4D0


def main():
    entry = ENTRY_R3 if len(sys.argv) > 2 and sys.argv[2] == 'r3' else ENTRY_R1
    img = bytearray(SIZE)
    img[entry:entry + 8] = b'\xB8\x01\x00\x00\x00\xC2\x0C\x00'  # DllMain: mov eax, 1; ret 0Ch

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
