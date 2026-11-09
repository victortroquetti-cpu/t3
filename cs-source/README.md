# CS:Source — área de mods (Trok)

Base para uma **conversão total** de Counter-Strike: Source: um mod do **Source SDK 2013
Multiplayer** (código oficial da Valve, 64 bits, atualização de 2025) que monta o conteúdo do
CS:S instalado e refaz os sistemas do jogo em C++.

Hoje o mod é o HL2MP (jogador, armas e bots NextBot) com regras próprias (`CTrokRules`) já
ligadas. Os sistemas novos entram em `src/game/*/trok/` conforme forem pedidos.

## Por que um mod do SDK, e não plugin de servidor

| | Mod do SDK 2013 MP (escolhido) | SourceMod no CS:S original |
|---|---|---|
| Código de jogo | inteiro, servidor e cliente | só servidor, por eventos e ganchos |
| HUD, armas, movimento, predição | refaz tudo | quase nada (sem código no cliente) |
| IA de bots | NextBot completo, do zero | só os bots do CS:S, com ajustes |
| Conteúdo do CS:S | montado do jogo instalado | nativo |
| O que vem do CS:S | **nada de código**: compra, bomba, movimento e bots do CS são refeitos | tudo |

O código de jogo do CS:S não é público. Código vazado (2020) não pode ser usado; ver a skill.

Estado do trabalho, riscos e pendências: [`HANDOFF.md`](HANDOFF.md).

## Requisitos

- **Para compilar:** Python 3.13+, git e
  - Windows: Visual Studio 2022 com "Desktop development with C++" (MSVC v143 + Windows SDK);
  - Linux: podman (o build roda no Steam Runtime "sniper").
- **Para rodar:** Steam com **Source SDK Base 2013 Multiplayer** (Ferramentas) e **Counter-Strike: Source**.

## Primeira vez

```bash
python cs-source/trok.py setup
```

Baixa o SDK da Valve em `cs-source/sdk/` no commit de `sdk.lock` (~600 MB), aplica
`patches/sdk.patch`, liga as pastas do mod dentro do SDK e gera os projetos. Depois:

- **Windows:** rode `cs-source\sdk\src\createtrok.bat`, abra `cs-source\sdk\src\trok.sln`,
  compile em Release e rode `Client (Trok)` (ou `cs-source\sdk\game\mod_trok.exe`).
- **Linux:** `cs-source/sdk/src/buildtrok` e depois `cs-source/sdk/game/mod_trok`.

## Dia a dia

| Comando | Quando |
|---|---|
| `python cs-source/trok.py gen` | criou ou apagou um `.cpp`/`.h` em `src/game/*/trok/` (no Windows, rode o `createtrok.bat` de novo) |
| `python cs-source/trok.py save-patch` | mudou arquivo da Valve dentro de `sdk/`; grava tudo em `patches/sdk.patch` |
| `python cs-source/trok.py check` | checa a sintaxe do código do mod com g++ (Linux/macOS), sem compilar o SDK |
| `python cs-source/trok.py status` | versão do SDK, patch pendente e links |
| `python cs-source/trok.py refs` | baixa os repositórios de consulta em `refs/` |

Regra de ouro: **código novo em `src/game/*/trok/`**, nunca solto dentro de `sdk/`. Arquivo da
Valve só se não houver outro jeito, com `#ifdef TROK`, e sempre com `save-patch` depois.

## Estrutura

```
cs-source/
  trok.py                  ferramenta (setup, gen, check, save-patch, status, refs)
  sdk.lock                 commit do ValveSoftware/source-sdk-2013 usado
  patches/sdk.patch        mudanças em arquivos da Valve (registro do jogo TROK no VPC
                           e InstallGameRules criando CTrokRules)
  src/game/server/trok/    código só do servidor
  src/game/client/trok/    código só do cliente
  src/game/shared/trok/    código dos dois lados (trok_gamerules.*)
  game/mod_trok/           pasta do jogo: gameinfo.txt, steam.inf, cfg/
  sdk/                     (fora do git) o SDK, com o patch e os links
  refs/                    (fora do git) repositórios de consulta
```

Gerados dentro do SDK pelo `trok.py` (não edite): `server_trok.vpc`, `client_trok.vpc` e
`launcher_main_mod_trok.vpc` (cópias dos do HL2MP com o define `TROK`, saída em
`game/mod_trok/bin` e os arquivos de `trok/`), `buildtrok` e `createtrok.bat`.

## Conteúdo do CS:S

`game/mod_trok/gameinfo.txt` monta o CS:S (`|appid_240|cstrike/cstrike_pak.vpk`) abaixo dos
arquivos do mod e do HL2MP: modelos, materiais, sons e mapas do CS:S ficam disponíveis, mas os
scripts e o HUD continuam os do HL2MP até o mod ter os seus. Mapas do CS:S abrem, porém as
entidades exclusivas do CS (zona de compra, bomba, reféns) só funcionam quando o mod as recriar.
O mod publicado não pode levar arquivos do CS:S.

## Skill e referências para o Claude

`.claude/skills/source-mod/` (raiz do repositório) ensina o fluxo deste mod e traz referências
conferidas no código do SDK: rede e predição, NextBot (IA de bots), entidades e regras de jogo,
armas/movimento/HUD, conteúdo e ferramentas, e o guia de `refs/`:

| `refs/` | Para quê |
|---|---|
| `zmr-game` | Zombie Master: Reborn, mod MP completo no SDK 2013 (regras, jogador, armas, IA de zumbis) |
| `mapbase` | fork do SDK com VScript e muitas correções |
| `sourcemod` | só consulta de eventos e props do CS:S original (GPL: não copiar código) |

## Atualizar o SDK

1. `cd cs-source/sdk && git fetch --depth 1 origin master && git log -1 FETCH_HEAD`
2. Guarde as suas mudanças (`trok.py save-patch`), volte os arquivos (`git checkout -- .`),
   faça `git checkout FETCH_HEAD` e rode `python cs-source/trok.py setup`.
3. Se o patch não entrar, aplique à mão (`git apply --3way ../patches/sdk.patch`), resolva e
   rode `save-patch`. Com tudo compilando, troque o commit em `sdk.lock`.

## O que já foi verificado

- `setup`, `gen`, `status` e `save-patch` rodam do zero no Linux; o patch entra limpo no commit de `sdk.lock`.
- `trok.py check` passa no servidor e no cliente (`trok_gamerules.cpp`).
- **Ainda não verificado:** o build completo (VPC + compilador) e o jogo rodando. Neste ambiente
  não há podman, e o `vpc` da Valve não foi executado. O primeiro build no Windows ou Linux confirma
  os .vpc gerados e o gameinfo.
