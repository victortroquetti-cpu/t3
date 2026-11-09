#!/usr/bin/env python3
"""Ferramenta da área de mods de CS:Source (mod Trok sobre o Source SDK 2013).

O SDK da Valve não fica no repositório: ele é baixado em cs-source/sdk/ na
versão de sdk.lock. O código do mod fica em cs-source/src e cs-source/game e é
ligado dentro do SDK (link simbólico no Linux/macOS, junção no Windows).
Mudanças em arquivos da Valve ficam em patches/sdk.patch.

Uso:
  python trok.py setup        baixa o SDK, aplica o patch, liga as pastas e gera os projetos
  python trok.py gen          gera de novo os .vpc (rode depois de criar ou apagar arquivos .cpp/.h)
  python trok.py save-patch   grava em patches/sdk.patch tudo o que você mudou em arquivos da Valve
  python trok.py status       mostra a versão do SDK, o patch e os links
  python trok.py check        checa a sintaxe do código do mod com g++ (Linux/macOS, sem compilar o SDK)
  python trok.py refs         baixa os repositórios de referência em refs/ (só leitura, para consulta)

Depois do setup:
  Windows: sdk\\src\\createtrok.bat e abra sdk\\src\\trok.sln no Visual Studio 2022
  Linux:   sdk/src/buildtrok (precisa de podman; compila dentro do Steam Runtime)
"""

import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.join(ROOT, "sdk")
SDK_URL = "https://github.com/ValveSoftware/source-sdk-2013"
PATCH = os.path.join(ROOT, "patches", "sdk.patch")

MOD = "trok"                 # pasta do código (src/game/*/trok) e define TROK
GAMEDIR = "mod_trok"         # pasta do jogo (game/mod_trok) e nome do executável
TITLE = "Trok"               # nome dos projetos no Visual Studio

# Pastas do mod -> onde aparecem dentro do SDK.
LINKS = [
    ("src/game/server/" + MOD, "src/game/server/" + MOD),
    ("src/game/client/" + MOD, "src/game/client/" + MOD),
    ("src/game/shared/" + MOD, "src/game/shared/" + MOD),
    ("game/" + GAMEDIR, "game/" + GAMEDIR),
]

# Arquivos gerados dentro do SDK (não vão para o patch).
GENERATED = [
    "src/game/server/server_%s.vpc" % MOD,
    "src/game/client/client_%s.vpc" % MOD,
    "src/launcher_main/launcher_main_mod_%s.vpc" % MOD,
    "src/build%s" % MOD,
    "src/create%s.bat" % MOD,
]

HEADER = (
    "//-----------------------------------------------------------------------------\n"
    "// GERADO por cs-source/trok.py a partir de {source}. Não edite: as mudanças\n"
    "// somem no próximo \"python trok.py gen\". Para mudar algo, mude o trok.py.\n"
    "//-----------------------------------------------------------------------------\n\n"
)


def die(msg):
    print("erro: " + msg, file=sys.stderr)
    sys.exit(1)


def git(*args, check=True, capture=False):
    result = subprocess.run(
        ["git", "-C", SDK] + list(args),
        check=False,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        text=True,
    )
    if check and result.returncode != 0:
        die("git %s falhou%s" % (" ".join(args), (":\n" + result.stderr) if capture else ""))
    return result


