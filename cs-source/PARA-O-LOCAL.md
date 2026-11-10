# O que a nuvem não consegue fazer (fica para o Claude Code no PC do dono)

A sessão na nuvem roda em Linux, sem Steam, sem jogo, sem tela e sem Visual Studio. Tudo abaixo depende do PC.
Ordem sugerida: de cima para baixo.

## Rodar e testar (nada disto foi testado ainda)
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
4. **Jogar com amigos:** testar o `exec online` e um amigo entrando pelo IP ou pela VPN.

## Medir e preencher (Fase 3)
- Converter para o Source os números de `design/decombat/specs/` (movimento, cone e recuo, modos) e preencher os
  valores MEDIR dos `cfg` e dos `scripts/weapon_trok_*.txt`. O LithTech usa Y para cima e outra escala de unidades;
  ver a spec de movimento.
- Conferir com `design/ca-pesquisa/numeros.md` e `armas-wiki.tsv`.
- **Decisão do dono pendente:** vida e regeneração dos zumbis do Surto. O CA usa de 135 a 450 de vida; o nosso
  `surto.cfg` está com 1500 e 2500.
- O que nenhuma fonte tem: medir no CA instalado (cadência real, recarga, queda de dano, faca, raio das granadas).

## Conteúdo
- **Assets do CA (provisório pessoal):** a versão atual que o dono tem traz os `.rez` com cabeçalho alterado, ou seja,
  protegidos, e não são abertos. Se ele tiver um cliente com `.rez` no formato padrão (por exemplo o EU de 2009),
  rodar `python cs-source/tools/ca_extrator.py listar <pasta>`. A saída vai só para `custom/ca_local/`.
- Músicas e sons soltos do CA (mp3/wav em `Game/BGM`): podem virar provisório em `custom/ca_local/`.
- Modelos e animações: Blender + Blender Source Tools + o Blender MCP do dono, e o `modelo_compilar` do MCP `trok`
  (`design/catalogo-assets.md`).
- **Mapa de teste do Surto no Hammer** (Fase 4).

## Decisões que ainda são do dono
- Lanterna (F) e largar arma (G): ficam ou saem?
- Recompensas e números da killstreak.
- Nome completo do jogo e da corporação.
