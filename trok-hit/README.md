# Trok Hitmarker

Indicador de dano para o SA-MP 0.3.7 R1, no estilo da casa: sinal de acerto na mira, local do acerto marcado no corpo, números de dano real, o informer com nome e combo, seta de onde veio o tiro levado, borda de dano recebido e sons trocáveis. Cada parte liga e desliga sozinha, e as que fazem sentido juntas podem ficar juntas. Menu: `/trokhit`.

**Estado:** `.asi` compilado e testado fora do jogo (Wine, com um GTA e um samp.dll de mentira: `test/smoke/run.sh`, 6 cenários). Falta o teste no jogo de verdade.

| | |
|---|---|
| ![](docs/01-cena-trok.png) | ![](docs/02-cena-damage-informer.png) |
| Estilo Trok: sinal na mira, marcas no corpo, números saindo do alvo, seta de onde veio o tiro | Estilo Damage Informer: nick parado e número subindo, simétrico ao centro, com o dano real e o combo somando |

## Instalar

1. Copie `dist/Trok Hitmarker.asi` para a pasta do GTA (a mesma dos outros `.asi`; precisa do ASI Loader).
2. Se quiser já começar com as imagens e os sons para editar, copie também a pasta `dist/Trok Hitmarker` para o mesmo lugar. Sem ela, o mod cria a pasta na primeira vez com os padrões.
3. Entre num servidor e digite `/trokhit`.

Precisa do `gta_sa.exe` 1.0 US e do SA-MP 0.3.7 R1; em outra versão o mod fica desligado sem mexer em nada (o `Trok Hitmarker.log` diz o motivo). As escolhas ficam no `Trok Hitmarker.ini`, por servidor (`[padrao]` e uma seção `ip:porta`), gravadas um instante depois da última mudança no menu.

**Arquivos ao lado do `.asi`:**

