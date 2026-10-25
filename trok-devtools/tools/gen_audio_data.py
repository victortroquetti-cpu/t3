# Trok DevTools: gera src/audio_data.h (nomes dos bancos e dos sons do SFX, os bancos de motor de cada veiculo e os
# sons de cada arma) a partir dos arquivos do gta-reversed (github.com/gta-reversed/gta-reversed, source/game_sa):
#   Audio/Enums/eSoundBank.h, Audio/Enums/SoundIDs.h, Audio/Entities/AEVehicleAudioEntity.VehicleAudioSettings.h,
#   Audio/Entities/AEWeaponAudioEntity.cpp
# Uso: python3 -I tools/gen_audio_data.py <pasta com os 4 arquivos> > src/audio_data.h
import os
import re
import sys

D = sys.argv[1]


def read(name):
    with open(os.path.join(D, name), encoding='utf-8', errors='replace') as f:
        return f.read()


PACKS = ['FEET', 'GENRL', 'PAIN_A', 'SCRIPT', 'SPC_EA', 'SPC_FA', 'SPC_GA', 'SPC_NA', 'SPC_PA']

# ---------------------------------------------------------------- bancos
banks = {}
for line in read('eSoundBank.h').splitlines():
    m = re.match(r'\s*SND_BANK_([A-Z0-9_]+)\s*=\s*([^,]+),?\s*(//\s*0x([0-9A-Fa-f]+))?', line)
    if not m:
        continue
    name, value, hexv = m.group(1), m.group(2).strip(), m.group(4)
    if name.endswith('_FIRST') or name.endswith('_LAST') or name in ('UNK', 'FIRST', 'LAST'):
        continue
    if hexv:
        num = int(hexv, 16)
    elif re.fullmatch(r'-?\d+', value):
        num = int(value)
    else:
        continue
    if num >= 0 and num not in banks:
        banks[num] = name
count = max(banks) + 1
missing = [i for i in range(count) if i not in banks]
if missing:
    sys.exit('bancos sem nome: %s' % missing[:20])
full = {v: k for k, v in banks.items()}


def short(name):
    for p in sorted(PACKS, key=len, reverse=True):
        if name.startswith(p + '_'):
            return name[len(p) + 1:]
    return name


# ---------------------------------------------------------------- nomes dos sons (por banco)
sounds = []
text = read('SoundIDs.h')
for enum, body in re.findall(r'enum\s+(SND_[A-Z0-9_]+)\s*:\s*eSoundID\s*\{(.*?)\};', text, re.S):
    bank = full.get(enum[len('SND_'):])
    if bank is None:
        continue
    for name, num in re.findall(r'\b' + enum + r'_([A-Z0-9_]+)\s*=\s*(\d+)', body):
        sounds.append((bank, int(num), name))
sounds.sort()

# ---------------------------------------------------------------- veiculos (modelo 400 + i)
vehicles = []
for m in re.finditer(r'\{\s*AE_[A-Z_]+\s*,\s*([A-Z0-9_\-]+)\s*,\s*([A-Z0-9_\-]+)\s*,', read('AEVehicleAudioEntity.VehicleAudioSettings.h')):
    def bank_of(tok):
        return full.get(tok[len('SND_BANK_'):], -1) if tok.startswith('SND_BANK_') else -1
    vehicles.append((bank_of(m.group(1)), bank_of(m.group(2))))
vehicles = vehicles[:212]
if len(vehicles) != 212:
    sys.exit('tabela de veiculos com %d entradas' % len(vehicles))

# ---------------------------------------------------------------- armas: sons no banco WEAPONS
weap_bank = full['GENRL_WEAPONS']
weap_names = {name: num for bank, num, name in sounds if bank == weap_bank}
src = read('AEWeaponAudioEntity.cpp')
by_case = {}


def block_ids(block):
    """Os sons de um case: os nomes SND_GENRL_WEAPONS_* e os numeros do PlaySound(evento ? a : b) da recarga."""
    ids = set(weap_names[n] for n in re.findall(r'SND_GENRL_WEAPONS_([A-Z0-9_]+)', block) if n in weap_names)
    for pair in re.findall(r'PlaySound\([^)]*?(\d+)\s*:\s*(\d+)', block):
        ids.update(int(x) for x in pair)
    return ids


