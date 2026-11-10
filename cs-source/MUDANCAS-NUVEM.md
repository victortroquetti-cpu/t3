# Mudanças do ramo `claude/rearmed-adiantar` (nuvem, a partir de 10/11/2026)

Ramo criado a partir de `claude/loving-knuth-xrrskr` para adiantar trabalho **sem encostar** na versão do PC
do dono (que tem mudanças locais ainda sem commit). Quem for juntar: cada item abaixo diz o que mudou e onde.
Sempre que possível, o trabalho novo fica em arquivos novos.

| # | O que | Arquivos | Tipo |
|---|---|---|---|
| 1 | **Troca de arma instantânea**, como no CA e no CoD: números, rodinha e última arma trocam na hora; a fileira de slots do HL2 (que pedia confirmação) não aparece mais. `hud_fastswitch` fica preso em 1, mesmo com `config.cfg` antigo. | `sdk/src/game/client/weapon_selection.cpp`, `sdk/src/game/client/hl2/hud_weaponselection.cpp` (via `patches/sdk.patch`, com `#ifdef TROK`) | patch da Valve |
