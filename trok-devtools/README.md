# Trok DevTools

Uma ferramenta para modders dentro do GTA San Andreas: ver todos os modelos que o jogo conhece, achar o `.dff`, o `.txd` e o `.img` de cada um, girar o modelo em 3D, ver as texturas e criar o modelo no mundo, do tamanho real, na sua frente. A aba **Animações** mostra cada animação num personagem em 3D, toca no seu personagem e marca as que algum mod mudou. A aba **Áudios** lista e toca todos os efeitos sonoros do jogo e mostra o que o jogo está tocando agora. Com o menu fechado, o **Info ao apontar** mostra o id e os arquivos do que está no meio da tela.

As abas **Modelos**, **Animações**, **Áudios** e **Geral** funcionam. Arquivos está no menu como "Em breve". As texturas não têm aba: ficam no card do modelo.

**Estado:** v0.4.2, `.asi` compilado e testado fora do jogo:
- no Wine, com um GTA de mentira que tem modelos, TXDs, colisões, `.ide`, `.img`, pacotes de som, `.ifp` e um personagem com ossos (`test/smoke/run.sh`, 79 conferências e 25 capturas);
- no Linux, o leitor dos `.ifp` e a conta da pose (`test/anim/run.sh`).

Falta o teste no jogo de verdade.

**Novo na v0.4.2:** a aba Áudios lê os sons trocados pelo **modloader** (`.wav` em `modloader\<mod>\<PACOTE>\Bank_<N>\sound_<N>.wav`). Eles ganham o selo **MOD**, e o player e o "Salvar em .wav" usam o `.wav` do modloader, que é o som que o jogo toca.

**Na v0.4.1:**
- Os personagens não aparecem mais deitados (na grade, no card e na prévia das animações). A pele dos personagens do jogo vem deitada, e agora o mod usa o esqueleto para pôr de pé, como o jogo faz.
- Animação curtíssima (o `abseil` tem 0,03 s) aparece como **pose**, parada, sem a linha do tempo pulando.
- O **botão do meio** do Info ao apontar é lido do mouse do próprio jogo. Com o menu fechado, o jogo pega o mouse pelo DirectInput e o clique não chegava.
- O histórico de áudio mostra também os sons do servidor (SA-MP 0.3.7 R1): o **PlayerPlaySound** (com o id) e os **streams** por URL (o mp3 que o servidor toca). Clique copia o id ou a URL.
- O histórico diz quantos sons o jogo já pediu, e o log também (com 100 e com 1.000).

**Na v0.4:** a aba **Animações** (abaixo).

**Na v0.3:** a aba **Áudios**.

**Na v0.2:**
- A textura clicada no card mostra quem mais usa a mesma textura.
- O visualizador ganhou a cor do fundo, no seletor de cor da casa.
- Os botões do visualizador (Grade, Colisão...) agora recebem o clique. Antes o clique virava arrasto do modelo.
- Os objetos nascem na altura dos olhos.
- O Info ao apontar ficou certo com modelos grandes (bairros, terrenos).

## Instalar

1. Copie `dist/Trok DevTools.asi` para a pasta do GTA (a mesma dos outros `.asi`; precisa do ASI Loader).
2. Abra o jogo. O `Trok DevTools.ini` nasce sozinho na primeira mudança de opção.

Precisa do `gta_sa.exe` 1.0 US. Em outra versão o mod fica desligado sem mexer em nada, e o `Trok DevTools.log` diz o motivo. Funciona no single player e no SA-MP.

A fonte é a da casa (`moonloader\resource\trok\font.ttf`) se ela estiver na pasta do jogo; se não, a Arial do Windows.

## Abrir

- **F10**, sempre (no single player é o único jeito);
- **`/trokdevtools`** no SA-MP 0.3.7 R1.

Esc volta da textura para o modelo, depois fecha o card e, com nenhum card aberto, fecha o menu.

## Aba Modelos

Uma grade com um card por modelo: a miniatura 3D, o id e o nome do `.dff`.

