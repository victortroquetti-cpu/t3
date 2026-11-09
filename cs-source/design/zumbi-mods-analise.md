# Mods de zumbi da lista: o que baixar e o que aproveitar

> **Atenção:** a referência de jogabilidade do Trok é a **quarentena do Combat Arms** e
> depois os **zumbis do Call of Duty** (`zumbi-referencias.md`). O dono não gosta da
> jogabilidade dos modos zumbi do CS (coice, zumbi esponja de bala). Esta análise serve só
> para infraestrutura (como organizar classes, rodadas, configs) e para saber o que **evitar**.

Análise (09/11/2026) da lista de mods e jogos de zumbi que o dono passou. O Trok vai ser
diferente; daqui só sai **referência de mecânica**: regras, números de equilíbrio e ideias.

## Regra para tudo que vier desses mods

| Tipo | Pode? | Por quê |
|---|---|---|
| Ler o código para entender regras e números | **sim** | Estudar não copia nada. |
| Copiar código para o Trok | **não** | Os abertos são GPL (v3 no Zombie:Reloaded e no Zombie Escape AMXX). GPL não combina com a licença do Source SDK. O jeito certo é reescrever do zero a partir da regra. |
| Usar números de equilíbrio (vida, velocidade, coice) como ponto de partida | **sim** | É ideia e dado, não código. Ajustamos para o nosso jogo. |
| Modelos, sons, sprites, mapas que vêm nos pacotes | **não** | Quase sempre são de outros jogos (muitos tirados do CS Online da Nexon) ou de autores sem licença declarada. |
| Jogos comerciais (CS Online, CS Online 2, CS Nexon) | **só vídeo** | Não há o que baixar legalmente; assistir gameplay para ideias. |
| Pacotes "CS Xtreme" e remakes | **não baixar** | Pacotes com conteúdo da Nexon sem licença, distribuídos por sites de repack (risco de vírus também). Ver vídeo basta. |

## A lista, item por item

| Item | Plataforma | Dá para baixar? | Serve para |
|---|---|---|---|
| Half-Life ZNPC | HL1 | não vale | só história |
| CS 1.5 Biohazard | CS 1.5 (AMX) | antigo, difícil de achar | ideia original de infecção |
| CS 1.6 ZM Plague (até v8, Advance) | CS 1.6 / AMX Mod X | **sim**: código `.sma` nos tópicos do AlliedModders (4.3 e 5.0) | regras de infecção: modos de rodada (infecção, um contra todos, sobrevivente), classes, itens extras, conversão por golpe |
| ZM Base Builder | CS 1.6 / AMX Mod X | sim (AlliedModders) | regras da fase de construção, se um dia fizermos esse modo |
| CS 1.6 ZM Escape | CS 1.6 / AMX Mod X | **sim**: `refs/zombie-escape-amxx` (GPLv3) | estrutura do modo fuga |
| CS Online NST / NST Final | CS 1.6 com conteúdo da Nexon | **não** | vídeo: classes com habilidade, suprimentos, herói |
| CS Xtreme v4–v7 | repack do CS 1.6 | **não** | vídeo |
| CS:S ZM Riot | CS:S / SourceMod | **sim**: `refs/zombie-riot` (fork do 1.9.1b; sem licença declarada, traz modelos e sons) | **muito útil**: ondas de zumbis feitos de **bots**, por "dias", no próprio CS:S. É o survival com bots que queremos |
| CSS ZM Extermination | CS:S | não achei fonte confiável | vídeo |
| CSS ZM Plague / ZM Escape | CS:S / SourceMod | **sim**: o principal é o **Zombie:Reloaded**, em `refs/zombiereloaded` (GPLv3) | **o melhor de todos para nós**: infecção completa feita para o CS:S, bem documentada, com configs de classes, coice por arma e por parte do corpo, áreas de mapa com regra própria |
| CS:GO ZM Survival | CS:GO / SourceMod | servidores da comunidade | vídeo |
| CS Online 2: Zombie Z | Source (Nexon) | **não** | vídeo; o parente aberto é o Zombie Master: Reborn (`refs/zmr-game`) |
| CS Nexon ZM Survival / Escape | jogo comercial | **não** | vídeo |
| CS 1.6 Remake UE5 / CS Xtreme Remake | projetos de fã com conteúdo de terceiros | **não** | vídeo |
| CS2 ZM Survival / Escape / Plague | CS2 (plugins da comunidade) | servidores | vídeo; a mecânica é a mesma da era CS:S |

## O que o Zombie:Reloaded ensina (resumo em palavras nossas)

- **Classe = um pacote de atributos.** Os mesmos campos servem para humano e zumbi: vida,
  velocidade, coice recebido, altura e distância de pulo, regeneração de vida (quanto e de
  quanto em quanto tempo), vida ganha a cada infecção, bônus por abate, transparência (com
  regras quando leva dano), visão noturna, FOV, sem dano de queda, tempo queimando com
  granada incendiária, imunidade, flags de admin/VIP, modelo e grupo. O arquivo de classes
  traz exemplos: zumbi clássico, rápido, pesado, mutante, "zumbi mãe" (o primeiro infectado)
  e humanos normal, rápido e VIP. O zumbi clássico tem **vida na casa dos milhares** e ganha
  vida ao infectar: o humano não mata zumbi, empurra e segura.
- **Coice é o coração do modo.** O empurrão sai na direção da mira de quem atira (ou do centro
  da explosão, na granada) e multiplica: coice da classe do zumbi × multiplicador da arma ×
  multiplicador da parte do corpo atingida × dano. Tem limite de força e de velocidade por
  quadro, para não arremessar longe demais. Por isso escopeta perto e tiro na cabeça seguram a
  horda num corredor.
- **Primeiro infectado ("zumbi mãe").** Sorteado depois de uma contagem; a quantidade segue
  uma proporção dos jogadores. Há modos de infecção e de imunidade configuráveis.
- **Coisas de servidor que viram regra de jogo:** anti-camping por área do mapa (volumes que
  machucam quem fica parado), áreas que trocam atributos de classe, renascer como zumbi,
  teleporte de volta ao spawn, proteção ao nascer, anti-travamento entre jogadores,
  granada incendiária que deixa o zumbi lento.

## O que o Zombie Riot ensina

- Survival no CS:S usando **bots do próprio jogo** como zumbis: modelo trocado, vida e
  velocidade alteradas, ondas organizadas em **"dias"** com quantidade e tipo de zumbi por dia.
- Prova que o formato funciona com poucos jogadores. No Trok, os zumbis seriam NPCs NextBot
  (não ocupam vaga) ou bots com o cérebro de zumbi.

## Para o dono baixar e olhar (opcional)

1. **Zombie:Reloaded:** já está em `refs/zombiereloaded`. Leia `common/addons/sourcemod/configs/zr/`
   (classes, armas, partes do corpo) e o manual em `src/docs`. É o que mais rende.
2. **Zombie Plague 4.3 ou 5.0** (AlliedModders): o `.sma` principal tem todos os modos de rodada
   comentados.
3. **Zombie Riot:** já está em `refs/zombie-riot`. Veja `cfg/` (dias e zumbis).
4. **Vídeos** do CS Online NST, do CS Online 2 Zombie Z e do CS Nexon ZM Survival, para as ideias
   de classes com habilidade, suprimentos e herói.

Nada desses pacotes vai para dentro do mod: nem código, nem modelo, nem som, nem mapa.
