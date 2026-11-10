# Roadmap: Surto jogável e divertido, com bots muito bons

Para o Claude Code no PC do dono. Escrito na nuvem em 10/11/2026. O dono quer, nesta rodada de trabalho local,
chegar ao **modo Surto bem jogável e divertido (se possível pronto)**, com **a IA dos bots muito boa**, juntando o
adiantamento da nuvem (ramo `claude/rearmed-adiantar`) com o que você já tem no PC (código, pesquisas, skills).

Você tem mais contexto e a palavra final técnica: se algo da nuvem for pior do que o que você já tem, fique com o seu
e registre o porquê. As **decisões do dono** em `CLAUDE.md` valem acima dos dois.

## Como trabalhar nesta rodada

- **Escopo:** Surto e bots. Corrida Armada, Hordas, economia e killstreak entram só se o Surto depender deles; o
  resto é não quebrar (compilar e abrir).
- **Diário:** mantenha `cs-source/PROGRESSO-SURTO.md` (o que fez, o que mediu, o que falta, próxima etapa) e faça
  commit a cada etapa. É o que salva o trabalho quando o contexto for resumido.
- **Teste de verdade:** cada etapa só termina depois de testar no jogo com o MCP `trok` (`partida_teste`, `estado`,
  `print_tela`, `erros`).
- **Números:** nada de número no código; tudo em cfg e scripts (regra 4 do `CLAUDE.md`).
- **Dono:** pergunte só o que for decisão dele (lista no fim). Para o resto, decida, faça e registre.

## Etapa 0: juntar os dois trabalhos

1. **Salvar o local.** Antes de puxar, faça commit de tudo o que existe no PC num ramo próprio (por exemplo
   `local/surto`).
2. **Ler a nuvem.**
   - `cs-source/MUDANCAS-NUVEM.md` (linhas 9 a 32, cada uma com "Testar");
   - `cs-source/PARA-O-LOCAL.md`;
   - `design/modo-surto.md`;
   - `design/ca-pesquisa/mutagenicos.md`;
   - `design/bots-pesquisa.md` e `design/bots-parkour.md`.
3. **Juntar.** `git fetch origin claude/rearmed-adiantar` e `git merge` no seu ramo.
   - **Arquivos do mod (`cs-source/src/...`):** resolva arquivo por arquivo. Nos bots, compare as duas versões e
     fique com a melhor de cada parte; a sua tende a ganhar na base, a da nuvem traz o Surto, o parkour e os tipos
     de zumbi.
   - **`cs-source/patches/sdk.patch`:** não resolva o conflito no texto do patch.
     1. Aplique o patch da nuvem num SDK limpo (`trok.py setup`).
     2. Traga por cima as suas mudanças em arquivos da Valve, sempre com `#ifdef TROK`.
     3. Rode `trok.py save-patch`.
4. **Compilar e conferir.** `trok.py gen`, compile Release x64 e confira que o build do GitHub Actions continua verde.

**Pronto quando:** um ramo só, compila, o jogo abre no menu.

## Etapa 1: abrir e rodar uma partida de Surto (Fase 1 do `CLAUDE.md`)

1. **Menu, carregamento e HUD.**
   - Siga as Fases 0 e 1 do `CLAUDE.md`.
   - Confira o HUD novo (`hud_trok_v2.cpp`, variante F do bloco da arma) e a escolha de time.
2. **Escala do CA** (corpo 100, agachado 80, olho 83/60).
   - Ela muda o tamanho do boneco (`trok_ca_corpo_escala 1.3888889`).
   - Os mapas do CS:S foram feitos para corpo de 72: dutos e passagens agachadas não passam. Use-os só para teste.
   - Gere o nav de novo em cada mapa (`nav_generate`, depois `nav_analyze`).
   - O `.nav` do de_dust2 do CS:S pode vir com formato errado (PARA-O-LOCAL, item 0).
3. **Primeira partida.** `trok_modo surto`, mapa de teste, 1 humano (você) e cerca de 9 bots. Corrija na ordem em
   que aparecer: travamento, aviso vermelho no console, bot parado.

**Pronto quando:** uma rodada inteira roda do preparo à vitória e começa a próxima, sem erro.

## Etapa 2: regras do Surto corretas (igual ao CA)

