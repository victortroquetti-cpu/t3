# Trok Headmove

O `/headmove` do SA-MP 0.3.7 R1 (a cabeça do personagem virando para onde a câmera olha) **sem o atraso do SA-MP**. O atraso fica no `Trok Headmove.ini`, em milissegundos, e pode ser zero: a cabeça vira junto com a câmera.

**Estado:** v1.0, `.asi` compilado e testado fora do jogo (Wine, com um GTA e um samp.dll de mentira que repetem a lógica do SA-MP: `test/smoke/run.sh`, 9 cenários). Falta o teste no jogo de verdade.

## Instalar

1. Copie `dist/Trok Headmove.asi` para a pasta do GTA (a mesma dos outros `.asi`; precisa do ASI Loader).
2. O `Trok Headmove.ini` é opcional: sem ele, o mod cria um com os padrões (atraso 0) na primeira vez. Se quiser ajustar antes de abrir o jogo, copie também `dist/Trok Headmove.ini`.
3. Entre num servidor. O `/headmove` do SA-MP precisa estar ligado (ele vem ligado; o comando liga e desliga).

Precisa do `gta_sa.exe` 1.0 US e do SA-MP 0.3.7 R1. Em outra versão o mod fica desligado sem mexer em nada, e o `Trok Headmove.log` diz o motivo.

## De onde vem o atraso

O SA-MP faz o headmove em duas etapas, para o seu personagem e para cada jogador perto de você:

1. **Uma vez por segundo**, ele guarda para onde a câmera olha. No seu personagem é a sua câmera; nos outros, a mira que chega pela rede (o aim sync).
2. **A cada 200 ms**, ele pede ao GTA que a cabeça olhe para lá. O pedido usa o giro mais lento do GTA, 0,1 por quadro: a cabeça anda 10% do caminho a cada quadro.

Somando as duas, a cabeça chega entre ~0,4 s e ~1,7 s depois de você virar a câmera. No teste, um giro de 90° levou 1,3 s.

## O que o mod muda

O SA-MP continua decidindo **quando** a cabeça segue a câmera: com o `/headmove` ligado, vivo, fora de cutscene. O mod só muda **como**:

- todo pedido do SA-MP sai com a direção de **agora** (a sua câmera ou a mira atual do outro jogador), não com a de até 1 s atrás;
- o giro do GTA vira o do `.ini` (1.0 = na hora);
- enquanto o SA-MP estiver pedindo, o mod repete o pedido **todo quadro**, e não a cada 200 ms. Quando o SA-MP para de pedir (`/headmove` desligado, por exemplo), o mod para junto em até 0,4 s.

Com `atraso_ms` acima de 0, a direção é suavizada no tempo: a cabeça chega a ~95% da direção nova no tempo escolhido, igual em qualquer FPS.

## Trok Headmove.ini

Uma seção `[headmove]`. **Mudou e salvou com o jogo aberto, vale na hora:** o mod confere o arquivo a cada segundo e avisa no chat (`[Trok Headmove] atraso 0 ms, outros 100 ms, giro 1.00`).

| Chave | Padrão | Faixa | O que faz |
|---|---|---|---|
| `ligado` | `1` | 0 ou 1 | `0` devolve tudo ao jeito do SA-MP, inclusive o giro da cabeça que já estava virando |
| `atraso_ms` | `0` | 0 a 3000 | seu personagem: tempo para a cabeça chegar onde a câmera olha. `0` = na hora |
| `outros_jogadores` | `1` | 0 ou 1 | `1` = o mod cuida também da cabeça dos outros (como **você** os vê); `0` = como o SA-MP faz |
| `atraso_outros_ms` | `100` | 0 a 3000 | atraso da cabeça dos outros. A mira deles chega pela rede aos pedaços; um pouco de atraso deixa o giro liso |
| `giro` | `1.0` | 0.05 a 1.0 | quanto a cabeça anda por quadro até o alvo. `0.1` é o do SA-MP. Com atraso 0, deixe `1.0` |

Número com vírgula também vale (`giro=0,5`). Valor fora da faixa vai para a borda, e chave apagada volta ao padrão.

## Limites

- **É só na sua tela.** Na tela dos outros, a sua cabeça continua com o atraso do SA-MP deles, a não ser que eles também usem o mod.
- **A cabeça dos outros depende da rede.** O mod usa a mira deles assim que ela chega, mas ela chega no ritmo do aim sync. Com `atraso_outros_ms=0`, a cabeça deles vai aos saltos, um a cada pacote.
- O GTA só vira a cabeça de quem está vivo, aparece na tela e está a até 20 m da câmera (sem limite de distância com a luneta da sniper). A regra é do GTA e vale igual com ou sem o mod.