def switch_bodies(text):
    """O corpo de cada switch (weaponType) / (type), pelas chaves (os cases tem blocos com chaves dentro)."""
    for m in re.finditer(r'switch \((?:weaponType|type)\) \{', text):
        depth, i = 1, m.end()
        while depth and i < len(text):
            depth += {'{': 1, '}': -1}.get(text[i], 0)
            i += 1
        yield text[m.end():i - 1]


for body in switch_bodies(src):
    pending, block = [], ''
    for line in body.splitlines():
        c = re.match(r'\s*case (WEAPON_[A-Z0-9_]+):', line)
        if c:
            if block:
                for w in pending:
                    by_case.setdefault(w, set()).update(block_ids(block))
                pending, block = [], ''
            pending.append(c.group(1))
            continue
        block += line + '\n'
    for w in pending:
        by_case.setdefault(w, set()).update(block_ids(block))


def names(*keys):
    return set(num for name, num in weap_names.items() if any(k in name for k in keys))


# (rotulo, palavras para a busca, constante do jogo, sons extras por nome)
WEAPONS = [
    ('Soco', 'soco punho fist', None, names('BODY_PUNCH', 'BOX_FACE', 'MARTIAL', 'SMACK', 'STOMP', 'WEAPON_SWIPE_FIST')),
    ('Soco-ingl\\xC3\\xAAs', 'brass knuckle', 'WEAPON_BRASSKNUCKLE', names('BRASS', 'KNUCKLE')),
    ('Taco de golfe', 'golf club', 'WEAPON_GOLFCLUB', names('CLUB_STRIKE', 'WEAPON_SWIPE_METAL')),
    ('Cassetete', 'nightstick', 'WEAPON_NIGHTSTICK', names('CLUB_STRIKE', 'WEAPON_SWIPE')),
    ('Faca', 'knife', 'WEAPON_KNIFE', names('KNIFE', 'STAB')),
    ('Taco de beisebol', 'bat baseball', 'WEAPON_BASEBALLBAT', names('BASEBALL_BAT', 'BAT_STRIKE')),
    ('P\\xC3\\xA1', 'shovel', 'WEAPON_SHOVEL', names('SHOVEL')),
    ('Taco de sinuca', 'pool cue', 'WEAPON_POOL_CUE', names('POOLCUE')),
    ('Katana', 'katana sword', 'WEAPON_KATANA', names('KATANA')),
    ('Bengala', 'cane', 'WEAPON_CANE', names('CANE')),
    ('Flores', 'flowers', 'WEAPON_FLOWERS', names('FLOWER')),
    ('Pistola (Colt 45)', 'colt 45 pistol 9mm', 'WEAPON_PISTOL', set()),
    ('Pistola com silenciador', 'silenced silencer', 'WEAPON_PISTOL_SILENCED', set()),
    ('Desert Eagle', 'deagle eagle', 'WEAPON_DESERT_EAGLE', set()),
    ('Escopeta', 'shotgun', 'WEAPON_SHOTGUN', set()),
    ('Escopeta de cano serrado', 'sawnoff sawn', 'WEAPON_SAWNOFF_SHOTGUN', set()),
    ('Escopeta de combate', 'spas12 spas combat', 'WEAPON_SPAS12_SHOTGUN', set()),
    ('Micro Uzi', 'uzi microsmg', 'WEAPON_MICRO_UZI', set()),
    ('MP5', 'mp5 smg', 'WEAPON_MP5', set()),
    ('Tec-9', 'tec9', 'WEAPON_TEC9', set()),
    ('AK-47', 'ak47 ak', 'WEAPON_AK47', set()),
    ('M4', 'm4', 'WEAPON_M4', set()),
    ('Rifle', 'country rifle cuntgun', 'WEAPON_COUNTRYRIFLE', set()),
    ('Sniper', 'sniper', 'WEAPON_SNIPERRIFLE', set()),
    ('Lan\\xC3\\xA7" "a-foguete', 'rocket rpg launcher bazuca', 'WEAPON_RLAUNCHER', names('ROCKET')),
    ('Lan\\xC3\\xA7" "a-chamas', 'flamethrower flame', 'WEAPON_FLAMETHROWER', names('GAS_LOOP')),
    ('Minigun', 'minigun', 'WEAPON_MINIGUN', names('MINIGUN')),
    ('Detonador', 'detonator satchel', 'WEAPON_DETONATOR', set()),
    ('Spray', 'spraycan spray', 'WEAPON_SPRAYCAN', set()),
    ('Extintor', 'extinguisher', 'WEAPON_EXTINGUISHER', set()),
    ('C\\xC3\\xA2mera', 'camera foto', 'WEAPON_CAMERA', names('CAMERA')),
    ('Vis\\xC3\\xA3o noturna', 'night vision infrared goggles', 'WEAPON_NIGHTVISION', set()),
    ('Paraquedas', 'parachute', 'WEAPON_PARACHUTE', names('PARACHUTE')),
]
weapons = []
for label, keys, const, extra in WEAPONS:
    ids = set(extra) | (by_case.get(const, set()) if const else set())
    ids.discard(-1)
    weapons.append((label, keys, sorted(i for i in ids if i >= 0)))
