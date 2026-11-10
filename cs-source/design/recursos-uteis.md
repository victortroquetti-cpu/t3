# Recursos úteis para o Rearmed (varredura de 10/11/2026)

Feita com busca na web, no GitHub e no SDK local. O wiki da Valve e a documentação da Steam não abriram nesta rede:
o que vem deles está marcado como "a conferir". São só sugestões: fazer quando o dono pedir.

## Top 10
1. **VScript (Squirrel) já vem no servidor do SDK 2025 e funciona no HL2MP**, conferido no código local.
   - **Como funciona:** o `server_base.vpc` compila `vscript_server.cpp`, e a VM roda `scripts/vscripts/mapspawn.nut`.
   - **O que já tem:**
     - ganchos `OnGameEvent_<evento>` (por exemplo `player_death`, `player_hurt`) e `OnTakeDamage`;
     - instâncias `NetProps`, `Convars`, `Entities`, `EntityOutputs` e `NavMesh`;
     - funções `EntFire`, `SpawnEntityFromTable`, `TraceLine` e outras;
     - o bot do HL2MP expõe `AddBotTag`, `AddWeaponRestriction` e `GetDifficulty`;
     - `CGameRules::RegisterScriptFunctions()` é virtual, então as regras podem expor funções nossas.
   - **Para que serve:** placar, contagem de abates, regras de modo e a futura killstreak (as recompensas são decisão do dono) em script, sem recompilar.
   - **Limites:**
     - só no servidor (o VScript do cliente não é compilado);
     - confirmar se o `vscript.dll` está no `bin/x64` do SDK Base.
   - **Primeiro passo:** `game/mod_trok/scripts/vscripts/mapspawn.nut` com um `printl` e um `OnGameEvent_player_death`; testar com `script_execute`.
2. **ALERTA para a Fase 1: o `.nav` do de_dust2 do CS:S provavelmente não carrega direito no HL2MP.**
   - **Mapas afetados:** de_dust, de_dust2 e de_inferno usam a versão 16, subversão 1 do formato, com 1 byte extra por área
     que o `CNavArea::Load` do HL2MP não lê. A leitura sai desalinhada, e esses mapas também não têm encounter spots.
   - **Os outros mapas:** de_nuke, de_aztec, de_train, cs_office e outros usam a v9 e devem carregar.
   - **Fonte:** spec `cs_source/nav.md` do Chimera, cruzada com `nav_file.cpp` do SDK. Não foi testado no jogo.
   - **O que fazer:** no de_dust2, rodar `nav_generate` e `nav_analyze` direto; ou testar primeiro num mapa v9; ou escrever um
     conversor que remova o byte.
3. **Chimera, além das specs do CA** (MIT; escrito por IA e sem revisão humana):
   - **`specs/cs_source/`:** 23 specs com números medidos num servidor real de CS:S:
     - **armas:** recoil, imprecisão (inclusive ao aterrissar), penetração;
     - **lentidão ao levar tiro:** 0,5 para pistola, SMG e acerto na perna; 0,65 para fuzil, sniper e escopeta;
     - **movimento por tick**, escadas do de_nuke, granadas e dano de queda.
   - **`specs/hl2dm/source_deltas.md`:** as cvars que o HL2DM muda em relação ao CS:S (gravidade 600 contra 800,
     accelerate 10 contra 5, stopspeed 100 contra 75...). É a nossa base.
   - **`games/combat_arms/data/`:** `bots.toml` (12 perfis de atributo por dificuldade) e `modes.toml`.
4. **crabby-navmeshes** (moonexpr, MIT, Rust):
   - **O que faz:** gera `.nav` direto do `.bsp`, sem abrir o jogo e com várias threads; também gera hiding spots e
     visibilidade, como o `nav_analyze`, e audita `.nav`.
   - **Como usar:** com `-b gmod`, deve servir ao HL2MP (a conferir).
   - **Cuidado:** é projeto novíssimo.
5. **Mapas (Fase 4):**
   - **Hammer++ do TF2 de 64 bits**, instalado no `bin\x64` do SDK Base (a versão "2013 MP" não é mais suportada).
   - **VRAD++** (ficool2).
   - **CompilePal** e o plugin **AutoNav**, que gera o nav ao compilar. São GPL, mas usados só como ferramenta, então não contaminam o jogo.
   - **Slammin'** não serve no ramo TF2.
6. **source-sdk-agent** (mixutin, 0BSD):
   - **O que é:** CLI e MCP com 40 ferramentas para Linux: compilar o jogo, editar VMF, VBSP/VVIS/VRAD, VTF/VMT/QC/VPK,
     servidor com RCON e teste de mapa.
   - **Para que serve:** comparar com o nosso MCP `trok` e compilar mapas na nuvem.
7. **Anti-cheat no servidor sem plugin:**
   - **`sv_maxunlag`:** vem com 1,0 s; 0,25 limita o backtrack.
   - **Já no SDK:** `sv_maxusrcmdprocessticks`, `sv_max_usercmd_move_magnitude`.
   - **Ideias do Little Anti-Cheat e do StAC** (tick_count, cmdnum, ângulos, aimsnap). São GPL: só a ideia, nunca o código.
8. **Distribuição:**
   - **Steam (a conferir):** mod grátis exige a papelada do Steamworks e a taxa do app (~US$ 100). O TF2 Classified saiu em
     jan/2026 depois de uma revisão lenta, e a Valve pediu troca de nome.
   - **Até lá:** itch.io + butler (MIT, patches delta, GitHub Action pronta) e ModDB.
9. **Relatório de crash nos PCs dos amigos:**
   - **`sentry-source-engine`** (MIT, binários x64).
   - **Ideias do `sdk13-gigalib`** (MIT); os bytepatches dele não valem no ramo 64.
   - **Quando:** só se a Fase 1 mostrar crashes difíceis de reproduzir.
10. **Servidor dedicado de 64 bits:** `speedvoltage/hl2mp-shutdown-crash-fix` (MIT, crash ao fechar) e `srcds64-manager` (MIT).

## Outros achados
- **Base de correções do HL2MP para o SDK 2025:** Alienmario/ssdk13-hl2dm-coop-base (licença do SDK).
- **Referência de números e lógica (CS 1.5 refeito, ISC):** FreeCS. ReGameDLL_CS tem origem cinzenta: só números e ideias.
- **VScript do TF2:** TweenVS (MIT), sw-zombie-fortress (modo zumbi em VScript), tf2_freezetag.
- **Leitor de `.nav`:** `mzdw89/navmesh-parser` (MIT).
- **O que não existe:** subtick em Source 1, Steam Audio pronto para Source e framework aberto de armas fora o NT;RE.
- **Remakes de mapas do CA:** só um Oil Rig (oficina do CS:GO/GMod), como referência de layout.

## Não usar
- **Código de jogo do CS:S** (nunca liberado; aparece vazado): vrad258/cstrike_sdk2013, StargateCoalition/sgcs,
  blackletum/cstrike-sdk-2013, SanyaSho/source-sdk-2013-hl1 ("leaked") e SourceSDK2013Ports/NextBot.
- **Descompilações:** cod4-decomp, cod3_REcomp. ReFoF é derivado de binário fechado.
- **GPL** (não misturar com código sob a licença do SDK; só ideias): BrainBread2, InsurgencyClassic, TF2Vintage, NavBot,
  plugins do SourceMod.