- **Busca:** por id, nome do `.dff`, do `.txd`, do `.ide` ou do `.img` (`411`, `infernus`, `lae.ide`, `samp.img`).
- **Filtros:** Todos, Objetos, Veículos, Skins, Armas; um `.img` só (gta3, gta_int, samp...); **Só substituídos**.
- **Miniaturas:** só dos cards que estão na tela, algumas por quadro (padrão 3; dá para mudar no Geral). O modelo é carregado, desenhado e solto em seguida, então a grade não enche a memória do jogo.
- **MOD:** o selo nos cards cujo arquivo não é o do `.ide` (ver "Modelo modificado", abaixo).
- **sem arquivo / sem malha:** ids que o jogo conhece mas não têm `.dff` num `.img`, ou cujo `.dff` não tem nada para desenhar.

### O card

Clique num card e abre o popup do modelo:

- **Visualizador 3D:**
  - Arraste para girar, roda para zoom, duplo clique para centralizar.
  - As opções embaixo ficam salvas: **Texturas**, **Arame**, **Colisão** (as caixas, esferas e triângulos da colisão, em cima do modelo), **Grade** (o chão, 1 m por quadrado; clique para tirar) e **Girar** (gira sozinho).
  - O botão **Fundo**, ao lado das opções, abre o seletor de cor da casa: o quadrado de saturação e brilho, a barra de matiz, RGB e hex, a cor padrão e cores rápidas. É o mesmo seletor do Geral. Para chroma key de vídeo, use verde puro (`00B140`) ou azul (`0047BB`).
- **Informações:** ID, tipo, DFF, TXD (com o pai, se houver), o `.img` e o tamanho do arquivo, se é substituído, o `.ide` que define, distância de desenho, flags decodificadas, tamanho em metros, vértices, triângulos, materiais, atômicos e a colisão.
- **Texturas:** as usadas pelo modelo e, separadas, as outras da mesma TXD. Cada uma mostra o tamanho e o formato. Clique numa textura para abrir a textura (abaixo).
- **Criar no mundo (1:1):** cria o modelo na sua frente, virado para você. A distância cresce com o tamanho do modelo, para ele não nascer em cima de você.
  - Objetos nascem com o meio na altura dos olhos do personagem e ficam parados no ar. Os que não cabem (mais altos que ~3 m) nascem com a base no chão.
  - Carros e personagens nascem no chão. Carros nascem trancados.
  - **Apagar criados** apaga todos os que você criou.
- **Copiar ID / DFF / TXD:** para a área de transferência.

### A textura aberta

Clique numa textura do card:

- **O visualizador** mostra a textura grande, sobre um xadrez (para ver o alfa), com o formato e os mipmaps.
- **O lado direito** troca as informações do modelo pelas da textura: a TXD onde ela está (e se é a TXD mãe, pelo `txdp`), tamanho, formato (DXT1, DXT3...), mipmaps, alfa e máscara. Embaixo ficam **Voltar ao modelo** e **Copiar nome**.
- **Modelos com esta textura:** a lista começa pelo modelo do card, marcado como "aberto", e segue com os outros que usam a mesma textura.
  - **Como acha:** só modelos da TXD onde a textura está, ou de TXDs filhas dela, podem usar a textura. O mod carrega cada um desses aos poucos (como as miniaturas), olha os materiais e solta em seguida.
  - **TXD grande:** numa TXD genérica com muitos modelos, ele vê só os 600 primeiros.
  - **Textura que o modelo não usa:** se você clicou numa do "Também na TXD", o modelo do card aparece com o aviso "está na TXD, mas este modelo não usa".
- **Clique num modelo da lista:** ele vira o modelo do card, e a mesma textura continua aberta. Para ver o modelo em 3D, use **Voltar ao modelo**.
- Embaixo, as texturas do modelo, para trocar de textura sem voltar.

### Modelo modificado

A miniatura e o visualizador desenham **o modelo que o jogo carregou**, e não o original. Então, se você trocou o `.dff` ou o `.txd` dentro do `gta3.img`, ou por um modloader, é o seu que aparece.

O selo **MOD** e o filtro **Só substituídos** pegam o caso que dá para provar sem o arquivo original: o jogo lê o modelo de uma entrada do `.img` com outro nome (por exemplo, `chair` lido de `chair02.dff`). Um arquivo trocado com o mesmo nome dentro do `.img` não tem como ser diferenciado sem o original.

## Aba Animações

Todas as animações do jogo: o `anim\ped.ifp` e os `.ifp` de dentro do `anim\anim.img`. Na esquerda, a lista (**.ifp › animações**); no meio, a prévia; na direita, os dados e o que fazer com a animação.