Confira cada uma no jogo:
- **Preparo e rodada:** 20 s de preparo, rodada de 2:40, de 1 a 3 hospedeiros.
- **Infecção:** só o golpe de garra infecta e o humano vira zumbi no lugar.
- **Outras mortes:** queda, explosão ou gás tiram o humano da rodada e ele fica assistindo em 1ª ou 3ª pessoa.
- **Gás:** só fere, não mata na hora.
- **Recuperação do zumbi:** regenera parado.
- **Alarme e abrigos:** o alarme toca e os abrigos abrem nos tempos certos, a grade fecha e o zumbi quebra.
- **Vitória:** a rodada nova limpa o mapa.

**Tipos de zumbi com Fúria** (`scripts/trok_zumbis.txt`, tecla N ou aba FÚRIA da loja):
- paga ao virar zumbi; sem Fúria suficiente, vira o normal;
- confira a velocidade da Striker, a regeneração e o golpe do Crusher, a explosão do Charger e as bolhas da Striker
  explosiva (matar com elas infecta);
- os olhos são vermelhos e mais saturados quanto mais forte o zumbi.

**Preencher os MEDIR do `cfg/surto.cfg`** com o CA instalado do dono e `design/decombat/`:
- velocidade do zumbi;
- alcance e tempo do golpe;
- regra de hospedeiros;
- tamanho do Crusher.

**Pronto quando:** cada regra foi vista funcionando e anotada no diário.

## Etapa 3: mapa do Surto (Fase 4 do `CLAUDE.md`)

O Surto só fica divertido num mapa feito para ele e na escala do CA. Os mapas do CS:S não servem para o produto.
1. **Mapa de caixas e parkour** com um abrigo.
   - Usar `info_trok_abrigo`, `func_trok_grade` e `trok_surto_logica` (OnAbrigos abre a porta).
   - Medidas em `design/medidas-mapas.md`; FGD em `game/mod_trok/trok.fgd`; compilar com `mapa_compilar` do MCP.
2. **O mapa do dono parecido com os do CA.** Trazer para o Source na escala certa. Ele deve ter:
   - de 2 a 4 abrigos;
   - rotas de cerco para os zumbis;
   - lugares altos para humanos que custem parkour.
3. **Nav.** Gere o nav, ligue os pulos à mão onde o gerador não liga e marque os lugares táticos.

**Pronto quando:** o mapa compila sem vazamento, bots andam em todo ele e há um abrigo funcionando.

## Etapa 4: bots muito bons no Surto (a parte maior)

Junte a sua base local de bots com o que a nuvem fez:
- percepção humana, estilos e tática de time;
- parkour com erro humano (`design/bots-parkour.md`);
- cérebro de zumbi e de humano do Surto (`design/modo-surto.md` > Bots).

Metas, nesta ordem:

**4.1 Zumbi bot que persegue de verdade**
- Segue o humano por caixas e pulos sem travar.
- Às vezes erra o pulo como gente e tenta de novo por outro lado.
- Chega por lados diferentes (cerco).
- Junta grupo antes de atacar abrigo e quebra a grade.

**4.2 Zumbi bot que usa o tipo**
- Escolhe e compra com Fúria de um jeito esperto (guarda para um tipo caro, compra o barato quando faz sentido).
- **Striker:** usa a velocidade para pegar quem está sozinho.
- **Crusher:** vai na frente e tampa a passagem.
- **Charger:** explode no grupo.
- **Striker explosiva:** solta bolhas no humano encurralado.

**4.3 Humano bot que sobrevive como gente**
- Segura lugar difícil.
- Recua atirando e recarrega protegido.
- Cobre o colega e não para sozinho no aberto.
- Corre para o abrigo no alarme e fecha a grade na hora certa.
- Mira e reação humanas por nível; erra, se assusta, desperdiça bala.

**4.4 Ferramentas para medir** (`design/bots-pesquisa.md` §6)
- Partidas só de bots, aceleradas (`host_timescale`), com registro por rodada:
  - quem venceu;
  - tempo até a primeira infecção;
  - humanos vivos no fim;
  - bots travados;
  - pulos errados.
- Use isso para ajustar os números do cfg sem precisar jogar cada vez.

**Pronto quando:**
- Em 30 rodadas só de bots: nenhum bot fica travado mais de 5 s e humanos vencem entre cerca de 30% e 50% (ajustar
  depois com o dono).
- Jogando junto, o dono não consegue dizer de cara quem é bot.

## Etapa 5: divertido (sensação e ritmo)

