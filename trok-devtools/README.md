# Trok DevTools

Uma ferramenta para modders dentro do GTA San Andreas: ver todos os modelos que o jogo conhece, achar o `.dff`, o `.txd` e o `.img` de cada um, girar o modelo em 3D, ver as texturas e criar o modelo no mundo, do tamanho real, na sua frente. Com o menu fechado, o **Info ao apontar** mostra o id e os arquivos do que está no meio da tela.

Por enquanto só a aba **Modelos** funciona. As outras (Texturas, Animações, Áudios, Arquivos) estão no menu como "Em breve".

**Estado:** v0.1, `.asi` compilado e testado fora do jogo (Wine, com um GTA de mentira que tem modelos, TXDs, colisões, `.ide` e `.img` de verdade: `test/smoke/run.sh`, 36 conferências e 7 capturas). Falta o teste no jogo de verdade.

## Instalar

1. Copie `dist/Trok DevTools.asi` para a pasta do GTA (a mesma dos outros `.asi`; precisa do ASI Loader).
2. Abra o jogo. O `Trok DevTools.ini` nasce sozinho na primeira mudança de opção.

Precisa do `gta_sa.exe` 1.0 US. Em outra versão o mod fica desligado sem mexer em nada, e o `Trok DevTools.log` diz o motivo. Funciona no single player e no SA-MP.

A fonte é a da casa (`moonloader\resource\trok\font.ttf`) se ela estiver na pasta do jogo; se não, a Arial do Windows.

## Abrir

- **F10**, sempre (no single player é o único jeito);
- **`/trokdevtools`** no SA-MP 0.3.7 R1.

Esc fecha o card aberto e, com nenhum card aberto, o menu.

## Aba Modelos

Uma grade com um card por modelo: a miniatura 3D, o id e o nome do `.dff`.

- **Busca:** por id, nome do `.dff`, do `.txd`, do `.ide` ou do `.img` (`411`, `infernus`, `lae.ide`, `samp.img`).
- **Filtros:** Todos, Objetos, Veículos, Skins, Armas; um `.img` só (gta3, gta_int, samp...); **Só substituídos**.
- **Miniaturas:** só dos cards que estão na tela, algumas por quadro (padrão 3; dá para mudar no Geral). O modelo é carregado, desenhado e solto em seguida, então a grade não enche a memória do jogo.
- **MOD:** o selo nos cards cujo arquivo não é o do `.ide` (ver "Modelo modificado", abaixo).
- **sem arquivo / sem malha:** ids que o jogo conhece mas não têm `.dff` num `.img`, ou cujo `.dff` não tem nada para desenhar.

### O card

Clique num card e abre o popup do modelo:

- **Visualizador 3D:** arraste para girar, roda para zoom, duplo clique para centralizar. As opções embaixo ficam salvas: **Texturas**, **Arame**, **Colisão** (as caixas, esferas e triângulos da colisão, em cima do modelo), **Grade** (o chão, 1 m por quadrado) e **Girar** (gira sozinho).
- **Informações:** ID, tipo, DFF, TXD (com o pai, se houver), o `.img` e o tamanho do arquivo, se é substituído, o `.ide` que define, distância de desenho, flags decodificadas, tamanho em metros, vértices, triângulos, materiais, atômicos e a colisão.
- **Texturas:** as usadas pelo modelo e, separadas, as outras da mesma TXD. Cada uma mostra o tamanho e o formato. Clique numa textura para vê-la grande no visualizador, sobre um xadrez (para ver o alfa), com o formato e os mipmaps.
- **Criar no mundo (1:1):** cria o modelo na sua frente, virado para você e com a base no chão. A distância cresce com o tamanho do modelo, para ele não nascer em cima de você. Objetos nascem parados, carros nascem trancados. **Apagar criados** apaga todos os que você criou.
- **Copiar ID / DFF / TXD:** para a área de transferência.

### Modelo modificado

A miniatura e o visualizador desenham **o modelo que o jogo carregou**, e não o original. Então, se você trocou o `.dff` ou o `.txd` dentro do `gta3.img`, ou por um modloader, é o seu que aparece.

O selo **MOD** e o filtro **Só substituídos** pegam o caso que dá para provar sem o arquivo original: o jogo lê o modelo de uma entrada do `.img` com outro nome (por exemplo, `chair` lido de `chair02.dff`). Um arquivo trocado com o mesmo nome dentro do `.img` não tem como ser diferenciado sem o original.

## Info ao apontar

O botão **Info ao apontar** (no canto da aba Modelos, e no Geral) liga e desliga. Com o menu fechado:

- um ponto no meio da tela;
- a caixa da colisão em volta do que está no meio da tela (até 300 m, a partir do seu personagem, pela câmera);
- um cartão ao lado dela com o id, o `.dff`, o tipo, o `.img`, a TXD, a posição, o interior, a distância e o LOD;
- **botão do meio do mouse** abre o card desse modelo.

## Aba Geral