## Trok Headmove.log

Ao lado do `.asi`, um por sessão de jogo:

- versões conferidas e ganchos instalados, com os valores do `.ini`;
- o **primeiro pedido do SA-MP** para o seu personagem e para outro jogador, com o que o SA-MP pediu (duração, giro, transição). É a prova de que o mod está pegando os pedidos certos no seu SA-MP;
- 15 s depois: quantos pedidos do SA-MP foram ajustados e quantos o mod repetiu, por segundo;
- cada vez que o `.ini` muda.

Se a cabeça não mudar em nada, mande esse arquivo.

## Como funciona (para quem mexe no código)

Três ganchos com o MinHook, todos só no processo do jogo:

| Onde | Endereço | Para quê |
|---|---|---|
| `IKChainManager_c::LookAt` (gta_sa.exe 1.0 US) | `0x618970` | o pedido `"FollowPedSA"` que o SA-MP faz de dentro do `ProcessHead` sai com a direção de agora e o giro do `.ini`; qualquer outro pedido do GTA passa direto |
| `CLocalPlayer::ProcessHead` (samp.dll R1) | `+0x2F80` | um passo da suavização antes do SA-MP; depois dele, repete o pedido se o SA-MP não pediu neste quadro |
| `CRemotePlayer::ProcessHead` (samp.dll R1) | `+0x10EA0` | o mesmo para cada jogador remoto, com a mira dele (`AimStuff::GetAim(m_nPlayerNumber)`, `+0x98390`) |

- A sua direção vem de `AimStuff::RefInternalAim` (`+0xF1590`), a mesma câmera que o SA-MP lê.
- O giro fica na corrente de IK do GTA, que nasce no primeiro pedido e não muda nos seguintes. Por isso o mod também troca o `m_Speed` (`+0x44`) da corrente `LOOK_AT` (`+0x55` = 0) desse personagem, nas 32 correntes do `g_ikChainMan` (`0xC15448`, 0x58 cada).
- A transição do GTA fica entre 150 e 500 ms. Abaixo de um quadro, a conta de transição do GTA anda para trás.

Os deslocamentos do SA-MP R1 saíram da SAMP-API (MIT, BlastHack), compilada e impressa: `CPed::m_pGamePed` `+0x2A4`, `m_nPlayerNumber` `+0x2B0`, `m_pPed` `+0` nos dois jogadores. A lógica do `ProcessHead` (1 s / 200 ms / giro 0.1 / transição 500 ms) vem da decompilação aberta do SA-MP 0.3.7 R5 (dashr9230/SA-MP), e o log confere esses números no seu R1. O `LookAt` e as correntes de IK vêm do gta-reversed.

## Compilar e testar

```
./build.sh              # dist/Trok Headmove.asi (MinGW i686; usa o MinHook de ../trok-ui/asi/third_party)
test/smoke/run.sh       # Wine 32 bits, sem janela
```

O teste gira a câmera 90° de uma vez e mede quanto a cabeça leva para ficar a menos de 5° da direção nova:

| Cenário | O que confere | Resultado |
|---|---|---|
| `sa-mp` | sem o mod (a referência) | 1321 ms, 4,7 pedidos/s, giro 0.1 |
| `zero` | sem `.ini`: cria o arquivo com os padrões; atraso 0 | 15 ms (1 quadro), 66 pedidos/s, giro 1.0 na corrente; as outras correntes de IK intactas |
| `atraso-300` | `atraso_ms=300` | 319 ms |
| `outros` | outros com 100 ms; um jogador sai e volta com outro personagem | os outros em 106 ms; quem voltou em 15 ms |
| `outros-desligado` | `outros_jogadores=0` | o seu em 15 ms, os outros em 1353 ms com o giro 0.1 do SA-MP |
| `recarregar` | o `.ini` muda para `ligado=0` com o jogo rodando | aviso no chat; de volta a 4,6 pedidos/s e giro 0.1; 1200 ms |
| `headmove-off` | o `/headmove` é desligado no meio | o mod para de pedir junto (0 pedidos) |
| `outra-versao` | samp.dll R3 | nada instalado, a cabeça como no SA-MP |
| `gta-errado` | gta_sa.exe sem a marca do 1.0 US | nada instalado |
