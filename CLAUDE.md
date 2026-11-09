# Rearmed (nome provisório): conversão total de CS:Source no Source SDK 2013

Este arquivo é o ponto de partida para o Claude Code no PC do dono. Leia inteiro antes de mexer.
Responda ao dono em **português do Brasil**, simples e direto.

- O jogo fica em `cs-source/` (código, conteúdo, ferramentas e documentos).
- `trok-ui/` é **outro projeto**, de mods de menu, que não tem relação com o jogo. Não mexa nele
  a menos que o dono peça.
- Os nomes internos do código são "trok". O nome que o jogador vê é "Rearmed".

## O que é

Sucessor de alma do Combat Arms (com referências secundárias de CoD), feito no **Source SDK 2013
Multiplayer** (código do HL2MP; o do CS:S nunca foi aberto). O conteúdo do CS:S e do HL2 fica
montado como provisório.

- **Modos:** Equipe (mata-mata em equipe), Exército solo (todos contra todos) e Surto (infecção,
  sucessor da quarentena).
- **Bots próprios** (NextBot), com percepção humana, estilos de jogo e tática de time.
- **Movimento:** fluido e arcade.
- **Interface própria:** menu, HUD, placar e tela de carregamento.

Quase tudo foi escrito na nuvem e **compila** no Windows (GitHub Actions), mas **nunca rodou numa
partida**. O primeiro trabalho aqui é fazer funcionar e testar.

## Regras do dono (não quebrar)

1. **Fazer só o que ele pedir.** Ele já reclamou quando algo foi implementado sem pedido. Para
   ideias novas, sugira e espere.
2. **Nada de assets do Combat Arms, CoD, CS Online ou de outros jogos:** nem modelos, sons,
   texturas, mapas ou nomes. Deles entram **só números medidos e sensações**.
   - O CS:S e o HL2 só **montados** (`gameinfo.txt`), nunca copiados para o pacote.
   - Das TCs estudadas (`cs-source/refs/`), só o **código** com licença compatível (NEOTOKYO
     Rebuild, Double Action, Momentum e Fortress Forever usam a Source 1 SDK License; dar crédito
     em `game/mod_trok/CREDITOS.txt`); nunca a arte deles.
   - Código vazado (CS:GO/TF2 2020, kisak-strike e parecidos): **proibido**.
3. **Mudanças em arquivo da Valve** sempre com `#ifdef TROK`. Depois de mexer: `python cs-source/trok.py save-patch`.
4. **Números em cfg ou script, não no código.** Ajuste de jogabilidade não deve precisar recompilar.
5. **Decisões já tomadas:**
   - o modo de infecção se chama **Surto**;
   - **sem deslize** de CoD (`trok_deslizar 0`);
   - visual **sóbrio de guerra / sobrevivência tática**, jogabilidade arcade. Ver `cs-source/design/lore-e-arte.md`.

## Ler primeiro (nesta ordem)

1. `cs-source/HANDOFF.md`: tudo o que foi feito, o que não foi testado, riscos e backlog.
2. `cs-source/design/modos.md` e `cs-source/design/modo-surto.md`: os modos.
3. `cs-source/design/sistemas-jogabilidade.md`: onde está cada número.
4. `cs-source/design/ui.md`: menu, HUD, placar e carregamento (concepts no link de dentro).
5. `cs-source/design/bots.md`: os bots.
6. A skill `.claude/skills/source-mod/`: referência do SDK, do NextBot, de rede e predição, de conteúdo.

## Plano de trabalho

### Fase 0: ambiente (uma vez)
- Steam: **Source SDK Base 2013 Multiplayer** (Ferramentas), **Counter-Strike: Source**.
- Visual Studio 2022 com C++ e Python 3.
- `python cs-source/trok.py setup` (baixa o SDK, aplica o patch, liga o código do mod),
  depois `cs-source\sdk\src\createtrok.bat`, abrir `trok.sln`, compilar **Release | win64**
  (é a única plataforma).
- `python cs-source/trok.py check` faz checagem rápida de sintaxe (g++) e serve no Linux ou no WSL.
- Para só jogar: o zip do último build em GitHub Actions > "Trok: build Windows" > Artifacts.

### Fase 1: fazer rodar (prioridade máxima)
Abrir com `-dev -console -novid` e consertar na ordem em que aparecer:
1. **Abre sem travar.** O menu próprio aparece e as fontes Barlow carregam (`TrokScheme.res`).
   Se a fonte cair na padrão, conferir o nome da família no Windows.
