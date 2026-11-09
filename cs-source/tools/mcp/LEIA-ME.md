# trok-mcp: o agente testando o jogo

Servidor MCP que deixa o Claude Code (ou o Codex) **abrir o Rearmed, mandar comandos, ler o console e
os erros, tirar print da tela** e compilar modelos e mapas. Com ele, o agente testa quase sozinho: abre um
mapa, coloca bots, troca de modo, lê o que deu errado e olha a tela. Você joga e conta o que sentiu.

Só precisa do Python 3.8+ (nada para instalar com pip). Arquivo: `cs-source/tools/mcp/trok_mcp.py`.

## Ligar

**Claude Code**: o repositório já tem `.mcp.json` na raiz. Abra o Claude Code na pasta `t3` e aprove o
servidor "trok" quando ele perguntar (ou confira com `/mcp`). Se o comando `python` não abrir o Python
certo no Windows, troque `"python"` por `"py"` no `.mcp.json`.

**Codex**: em `%USERPROFILE%\.codex\config.toml`, com o caminho do seu clone:

```toml
[mcp_servers.trok]
command = "python"
args = ['C:\caminho\do\t3\cs-source\tools\mcp\trok_mcp.py']
```

Primeiro teste: peça ao agente "rode o diagnostico do trok". Ele mostra se achou a Steam, o SDK Base, o mod,
o executável e as ferramentas de compilação.

## Ferramentas

| Ferramenta | O que faz |
|---|---|
| `diagnostico` | caminhos detectados, se o jogo está aberto e se o console remoto conecta |
| `jogo_abrir` | abre em janela com `-dev -console -condebug -netconport 2121` (opcional: já num mapa) |
| `jogo_fechar` | fecha o jogo |
| `comando` | roda comandos no console e devolve a saída (`trok_modo surto; map de_dust2`) |
| `console` | últimas linhas do console, com filtro |
| `erros` | erros e avisos da sessão, agrupados e contados (som, modelo, material, comando) |
| `print_tela` | tira print e devolve a imagem para o agente olhar |
| `estado` | mapa, jogadores, modo, regras, bots com nível e estilo |
| `cvar` | lê ou muda uma ConVar e confirma o valor |
| `partida_teste` | começa partida com bots, igual ao INICIAR do menu (modo, mapa, nível, bots por time) |
| `modelo_compilar` | compila um `.qc` com o studiomdl do SDK Base |
| `mapa_compilar` | compila um `.vmf` (vbsp, vvis, vrad), avisa se vazou e copia o `.bsp` para o mod |

## Como fala com o jogo

1. **netcon**: o motor abre um console remoto com `-netconport 2121` (`jogo_abrir` já passa isso). Se você
   abrir o jogo pela Steam, ponha `-netconport 2121 -condebug` nas opções de inicialização.
2. **RCON** (se o netcon não funcionar neste SDK): no jogo, `rcon_password algumasenha`; no ambiente do
   agente, `TROK_RCON_SENHA=algumasenha`. O servidor usa RCON quando o netcon não conecta.
3. **console.log**: com `-condebug`, o jogo grava tudo em `mod_trok\console.log`. A ferramenta `erros` lê
   esse arquivo, então funciona mesmo sem netcon nem RCON.

Ainda não foi testado com o jogo de verdade (foi testado com um jogo falso que imita o netcon e o RCON).
O ponto a confirmar no primeiro uso é se o SDK de 2025 aceita `-netconport`.

## Configuração (opcional)

O servidor acha tudo sozinho pelo registro da Steam e pelas bibliotecas. Se algo estiver em outro lugar,
defina no `.mcp.json` (campo `"env"`) ou no `config.toml` do Codex (`env = { ... }`):

| Variável | Padrão |
|---|---|
| `TROK_STEAM` | pasta da Steam (registro do Windows) |
| `TROK_SDK_BASE` | `steamapps\common\Source SDK Base 2013 Multiplayer` |
| `TROK_MOD_DIR` | `steamapps\sourcemods\mod_trok` |
| `TROK_JOGO_EXE` | `hl2_win64.exe` ou `hl2.exe` do SDK Base; sem eles, usa `steam -applaunch 243750` |
| `TROK_NETCON_PORTA` / `TROK_NETCON_SENHA` | `2121` / nenhuma |
| `TROK_RCON_HOST` / `TROK_RCON_PORTA` / `TROK_RCON_SENHA` | `127.0.0.1` / `27015` / nenhuma |

## Blender (o seu Blender MCP)

Não precisa trocar o Blender MCP que você usa nos mods de GTA. Falta só o lado do Source:

1. Instale o add-on **Blender Source Tools** no Blender. Com ele, o agente exporta SMD/DMX pelo próprio
   Blender MCP (a ferramenta que roda código no Blender chama a exportação do add-on) e compila com
   `modelo_compilar`.
2. **Escala é a maior diferença do GTA.** O GTA trabalha em metros; o Source, em unidades. O jogador tem
   **72 unidades** de altura (1 unidade ≈ 1,9 cm na escala de personagem). Um modelo feito em metros sai
   minúsculo: ajuste a escala da exportação ou modele direto em unidades do Source. Para conferir, use as
   medidas de `design/medidas-mapas.md`.
3. **Eixos**: os dois usam Z para cima. No Source, a frente de um modelo é **+X**.
4. **Coisas que o Source pede e o GTA não**: malha de colisão separada (`$collisionmodel`, em peças
   convexas), materiais `.vmt` com texturas `.vtf` (VTFEdit), esqueleto `ValveBiped` para personagem e para
   arma na mão (para usar as animações do HL2MP) e o `.qc` que junta tudo.
5. Um modelo que veio do GTA **não pode** entrar no jogo (regra de assets). O pipeline serve para os
   modelos próprios e para os CC0 da pesquisa (Quaternius, Poly Haven).