Info ao apontar, miniaturas por quadro (1 a 8), refazer as miniaturas, quantos você criou (e apagar), o atalho e a versão.

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
| `miniaturas_por_quadro` | `3` | 1 a 8: mais = a grade enche mais rápido, mas pesa mais por quadro |

## Limites

- **No SA-MP, o que você cria é só seu:** o servidor não sabe desses objetos e carros, e os outros jogadores não os veem. Igual ao `/model`.
- **No single player** não existe o travamento de câmera do SA-MP: com o menu aberto, o jogo não pausa e o mouse ainda mexe a câmera. Pare o personagem antes de abrir.
- A colisão só aparece no card quando o jogo já a carregou, o que acontece perto dela no mapa. Longe, o card diz "não carregada".
- Trens e o id 0 não são criados.
- O catálogo lê os `.ide` do `default.dat`, do `gta.dat` e do SA-MP (`SAMP\SAMP.IDE` e `SAMP\CUSTOM.IDE`). Todo id que o jogo carregou aparece na grade, mas um modelo de um `.ide` fora desses (de um modloader, por exemplo) aparece só com o id e o nome do arquivo no `.img`, sem o `.ide` e às vezes sem a TXD.

## Trok DevTools.log

Ao lado do `.asi`, um por sessão de jogo:

- versão conferida e ganchos instalados;
- o catálogo: quantos `.ide`, modelos, `.img` e substituídos;
- cada modelo criado (id, onde, handle) e cada pedido recusado, com o motivo.

Se algo não abrir ou travar, mande esse arquivo.

## Como funciona (para quem mexe no código)

| Arquivo | O que faz |
|---|---|
| `src/main.cpp` | o `.asi`: confere o 1.0 US, desvia Present/Reset do D3D9 e o `CGame::Process` (`0x53BEE0`), registra `/trokdevtools` no SA-MP R1 e o F10 |
| `src/catalog.cpp` | numa thread: lê os `.ide` e os diretórios dos `.img` (VER2); no jogo: cruza com a tabela de modelos e o streaming |
| `src/core.cpp` | na thread do jogo: carrega e solta modelos (miniaturas e card), cria e apaga no mundo, a linha de visão do Info ao apontar |
| `src/mesh.cpp` | copia a malha do RenderWare (vértices, normais, UV, materiais, texturas) e a colisão para um formato próprio |
| `src/render.cpp` | desenha com o D3D9 fixo: o atlas das miniaturas (2048 px, células de 128) e o visualizador |
| `src/devtools.cpp` | o menu, com o kit da casa (`trok-ui/asi`) |

- **Carregar:** `CStreaming::RequestModel` (`0x4087E0`) com a flag de missão + `LoadAllRequestedModels` (`0x40EA10`); soltar: `SetMissionDoesntRequireModel` (`0x409C90`), só se fomos nós que pusemos a flag. Depois disso o jogo descarrega quando quiser, como faz com qualquer modelo; um que está no mapa continua carregado.
- **Tabela de modelos:** o ponteiro vem de `0x40CD67` (para pegar o limit adjuster), 20.000 ids. Streaming: `ms_aInfoForModel` (`0x8E4CC0`) e `ms_files` (`0x8E48D8`). TXDs: o pool do `CTxdStore` (`0xC8800C`).
- **Veículos:** os atômicos `_vlo` (de longe) e os amassados (sem a flag de render) ficam de fora; as rodas da esquerda giram 180°, e as cores de marcação do material (verde, magenta, faróis) viram cores neutras.
- **Criar e apagar:** pelos comandos de script do próprio jogo (`CRunningScript::ProcessOneCommand`, `0x469EB0`): `0107`/`00A5`/`009A` criam, `0177`/`0175`/`0173` viram, `0550` congela o objeto, `020A` tranca o carro; para apagar, `03CA`/`056E`/`056D` conferem se ainda existe e `0108`/`00A6`/`009B` apagam.
- **Info ao apontar:** `CWorld::ProcessLineOfSight` (`0x56BA00`) pela câmera (`0xB6F028`), começando depois do personagem; a caixa vem do `CColModel` e vai para a tela com `CSprite::CalcScreenCoors` (`0x70CE30`).
- **Reset do device (Alt+Tab):** os alvos de render saem antes e as miniaturas são refeitas depois.

### Compilar e testar

- `build.sh`: MinGW i686 + ImGui 1.89.9 + MinHook + o kit da casa → `build/` e `dist/Trok DevTools.asi`.
- `test/smoke/run.sh`: roda o `build/Trok DevTools.asi` (rode o `build.sh` antes) no Wine 32 bits com Xvfb. O `host.c` é um `.exe` mínimo cuja `.bss` cobre os endereços do GTA, e o `fake_gta.cpp` monta ali o jogo de mentira: modelos, TXDs, colisões, `.ide`, `.img`, streaming, linha de visão e comandos de script. As capturas ficam em `test/out/smoke/`.
