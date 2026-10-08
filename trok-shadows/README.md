# Trok Shadows — sombras para o GTA SA (1.0 US e SA-MP)

O **Shadows Extender 2.0** (DK22Pac, 2014) refeito com código aberto. Faz tudo o que ele fazia, com as mesmas chaves no INI, e corrige o que estava errado nele.

> **Estado:** versão 1.1, compilada e testada no Wine, sem o jogo: as 113 conferências do `test/run.sh` passam. A 1.0 já rodou no GTA; a 1.1 traz as correções para os dois problemas achados nela (ver [Versões](#versões)) e ainda não foi testada dentro do jogo.

## O que muda em relação ao Shadows Extender

- **Não precisa do `d3dx9_43.dll` nem dos `.fx`.** O original compilava os shaders ao abrir o jogo. Sem o DirectX de junho de 2010, o `.asi` nem carregava. Aqui os dois shaders já vêm montados dentro do `.asi`.
- **O INI vale na hora.** Salve o arquivo com o jogo aberto e cor, força, distâncias e os liga/desliga mudam no mesmo segundo. Só `MaxShadows`, `Raster*`, `Blur*` e `Gradient*` esperam o jogo abrir de novo.
- **Corrige o `DisplayShadowsAtLowSettings` do stencil.** O original lia a chave do `[REALTIME_SHADOWS]` duas vezes: a do `[STENCIL_SHADOWS]` nunca valia.
- **O shader só pinta a sombra em tempo real.** No original, qualquer efeito que enchesse o buffer de desenho do jogo podia sair com a cor da sombra.
- **Não trava com dados faltando.** Ele confere ponteiro nulo onde o original travava: pedestre sem modelo, câmera da sombra fechada.
- **Confere o jogo antes de mexer.** Só liga no `gta_sa.exe` 1.0 US e confere cada ponto antes de escrever. Se outro mod já desviou a mesma chamada, encadeia com ele em vez de apagar o gancho dele.
- **Avisa se o `shadows.asi` antigo também estiver na pasta.** Com os dois, um bagunçaria o outro, então o Trok Shadows fica desligado e mostra uma mensagem.
- **Corrige valores fora da faixa e avisa no log.** `CreateBlur1=0`, que travava o jogo, vira 1.
- **Não escurece dobrado no veículo.** Quem está dentro de um veículo com sombra em tempo real entra na sombra dele: uma sombra só. No original, piloto e moto tinham cada um a sua, e onde as duas se cruzavam ficava mais escuro.
- **As sombras não somem por falta de vaga.** O jogo tem 16 vagas de sombra em tempo real. O original pedia sombra para todo pedestre e veículo carregado, até os que estavam longe demais para a sombra aparecer: eles ocupavam as vagas e quem estava perto ficava sem sombra. Aqui só pede quem está dentro de `MaxDistance`, e só os `MaxRealTimeShadows` mais perto da câmera.
- **Liga e desliga a sombra em tempo real, a dos veículos e a arma na sombra** pelo INI, com o jogo aberto.
- **`MoreThanOnePlayer=auto`** liga sozinho quando o SA-MP está aberto.
- **Log** em `Trok Shadows.log`, ao lado do `.asi`, com tudo o que foi aplicado ou pulado.

## Instalar

1. Tire o `shadows.asi` da pasta do GTA. Pode deixar o `shadows.ini` na primeira vez.
2. Copie `dist/Trok Shadows.asi` para a pasta do GTA, ao lado do `gta_sa.exe`, ou para `scripts\`.
3. Abra o jogo. O mod cria o `Trok Shadows.ini` ao lado do `.asi`:
   - se achar o `shadows.ini` antigo na mesma pasta, copia os valores dele;
   - se não achar, usa os padrões do Shadows Extender.
4. Depois disso o `shadows.ini` e os `shadows_pixel*.fx` podem ser apagados.

## INI (`Trok Shadows.ini`)

As seções e chaves são as do Shadows Extender, mais quatro novas. O arquivo criado pelo mod explica cada uma. Um INI da versão 1.0 ganha as chaves novas sozinho, com os mesmos valores nas outras.

| Seção | Chave | O que faz | Jogo | Na hora? |
|---|---|---|---|---|
| `STENCIL_SHADOWS` | `MaxShadows` | sombras stencil ao mesmo tempo (16–4096) | 64 | reinicie |
| | `MaxDistance` | até quantos metros elas aparecem | 50 | sim |
| | `FlagIgnoreSomeShadows` | todos os objetos em todo quadro (o jogo reveza 1 em 4) | 0 | sim |
| | `DisableBuildingShadows` | tira a sombra stencil de objetos e prédios | 0 | sim |
| | `DisplayShadowsAtLowSettings` | sombra stencil com Sombras no baixo | 0 | sim |
| `STENCIL_SHADOWS_COLOR` | `R` `G` `B` `A` | cor e força do escurecimento do stencil | 0 0 0 50 | sim |
| `REALTIME_SHADOWS_COLOR` | `R` `G` `B` `A` | cor e força da sombra em tempo real (fora do modo combinado) | — | sim |
| `REALTIME_SHADOWS` | `DisplayShadowsAtLowSettings` | sombra em tempo real com Sombras no baixo | 0 | sim |
| | `CombineRealTimeShadowsWithStencil` | a sombra em tempo real entra no stencil: mesma cor e nada de escurecer dobrado | — | sim |
| | `MaxDistance` | até quantos metros elas aparecem | 15 | sim |
| | `CreateBlur1` `BlurLevel` | desfoque (CreateBlur1 fica sempre em 1) | 1, 4 | reinicie |
| | `CreateBlur2` `GradientMax` `GradientMin` | degradê | 1, 128, 64 | reinicie |
| | `RasterSize` `BlurRasterSize` `RasterSize2` `BlurRasterSize2` | resolução em potência de 2 (7 = 128 px, 10 = 1024 px) | 7, 6, 6, 6 | reinicie |
| | `ShadowBoundSphere` `ShadowBoundSphereInAir` | raio (m) em volta da sombra onde ela é projetada; InAir = helicóptero e avião | 2 | sim |
| | `ShadowZDistanceLimit` `ShadowZDistanceLimitInAir` | distância vertical até o chão | 4 | sim |
| | `ShadowSunZLimit` | altura mínima do sol (de noite a luz fica em cima) | — | sim |
| | `DrawVehicleDefaultShadowWithRealTime` | veículo com sombra em tempo real ganha também a sombra simples | — | sim |
| | `DisableVehicleDefaultShadow` | tira a sombra simples dos veículos | 0 | sim |
| | `EnableShadowsShader` | usa o shader do mod (cor própria e modo combinado) | — | sim |
| | `ShadowIntensityNightFactor` `ShadowIntensityCloudsFactor` | quanto da força sobra à noite e com nuvens | — | sim |
| | `MoreThanOnePlayer` | sombra em tempo real para todos os jogadores (`auto` = com o SA-MP) | 0 | sim |
| | `EnableRealTimeShadows` | sombra em tempo real ligada (0 = fica a sombra simples do jogo) — *nova* | — | sim |
| | `VehicleRealTimeShadows` | veículos com sombra em tempo real; quem está dentro entra na sombra do veículo — *nova* | — | sim |
| | `WeaponsInShadow` | arma, paraquedas e mochila a jato na sombra — *nova* | — | sim |
| | `MaxRealTimeShadows` | quantas sombras em tempo real ao mesmo tempo, as mais perto da câmera (1–16; padrão 12) — *nova* | 16 vagas | sim |
| `GERAL` | `recarregar` | aplica o INI quando você salva | — | sim |

A força final é `A × máx(1 − nuvens, fator de nuvens) × máx(1 − noite, fator de noite)`. Nuvens vem de `CWeather::CloudCoverage`. Noite vem do equilíbrio dia/noite dos prédios: 0 das 7h às 20h, 1 das 21h às 6h.

## O que o mod mexe no jogo

Todos os endereços são do `gta_sa.exe` 1.0 US. Os nomes vêm do [gta-reversed](https://github.com/gta-reversed/gta-reversed) e do [plugin-sdk](https://github.com/DK22Pac/plugin-sdk). O comportamento foi lido do binário do Shadows Extender.

**Sombras stencil** (objetos, prédios; e, no modo combinado, também as sombras em tempo real)

- **Pool:** `CStencilShadows::Init`, chamado em `CGame::Initialise` (`0x53BCAB`), é trocado por um que cria `MaxShadows` objetos no lugar dos 64 fixos.
- **Distância:** nove constantes de `RegisterStencilShadows` (`0x711827`–`0x711D22`) passam a apontar para a distância do INI.
- **Testes em `CStencilShadows::Process`:**
  - um objeto em cada quatro (`0x711E3D`);
  - a sombra dos objetos (`0x711E41`);
  - a sombra stencil dos veículos (`0x711E26`), sempre desligada: os veículos ganham sombra em tempo real no lugar.
- **Qualidade gráfica:** em `Process` (`0x711D9D`) e em `RenderStencilShadows` (`0x7113C0`).
- **Escurecimento:** o retângulo de tela inteira de `RenderStencilShadows` (`0x71167F`) passa a usar a cor e a força do INI.

**Sombras em tempo real** (pedestres e veículos)

- **Distância:** `MAX_DISTANCE_PED_SHADOWS` (`0x8D5240`) e o quadrado dela (`0xC4B6B0`).
- **Resolução, desfoque e degradê:** os `push` de `CRealTimeShadow::Create` (`0x7064C2`, `0x7064F9`) e de `CRealTimeShadowManager::Init` (`0x706810`–`0x706832`), mais `0x8D5218` e `0x8D521C`.
- **Veículos:** o `StoreShadowForVehicle` dos `PreRender` de cinco classes de veículo (`0x6ABCF5`, `0x6BD667`, `0x6C0B21`, `0x6C58A0`, `0x6CA73A`) passa a pedir também a sombra em tempo real. O teste da sombra simples fica em `0x70BDAB`.
- **Todo pedestre:** o pedido de sombra em `CPed::PreRenderAfterTest` sai do teste de qualidade e de veículo (`0x5E6664`, NOP em `0x5E68A2`). As cutscenes estão em `0x5B1F3C`.
- **Quem pede sombra:** só quem está dentro de `MaxDistance` (medida no plano, como o jogo mede), e só os `MaxRealTimeShadows` mais perto da câmera, pelo raio do quadro anterior. Quem já tem sombra ganha uma folga de ~14% no raio, para não perder a vaga na borda. O jogador e o veículo dele sempre pedem. Quem está dentro de um veículo com sombra em tempo real não pede: entra na sombra do veículo.
- **Arma, pistolas duplas, paraquedas, mochila a jato e quem está no veículo na sombra:**
  - `CShadowCamera::Update(RpClump*)` deixa a câmera aberta (NOP em `0x705C57` e `0x705C5F`; o `RpClumpForAllAtomics` dela, em `0x705C4A`, marca que a câmera ficou aberta);
  - em `CRealTimeShadow::Update` (`0x706676`), o mod desenha esses objetos na posição do osso da mão (ou das costas, no paraquedas), desenha a silhueta de quem está no veículo (motorista e até 8 passageiros) e fecha a câmera.
- **Luz segue o sol:** o jogo calcula a elevação da luz e não usa. O mod guarda essa elevação (`0x707E4F`) e a usa na rotação da luz (`0x70596A`). `ShadowSunZLimit` limita a altura do sol (`0x707E2B`).
- **Projeção:**
  - quadrado da sombra 2.15 no lugar de 1.5 (`0x707EF7`–`0x707F21`);
  - raio (`0x70A2C8`) e distância vertical (`0x707F2C`) do INI;
  - constantes do Shadows Extender em `CastShadowEntityXYZ` (`0x70A0C9`, `0x70A1AC`, `0x70A211`, `0x70A228`), copiadas como estão.
- **O que entra na sombra:** o LOD de veículo fica fora. No modo combinado, as hélices só fazem sombra onde são quase opacas (`0x705C4A`).
- **Shader:** cada sombra em tempo real projetada (`0x70AD0D`) é desenhada com o pixel shader do mod, e o buffer cheio também (`0x7082A4`, `0x7082BD`).
  - fora do modo combinado: `(1 − cor) × força × sombra`, escurecendo na direção da cor;
  - no modo combinado: só marca o stencil, e o retângulo do stencil escurece tudo junto.
- **Qualidade gráfica:** `0x706BCC` e `0x5E6766`.
- **SA-MP:**
  - `MoreThanOnePlayer` (`0x7069F5`);
  - quando o jogo termina de iniciar, o mod religa `CRealTimeShadowManager::Update` (`0x53EA08`, `0x706AB0`), que o SA-MP desliga, e confere de novo a cada quadro. Só mexe se cada byte for o do jogo ou o que o SA-MP põe (NOP, `ret`): o gancho de outro mod nesses lugares fica.

**Pontos de entrada:** depois de `CGame::InitialiseRenderWare` (`0x5BD779`), em `CGame::ShutdownRenderWare` (`0x53BC21`), quando o jogo termina de iniciar (`0x748CFB`) e a cada quadro (`0x53E981`: limite de sombras, atualização ligada e INI mudado).

**Diagnóstico no log** (uma vez cada): mais pedidos de sombra que vagas num quadro; uma sombra atualizada duas vezes no mesmo quadro (outro mod chamando `CRealTimeShadowManager::Update`); câmera aberta que não é a da sombra; atualização desligada de novo com o jogo aberto.

## Compilar

```
./build.sh
```

Precisa do MinGW i686 (`g++-mingw-w64-i686-posix`) e de mais nada. O `.asi` sai em `dist/`.

## Testes (sem o jogo)

```
test/run.sh
```

Precisa de `wine` (com `wine32`), `xvfb-run` e MinGW i686.

O `test/launcher.exe` ocupa a faixa de endereços do `gta_sa.exe` (`0x400000`–`0xD00000`). O `test/host.cpp` monta ali os pontos do 1.0 US que o mod confere, carrega o `.asi` e chama cada gancho com dados falsos. As funções do jogo viram gravadores. O teste confere:

- **Remendos:** cada byte escrito, com os valores do `test/shadows.ini` (o INI do Shadows Extender, importado na primeira vez).
- **Chamadas:** argumentos, ordem e convenção de chamada de cada gancho, comparados com o que o Shadows Extender fazia.
- **Assembly:** os trechos em assembly mantêm os registradores do jogo.
- **Shaders:** o device D3D9 do Wine aceita os dois.
- **INI na hora:** salvar o INI com o "jogo" aberto muda os valores e o liga/desliga.
- **Veículo:** a moto desenha o piloto e a garupa em silhueta na sombra dela, e eles não pedem sombra própria.
- **Limite:** de 20 veículos pedindo sombra, só os 12 mais perto ganham; quem já tem sombra não perde a vaga na borda; o veículo do jogador sempre ganha; além de `MaxDistance` ninguém pede.
- **SA-MP:** a atualização desligada de novo com o jogo aberto volta no quadro seguinte; o gancho de outro mod no mesmo lugar fica.
- **Outro exe:** o mod não escreve nada.
- **Conflito:** com o `shadows.asi` também carregado, o mod fica desligado e avisa.
- **INI da 1.0:** ganha as chaves novas, com os mesmos valores nas outras.

## Versões

- **1.1**
  - Piloto, garupa e passageiros entram na sombra do veículo: nada de escurecer dobrado onde as duas sombras se cruzavam (o Shadows Extender também fazia isso).
  - Para as sombras que piscavam e sumiam: só pede sombra quem está dentro de `MaxDistance`, e só os `MaxRealTimeShadows` mais perto. A atualização das sombras é religada se alguém a desligar de novo com o jogo aberto.
  - Chaves novas: `EnableRealTimeShadows`, `VehicleRealTimeShadows`, `WeaponsInShadow`, `MaxRealTimeShadows`.
  - Diagnósticos no log.
- **1.0** — primeira versão.

## Créditos

- Shadows Extender: DK22Pac.
- Nomes e estruturas do jogo: gta-reversed e plugin-sdk.