def pinned_commit():
    with open(os.path.join(ROOT, "sdk.lock"), encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                return line
    die("sdk.lock sem commit")


def sdk_path(rel):
    return os.path.join(SDK, *rel.split("/"))


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write(path, text, executable=False):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
    if executable and os.name != "nt":
        os.chmod(path, 0o755)


# ----------------------------------------------------------------------------
# setup
# ----------------------------------------------------------------------------

def fetch_sdk():
    commit = pinned_commit()
    if not os.path.isdir(os.path.join(SDK, ".git")):
        print("baixando o Source SDK 2013 (%s) ..." % commit[:12])
        os.makedirs(SDK, exist_ok=True)
        git("init", "-q")
        git("remote", "add", "origin", SDK_URL)
        git("fetch", "--depth", "1", "origin", commit)
        git("checkout", "-q", "FETCH_HEAD")
        return

    head = git("rev-parse", "HEAD", capture=True).stdout.strip()
    if head != commit:
        print("aviso: o SDK está em %s, mas sdk.lock pede %s." % (head[:12], commit[:12]))
        print("       Para trocar de versão, veja \"Atualizar o SDK\" no README.")


def apply_patch():
    if not os.path.exists(PATCH) or os.path.getsize(PATCH) == 0:
        return
    if git("apply", "--check", PATCH, check=False, capture=True).returncode == 0:
        git("apply", "--whitespace=nowarn", PATCH)
        print("patch aplicado: patches/sdk.patch")
    elif git("apply", "--reverse", "--check", PATCH, check=False, capture=True).returncode == 0:
        print("patch já aplicado: patches/sdk.patch")
    else:
        die("patches/sdk.patch não entra no SDK. Se você mudou arquivos da Valve à mão, "
            "rode \"python trok.py save-patch\" antes; se trocou a versão do SDK, refaça o patch.")


def make_link(src, dst):
    if os.path.lexists(dst):
        if os.path.islink(dst) or is_junction(dst):
            return
        die("%s já existe e não é um link. Apague ou mova antes." % dst)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.name == "nt":
        # Junção: não precisa de administrador nem do modo de desenvolvedor.
        subprocess.run(["cmd", "/c", "mklink", "/J", dst, src], check=True, stdout=subprocess.DEVNULL)
    else:
        os.symlink(os.path.relpath(src, os.path.dirname(dst)), dst)


def is_junction(path):
    isjunction = getattr(os.path, "isjunction", None)  # Python 3.12+
    return bool(isjunction and isjunction(path))


def link_mod():
    for src_rel, dst_rel in LINKS:
        src = os.path.join(ROOT, *src_rel.split("/"))
        os.makedirs(src, exist_ok=True)
        make_link(src, sdk_path(dst_rel))

    # Os links e os arquivos gerados não entram no git do SDK, então o
    # save-patch só vê mudanças de verdade em arquivos da Valve.
    exclude = sdk_path(".git/info/exclude")
    current = read(exclude) if os.path.exists(exclude) else ""
    wanted = ["/" + dst for _, dst in LINKS] + ["/" + g for g in GENERATED] + ["/src/%s.sln" % MOD]
    missing = [w for w in wanted if w not in current.splitlines()]
    if missing:
        with open(exclude, "a", encoding="utf-8") as f:
            f.write("\n# cs-source/trok.py\n" + "\n".join(missing) + "\n")


# ----------------------------------------------------------------------------
# gen: .vpc e scripts de build gerados a partir dos do HL2MP
# ----------------------------------------------------------------------------

def mod_files(side):
    """Arquivos .cpp/.h de src/game/<side>/trok, com caminho relativo à pasta."""
    base = os.path.join(ROOT, "src", "game", side, MOD)
    found = []
    for dirpath, _, names in os.walk(base):
        for name in names:
            if name.endswith((".cpp", ".h")):
                rel = os.path.relpath(os.path.join(dirpath, name), base)
                found.append(rel.replace(os.sep, "\\"))
    return sorted(found, key=str.lower)


def sub_once(pattern, repl, text, what):
    new, n = re.subn(pattern, repl, text, count=1, flags=re.M)
    if n != 1:
        die("não achei %s no .vpc do HL2MP; o SDK mudou e o trok.py precisa de ajuste." % what)
    return new


def gen_game_vpc(side):
    """server_trok.vpc / client_trok.vpc a partir de server_hl2mp.vpc / client_hl2mp.vpc."""
    src_rel = "src/game/%s/%s_hl2mp.vpc" % (side, side)
    text = read(sdk_path(src_rel))

    # GAMENAME define a pasta de saída das DLLs (game/mod_trok/bin).
    text = re.sub(r'^\$Macro GAMENAME[^\n]*\n', "", text, flags=re.M)
    text = sub_once(r'^(\$Macro SRCDIR[^\n]*\n)', r'\1$Macro GAMENAME \t"%s"\n' % GAMEDIR, text, "$Macro SRCDIR")

    text = sub_once(r'(\$AdditionalIncludeDirectories\s+"\$BASE;[^"]*)"',
                    r'\1,.\\%s,$SRCDIR\\game\\shared\\%s"' % (MOD, MOD), text, "$AdditionalIncludeDirectories")
    text = sub_once(r'(\$PreprocessorDefinitions\s+"\$BASE;HL2MP;[^"]*)"',
                    r'\1;%s"' % MOD.upper(), text, "$PreprocessorDefinitions com HL2MP")
    label = "Server" if side == "server" else "Client"
    # A pasta do mod entra logo no começo do $Project do HL2MP, como as
    # outras pastas dele (nada de bloco $Project extra).
    entries = ["\t\t$File\t\"%s\\%s\"" % (MOD, f) for f in mod_files(side)]
    entries += ["\t\t$File\t\"$SRCDIR\\game\\shared\\%s\\%s\"" % (MOD, f) for f in mod_files("shared")]
    folder = ("\t// Código do mod (cs-source/src/game/{%s,shared}/%s), listado pelo trok.py.\n"
              "\t$Folder\t\"%s\"\n\t{\n%s\n\t}\n\n" % (side, MOD, TITLE, "\n".join(entries)))
    text = sub_once(r'^\$Project "%s \(HL2MP\)"\s*\n\{[ \t]*\r?\n' % label,
                    lambda m: '$Project "%s (%s)"\n{\n%s' % (label, TITLE, folder), text,
                    '$Project "%s (HL2MP)"' % label)

    write(sdk_path("src/game/%s/%s_%s.vpc" % (side, side, MOD)), HEADER.format(source=src_rel) + text)


def gen_launcher_vpc():
    src_rel = "src/launcher_main/launcher_main_mod_hl2mp.vpc"
    text = read(sdk_path(src_rel))
    text = sub_once(r'^\$Macro OUTBINNAME "mod_hl2mp"', '$Macro OUTBINNAME "%s"' % GAMEDIR, text, "OUTBINNAME")
    text = sub_once(r'^\$Project "Launcher Main \(Mod HL2MP\)"', '$Project "Launcher Main (Mod %s)"' % TITLE,
                    text, '$Project "Launcher Main (Mod HL2MP)"')
    write(sdk_path("src/launcher_main/launcher_main_mod_%s.vpc" % MOD), HEADER.format(source=src_rel) + text)


def gen_build_scripts():
    src_rel = "src/buildallprojects"
    text = read(sdk_path(src_rel))
    text = sub_once(r'/hl2mp /tf ', "/%s " % MOD, text, "\"/hl2mp /tf\" no buildallprojects")
    text = sub_once(r'sdk_everything_', "%s_everything_" % MOD, text, "sdk_everything_ no buildallprojects")
    text = text.replace("#!/bin/bash\n", "#!/bin/bash\n# GERADO por cs-source/trok.py a partir de src/buildallprojects.\n", 1)
    write(sdk_path("src/build%s" % MOD), text, executable=True)

    write(sdk_path("src/create%s.bat" % MOD),
          "@rem GERADO por cs-source/trok.py. Gera trok.sln (Visual Studio 2022).\r\n"
          "devtools\\bin\\vpc.exe /%s /define:SOURCESDK +everything /mksln %s.sln\r\n" % (MOD, MOD))



def generate():
    if not os.path.isdir(sdk_path("src")):
        die("o SDK não está em cs-source/sdk. Rode \"python trok.py setup\" antes.")
    gen_game_vpc("server")
    gen_game_vpc("client")
    gen_launcher_vpc()
    gen_build_scripts()
    n = len(mod_files("server")) + len(mod_files("client")) + len(mod_files("shared"))
    print("projetos gerados: server_%s.vpc, client_%s.vpc, launcher_main_mod_%s.vpc (%d arquivos do mod)"
          % (MOD, MOD, MOD, n))
    print("o Visual Studio só vê arquivos novos depois de rodar sdk\\src\\create%s.bat de novo." % MOD)


# ----------------------------------------------------------------------------
# comandos
# ----------------------------------------------------------------------------

def cmd_setup():
    if shutil.which("git") is None:
        die("git não encontrado")
    fetch_sdk()
    apply_patch()
    link_mod()
    generate()
    print("\npronto. Próximo passo:")
    if os.name == "nt":
        print("  cd sdk\\src && create%s.bat   (depois abra %s.sln)" % (MOD, MOD))
    else:
        print("  sdk/src/build%s   (precisa de podman)" % MOD)


def cmd_save_patch():
    diff = git("diff", "--binary", "HEAD", capture=True).stdout
    write(PATCH, diff)
    files = re.findall(r'^diff --git a/(\S+)', diff, flags=re.M)
    print("patches/sdk.patch: %d arquivo(s) da Valve mudado(s)" % len(files))
    for f in files:
        print("  " + f)
    untracked = git("ls-files", "--others", "--exclude-standard", capture=True).stdout.split()
    untracked = [u for u in untracked if not u.endswith((".pyc",))]
    if untracked:
        print("\naviso: arquivos novos dentro de sdk/ (fora das pastas do mod) não entram no patch:")
        for u in untracked[:20]:
            print("  " + u)
        print("Mova-os para cs-source/src/game/*/%s (código) ou cs-source/game/%s (conteúdo)." % (MOD, GAMEDIR))


def cmd_status():
    print("SDK pedido (sdk.lock): " + pinned_commit())
    if not os.path.isdir(os.path.join(SDK, ".git")):
        print("SDK: não baixado (rode setup)")
        return
    print("SDK baixado:           " + git("rev-parse", "HEAD", capture=True).stdout.strip())
    changed = git("diff", "--name-only", "HEAD", capture=True).stdout.split()
    print("arquivos da Valve mudados: %d" % len(changed))
    saved = read(PATCH) if os.path.exists(PATCH) else ""
    current = git("diff", "--binary", "HEAD", capture=True).stdout
    if current != saved:
        print("  (diferente de patches/sdk.patch: rode save-patch para guardar)")
    for _, dst in LINKS:
        path = sdk_path(dst)
        ok = os.path.islink(path) or is_junction(path)
        print("link %-28s %s" % (dst, "ok" if ok else "FALTANDO (rode setup)"))


# Flags do Linux 64 bits tiradas dos .vpc do SDK (source_posix_base, server_base,
# client_base e *_hl2mp). Só checam sintaxe e tipos; não substituem o build.
CHECK_COMMON = ("-std=gnu++17 -fsyntax-only -m64 -DVPC -DSOURCESDK -DSOURCE_HAS_FREETYPE -DNDEBUG -DGNUC "
                "-DPOSIX -D_POSIX -DCOMPILER_GCC -D_DLL_EXT=.so -D_LINUX -DLINUX -DPLATFORM_64BITS "
                "-DDEV_BUILD -DFRAME_POINTER_OMISSION_DISABLED -DVECTOR -DVERSION_SAFE_STEAM_API_INTERFACES "
                "-DPROTECTED_THINGS_ENABLE -DINCLUDED_STEAM2_USERID_STRUCTS -DHL2MP -DNEXT_BOT -DTROK "
                "-Dstrncpy=use_Q_strncpy_instead -D_snprintf=use_Q_snprintf_instead "
                "-I../../common -I../../public -I../../public/tier0 -I../../public/tier1 -I. -I../shared")
CHECK_SIDE = {
    "server": ("-DDLLNAME=server -DGAME_DLL -DHL2_DLL -DSWDS -Dsprintf=use_Q_snprintf_instead_of_sprintf "
               "-I../../utils/common -I../shared/econ -INextBot -I../shared/hl2 -Ihl2 -Ihl2mp "
               "-I../shared/hl2mp -Itrok -I../shared/trok"),
    "client": ("-DDLLNAME=client -DCLIENT_DLL -DHL2_CLIENT_DLL -DNO_STRING_T -I../../vgui2/include "
               "-I../../vgui2/controls -I./game_controls -I../../thirdparty/sixensesdk/include -Ihl2mp/ui "
               "-Ihl2mp -I../shared/hl2mp -Ihl2 -Ihl2/elements -I../shared/hl2 -Itrok -I../shared/trok"),
}


def cmd_check():
    cxx = os.environ.get("CXX", "g++")
    if os.name == "nt" or shutil.which(cxx) is None:
        die("check precisa de g++ (Linux/macOS). No Windows, compile pelo Visual Studio.")
    if not os.path.isdir(sdk_path("src")):
        die("rode \"python trok.py setup\" antes.")
    failed = 0
    for side in ("server", "client"):
        cwd = sdk_path("src/game/" + side)
        files = [os.path.join(MOD, f.replace("\\", os.sep)) for f in mod_files(side) if f.endswith(".cpp")]
        files += [os.path.join("..", "shared", MOD, f.replace("\\", os.sep))
                  for f in mod_files("shared") if f.endswith(".cpp")]
        # Arquivos da Valve que o sdk.patch altera (só os .cpp deste lado ou do shared).
        patch = os.path.join(ROOT, "patches", "sdk.patch")
        if os.path.isfile(patch):
            for line in open(patch, encoding="utf-8", errors="replace"):
                if not line.startswith("+++ b/src/game/") or not line.rstrip().endswith(".cpp"):
                    continue
                rel = line[len("+++ b/src/game/"):].strip()
                top, rest = rel.split("/", 1)
                if top == side:
                    files.append(rest)
                elif top == "shared":
                    files.append(os.path.join("..", "shared", rest))
        for f in files:
            args = [cxx] + (CHECK_COMMON + " " + CHECK_SIDE[side]).split() + ["-w", f]
            rc = subprocess.run(args, cwd=cwd).returncode
            print("%-6s %-50s %s" % (side, f, "ok" if rc == 0 else "ERRO"))
            failed += rc != 0
    if failed:
        die("%d arquivo(s) com erro" % failed)


# Repositórios de consulta (código legal, sem nada vazado). Ficam em refs/,
# fora do git, e servem para ler e copiar ideias; nada daqui entra no build.
REFS = [
    ("zmr-game", "https://github.com/zm-reborn/zmr-game",
     "Zombie Master: Reborn: mod MP completo no SDK 2013 (zumbis com IA, rodadas, HUD próprio)"),
    ("mapbase", "https://github.com/mapbase-source/source-sdk-2013",
     "Mapbase: fork do SDK com VScript, entidades e correções (base SP, porte MP em andamento)"),
    ("sourcemod", "https://github.com/alliedmodders/sourcemod",
     "SourceMod: plugins de servidor para o CS:S original (referência de eventos e gamedata do CS:S)"),
    # Modos zumbi (só leitura de regras e números: GPL ou sem licença; não copiar código nem assets).
    ("zombiereloaded", "https://github.com/srcdslab/sm-plugin-zombiereloaded",
     "Zombie:Reloaded (GPLv3): infecção completa para CS:S; classes, coice, configs"),
    ("zombie-riot", "https://github.com/jono666/Zombie-Riot",
     "Zombie Riot (fork 1.9.1b, sem licença declarada): survival em ondas com bots no CS:S"),
    ("zombie-escape-amxx", "https://github.com/z0h1r-LK/Zombie_Escape",
     "Zombie Escape para CS 1.6 (GPLv3, base Zombie Plague 5.0): regras do modo fuga"),
    # Mods do SDK com a MESMA licença do Trok (Source 1 SDK): código pode ser reaproveitado
    # com crédito no CREDITOS.txt. Arte, modelos e sons deles não. Só o código é baixado.
    ("neotokyo-rebuild", "https://github.com/NeotokyoRebuild/neo",
     "NEOTOKYO Rebuild: SDK 2013 MP 2025 (a nossa base); bots NextBot, menu próprio, mira, inclinar",
     ["src/game", "src/public/GameUI", "src/common/GameUI"]),
    ("double-action", "https://github.com/BSVino/DoubleAction",
     "Double Action: Boogaloo: movimento acrobático, viewmodel, bots, menu próprio",
     ["mp/src/game"]),
    ("momentum-mod", "https://github.com/momentum-mod/game",
     "Momentum Mod (código congelado em 2025): movimento estilo CS/HL, UI própria",
     ["mp/src/game"]),
    ("fortress-forever", "https://github.com/fortressforever/fortressforever",
     "Fortress Forever (SDK 2006): classes e modos; referência antiga",
     ["game_shared", "cl_dll", "dlls"]),
]


def cmd_refs():
    base = os.path.join(ROOT, "refs")
    os.makedirs(base, exist_ok=True)
    for ref in REFS:
        name, url, what = ref[:3]
        sparse = ref[3] if len(ref) > 3 else None
        dst = os.path.join(base, name)
        if os.path.isdir(os.path.join(dst, ".git")):
            print("ok      refs/%s" % name)
            continue
        print("baixando refs/%s: %s" % (name, what))
        if sparse:
            # Sem arte: só as pastas de código.
            rc = subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none",
                                 "--sparse", url, dst]).returncode
            if rc == 0:
                rc = subprocess.run(["git", "-C", dst, "sparse-checkout", "set"] + sparse).returncode
        else:
            rc = subprocess.run(["git", "clone", "-q", "--depth", "1", url, dst]).returncode
        if rc != 0:
            print("aviso: não consegui baixar %s" % url)


COMMANDS = {
    "setup": cmd_setup,
    "gen": generate,
    "save-patch": cmd_save_patch,
    "status": cmd_status,
    "check": cmd_check,
    "refs": cmd_refs,
}

if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in COMMANDS:
        print(__doc__)
        sys.exit(1)
    COMMANDS[sys.argv[1]]()