| | |
|---|---|
| `Trok Hitmarker.ini` | as opções (criado na primeira mudança no menu) |
| `Trok Hitmarker.log` | o que o mod fez nesta sessão: versões conferidas, ganchos, imagens e sons carregados e os primeiros acertos com o dano do GTA e o real (para conferir no seu servidor) |
| `Trok Hitmarker\mira\` | `mira.png`, `acerto.png`, `colete.png`, `cabeca.png`, `abate.png`, `corpo.png` |
| `Trok Hitmarker\sons\` | `acerto`, `cabeca`, `dano_levado`, `abate`, `abate_cabeca`, `morte` (`.mp3` ou `.wav`) |

## O que o projeto antigo faz (e o que muda)

- **`damage-indicator-samp.lua`**: lê o pacote de dano que o seu SA-MP manda ao servidor (RPC 115) e mostra o número dele, `46.2`. Esse é o valor que o GTA calcula para a arma, não o que o servidor tirou: em servidor com dano de arma configurado, colete ou headshot que mata de uma vez, o número não bate. O combo `(x3)` conta tiros, não soma dano, e cada informer guarda um tiro só (com a M4, o tiro novo apaga o anterior).
- **`hitmarker.lua`**: o X (`hitmarker.txd`) no osso do corpo onde o tiro pegou. A duração é em quadros (28), então muda com o FPS: uns 0,47 s a 60 FPS e 0,2 s a 144 FPS.
- **`Take Damage Notification.lua`**: escreve cada acerto no chat (jogador, arma, parte do corpo e o mesmo `46.2`).

**No Trok Hitmarker:** o acerto continua vindo do pacote do SA-MP (é na hora e diz quem, arma e parte do corpo), mas o número é a **vida + colete que o alvo perdeu de verdade**, lidos do sync dele logo depois do tiro. Assim vale a regra de cada servidor sozinha: dano de arma configurado, colete, HS sem capacete que mata em 1. Se o servidor esconde a vida (o alvo não mostra a perda), aparece o número do GTA com um `~` na frente. O combo do informer soma o dano real no mesmo alvo. Tudo é medido em tempo, não em quadros.

## Mira, corpo e centro da tela

Só o sinal de acerto acompanha a mira. O resto fica preso à tela: a seta de onde veio o tiro gira em volta do centro, o informer fica dos dois lados do centro (dado à direita, levado à esquerda) e os números "na tela" sobem do lado do centro. O botão **Mover** (abas Informer e Geral) mostra as duas caixas do informer com um exemplo dentro: arraste cada uma, Enter salva, Esc ou o botão direito desiste (`docs/09-mover.png`).

- **Mira:** a do jogo ou a **do Trok** (a Mira Centralizada refeita: a imagem `mira.png`). Um tamanho só vale para a mira e para tudo que aparece nela. A do Trok pode ficar **sempre visível** (também sem mirar, a pé; some no carro e com sniper, bazuca e lança-mísseis, como no script original), e o GTA não desenha a mira dele. A do jogo pode ficar **estática** (não abre enquanto você atira; muda só o desenho, o espalhamento dos tiros continua o do jogo).
- **Uma imagem para cada coisa, sem estilos no código:** `mira.png`, `acerto.png`, `colete.png`, `cabeca.png`, `abate.png` e `corpo.png` (o X do local do acerto), na pasta `Trok Hitmarker\mira`. O visual é o da imagem: quer outro, edite o PNG (apagou, volta o padrão que vem no `.asi`). A imagem inteira ocupa o tamanho da mira (40 px numa tela de 1080, vezes o Tamanho; o corpo, 48 px), então qualquer resolução serve. O mod só faz o pulo (35% maior, voltando em 0,1 s), o tempo (acerto 0,35 s, cabeça 0,55 s, abate 1,2 s, em qualquer FPS) e o sumir. O efeito fica no lugar da mira enquanto aparece, por isso as imagens de efeito já trazem o ponto (`docs/04-hitmarker.png`; as padrão saem de `res/make_images.py`).
- **O que aparece:** acerto, colete, cabeça e abate ligam e desligam um por um. Colete, cabeça ou abate desligados mostram a imagem do acerto naquele tiro.
- **Onde:** desenhado onde a mira está naquele quadro. O mod lê onde o próprio GTA desenhou a mira, então mod que centraliza ou sobe a mira não precisa de ajuste nem de opção no menu (`docs/08-mira-seguida.png`).
- **Local do acerto:** o `corpo.png` fica na parte do corpo que o tiro pegou (cabeça, tronco, braço, perna) por 1 a 5 s, acompanhando o alvo quando ele anda (`docs/06-local-do-acerto.png`).

## Dano recebido

- **Seta de onde veio o tiro:** sempre vermelha, em volta do centro da tela, apontando para quem atirou (em cima = pela frente, embaixo = por trás). Gira junto quando você vira a câmera.
- **Borda de dano recebido:** as bordas da tela piscam na cor do que foi atingido: vida (vermelho), colete (preto) ou cabeça (azul). As três cores mudam no menu (`docs/15-borda.png`).

## Sons

Pasta `Trok Hitmarker\sons`, ao lado do `.asi`, com `.mp3` ou `.wav` destes nomes: `acerto`, `cabeca` (no lugar do acerto quando o tiro pega na cabeça, se "Som diferente na cabeça" estiver ligado), `dano_levado`, `abate`, `abate_cabeca` e `morte`. Se tiver os dois, vale o `.mp3`. Trocou ou apagou o arquivo, vale em até um segundo, sem reiniciar o jogo (apagou: volta o padrão). Os padrões foram sintetizados para o mod (`res/make_sounds.py`, nada copiado de outro jogo). Tocam pelo DirectSound do Windows, vários ao mesmo tempo, com o volume do menu; com o jogo minimizado ficam mudos.

## Como funciona por dentro

- **Acertos** (`src/samp.cpp`): ganchos no `CLocalPlayer::SendGiveDamage` e `SendTakeDamage` do samp.dll R1 (o mesmo pacote que o damage informer antigo lia) e na mensagem de morte (`CDeathWindow::AddMessage`). Os ganchos só avisam: o pacote segue para o servidor como sempre.
- **Dano real** (`src/logic.cpp`): a cada tiro, a vida + colete do sync do alvo antes; a queda que chega depois é o dano. Tiros que chegam juntos dividem a queda pelo que cada um costuma tirar (o mod aprende a proporção de cada arma com os tiros sozinhos). Sem queda em 1 s, vale o número do GTA com `~`. Abate: a mensagem de morte com o seu nick, ou a vida do alvo zerando depois de um tiro seu (conta uma vez só).
- **Mira** (`src/game.cpp`): ganchos no `CHud::DrawCrossHairs`, `CSprite2d::Draw`/`DrawRect` e `CPlayerPed::GetWeaponRadiusOnScreen` do 1.0 US. O mod lê onde o GTA desenha a mira em cada quadro (por isso segue mods que mexem nela); com a mira do Trok, os quartos da mira do jogo não chegam a ser desenhados; a estática zera o contador de tiros só durante o desenho; o tamanho da mira do jogo escala os retângulos dela. Se o desenho do mod parar, a mira do jogo volta sozinha em meio segundo.
- **Desenho** (`src/hud.cpp`, `src/menu.cpp`): Dear ImGui no Present do d3d9, como os outros mods da casa (`src/main.cpp`). Imagens (`src/images.cpp`, stb_image) viram texturas do D3D9; imagem grande é reduzida até 256 px.

## Compilar e testar

- `./build.sh`: MinGW i686, usa o kit da casa e o ImGui/MinHook da vitrine (`../trok-ui/asi`). As imagens e os sons de `dist/Trok Hitmarker` entram no `.asi` (`res/embed.py`).
- `test/smoke/run.sh`: Wine 32 bits + Xvfb. Cenários: `trok` (mira do Trok, acerto, número, local do acerto, abate, dano levado com seta e borda, dano sem queda de vida, menu e gravação do INI), `mira-do-jogo` (estática e 150%), `pasta-propria` (mp3, PNG retangular, troca da mira com o jogo rodando), `desligado`, `outra-versao` e `gta-errado`.
- `test/concept.sh`: as imagens de `docs/`, do código de desenho de verdade rasterizado no Linux.

Bibliotecas de terceiros em `third_party/`: minimp3 (CC0) e stb_image (domínio público/MIT).

## Imagens

`docs/04-hitmarker.png` (as imagens e a animação), `05-numeros.png`, `06-local-do-acerto.png`, `03-informer-lista.png`, `07-recebido.png`, `15-borda.png`, `08-mira-seguida.png`, `09-mover.png` e `10`–`18` (menu).
