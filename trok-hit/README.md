# Trok Hitmarker (conceito)

Indicador de dano para o SA-MP 0.3.7 R1, no estilo da casa: sinal de acerto na mira, local do acerto marcado no corpo, números de dano, o informer com nome e combo, seta de onde veio o tiro levado, borda de dano recebido e sons trocáveis. Cada parte liga e desliga sozinha, e as que fazem sentido juntas podem ficar juntas.

**Estado:** conceito para aprovação. As imagens abaixo saem do código de desenho de verdade (`src/hud.cpp` e `src/menu.cpp`, com o kit da casa), rasterizado no Linux por `test/concept.sh`. Ainda não existe o `.asi`.

| | |
|---|---|
| ![](docs/01-cena-trok.png) | ![](docs/02-cena-damage-informer.png) |
| Estilo Trok: sinal na mira, marcas no corpo, números saindo do alvo, seta de onde veio o tiro | Estilo Damage Informer: nick parado e número subindo, simétrico ao centro, com o dano real e o combo somando |

## O que o projeto antigo faz (e o que muda)

- **`damage-indicator-samp.lua`**: lê o pacote de dano que o seu SA-MP manda ao servidor (RPC 115) e mostra o número dele, `46.2`. Esse é o valor que o GTA calcula para a arma, não o que o servidor tirou: em servidor com dano de arma configurado, colete ou headshot que mata de uma vez, o número não bate. O combo `(x3)` conta tiros, não soma dano, e cada informer guarda um tiro só (com a M4, o tiro novo apaga o anterior).
- **`hitmarker.lua`**: o X (`hitmarker.txd`) no osso do corpo onde o tiro pegou. A duração é em quadros (28), então muda com o FPS: uns 0,47 s a 60 FPS e 0,2 s a 144 FPS.
- **`Take Damage Notification.lua`**: escreve cada acerto no chat (jogador, arma, parte do corpo e o mesmo `46.2`).

**No Trok Hitmarker:** o acerto continua vindo do pacote do SA-MP (é na hora e diz quem, arma e parte do corpo), mas o número é a **vida + colete que o alvo perdeu de verdade**, lidos do sync dele logo depois do tiro. Assim vale a regra de cada servidor sozinha: dano de arma configurado, colete, HS sem capacete que mata em 1. Se o servidor esconde a vida (o alvo não mostra a perda), aparece o número do GTA com um `~` na frente. O combo do informer soma o dano real no mesmo alvo. Tudo é medido em tempo, não em quadros.

## Mira, corpo e centro da tela

Só o sinal de acerto acompanha a mira. O resto fica preso à tela: a seta de onde veio o tiro gira em volta do centro, o informer fica dos dois lados do centro (dado à direita, levado à esquerda) e os números "na tela" sobem do lado do centro.

- **Mira:** a do jogo ou a **do Trok** (a Mira Centralizada refeita: a imagem `mira.png`). Um tamanho só vale para a mira e para tudo que aparece nela. A do Trok pode ficar **sempre visível** (também sem mirar, a pé; some no carro e com sniper, bazuca e lança-mísseis, como no script original), e o GTA não desenha a mira dele. A do jogo pode ficar **estática** (não abre enquanto você atira; muda só o desenho, o espalhamento dos tiros continua o do jogo).
- **Uma imagem para cada coisa, sem estilos no código:** `mira.png`, `acerto.png`, `colete.png`, `cabeca.png`, `abate.png` e `corpo.png` (o X do local do acerto), na pasta `Trok Hitmarker\mira`. O visual é o da imagem: quer outro, edite o PNG (apagou, volta o padrão que vem no `.asi`). A imagem inteira ocupa o tamanho da mira (40 px numa tela de 1080, vezes o Tamanho; o corpo, 48 px), então qualquer resolução serve. O mod só faz o pulo (35% maior, voltando em 0,1 s), o tempo (acerto 0,35 s, cabeça 0,55 s, abate 1,2 s, em qualquer FPS) e o sumir. O efeito fica no lugar da mira enquanto aparece, por isso as imagens de efeito já trazem o ponto (`docs/04-hitmarker.png`; as padrão saem de `res/make_images.py`).
- **O que aparece:** acerto, colete, cabeça e abate ligam e desligam um por um. Colete, cabeça ou abate desligados mostram a imagem do acerto naquele tiro.
- **Onde:** desenhado onde a mira está naquele quadro. O mod lê onde o próprio GTA desenhou a mira, então mod que centraliza ou sobe a mira não precisa de ajuste nem de opção no menu (`docs/08-mira-seguida.png`).
- **Local do acerto:** o `corpo.png` fica na parte do corpo que o tiro pegou (cabeça, tronco, braço, perna) por 1 a 5 s, acompanhando o alvo quando ele anda (`docs/06-local-do-acerto.png`).

## Dano recebido

- **Seta de onde veio o tiro:** sempre vermelha, em volta do centro da tela, apontando para quem atirou (em cima = pela frente, embaixo = por trás). Gira junto quando você vira a câmera.
- **Borda de dano recebido:** as bordas da tela piscam na cor do que foi atingido: vida (vermelho), colete (preto) ou cabeça (azul). As três cores mudam no menu (`docs/15-borda.png`).

## Sons

Pasta `Trok Hitmarker\sons`, ao lado do `.asi`, com `.mp3` ou `.wav` destes nomes: `acerto`, `cabeca`, `dano_levado`, `abate`, `abate_cabeca` e `morte`. Trocou o arquivo, vale na hora. Os sons que vão junto vão ser feitos para o mod (nada copiado de outro jogo).

## Imagens

`docs/04-hitmarker.png` (estilos e a animação), `05-numeros.png`, `06-local-do-acerto.png`, `03-informer-lista.png`, `07-recebido.png`, `15-borda.png`, `08-mira-seguida.png`, `09-mover.png` e `10`–`14` (menu).
