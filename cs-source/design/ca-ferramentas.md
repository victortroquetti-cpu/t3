# Arquivos do Combat Arms: ferramentas e o que dá para usar

Pesquisa de 10/11/2026 para usar os assets do CA como **provisório pessoal** (regra 2 do `CLAUDE.md`):
- os arquivos ficam em `game/mod_trok/custom/ca_local/` (no `.gitignore`);
- cada pessoa extrai da própria instalação do jogo.

O CA roda no **LithTech Jupiter EX**, o mesmo motor do F.E.A.R. e, com mudanças, do CrossFire. Por isso, muitas
ferramentas feitas para o CrossFire também servem para o CA. **Falta testar nos arquivos de verdade**: a versão de
cada formato muda de jogo para jogo.

## Resultado do teste (10/11/2026, versão atual que o dono mandou)

- **Os 109 `.rez` são protegidos.** Todos começam com a assinatura `RezMgr`, por isso parecem abertos à
  primeira vista. Mas o cabeçalho foi alterado de propósito: os bytes de separação foram trocados e a posição
  do índice está codificada. Nenhuma ferramenta comum lê esses pacotes. O `tools/ca_extrator.py` detecta e pula
  esses pacotes com aviso.
- **Soltos, fora dos `.rez`, e usáveis como provisório pessoal:**
  - 61 músicas `.mp3` em `BGM/` (menu, modos, mapas);
  - 10 sons `.wav`;
  - 24 vídeos `.bik` (Bink; abertura e finais).
- **Não usáveis:** `.dll` e `.fxd` (código e efeitos do jogo).
- `tools/ca_extrator.py` continua servindo para versões ou instalações com `.rez` padrão (cabeçalho original).

## Resumo por formato

| Arquivo | O que é | Dá para usar? | Ferramenta | Até o Source |
|---|---|---|---|---|
| `.rez` aberto | pacote com tudo dentro | **sim** | RezExtract, gorez (MIT); vamos fazer o nosso em Python | extrair |
| `.wav` | sons | **sim, o mais fácil** | nenhuma (já é WAV) | converter para PCM 16 bits, 44,1 kHz se precisar, e apontar no `game_sounds_trok.txt` |
| `.dtx` | texturas, ícones, UI | **sim** | DTX→DDS (Kelthic, Python), DTX-EYES (Python, lida com LZMA), dtx2png (só sem compressão) | DTX → TGA/PNG → VTF (VTFEdit) + `.vmt` |
| `.ltb` | modelos (armas, personagens), com esqueleto e animações | **sim, com trabalho** | LTB2FBX (FBX + texturas), cf-model-exporter (Python: GLB/OBJ/FBX), LTB2SMD, plugin do MilkShape | LTB → FBX → Blender → (retarget ValveBiped) → SMD → QC → `.mdl` |
| `.dat` (mundo) | mapas compilados | **só como referência** | importador para Blender testado em mapas do CA (leoschur), DAT-Reader (exporta OBJ) | não converte direto para BSP: o mapa é **refeito no Hammer** por cima da geometria importada |
| atributos (texto ou banco de dados) | números de armas, modos e movimento | **sim** | leitura direta, se for texto | preencher os valores MEDIR nos cfg e scripts (Fase 3) |
| efeitos, shaders (`.fxo`), scripts | — | **não** | — | refazer no Source |

## Ferramentas encontradas

| Ferramenta | Faz | Licença | Observação |
|---|---|---|---|
| [no-lith/RezExtract](https://github.com/no-lith/RezExtract) | extrai `.rez` (vários de uma vez), converte DTX ao extrair | MIT | Windows |
| [K4rian/gorez](https://github.com/K4rian/gorez) | lê e extrai `.rez` | MIT | Go; boa referência do formato |
| [LRolinx/CFRezManager](https://github.com/LRolinx/CFRezManager) | gerenciador de `.rez` do CrossFire, com prévia | sem licença | ativo (nov/2026); testar com o CA |
| [YoungFine0825/LTB2FBX](https://github.com/YoungFine0825/LTB2FBX) | LTB → FBX, DTX → TGA, lida com LZMA | sem licença | ferramenta externa |
| [YESJINZ/cf-model-exporter](https://github.com/YESJINZ/cf-model-exporter) | LTB v9 e DAT → GLB/OBJ/FBX, decodifica DXT | sem licença | Python puro; feito para o CrossFire |
| [giaynhap/LTB2SMD](https://github.com/giaynhap/LTB2SMD) | LTB → SMD da Valve, com animações | sem licença | antigo (2017); caminho mais curto para o Source, se funcionar |
| [crskycode/msLTBImporter](https://github.com/crskycode/msLTBImporter) | importa LTB no MilkShape 3D | MIT | — |
| [Kelthic/LithTech_DTX_Convertor](https://github.com/Kelthic/LithTech_DTX_Convertor) | DTX → DDS em lote | sem licença | Python |
| [nnerdcs/DTX-EYES](https://github.com/nnerdcs/DTX-EYES) | DTX v2 → DDS, detecta LZMA | AGPL-3.0 | testado no CrossFire |
| [leoschur/blender-lithtech-dat-import](https://github.com/leoschur/blender-lithtech-dat-import) | importa mapa `.dat` (versão 85) no Blender | sem licença declarada | **feito e testado com mapas do Combat Arms**; texturas precisam virar TGA antes |
| [burmaraider/DAT-Reader](https://github.com/burmaraider/DAT-Reader) | visualizador de `.dat`, exporta OBJ | sem licença | AVP2 e NOLF; CA não confirmado |
| [jerry4718/js-lithtech-parser](https://github.com/jerry4718/js-lithtech-parser) | lê DTX, LTB e DAT | MIT | Node.js; boa referência dos formatos |
| [bibendovsky/ltjs](https://github.com/bibendovsky/ltjs) | porte do motor LithTech Jupiter | — | referência, não ferramenta |

## Plano do extrator (`cs-source/tools/ca_extrator.py`, a fazer)

Código **nosso**, com base no que os projetos abertos documentam do formato:
1. Lê a pasta do CA instalado (ou de um zip) e **pula** os `.rez` criptografados, avisando quais.
2. Extrai para `game/mod_trok/custom/ca_local/` mantendo as pastas.
3. Converte: WAV → PCM 16 bits; DTX → TGA → VTF (+ `.vmt` simples).
4. LTB: chama um conversor externo (LTB2FBX ou cf-model-exporter) se estiver instalado. Armas e personagens
   ainda passam pelo Blender (retarget para ValveBiped, QC, `modelo_compilar` do MCP `trok`).
5. Gera um índice (`ca_local/indice.txt`) do que foi extraído, para escolher o que vira provisório.

Ordem de valor: **sons primeiro** (troca imediata e grande ganho de vibe), depois texturas e ícones, depois armas
(modelo + animação de primeira pessoa, que é o mais trabalhoso), e mapas só como referência para refazer no Hammer.