- **Busca:** todas as palavras precisam bater. Vale o nome da animação, o do `.ifp` e as marcas (`walk`, `dance`, `ped`, `mod`, `nova`).
- **Filtros:** um `.ifp` só, e **Só modificadas e novas**.
- **Setas** para cima e para baixo trocam de animação; **espaço** pausa a prévia.

### A prévia

- O personagem é o skin do seu personagem (o CJ, que o jogo monta com as roupas, vira o primeiro personagem comum). O botão **Personagem**, no canto de cima, troca por qualquer skin.
- A animação toca nos ossos do personagem, como o jogo faz. Embaixo ficam tocar/pausar, a linha do tempo (arraste para ir a um instante), a velocidade (0,25× a 2×) e **repetir**.
- As opções são as mesmas do visualizador do card: **Texturas**, **Arame**, **Grade**, **Girar** e **Fundo**. Tem também **No lugar**: andar e correr sem sair do lugar.
- Animação que não mexe em nenhum osso do personagem (de objetos, por exemplo) mostra um aviso.

### No seu personagem

- **Tocar no meu personagem:** o seu personagem toca a animação no jogo (`TASK_PLAY_ANIM`), repetindo ou uma vez, como na prévia. Com **No lugar** desligado, a animação leva o personagem. O `.ifp` é carregado na hora e fica preso até parar.
- **Parar no personagem:** para na hora.
- Dentro de um veículo não toca: o mod avisa para sair.
- **Copiar nome** copia `"PED", "WALK_civi"`. **Copiar ApplyAnimation** copia a linha pronta do SA-MP, com repetir e no lugar como estão.

### MOD e NOVA

As marcas aparecem nas animações e, com quantas, nos `.ifp`.

- **NOVA:** a animação não existe no jogo original. Funciona sempre: a lista das animações originais (com o índice do SA-MP) vem do open.mp. Um `.ifp` que não é do jogo fica com todas novas.
- **MOD:** a animação existe no original, mas é diferente, até dentro de um `.ifp` que tem outras originais. Para isso o mod precisa da **referência**: copie o `anim.img` e o `ped.ifp` de um jogo sem mods para a pasta `Trok DevTools\originais`.
  - O mod compara os bytes de cada animação com a original e grava as impressões em `Trok DevTools\animacoes_originais.txt`.
  - Esse `.txt` sozinho já serve de referência: dá para levar para outro jogo (ou mandar para virar a referência embutida no mod) e apagar a pasta.
- O `.ifp` que tem um **nome interno** diferente do nome do arquivo ganha um aviso. O jogo guarda as animações dele com o nome interno, e os scripts que usam o nome do arquivo não acham.
- O lado direito também diz quais originais **faltam** no `.ifp`.

## Aba Áudios

Os efeitos sonoros do jogo (`audio\SFX`), na ordem do jogo: **pacote › bank › som**.

