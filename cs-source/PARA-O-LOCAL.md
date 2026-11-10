# O que a nuvem não consegue fazer (fica para o Claude Code no PC do dono)

A sessão na nuvem roda em Linux, sem Steam, sem jogo, sem tela e sem Visual Studio. Tudo abaixo depende do PC.
Ordem sugerida: de cima para baixo.

## Rodar e testar (nada disto foi testado ainda)
0. **Alerta do nav:** o `.nav` do de_dust2 (e do de_dust e do de_inferno) do CS:S tem um formato com um byte a mais por
   área que o HL2MP provavelmente lê errado, e o JOGAR do menu usa o de_dust2. Se os bots travarem ou o console acusar o nav,
   rode `nav_generate` e `nav_analyze` nesse mapa ou teste antes num mapa v9 (de_nuke, de_aztec, cs_office).
   Ver `design/recursos-uteis.md`, item 2.
1. **Compilar e abrir o jogo** (Fases 0 e 1 do `CLAUDE.md`). O build do GitHub Actions compila, mas o jogo nunca
   abriu numa partida.
2. **MCP `trok`:** confirmar se o SDK de 2025 aceita `-netconport`; se não aceitar, usar RCON ou `-condebug`.
3. **Ramo `claude/rearmed-adiantar`** (`cs-source/MUDANCAS-NUVEM.md`), testar cada item:
   - troca de arma instantânea (números, rodinha, Q) e a lista do HL2 que não aparece mais;
   - slots 1 primária, 2 pistola e 3 faca;
   - sem a lista de itens pegos; pegar arma do chão não troca sozinho; E não carrega caixa;
   - **CONFIGURAÇÕES:**
     - todas as abas;
     - mudar uma tecla, inclusive Shift e botão do mouse;
     - APLICAR resolução;
     - o `config.cfg` é gravado ao sair;
   - **CRIAR PARTIDA:**
     - o texto (nome e senha) é digitado direito, com acento;
     - o `cfg/partida_menu.cfg` é criado e executado depois do cfg do modo (conferir `mp_fraglimit` no console);
     - um amigo consegue entrar;
   - **`listenserver.cfg`:** no console, ao carregar o mapa, aparece "Executing listen server config file", e
     `trok_pulo_altura` vem do `movimento.cfg`.
4. **Calibrar a mira pelo cano de cada arma** (todas estão com o mesmo chute `-3 1 -1.5`):
   - no jogo, com a arma na mão: `trok_mira_calibrar 1` (arma presa na mira e linhas vermelhas no centro);
   - ajustar com `trok_mira_mexer cima 0.05`, `direita`, `frente`, `pitch`, `yaw` e `roll` até a alça e a massa ficarem
     sobre o cruzamento das linhas, com o cano reto (dá para fazer sozinho com o MCP `trok`: `comando` + `print_tela`);
   - `trok_mira_imprimir` e colar as linhas no `scripts/weapon_trok_<arma>.txt`;
   - depois, testar segurar e alternar, o zoom com FOV 75 e 90, e se o clarão aparece mirando (se não aparecer,
     o modelo usa outro evento: usar `mira_animacao_tiro 1` nessa arma ou avisar a nuvem).
5. **Jogar com amigos:** testar o `exec online` e um amigo entrando pelo IP ou pela VPN.

## Medir e preencher (Fase 3)
- Converter para o Source os números de `design/decombat/specs/` (movimento, cone e recuo, modos) e preencher os
  valores MEDIR dos `cfg` e dos `scripts/weapon_trok_*.txt`. O LithTech usa Y para cima e outra escala de unidades;
  ver a spec de movimento.
- Conferir com `design/ca-pesquisa/numeros.md` e `armas-wiki.tsv`.
- Vida dos zumbis do Surto: já está igual ao CA (300/200, `surto.cfg`); conferir a regeneração.
- O que nenhuma fonte tem: medir no CA instalado (cadência real, recarga, queda de dano, faca, raio das granadas).

## Conteúdo
- **Assets do CA (provisório pessoal):** a versão atual que o dono tem traz os `.rez` com cabeçalho alterado
  (protegidos), que o extrator pula. Se ele tiver um cliente com `.rez` no formato padrão (por exemplo o EU de 2009),
  rodar `python cs-source/tools/ca_extrator.py listar <pasta>`. A saída vai só para `custom/ca_local/`.
- Músicas e sons soltos do CA (mp3/wav em `Game/BGM`): podem virar provisório em `custom/ca_local/`.
- Modelos e animações: Blender + Blender Source Tools + o Blender MCP do dono, e o `modelo_compilar` do MCP `trok`
  (`design/catalogo-assets.md`).
- **Mapa de teste do Surto no Hammer** (Fase 4).

## Decisões que ainda são do dono
- Já decididas (10/11/2026, ver `CLAUDE.md`): lanterna fica, sem tecla de largar arma, nome Rearmed, killstreak igual
  ao CA, vida do zumbi do Surto igual ao CA (300/200 no `surto.cfg`).
- Ainda abertas: nome da corporação; recompensas próprias de série (depois).

**Próxima rodada local: `ROADMAP-SURTO.md`** (Surto jogável e bots muito bons).

## Rodada grande da nuvem (10/11/2026)
- Testar cada item da tabela de `MUDANCAS-NUVEM.md` (itens 9 a 32): cada linha tem o "Testar" dela. Ordem sugerida:
  escala do corpo e nav novo (14) → armas e inventário (16–18) → HUD novo (28) → interface (22–25) → Surto com tipos de
  zumbi e Fúria (15, 31) → explosivos (26–27) → série e economia (19–20, 31) → Corrida Armada (21) → Hordas (32).
- **Gerar o `.nav` de novo** em todo mapa (o corpo agora é o do CA, 100u).
- **Ícones SVG** (armas, explosivos, cabeça, infecção, lados): lista e receita em `design/ui.md` > Ícones.
- **Mapa de teste do Hordas:** `python cs-source/tools/mapas/gerar_hordas_teste.py`, compilar com o MCP `trok`, abrir tudo e gerar o nav (ver `design/modo-hordas.md`, seção 17).
- Decisões que esperam o dono: `design/modo-hordas.md` seção 18; `design/economia.md` (pendências).
