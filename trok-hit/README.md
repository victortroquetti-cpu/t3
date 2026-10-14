# Trok Hitmarker (conceito)

Indicador de dano para o SA-MP 0.3.7 R1, no estilo da casa: hitmarker, números de dano, o informer com nome e combo, combo no alvo com vida e colete reais, de onde veio o tiro, aviso de abate, resumo da morte e sons trocáveis. Cada parte liga e desliga sozinha, e as que fazem sentido juntas podem ficar juntas.

**Estado:** conceito para aprovação. As imagens abaixo saem do código de desenho de verdade (`src/hud.cpp` e `src/menu.cpp`, com o kit da casa), rasterizado no Linux por `test/concept.sh`. Ainda não existe o `.asi`.

| | |
|---|---|
| ![](docs/01-cena-trok.png) | ![](docs/02-cena-damage-informer.png) |
| Estilo Trok: hitmarker na mira, números saindo do alvo, combo no alvo, de onde veio o tiro | Estilo Damage Informer: o layout antigo (60% e 30% da tela), com o dano real e o combo somando |

## O que o projeto antigo faz (e o que muda)

- **`damage-indicator-samp.lua`**: lê o pacote de dano que o seu SA-MP manda ao servidor (RPC 115) e mostra o número dele, `46.2`. Esse é o valor que o GTA calcula para a arma, não o que o servidor tirou: em servidor com dano de arma configurado, colete ou headshot que mata de uma vez, o número não bate. O combo `(x3)` conta tiros, não soma dano, e cada informer guarda um tiro só (com a M4, o tiro novo apaga o anterior).
- **`hitmarker.lua`**: o X (`hitmarker.txd`) no osso do corpo onde o tiro pegou. A duração é em quadros (28), então muda com o FPS: uns 0,47 s a 60 FPS e 0,2 s a 144 FPS.
- **`Take Damage Notification.lua`**: escreve cada acerto no chat (jogador, arma, parte do corpo e o mesmo `46.2`).

**No Trok Hitmarker:** o acerto continua vindo do pacote do SA-MP (é na hora e diz quem, arma e parte do corpo), mas o número é a **vida + colete que o alvo perdeu de verdade**, lidos do sync dele logo depois do tiro. Assim vale a regra de cada servidor sozinha: dano de arma configurado, colete, HS sem capacete que mata em 1. Se o servidor esconde a vida (o alvo não mostra a perda), aparece o número do GTA com um `~` na frente. O combo soma o dano real no mesmo alvo e mostra o total que ele tinha quando o combo começou (200 com colete, 100 só com vida). Tudo é medido em tempo, não em quadros.

## Sons

Pasta `Trok Hitmarker\sons`, ao lado do `.asi`, com `.mp3` ou `.wav` destes nomes: `acerto`, `cabeca`, `dano_levado`, `abate`, `abate_cabeca` e `morte`. Trocou o arquivo, vale na hora. Os sons que vão junto vão ser feitos para o mod (nada copiado de outro jogo).

## Imagens

`docs/04-hitmarker.png` (estilos e a animação), `05-numeros.png`, `06-combo.png` (o exemplo da doze + M4 + Deagle), `03-informer-lista.png`, `07-recebido.png`, `08-abate-e-morte.png`, `09-mover.png` e `10`–`13` (menu).
