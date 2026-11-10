# Fontes abertas sobre o Combat Arms (além do DeCombat)

Pesquisa de 10/11/2026 (GitHub e busca na web). O que pode e o que não pode, conforme a regra 2 do `CLAUDE.md`.

## Pode usar
| Fonte | O que tem | Confiança |
|---|---|---|
| **Chimera** (github.com/r0pi0x/Chimera, `specs/combat_arms/`) | 32 specs do CA Classic: movimento, tiro, classes de arma, granadas, 22 modos, killstreak, bots, HUD, radar. O projeto diz que usou código descompilado e atributos descriptografados; o dono decidiu usar como referência (10/11/2026) | alta, mas escrito por IA e sem revisão humana |
| **DeCombat** (`design/decombat/`) | movimento, tiro, modos, radar, áudio e formatos do CA de 2009; código "clean room" (sem código vazado), MIT/Apache | alta |
| CAWiki (combatarms.fandom.com) | notas de 0 a 100 por arma, portabilidade, fôlego, modos, mapas | média (escala do site oficial, não RPM) |
| StrategyWiki (Combat_Arms/...) | as mesmas notas, alguns RPM medidos no jogo, recuo avaliado em teste, modos, mapas | média |
| Notícias antigas (Blue's News, Co-Optimus, Gaming Nexus, Ten Ton Hammer) | datas e regras de modo; confirma os **3 s parado** para o infectado regenerar; Cabin Fever (Fireteam) com 3 níveis e 4 tipos de infectado | boa |
| IMFDB (Combat Arms) | quais armas reais existem no jogo (nomes de arma real não são do CA) | boa |
| Ferramentas LithTech MIT (RezExtract, gorez, DAT2LTA85, io_scene_jupex, msLTBImporter, js-lithtech-parser) | formatos; servem só para `.rez` **abertos** do próprio jogador, dentro de `ca_local/` | — |

Os números já levantados estão em `numeros.md`, e as lições de design em `design-e-sensacao.md`.

## Não usar
| Fonte | Por quê |
|---|---|
| TechX1320/fireteam-fps | usa código-fonte da engine de origem provavelmente vazada, atributos "decrypted" e tenta quebrar a criptografia |
| jsj2008/lithtech, madeinserver/lithtech, nolffan/nolfsrc | código de engine e de jogo sem licença aberta (vazado) |
| Repositórios de cheat (Combat-Arms, CAScratch, combatarms-internal, CAClassic-Nightmare...) e "Rez Patcher" | cheat ou burla de proteção |
| Dezenas de repositórios "Combat Arms download/hack" | isca de malware: não abrir |
| packet loggers, raspadores de estatísticas, remakes em navegador com números chutados, sites com prints do jogo | sem valor ou com conteúdo do CA |

## Onde ainda dá para procurar
- **Wikis do CA:** as páginas inteiras da CAWiki e da StrategyWiki. Os hosts estão bloqueados na rede da nuvem; dá para liberar no ambiente "t3" ou ler no PC do dono.
- **namu.wiki (coreano):** não apareceu na busca.
- **Notas de patch da Valofe com números:** não apareceram na busca.
- **Fonte principal da Fase 3:** continua sendo o DeCombat mais a medição do dono no CA instalado.