2. **Menu:**
   - clique e passar o mouse funcionam;
   - ESC dentro da partida abre a pausa;
   - CONFIGURAÇÕES abre (ainda é a janela do motor);
   - JOGAR inicia uma partida com bots em `de_dust2`.
3. **Carregamento:** aparece a tela de carregamento própria, e não a do Source.
4. **Em partida:**
   - HUD (vida, munição, radar, lista de abates, mira, direção do dano);
   - TAB (placar com K/D);
   - tela de morte com contagem e renascer sozinho;
   - proteção ao nascer.
5. **Bots andam.** Se não andarem, o `.nav` do CS:S não carregou: `nav_generate`, depois `nav_analyze`.
6. **Console:** registrar cada aviso vermelho ou amarelo do mod (som que falta, modelo, evento).

### Fase 2: testar cada sistema (checklists no HANDOFF)
- **Movimento:**
  - pulo alto;
  - escalar (segurar pular contra a borda);
  - virar no ar logo depois do pulo;
  - pulo atrasado na borda e pulo antecipado;
  - teto de velocidade;
  - escadas dos mapas do CS:S (de_nuke, de_aztec).
  - Conferir erro de predição com `cl_showerror 1`.
- **Armas:**
  - mira pelo cano: alinhar cada arma com `trok_mira_ajuste` e `trok_mira_imprimir`;
  - recuo que fica, dispersão, queda de dano;
  - correr abaixa a arma; C anda sem som.
- **Bots:** níveis, estilos, mira humana, ritmo de tiro, checar cantos, supressão, mira pelo cano,
  memória de onde travaram, parkour (`nb_debug BEHAVIOR`, `trok_bot_debug_percepcao 1`).
- **Modos:**
  - Equipe e Solo: meta, tempo, renascer, proteção, nascer longe.
  - Surto: sorteio, virar zumbi no lugar, contagem, alarme e abrigos, vencedor, rodada nova,
    empurrão por arma, zumbis bots cercando, humanos recuando.

### Fase 3: números do Combat Arms
O dono tem o CA instalado e skills com os estudos. Medir e preencher todo valor marcado
**MEDIR** em `cs-source/game/mod_trok/cfg/*.cfg` e nos `scripts/weapon_trok_*.txt`:
velocidades, pulo, dano, cadência, recuo, tempos de modo, vida e regeneração do infectado.

### Fase 4: mapa de teste do Surto
Montar no Hammer, com `game/mod_trok/trok.fgd` e as medidas de `design/medidas-mapas.md`, um mapa
de caixas e parkour com um abrigo:
- `info_trok_abrigo`;
- `func_trok_grade`;
- `trok_surto_logica` com OnAbrigos abrindo a porta do abrigo.

Depois, nav mesh. O dono tem um mapa parecido com os do CA: ajudar a trazer para o Source.

### Fase 5: conteúdo próprio
Seguir `cs-source/design/conteudo-proprio.md` (inventário do que ainda é do CS:S e do HL2, e onde
trocar) com a direção de `cs-source/design/lore-e-arte.md`. O último passo é tirar o CS:S do
`gameinfo.txt`.

### Depois
O backlog está no fim do `HANDOFF.md`. Ideias já oferecidas ao dono e **ainda não pedidas** (não
fazer sem ele pedir):
- tela de resultado do fim da partida;
- criar partida pelo menu;
- progressão / XP;
- outros modos do CA;
- faca e granadas;
- arsenal;
- luneta;
- mapa de teste gerado por código;
- opções próprias.

## Comandos úteis

| Comando | Para quê |
|---|---|
| `python cs-source/trok.py setup` / `gen` / `check` / `save-patch` / `status` / `refs` | preparar, gerar projetos, checar, salvar patch, estado, baixar as referências |
| no jogo: `trok_modo equipe\|solo\|surto` e `map X` | trocar de modo (vale no próximo mapa) |
| `exec bots`, `hl2mp_bot_add`, `trok_bot_estilos_info` | bots |
| `nb_select`, `nb_debug BEHAVIOR` | depurar bots |

## Git

- O trabalho da nuvem está no ramo `claude/loving-knuth-xrrskr`.
- Cada push compila no Windows (Actions). Commit só de documento: pôr `[skip ci]`.
- Antes de push: `trok.py check` e `save-patch` se mexeu em arquivo da Valve.