- **Os pacotes:** FEET, GENRL, PAIN_A, SCRIPT e as falas (SPC_EA, SPC_FA, SPC_GA, SPC_NA, SPC_PA). Cada um mostra quantos banks e sons tem, e o que tem dentro.
- **Os banks:** `Bank_002  90S_P`, quantos sons e, quando se sabe, o que é: "motor, com você dirigindo", com os veículos que usam esse banco ("Bravura, Previon, Solair e mais 9").
- **Os sons:** `sound_001`, a duração, a marca **loop** (o som tem ponto de repetição), o nome do jogo quando existe (`AK_A`) e as armas que usam o som.
- **Busca:** todas as palavras precisam bater. Vale o pacote, o banco (`90s_p`, `137`), o nome do som, o veículo (`sultan`), a arma (`m4`, `ak`) e o que o banco tem (`ricochete`, `porta`).
- **Filtros:** **Tipo** (motor no tráfego, motor com você dirigindo, armas, passos, dor, missões, falas, outros), **Veículo**, **Arma** e **Loop** (todos, só com loop, sem loop). Com filtro, os pacotes com resultado abrem sozinhos.
- **Modloader:** os sons trocados por `.wav` no modloader (do jeito que o `std.bank` dele lê: `modloader\<mod>\...\<PACOTE>\Bank_<N>\sound_<N>.wav`, mono PCM 16) aparecem com o selo **MOD**, no som, no bank e no pacote (com quantos). O filtro **Só do modloader** mostra só esses, e a dica do selo diz o caminho do `.wav`. O player e o `.wav` salvo usam o som do modloader. Se dois mods trocam o mesmo som, vale o primeiro em ordem alfabética, e as pastas que começam com ponto ficam de fora.
- **O player** (embaixo da lista): clique num som para tocar, e de novo para parar. Tem **repetir** e **salvar em .wav**. O ícone de download na linha também salva. Os `.wav` vão para `<pasta do jogo>\Trok DevTools\sons\`, com o nome `GENRL_Bank_002_90S_P_sound_001.wav`.

### Áudio tocando agora

O lado direito da aba:

- **Mostrar na tela (menu fechado):** com o menu fechado, os sons que o jogo toca aparecem em cartões no lado direito da tela: o pacote, o bank, o som, a duração e o nome. O jogo toca muitos sons, então a tela mostra só os 3 últimos; a lista toda fica no histórico. O mesmo som repetido em menos de 1,5 s soma no mesmo cartão (`×3`). O botão **Info do áudio**, no canto da aba, faz o mesmo.
- **Tempo na tela:** quanto tempo cada cartão fica (1 a 15 s, padrão 4 s). Depois ele some aos poucos.
- **Ignorar os passos:** sem os passos (FEET), que tocam o tempo todo.
- **Histórico:** os últimos 100 sons, o mais novo em cima, com a hora. Clique num deles para achar o som na lista e tocar. Se o som está fora do filtro atual, o filtro é limpo. **Limpar o histórico** zera a lista. Em cima, quantos sons o jogo já pediu (com o histórico vazio, isso diz se o mod está pegando os sons).
- **Sons do servidor (SA-MP 0.3.7 R1):** o PlayerPlaySound aparece como **Servidor › PlayerPlaySound › som N**, e logo depois vem o som do jogo que ele toca. Um stream (o servidor tocando um mp3 por URL) aparece como **Servidor › stream › arquivo.mp3**. O clique copia o id ou a URL.
- Os sons tocados pelo player da aba não entram no histórico: eles tocam por fora do jogo.

O rádio e as músicas (pasta `STREAMS`) não são do SFX e não aparecem. Um mod que toca sons por conta própria (com o BASS, por exemplo) também fica de fora: o mod só vê o que passa pelo áudio do jogo e pelo SA-MP.

## Info ao apontar

O botão **Info ao apontar** (no canto da aba Modelos, e no Geral) liga e desliga. Com o menu fechado:

- um ponto no meio da tela;
- a caixa da colisão em volta do que está no meio da tela (até 300 m, a partir do seu personagem, pela câmera). Em modelos grandes, que passam por trás da câmera, a caixa é cortada no plano da câmera. Com a câmera dentro da caixa (no meio de um bairro, por exemplo), só o cartão aparece;
- um cartão ao lado dela com o id, o `.dff`, o tipo, o `.img`, a TXD, a posição, o interior, a distância e o LOD;
- **botão do meio do mouse** abre o card desse modelo (o botão é lido do mouse do próprio jogo, o `CPad`).

## Aba Geral

- Info ao apontar.
- Áudio: mostrar o áudio tocando agora e o tempo na tela.
- Visualizador: a cor do fundo (qualquer cor, com o código hex) e a grade.
- Miniaturas por quadro (1 a 8) e refazer as miniaturas.
- Quantos modelos você criou, e apagar.
- O atalho e a versão.

## Trok DevTools.ini

Uma seção `[devtools]`, salva pelo próprio menu:

| Chave | Padrão | O que faz |
|---|---|---|
| `inspecionar` | `0` | Info ao apontar |
| `visual_texturas` | `1` | visualizador com texturas |
| `visual_arame` | `0` | visualizador em arame |
| `visual_colisao` | `0` | colisão por cima do modelo |
| `visual_grade` | `1` | o chão em grade |
| `visual_girar` | `1` | o modelo gira sozinho |
| `visual_fundo` | `141414` | a cor do fundo do visualizador, em hex `RRGGBB` (verde chroma: `00B140`) |
| `miniaturas_por_quadro` | `3` | 1 a 8: mais = a grade enche mais rápido, mas pesa mais por quadro |
| `audio_mostrar` | `0` | o áudio tocando agora na tela, com o menu fechado |
| `audio_tempo` | `4` | segundos de cada cartão na tela (1 a 15) |
| `audio_ignorar_passos` | `1` | os passos ficam fora da tela e do histórico |
| `audio_repetir` | `0` | o player da aba repete o som |
| `anim_repetir` | `1` | a prévia (e o personagem) repete a animação |
| `anim_no_lugar` | `1` | andar e correr sem sair do lugar |
| `anim_girar` | `0` | a prévia gira sozinha |
| `anim_personagem` | `-1` | o skin da prévia (`-1` = o do seu personagem) |
| `anim_velocidade` | `2` | a velocidade da prévia: 0 = 0,25×, 1 = 0,5×, 2 = 1×, 3 = 2× |

## Limites

- **No SA-MP, o que você cria é só seu:** o servidor não sabe desses objetos e carros, e os outros jogadores não os veem. Igual ao `/model`.
- **No single player** não existe o travamento de câmera do SA-MP: com o menu aberto, o jogo não pausa e o mouse ainda mexe a câmera. Pare o personagem antes de abrir.
- A colisão só aparece no card quando o jogo já a carregou, o que acontece perto dela no mapa. Longe, o card diz "não carregada".
- Trens e o id 0 não são criados.
- **Modloader nos áudios:** só os `.wav`. Um pacote do SFX inteiro ou um `BankLkup.dat` trocado dentro do modloader não é lido (a aba mostra o pacote da pasta `audio`).
- **O player da aba Áudios** toca o som cru (do pacote ou do `.wav` do modloader), pelo Windows (`waveOut`), por fora do áudio do jogo: sem o volume do menu do jogo, sem efeito de distância e sem o tom que o jogo muda (um motor em rotação alta, por exemplo).
- **A prévia das animações** faz a conta dos ossos como o jogo, mas sem a mistura de animações e sem as partes que o jogo calcula na hora (olhar para os lados, segurar a arma). A animação no seu personagem é a do próprio jogo.
- **MOD nas animações** precisa da referência das originais (ver "MOD e NOVA"). Sem ela, só as novas são marcadas.
- **Nomes:** os sons do SFX não têm nome nos arquivos. Os nomes dos banks e de uns 360 sons, a tabela de bancos de cada veículo e os sons de cada arma vêm do gta-reversed (o código do jogo reconstruído, em `github.com/gta-reversed/gta-reversed`). O resto aparece só como `sound_NNN`.
- O catálogo lê os `.ide` do `default.dat`, do `gta.dat` e do SA-MP (`SAMP\SAMP.IDE` e `SAMP\CUSTOM.IDE`). Todo id que o jogo carregou aparece na grade, mas um modelo de um `.ide` fora desses (de um modloader, por exemplo) aparece só com o id e o nome do arquivo no `.img`, sem o `.ide` e às vezes sem a TXD.

## Trok DevTools.log

Ao lado do `.asi`, um por sessão de jogo:

- versão conferida e ganchos instalados;
- o catálogo: quantos `.ide`, modelos, `.img` e substituídos;
- cada modelo criado (id, onde, handle) e cada pedido recusado, com o motivo;
- cada textura aberta: a TXD, quantos candidatos e quantos usam.
- as animações: `ifp: N arquivos, N animacoes (N modificadas, N novas; referencia: ...)`, os `.ifp` com outro nome interno, o personagem da prévia (`previa: personagem 46 com 32 ossos`) e cada animação tocada no seu personagem;
- os sons: `sfx: 9 pacotes, 710 bancos, ... sons`, `audio: carregador de bancos em AEAudioHardware+0x...` e `audio: primeiro som: ...` (a prova de que o áudio tocando agora pega os sons do jogo com o bank certo).

Se algo não abrir ou travar, mande esse arquivo.

## Como funciona (para quem mexe no código)

| Arquivo | O que faz |
|---|---|
| `src/main.cpp` | o `.asi`: confere o 1.0 US, desvia Present/Reset do D3D9 e o `CGame::Process` (`0x53BEE0`), registra `/trokdevtools` no SA-MP R1 e o F10 |
| `src/catalog.cpp` | numa thread: lê os `.ide` e os diretórios dos `.img` (VER2); no jogo: cruza com a tabela de modelos e o streaming |
| `src/core.cpp` | na thread do jogo: carrega e solta modelos (miniaturas e card), procura quem usa uma textura, cria e apaga no mundo, a linha de visão do Info ao apontar |
| `src/mesh.cpp` | copia a malha do RenderWare (vértices, normais, UV, materiais, texturas) e a colisão para um formato próprio; diz se uma TXD tem uma textura e se um modelo usa |
| `src/render.cpp` | desenha com o D3D9 fixo: o atlas das miniaturas (2048 px, células de 128) e o visualizador |
| `src/sfx.cpp` | numa thread: lê o `PakFiles.dat`, o `BankLkup.dat` e o cabeçalho de cada bank nos pacotes; o PCM de um som e o `.wav` |
| `src/player.cpp` | toca um som pelo `waveOut` (repetir, posição) |
| `src/audiospy.cpp` | o áudio tocando agora: o gancho no `CAESoundManager::RequestNewSound` e o histórico |
| `src/audio_data.h` | gerado por `tools/gen_audio_data.py` a partir do gta-reversed: os nomes dos banks e dos sons, os bancos de cada veículo e os sons de cada arma |
| `src/ifp.cpp` | numa thread: lê o `ped.ifp` e os `.ifp` do `anim.img` (ANP3, ANP2, ANPK), as impressões digitais, as marcas MOD/NOVA e a referência; decodifica os quadros-chave de uma animação |
| `src/pose.cpp` | a pose de um instante da animação nos ossos do personagem, e a pele |
| `src/anim_names.h` | gerado por `tools/gen_anim_names.py` a partir do open.mp: as animações originais por `.ifp`, com o índice do SA-MP |
| `src/devtools.cpp` | o menu, com o kit da casa (`trok-ui/asi`) |

- **Carregar:** `CStreaming::RequestModel` (`0x4087E0`) com a flag de missão + `LoadAllRequestedModels` (`0x40EA10`); soltar: `SetMissionDoesntRequireModel` (`0x409C90`), só se fomos nós que pusemos a flag. Depois disso o jogo descarrega quando quiser, como faz com qualquer modelo; um que está no mapa continua carregado.
- **Tabela de modelos:** o ponteiro vem de `0x40CD67` (para pegar o limit adjuster), 20.000 ids. Streaming: `ms_aInfoForModel` (`0x8E4CC0`) e `ms_files` (`0x8E48D8`). TXDs: o pool do `CTxdStore` (`0xC8800C`).
- **Veículos:** os atômicos `_vlo` (de longe) e os amassados (sem a flag de render) ficam de fora; as rodas da esquerda giram 180°, e as cores de marcação do material (verde, magenta, faróis) viram cores neutras.
- **Criar e apagar:** pelos comandos de script do próprio jogo (`CRunningScript::ProcessOneCommand`, `0x469EB0`): `0107`/`00A5`/`009A` criam, `0177`/`0175`/`0173` viram, `0550` congela o objeto, `020A` tranca o carro; para apagar, `03CA`/`056E`/`056D` conferem se ainda existe e `0108`/`00A6`/`009B` apagam.
- **Info ao apontar:** `CWorld::ProcessLineOfSight` (`0x56BA00`) pela câmera (`0xB6F028`), começando depois do personagem. A caixa vem do `CColModel`. Cada aresta é cortada a 1,5 m na frente da câmera e só então vai para a tela, com `CSprite::CalcScreenCoors` (`0x70CE30`).
- **Quem usa uma textura:** a TXD que tem a textura sai da lista de texturas do dicionário (`RwTexDictionary`, +0x08), procurando na TXD do modelo e depois nas mães (`CTxdStore`, pai em +6). Os candidatos são as entradas do catálogo cuja TXD é essa ou descende dela. Para cada um, o mod olha os materiais de todos os atômicos (`RpGeometry` +0x20/+0x24, textura em `RpMaterial` +0, nome em `RwTexture` +0x10).
- **Sons:** o `BankLkup.dat` (12 bytes por bank: pacote, deslocamento, tamanho) aponta para o bank dentro do pacote. O bank começa com um cabeçalho de `0x12C4` bytes: quantos sons e, para cada um dos 400, o início, o início do loop (`-1` = sem loop), a frequência e o headroom. Depois vem o PCM de 16 bits mono.
- **Sons do servidor:** ganchos no `CAudio::Play` (`samp.dll+0x9D730`, o PlayerPlaySound) e no `CAudioStream::Play` (`samp.dll+0x62DA0`, a URL) do SA-MP 0.3.7 R1, pelos endereços do SAMP-API.
- **Áudio tocando agora:** um gancho no `CAESoundManager::RequestNewSound` (`0x4EFB10`) lê, de cada `CAESound` pedido, o slot (+0), o som (+2) e o evento (+0xC). O bank que está naquele slot vem do `CAEMP3BankLoader` do `AEAudioHardware` (`0xB5F8B8`): o mod procura o ponteiro perto de `+0xD98` e só usa se o número de banks e de pacotes bater com os arquivos. O bank do slot fica em `CAEBankSlot` +0x10 (`0x12D4` bytes por slot).
- **Animações:** o formato é o do `CAnimManager::LoadAnimFile` (gta-reversed). No ANP3, cada osso tem a etiqueta (`BONE_ROOT` = 0...) e os quadros (rotação em `int16 / 4096`, tempo em `int16 / 60`, translação em `int16 / 1024` nos comprimidos); o tempo é desde o começo. O ANPK (do GTA3/VC) vem com a rotação conjugada.
- **Personagem de pé:** o jogo desenha a pele com `skinToBone` × a matriz de cada osso (dos frames do esqueleto), sem o frame do atômico. O espaço da pele dos personagens vem deitado, então o modelo parado usa `skinToBone[0]` × o frame raiz do esqueleto (o `parentFrame` do `RpHAnimHierarchy`, +0x14).
- **Pose:** a pele do personagem sai do RenderWare (`RpSkinGeometryGetSkin` `0x7C7550`, pesos, índices e `skinToBone` de cada osso, e a hierarquia do `RpSkinAtomicGetHAnimHierarchy` `0x7C7540`). O osso com quadros-chave usa a rotação deles (a matriz do `CQuaternion::Get`) e a translação, se tiver. Os outros ficam na pose do modelo, e a raiz sem translação fica na origem, como o `BonePos` do jogo.
- **No seu personagem:** o bloco vem de `CAnimManager::ms_aAnimBlocks` (`0xB5D4A0`) e carrega pelo streaming (recurso `25575` + o bloco). Depois, `GET_PLAYER_CHAR` (`01F5`), `IS_CHAR_IN_ANY_CAR` (`00DF`) e `TASK_PLAY_ANIM` (`0605`); para parar, `CLEAR_CHAR_TASKS_IMMEDIATELY` (`0792`).
- **Reset do device (Alt+Tab):** os alvos de render saem antes e as miniaturas são refeitas depois.

### Compilar e testar

- `tools/gen_anim_names.py`: gera o `src/anim_names.h` a partir do `include/anim.hpp` do open.mp-sdk (licença MIT) (`python3 -I tools/gen_anim_names.py anim.hpp > src/anim_names.h`).
- `test/anim/run.sh`: no Linux, sem o jogo. Testa o leitor dos `.ifp` (as marcas pelos nomes, pela pasta das originais e pelo `.txt`, os quadros-chave do ANP3 comprimido e não comprimido e do ANPK) e a conta da pose num esqueleto de 2 ossos.
- `tools/gen_audio_data.py`: gera o `src/audio_data.h` a partir de `eSoundBank.h`, `SoundIDs.h`, `AEVehicleAudioEntity.VehicleAudioSettings.h` e `AEWeaponAudioEntity.cpp` do gta-reversed (`python3 -I tools/gen_audio_data.py <pasta> > src/audio_data.h`).
- `build.sh`: MinGW i686 + ImGui 1.89.9 + MinHook + o kit da casa → `build/` e `dist/Trok DevTools.asi`.
- `test/concept/concept.sh`: as imagens de conceito em `test/out/concept/`, no Linux e sem o jogo. O menu de verdade (`devtools.cpp` e o kit) é desenhado sobre uma rua de mentira. Os modelos são desenhos low-poly feitos em Python (`models.py`), com os ids e nomes do jogo. O catálogo, o carregamento e o desenho 3D do `.asi` são trocados por versões que leem o que o `assets.py` gerou.
- `test/smoke/run.sh`: roda o `build/Trok DevTools.asi` (rode o `build.sh` antes) no Wine 32 bits com Xvfb. O `host.c` é um `.exe` mínimo cuja `.bss` cobre os endereços do GTA, e o `fake_gta.cpp` monta ali o jogo de mentira: modelos, TXDs, colisões, `.ide`, `.img`, streaming, linha de visão, comandos de script, os pacotes de som e o carregador de bancos, com sons pedidos pelo `RequestNewSound`, os `.ifp` (com a pasta das originais) e um personagem com pele e 8 ossos. As capturas ficam em `test/out/smoke/`.
