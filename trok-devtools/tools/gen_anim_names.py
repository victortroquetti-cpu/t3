# Trok DevTools: gera src/anim_names.h (as animacoes originais do GTA SA, por .ifp, com o indice do SA-MP) a partir da
# tabela AnimationNames do open.mp (github.com/openmultiplayer/open.mp-sdk, include/anim.hpp, licenca MIT). O indice na
# tabela e o mesmo do GetPlayerAnimationIndex do SA-MP.
# Uso: python3 -I tools/gen_anim_names.py <anim.hpp> > src/anim_names.h
import re
import sys

with open(sys.argv[1], encoding='utf-8') as f:
    text = re.sub(r'//[^\n]*', '', f.read())


def table(name):
    start = text.index(name + '[]')
    body = text[text.index('{', start) + 1:text.index('};', start)]
    return re.findall(r'"([^"]*)"', body)


names = table('AnimationNames')
# Os .ifp do jogo (o open.mp lista tambem os que ficaram de fora da tabela de nomes)
allLibs = table('AnimLibs')
# Do anim.img do 1.0, fora das duas tabelas do open.mp (ficam sem nomes, entao sem a marca de nova)
allLibs += ['BLOWJOBZ', 'SEX', 'SNM']

rows = []
libs = []
for index, full in enumerate(names):
    if not full:
        continue
    lib, name = full.split(':', 1)
    if lib not in libs:
        libs.append(lib)
    rows.append((lib, name, index))

out = []
out.append('// Trok DevTools -- gerado por tools/gen_anim_names.py (open.mp-sdk, include/anim.hpp, licenca MIT): as')
out.append('// animacoes originais do GTA SA por .ifp (o nome do bloco), com o indice do SA-MP. Nao editar a mao.')
out.append('#pragma once')
out.append('')
out.append('namespace anim_names {')
out.append('')
out.append('struct Name {')
out.append('    const char* lib;  // o bloco (o .ifp sem a extensao), em maiusculas')
out.append('    const char* name; // como o open.mp escreve (o jogo compara sem diferenciar maiusculas)')
out.append('    short sampId;')
out.append('};')
out.append('')
out.append('// Todos os .ifp do anim.img e o ped.ifp; os que nao tem nomes na tabela abaixo ficam sem a marca de nova.')
out.append('constexpr const char* LIBS[] = {')
for lib in sorted(set(allLibs) | set(libs)):
    out.append('    "%s",' % lib)
out.append('};')
out.append('')
out.append('constexpr Name NAMES[] = {')
for lib, name, index in rows:
    out.append('    {"%s", "%s", %d},' % (lib, name, index))
out.append('};')
out.append('')
out.append('} // namespace anim_names')
print('\n'.join(out))
print('%d .ifp (%d com nomes), %d animacoes' % (len(set(allLibs) | set(libs)), len(libs), len(rows)), file=sys.stderr)
