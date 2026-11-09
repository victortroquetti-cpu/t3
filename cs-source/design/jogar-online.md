# Jogar online com amigos

Tudo aqui é de graça. O Source é multijogador de fábrica: o mod já funciona online, os bots completam as vagas
e o menu já tem SERVIDORES (lista do motor) e CRIAR PARTIDA (janela do motor). **Ainda não testado em partida.**

## O que cada amigo precisa

- Steam com **Source SDK Base 2013 Multiplayer** e **Counter-Strike: Source** (o conteúdo provisório vem dele).
- A **mesma versão** do mod (o mesmo zip do GitHub Actions) em `steamapps\sourcemods\mod_trok`.

## Jeito 1: você hospeda (servidor local)

1. Abra o jogo, abra o console (tecla ') e digite `exec online`.
   - Isso desliga as trapaças do modo de teste, abre a partida para a internet e passa a valer em todos os
     mapas até você fechar o jogo.
   - Senha da sala: `sv_password "suasenha"` (ou edite `cfg/online.cfg`).
2. Comece a partida pelo menu (JOGAR, com ou sem bots).
3. Os amigos entram de um destes jeitos:
   - console: `connect SEU_IP` (e `password suasenha` antes, se tiver senha);
   - SERVIDORES > Favoritos, adicionando o seu IP;
   - pela lista de amigos da Steam, "Entrar no jogo" (a testar).

### Seu IP e o roteador

- **Com acesso ao roteador:** libere a porta **27015** (UDP e TCP) para o seu PC e passe aos amigos o seu IP
  público.
- **Sem acesso, ou a operadora não deixa** (CGNAT, comum em fibra no Brasil): use uma rede virtual grátis.
  Todos instalam a mesma rede, e os amigos usam o IP que ela mostra (`connect 26.x.x.x`):
  - Radmin VPN: grátis e simples, muito usado para jogar;
  - ZeroTier ou Tailscale: têm plano grátis.
  - Com essas redes, `sv_lan 1` também funciona.

## Jeito 2: servidor dedicado (fica ligado sem você jogar)

Roda num PC seu (grátis) ou num servidor alugado (pago). Passos, **a testar**:

1. Baixar o **SteamCMD** (grátis, da Valve) e instalar o servidor do SDK Base (app **244310**):
   `steamcmd +force_install_dir C:\rearmed_srv +login anonymous +app_update 244310 validate +quit`
2. O servidor também precisa dos mapas e modelos do CS:S. Instalar o servidor do CS:S (app **232330**) numa
   pasta ao lado e, no `gameinfo.txt` da cópia do servidor, trocar as linhas `|appid_240|cstrike` pelo
   caminho dessa pasta (o `|appid_240|` depende do cliente da Steam e pode não funcionar no dedicado).
3. Copiar `mod_trok` para dentro de `C:\rearmed_srv` e abrir (o executável é `srcds_win64.exe` ou
   `srcds.exe`, o que existir):
   `srcds_win64.exe -game mod_trok -console -port 27015 +maxplayers 12 +trok_modo equipe +servercfgfile online.cfg +map de_dust2`
4. Bots no dedicado: `hl2mp_bot_quota 8` e `hl2mp_bot_quota_mode fill` no console do servidor.
5. Liberar a porta 27015 (UDP e TCP), como no jeito 1.

## Tela dividida

Não dá. Essa versão do motor não tem tela dividida (só as versões do L4D2 e do Portal 2, que não são
abertas). Seria reescrever boa parte do motor.

## Arquivos

| Arquivo | Para quê |
|---|---|
| `cfg/listenserver.cfg` | o motor roda a cada mapa num servidor local; chama o `server.cfg` |
| `cfg/server.cfg` | modo de teste (trapaças ligadas) + balanceamento e movimento |
| `cfg/online.cfg` | partida online: mesma jogabilidade, sem trapaças, aberta na internet, com senha opcional |