chainsaw = full['GENRL_CHAINSAW_P']

# ---------------------------------------------------------------- saida
out = []
w = out.append
w('// Trok DevTools (.asi) -- Victor_Trok')
w('// GERADO por tools/gen_audio_data.py a partir do gta-reversed (nomes dos bancos e dos sons do SFX, os bancos de motor')
w('// de cada veiculo e os sons de cada arma). Nao edite a mao.')
w('#pragma once')
w('')
w('namespace audio_data {')
w('')
w('constexpr int BANK_COUNT = %d;' % count)
w('// Nome curto de cada banco (sem o pacote): o indice e o do BankLkup.dat.')
w('inline const char* const BANK_NAMES[BANK_COUNT] = {')
row = []
for i in range(count):
    row.append('"%s"' % short(banks[i]))
    if len(row) == 8:
        w('    ' + ', '.join(row) + ',')
        row = []
if row:
    w('    ' + ', '.join(row) + ',')
w('};')
w('')
w('struct SoundName {')
w('    short bank, sound;')
w('    const char* name;')
w('};')
w('// Os sons com nome no jogo (em ordem de banco e de som).')
w('inline const SoundName SOUND_NAMES[] = {')
for bank, num, name in sounds:
    w('    {%d, %d, "%s"},' % (bank, num, name))
w('};')
w('')
w('struct VehicleBanks {')
w('    short player, dummy; // com voce dirigindo / no trafego (-1 = nenhum)')
w('};')
w('// O modelo 400 + i.')
w('inline const VehicleBanks VEHICLE_BANKS[212] = {')
for i in range(0, 212, 8):
    w('    ' + ', '.join('{%d, %d}' % v for v in vehicles[i:i + 8]) + ',')
w('};')
w('')
w('constexpr short WEAPONS_BANK = %d, CHAINSAW_BANK = %d;' % (weap_bank, chainsaw))
w('struct Weapon {')
w('    const char* name;  // em portugues')
w('    const char* keys;  // palavras para a busca')
w('    const short* sounds; // sons do banco WEAPONS (-1 no fim)')
w('};')
for i, (label, keys, ids) in enumerate(weapons):
    w('inline const short WEAPON_SOUNDS_%d[] = {%s};' % (i, ', '.join(str(x) for x in ids + [-1])))
w('inline const Weapon WEAPONS[] = {')
for i, (label, keys, ids) in enumerate(weapons):
    w('    {"%s", "%s", WEAPON_SOUNDS_%d},' % (label, keys, i))
w('    {"Motosserra", "chainsaw motosserra", nullptr}, // o banco CHAINSAW_P inteiro')
w('};')
w('')
w('} // namespace audio_data')
print('\n'.join(out))
print('bancos %d, sons com nome %d, armas %d' % (count, len(sounds), len(weapons)), file=sys.stderr)
for label, keys, ids in weapons:
    if not ids:
        print('  arma sem som: %s' % label, file=sys.stderr)
