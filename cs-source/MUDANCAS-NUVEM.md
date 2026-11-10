# Mudanças do ramo `claude/rearmed-adiantar` (nuvem, a partir de 10/11/2026)

Ramo criado a partir de `claude/loving-knuth-xrrskr` para adiantar trabalho **sem encostar** na versão do PC
do dono (que tem mudanças locais ainda sem commit). Quem for juntar: cada item abaixo diz o que mudou e onde.
Sempre que possível, o trabalho novo fica em arquivos novos.

| # | O que | Arquivos | Tipo |
|---|---|---|---|
| 1 | **Troca de arma instantânea**, como no CA e no CoD: números, rodinha e última arma trocam na hora; a fileira de slots do HL2 (que pedia confirmação) não aparece mais. `hud_fastswitch` fica preso em 1, mesmo com `config.cfg` antigo. | `sdk/src/game/client/weapon_selection.cpp`, `sdk/src/game/client/hl2/hud_weaponselection.cpp` (via `patches/sdk.patch`, com `#ifdef TROK`) | patch da Valve |
| 2 | **Slots como no CA**: 1 primária (submetralhadora, fuzil, escopeta), 2 pistola, 3 faca/garras. A arma corpo a corpo provisória (pé de cabra do HL2) virou "Faca" no slot 3. | `game/mod_trok/scripts/weapon_trok_*.txt` (bucket), `game/mod_trok/scripts/weapon_crowbar.txt` (novo, cópia do script do SDK) | conteúdo |
| 3 | **Sem a lista de itens pegos** (ícones de arma e munição no canto, coisa do HL2). | `sdk/src/game/client/history_resource.cpp` (patch, `#ifdef TROK`) | patch da Valve |
| 4 | **Pegar arma do chão não troca sozinho** (`cl_autowepswitch` padrão 0), como no CoD. | `sdk/src/game/shared/hl2mp/hl2mp_gamerules.cpp` (patch, `#ifdef TROK`) | patch da Valve |
| 5 | **Sem carregar objetos com E** (`hl2mp_allow_pickup 0`). O E continua abrindo as grades do Surto. | `game/mod_trok/cfg/server.cfg` | cfg |
