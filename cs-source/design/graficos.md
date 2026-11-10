# Gráficos no Source SDK 2013 MP (2025, 64 bits): o que dá para melhorar

Pesquisa de 10/11/2026. São só sugestões: implementar quando o dono pedir. As páginas do VDC não abriram; o que vem
delas veio de trechos de busca. Os repositórios foram lidos direto, em `refs/` e em clones rasos.

## Três descobertas
1. **O SDK de 2025 permite trocar os shaders padrão.**
   - **O que vem pronto:** o código inteiro do `stdshader_dx9` (LightmappedGeneric, VertexLitGeneric, pele, olho, bloom,
     color correction, motion blur...) e um projeto de exemplo que gera uma DLL de shader do mod
     (`game_shader_generic_std_hl2mp.vpc`).
   - **Compilador:** já vem em `sdk/src/devtools/bin` (ShaderCompile2 e `process_shaders.ps1`).
   - **A confirmar no jogo:** se o motor prefere a DLL do mod à dele.
2. **O ramo TF2 traz melhorias de graça:**
   - lightmap bicúbico (`r_lightmap_bicubic 1`) e neblina radial;
   - texturas carregadas sob demanda e opção Vulkan (DXVK);
   - código do VRAD de 64 bits no `sdk/src/utils/vrad`.
3. **Dois mods no mesmo SDK, com a mesma licença, já estão em `refs/`:**
   - **Zombie Master Reborn:** shader PBR com parallax (`pbr_dx9.cpp`) e lanternas com sombra para vários jogadores;
   - **NEOTOKYO;REBUILD:** SSAO, motion blur e efeitos de tela.

## Melhores apostas (efeito ÷ esforço)
| # | Aposta | Ganho | Esforço | Desempenho | Primeiro passo |
|---|---|---|---|---|---|
| 1 | **Sem código:** HDR + `env_tonemap_controller`, `color_correction` com LUT sóbria por mapa, `r_lightmap_bicubic 1`, MSAA 4x, neblina radial, `buildcubemaps` em todo mapa | alto | baixo | baixo | Num mapa do CS:S: tonemap + LUT (ferramenta `colorcorrectionui`), compilar com HDR, comparar prints |
| 2 | **VRAD de qualidade:** `-final`/`-extrasky 16`, `-softsun`, `-StaticPropLighting -StaticPropPolys`, `-textureshadows`, `$lightmapscale` 4–8 nas superfícies principais | alto (a maior diferença entre um mapa "cru" e um bonito) | baixo | zero no jogo | Pôr essas opções no `mapa_compilar` do MCP `trok` |
| 3 | **Materiais:** `$phong`, `$phongfresnelranges`, `$rimlight`, `$envmap` + `$envmapmask` em armas e personagens; `$detail`, normal e ssbump no mundo | alto | médio (arte) | baixo | Refazer o VMT de uma arma (conversão PBR→Phong: FastValveMaterial, MIT) |
| 4 | **DLL de shader do mod + PBR do ZMR** (com parallax) | alto de perto | médio (1–3 dias) | médio | Copiar `pbr_*` do ZMR para um `game_shader_dx9_trok.vpc`, compilar, testar numa arma |
| 5 | **Pós-processamento:** SSAO do NT;RE, nitidez (AMD CAS, MIT), vinheta e grão leve | médio | baixo a médio | médio | Portar `Neo_SSAO_*` + o trecho de `viewpostprocess.cpp` com `#ifdef TROK` |
| 6 | **Sombra do sol em tempo real** (projetor ortográfico que segue o jogador, ideia do "Fake CSM") | alto em mapa aberto | alto | alto | Protótipo com um projetor e `r_flashlightdepthres 4096` |
| 7 | Iluminação deferred | muito alto | muito alto (troca a luz pré-calculada) | alto | não agora |

## O que o Combat Arms Reloaded mudou
- **O que foi:** reforma do mesmo motor LithTech, com texturas maiores, "iluminação melhor" e sombras em tempo real. O motor
  clássico continuou como opção para PC fraco.
- **O que não se confirmou:** HDR, AO e PBR.
- **Para o Rearmed:** as apostas 1, 2, 3 e 6 cobrem isso. Só a sombra do sol em tempo real custa caro no Source.

## Detalhes por tema
- **PBR:**
  - **ZMR:** 64 bits e ativo; é o caminho mais curto.
  - **thexa4/source-pbr:** a origem do PBR do ZMR; serve de documentação.
  - **LUX:** pacote grande, mas o repositório principal sumiu da busca.
  - **Mapbase:** porte para o MP 2025 ainda em rascunho (PR #409).
- **Ferramentas de textura:** podem ser usadas como ferramenta (PBR-2-Source é GPL, então não copiar código dele).
- **Sombras:** `env_projectedtexture` com sombra (só uma por padrão). O ZMR limita e escolhe as lanternas com sombra.
  Fake CSM (sem licença declarada; serve de referência de técnica).
- **Pós:**
  - **Já no SDK:** color correction, tonemap, bloom, motion blur e grão.
  - **SSAO:** NT;RE (64 bits) e Estranged.
  - **Antisserrilhado:** o MSAA nativo é melhor que FXAA; TAA não é viável no DX9 do Source (não tem vetores de movimento).
- **Mapas:** VRAD++ e VVIS++ (ficool2) são rápidos, mas redistribuir pede permissão. Hammer++ (versão TF2) melhora o
  editor. O VBSP do Slammin' não funciona no ramo TF2.
- **Shaders:** ShaderCompile2 com `-ver 20` ou `-ver 30`. O ps_3_0 tem 224 constantes e 16 samplers. A profundidade sai do
  alfa do framebuffer, com pouca precisão.

## Não usar (código vazado ou origem duvidosa)
- **Vazados:** quase todo `c_env_cascade_light.cpp` do GitHub (vem do CS:GO vazado: cstrike15_src, Kisak-Strike, csgo-src,
  SourceSDK2013Ports); Nhqdfy/...Cascaded-Shadow-Maps (vazamento do TF2 2018); etkramer/VRADPort (provavelmente do CS:GO).
- **Origem incerta:** SourceRevitalize, REDSource, CaliberSource.
- **Só referência visual:** Black Mesa (Xengine) e Strata Source, motores fechados.
