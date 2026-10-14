# Junta as imagens e os sons padrao (dist/Trok Hitmarker) num .cpp para o .asi: sem um arquivo na pasta, vale o que
# vem dentro dele.
# Uso: python3 -I embed.py <pasta "Trok Hitmarker"> <saida.cpp>
import os
import sys

IMAGES = ['mira', 'acerto', 'colete', 'cabeca', 'abate', 'corpo']  # a ordem do hud::ImageSlot
SOUNDS = ['acerto', 'cabeca', 'dano_levado', 'abate', 'abate_cabeca', 'morte']  # a ordem do sound::Id


def array(name, data):
    lines = []
    for i in range(0, len(data), 24):
        lines.append('    ' + ','.join(str(b) for b in data[i:i + 24]) + ',')
    return 'const unsigned char %s[] = {\n%s\n};\n' % (name, '\n'.join(lines))


def main(root, out):
    parts = ['// Gerado por res/embed.py: nao editar.\n#include "res.h"\n\nnamespace res {\nnamespace {\n']
    tables = []
    for folder, names, ext, table in (('mira', IMAGES, '.png', 'IMAGES'), ('sons', SOUNDS, '.wav', 'SOUNDS')):
        rows = []
        for n in names:
            with open(os.path.join(root, folder, n + ext), 'rb') as f:
                data = f.read()
            var = '%s_%s' % (folder, n)
            parts.append(array(var, data))
            rows.append('    {"%s%s", %s, %d},' % (n, ext, var, len(data)))
        tables.append('const File %s[%d] = {\n%s\n};\n' % (table, len(names), '\n'.join(rows)))
    parts.append('} // namespace\n\n')
    parts.extend(tables)
    parts.append('\n} // namespace res\n')
    with open(out, 'w') as f:
        f.write(''.join(parts))


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