- **Tiro que segura e empurra o zumbi** (campos `empurrao` e `segurar` das armas): a escopeta tem que "parar" o zumbi.
- **Sons:**
  - provisórios do CA em `custom/ca_local/`: rosnado, golpe, alarme, grade, infecção;
  - avisos do anunciador.
- **Tela de zumbi:** a textura vermelha com veias (do CA, local) e os olhos vermelhos.
- **Rodada:**
  - tensão crescente: alarme, abrigos, último humano;
  - destaque do fim da rodada;
  - placar com K/D e infecções.
- **Fúria:** confira se dá para chegar aos zumbis caros em uma partida normal sem ser fácil (`cfg/surto.cfg`, preços
  em `trok_zumbis.txt`).

**Pronto quando:** o dono joga 3 partidas e diz que está divertido. Anote o que ele reclamar e corrija.

## Etapa 6: arte provisória e acabamento

- **Modelos do CA via extração local** (`custom/ca_local/`, fora do git), com os nomes do CA escondidos do jogador:
  - zumbis: normal, Striker e Crusher;
  - humanos das duas facções.
- **Ícones SVG do HUD:** armas, garras, infecção, cabeça e lados (`design/ui.md` > Ícones).
- **Animação nova do jogador** (`trok_anim_nova`): conferir andar, correr, pulo e agachar com o corpo maior.

## Etapa 7: jogar com amigos

- O zip do build com o servidor (`exec online`, `jogar-online.md`).
- Amigos usam a instalação do CA deles para os provisórios.
- Uma noite de teste com gente de verdade e bots completando a sala.

## O que é "Surto pronto" (versão 1)

- **Mapa:** um mapa próprio na escala do CA, com abrigos.
- **Regras:** as do CA conferidas no jogo, com os MEDIR preenchidos.
- **Tipos de zumbi:** todos funcionando, com Fúria equilibrada.
- **Bots:** bons nos dois lados, em todos os níveis.
- **Interface:** HUD, placar, tela de morte e espectador sem erro.
- **Sensação:** sons e tela de infectado.
- **Rede:** roda online com amigos sem erro de predição visível (`cl_showerror 1`).

## Decisões que são do dono (perguntar, não decidir)

- Taxa de vitória alvo dos humanos e duração da rodada, se for diferente do CA.
- Preços em Fúria depois do teste.
- O que fazer com a escala nos mapas do CS:S (só teste, ou largar de vez).
- Sons e modelos definitivos (arte própria).
- Qualquer coisa nova fora desta lista: sugerir e esperar (regra 1 do `CLAUDE.md`).

## Jogos e arquivos de referência para estudar

Para abrir os arquivos: **VPKEdit** ou **GCFScape** (VPK), **Crowbar** (modelos), **BSPSource** (mapas). O pacote
`vpk` do Python também lê VPK.

| Jogo (Steam) | Preço | O que tirar dele |
|---|---|---|
| Zombie Panic! Source | grátis | Humanos contra zumbis no Source. Ver mapas (rotas, abrigos, objetivos), ritmo, sons de zumbi e como os mapas desenham passagens para cada lado |
| No More Room in Hell | grátis | Zumbis no Source: horda, navegação, sons, mapas de sobrevivência |
| Zombie Master: Reborn | grátis | O código já está em `refs/`. O jogo mostra o código rodando: zumbis com NextBot, lanternas com sombra, PBR |
| Team Fortress 2 | grátis | Os bots do TF2 vieram no SDK (`src/game/server/tf/bot`). Ver rodando com `tf_bot_add`, e o MvM (ondas) |
| Left 4 Dead 2 | pago, sai barato em promoção | A melhor referência de IA de zumbi e de diretor de ritmo: scripts do diretor (`.nut`), atributos do nav, infectados especiais (Charger, Smoker, Tank). Só para estudar |
| Counter-Strike: Source | já tem | Bots do CS (comportamento de referência), mapas provisórios |
| NEOTOKYO e Double Action: Boogaloo | grátis | Já em `refs/` (UI, efeitos, movimento arcade). Só baixar para ver rodando |

Código para ler sem baixar jogo:
- o gamemode Zombie Survival do Garry's Mod (GitHub, JetBoom/zombiesurvival): classes de zumbi, rodada e
  infecção num jogo Source.
- `design/bots-pesquisa.md` §5 lista o resto.
