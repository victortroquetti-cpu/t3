# Minha TV: tela inicial para a Samsung UN46F6400 (2013)

Uma tela limpa e rápida para a sua Samsung: relógio, clima, atalhos para os apps,
canais ao vivo, os filmes e séries do seu PC (Jellyfin), filmes em alta e a previsão da
semana. Sem anúncios, sem root e sem mexer no sistema da TV.

![Prévia da tela inicial no navegador do PC, com pôsteres de exemplo](docs/previa.png)

## Por que não é igual ao dos vídeos

Os vídeos usam TVs LG (webOS), Android/Google TV ou Samsung de 2017 em diante (Tizen).
A sua é uma **Samsung série F de 2013**, que roda um sistema mais antigo, o **Orsay**.
O [Free My TV](https://github.com/simbasjungles/free-my-tv) avisa que não funciona nas
Samsung antes de 2017. Por isso este projeto é um app feito sob medida para a sua TV,
com o mesmo espírito.

**O que dá para fazer:**

- um app em tela cheia com relógio, clima, apps, filmes/séries em alta e previsão;
- abrir os apps da TV (YouTube, Netflix...) direto pelo Minha TV;
- ver detalhes de um filme e "onde assistir" no Brasil, com um botão para abrir o app;
- assistir canais ao vivo que você escolher (veja [Canais ao vivo](#canais-ao-vivo));
- assistir os filmes e séries do PC, com "continuar de onde parou" (veja [Meus vídeos](#meus-vídeos-jellyfin)).

**O que não dá, numa TV de 2013 sem root:**

- trocar a tela do Smart Hub ou fazer o app abrir sozinho quando a TV liga;
- voltar para o Minha TV quando você sai de outro app: a TV volta para o Smart Hub.

O Minha TV fica em *Smart Hub → Apps → Mais Apps*. Depois de aberto uma vez, ele aparece
nos apps recentes do Smart Hub.

## Como funciona

```
 PC (cabo de rede)                                    TV (Wi-Fi, mesmo roteador)
 ┌─────────────────────────┐   1. instala o app     ┌──────────────────────┐
 │ servidor/servidor.py    │ ─────────────────────► │ Smart Hub "develop"  │
 │  • monta o MinhaTV.zip  │   2. clima, filmes,    │  └ app Minha TV      │
 │  • busca clima e TMDB   │      canais e vídeos   │                      │
 │  • repassa o vídeo      │ ◄───────────────────── │                      │
 └─────────────────────────┘                        └──────────────────────┘
```

A TV de 2013 nem sempre consegue acessar sites modernos (HTTPS novo), por isso o PC
busca o clima, os filmes e o vídeo e repassa para a TV. **Com o PC desligado, o app continua
abrindo**: o relógio e os apps funcionam, e o clima tenta ir direto ao Open-Meteo.

## Passo 1: baixar o projeto no PC

Com o Git:

```
git clone https://github.com/victortroquetti-cpu/t3.git
cd t3
git checkout claude/nice-babbage-ba1vo4
cd tv-samsung
```

Ou baixe o ZIP pelo GitHub (botão *Code → Download ZIP*, no branch `claude/nice-babbage-ba1vo4`)
e abra a pasta `tv-samsung`.

## Passo 2: ligar o servidor no PC

Você precisa do **Python 3** (gratuito).

- **Windows:** dê dois cliques em `INICIAR-WINDOWS.bat`. Se ele disser que não achou o
  Python, instale pelo [python.org](https://www.python.org/downloads/) (marque *Add
  python.exe to PATH*) ou rode `winget install Python.Python.3.12` no PowerShell.
  Quando o Windows perguntar sobre o firewall, **permita em redes privadas**.
- **macOS:** `./iniciar.sh` no Terminal.
- **Linux:** `sudo ./iniciar.sh` (a porta 80 pede administrador).

A janela mostra o **IP do PC** (algo como `192.168.0.10`). Anote esse número e **deixe a
janela aberta**.

Para ver a tela no navegador do PC antes de ir para a TV, abra `http://<IP do PC>/app/`.
Use as setas, Enter (OK), Backspace (RETURN) e a tecla R (botão vermelho).

## Passo 3: entrar como "develop" na TV

> Vai fazer a limpeza do Smart Hub (veja [Otimização](#otimização))? Faça **antes** deste
> passo: ela apaga o login "develop" e os apps instalados por ele.

É o "modo desenvolvedor" das Samsung de 2013. Não muda nada no sistema e dá para
desfazer.

1. Aperte **MENU** no controle e vá em **Recursos Smart** (*Smart Features*).
2. Abra **Conta Samsung** e depois **Iniciar sessão** (*Log In*).
3. No e-mail, digite **`develop`**. Deixe a senha **em branco**.
4. Confirme e saia dos menus.

## Passo 4: instalar o app (sincronizar)

1. Aperte **SMART HUB** no controle e vá em **Apps**.
2. Embaixo, escolha **Mais Apps** (*More Apps*).
3. No canto de cima, à direita, abra **Opções** (*Options*).
4. Escolha **Configuração de IP** (*IP Setting*) e digite o IP do PC do passo 2.
5. Em **Opções** de novo, escolha **Iniciar sincronização de apps** (*Start App Sync*).

Na janela do servidor aparecem *"A TV pediu a lista de apps"* e depois *"A TV baixou o
app"*. A TV instala sozinha em alguns segundos.

> Os nomes dos menus podem variar um pouco conforme o idioma e a versão da TV.
> A sincronização substitui outros apps instalados pelo "develop". Os apps da loja
> Samsung (Netflix, YouTube...) não são afetados.

## Passo 5: abrir

**Smart Hub → Apps → Mais Apps → Minha TV.**

| Botão do controle | O que faz |
|---|---|
| Setas | Navegar |
| OK | Abrir o app, o filme ou o botão |
| RETURN | Fechar o detalhe do filme |
| EXIT | Sair do Minha TV |
| Vermelho (A) | Atualizar clima e filmes |
| INFO | Mostrar a versão e o IP do PC |
| Volume e mudo | Continuam com a TV |

Vendo um vídeo ou canal:

| Botão do controle | O que faz |
|---|---|
| Cima/baixo ou CH +/- | Trocar de canal (ao vivo) |
| OK, PLAY ou PAUSE | Pausar e continuar (vídeos do PC) |
| Esquerda/direita | Voltar 10 s / avançar 30 s |
| ⏪ / ⏩ | Voltar / avançar 1 minuto |
| RETURN ou STOP | Parar e voltar para a tela inicial |

## Personalizar

O servidor cria o arquivo `config.json` na primeira vez que roda:

```json
{
  "cidade": "São Paulo",
  "latitude": -23.5505,
  "longitude": -46.6333,
  "tmdb_chave": "",
  "tmdb_regiao": "BR",
  "apps": [
    { "nome": "YouTube", "id": "111299001912", "cor": "#c4302b" },
    { "nome": "Netflix", "id": "11101200001", "cor": "#b20710" }
  ],
  "canais": [],
  "canais_listas": [],
  "canais_pelo_pc": true,
  "jellyfin": { "endereco": "http://localhost:8096", "usuario": "", "senha": "" }
}
```

- **Cidade:** troque o nome, a latitude e a longitude (o Google Maps mostra as duas ao
  clicar no mapa).
- **Filmes e séries em alta:** crie uma conta grátis no [TMDB](https://www.themoviedb.org/signup),
  vá em *Configurações → API*, peça uma chave e copie o **Token de leitura da API** (ou a
  *Chave da API*) para `tmdb_chave`. A chave é sua: fica só no seu PC e nunca vai para o
  GitHub (o `config.json` está no `.gitignore`).
- **Apps:** cada app da TV tem um ID. Os de YouTube e Netflix acima são os conhecidos para
  as Samsung antigas. Para incluir outros, diga quais apps aparecem no seu Smart Hub.

Os canais e o Jellyfin têm seções próprias logo abaixo.

Mudou o `config.json`? Feche e abra o servidor de novo. A cidade e os apps mudam na
próxima vez que você abrir o Minha TV, **sem reinstalar**. Mudanças no visual ou no código
precisam de uma nova sincronização (passo 4, item 5).

## Canais ao vivo

O Minha TV toca transmissões em **HLS** (endereços que terminam em `.m3u8`), o formato que
as emissoras usam nos sites "ao vivo". Você escolhe os canais e coloca no `config.json`:

```json
"canais": [
  { "nome": "Nome do canal", "url": "https://endereço-da-transmissão/playlist.m3u8" },
  { "nome": "Outro canal", "url": "https://..." }
],
"canais_listas": [ "https://endereço-de-uma-lista.m3u" ]
```

- `canais`: um por linha, com nome e endereço.
- `canais_listas`: listas no formato M3U (até 300 canais por lista), se preferir.
- `canais_pelo_pc`: deixe `true`. O vídeo passa pelo PC, que resolve o HTTPS moderno que a
  TV não entende. Com `false`, a TV tenta abrir direto.

Use só transmissões **oficiais e gratuitas** (as próprias emissoras, TVs públicas e
legislativas). Para **Globo, SBT, Record, Band e os outros canais abertos**, o jeito
gratuito e legal é uma **antena digital**: a sua TV já tem o receptor, e o botão
**Ver TV** do Minha TV leva direto para ela.

## Meus vídeos (Jellyfin)

O [Jellyfin](https://jellyfin.org) é um servidor de mídia grátis: ele organiza os filmes e
as séries que estão no seu PC, com capa e sinopse, e o Minha TV mostra tudo em três
linhas: **Continuar assistindo**, **Próximos episódios** e **Filmes adicionados**.

1. Baixe o **Jellyfin Server** para Windows em [jellyfin.org/downloads](https://jellyfin.org/downloads)
   e instale.
2. Abra `http://localhost:8096` no navegador e siga o assistente: idioma português, crie um
   **usuário e senha** e adicione as pastas dos seus vídeos (Filmes, Séries).
3. No `config.json`, preencha:
   ```json
   "jellyfin": { "endereco": "http://localhost:8096", "usuario": "seu usuário", "senha": "sua senha" }
   ```
4. Reinicie o servidor do Minha TV.

O Jellyfin converte cada vídeo para o formato que a TV de 2013 toca (H.264 e som AAC, até
1080p), e o vídeo passa pelo servidor do Minha TV. Quando você para um vídeo, o Jellyfin
guarda onde parou. Converter usa o processador do PC; se travar, diminua a qualidade no
Jellyfin (*Painel → Reprodução*).

## Otimização

**Sem root, dá para fazer já:**

1. **Limpar o Smart Hub:** *MENU → Recursos Smart → Redefinir Smart Hub* (a senha padrão
   é `0000`). Apaga os apps que você baixou, as contas e o cache, e o Smart Hub volta como
   novo. Faça **antes** do passo 3.
2. **Apagar os apps que não usa:** no Smart Hub, selecione o app, segure OK (ou aperte
   TOOLS) até aparecer o menu e escolha **Excluir**. Os apps que vieram de fábrica (a loja Samsung Apps, por exemplo) não
   saem sem root.
3. **Desligar a atualização automática de software** (*MENU → Suporte → Atualização de
   software*). Isso mantém a TV como está, o que também protege o root.

Apagar apps libera espaço, mas **quase não libera memória RAM**: nesta TV, app fechado não
roda em segundo plano. Quem ocupa a memória é o próprio sistema e o Smart Hub. O ganho de
velocidade vem de usar o Minha TV no lugar do Smart Hub e de limpar o que o sistema carrega
ao ligar, e esta segunda parte precisa de root.

**Com root (SamyGO):** dá para ver quais programas ocupam a memória, tirar apps de fábrica
como a loja, bloquear os servidores antigos da Samsung que a TV fica esperando e fazer o
Minha TV abrir ao ligar. Isso é a próxima etapa.

## Se algo der errado

- **A sincronização falha ou a TV não acha o PC:**
  - O PC e a TV precisam estar no mesmo roteador. Rede de convidados não serve, nem um
    roteador que isola os aparelhos.
  - No Windows, a rede precisa estar como **Privada**, com o Python liberado no firewall.
  - Confira se o IP digitado na TV é o que a janela do servidor mostra, e se a janela
    continua aberta.
- **"Porta 80 em uso":** feche o programa que usa a porta (IIS, Skype antigo, outro
  servidor) e abra de novo.
- **O IP do PC mudou:** reserve o IP do PC no roteador, ou refaça o passo 4 com o IP novo.
- **O login "develop" não entra:** anote a mensagem exata da TV e me mande.
- **"Sem conexão com o PC" no topo do app:** o servidor está fechado ou o PC está desligado.
- **Os filmes não aparecem:** falta a chave do TMDB, a chave está errada, ou o PC está sem
  internet. A própria tela do app explica qual dos casos é.
- **Um canal não toca:** a transmissão pode estar fora do ar, ter mudado de endereço ou
  usar um formato que a TV de 2013 não toca (H.265 ou HLS muito novo). A janela do servidor
  mostra quando a fonte não responde. Teste o endereço no VLC do PC.
- **"Usuário ou senha do Jellyfin recusados":** confira o `config.json`. **"O Jellyfin não
  respondeu":** veja se o Jellyfin está aberto no PC (`http://localhost:8096`).

## Desfazer tudo

1. No Smart Hub, selecione **Minha TV**, abra as opções (botão TOOLS ou segure OK) e
   escolha **Excluir**.
2. Saia da conta "develop": **MENU → Recursos Smart → Conta Samsung → Sair**.

Nada mais na TV foi alterado.

## Para quem vai mexer no código

| Arquivo | O que é |
|---|---|
| [`app/index.html`](app/index.html) | A página do app (1280x720) |
| [`app/js/home.js`](app/js/home.js) | A tela: linhas, foco, clima, detalhe dos filmes |
| [`app/js/tv.js`](app/js/tv.js) | O que é da TV: teclas, abrir apps, volume |
| [`app/js/dados.js`](app/js/dados.js) | Os pedidos ao servidor do PC |
| [`app/js/player.js`](app/js/player.js) | O tocador de vídeo (canais e Jellyfin) |
| [`app/css/home.css`](app/css/home.css) | O visual |
| [`app/config.xml`](app/config.xml) | Nome, ícones e tamanho do app para o Smart Hub |
| [`servidor/servidor.py`](servidor/servidor.py) | O servidor do PC (instalação, clima, TMDB) |
| [`servidor/video.py`](servidor/video.py) | Canais ao vivo e o repasse do vídeo para a TV |
| [`servidor/jellyfin.py`](servidor/jellyfin.py) | A conversa com o Jellyfin |
| [`servidor/rede.py`](servidor/rede.py) | Acesso à internet e cache |
| [`ferramentas/verificar.py`](ferramentas/verificar.py) | Confere se o app vai rodar na TV antiga |
| [`ferramentas/gerar_imagens.py`](ferramentas/gerar_imagens.py) | Desenha os ícones |

As regras da TV antiga (JavaScript ES5, CSS simples, memória curta) estão em
[`CLAUDE.md`](CLAUDE.md). Antes de sincronizar uma mudança, rode
`python ferramentas/verificar.py`.

## Créditos

- O caminho de instalação (conta "develop" e sincronização de apps) foi documentado pela
  comunidade SamyGO e por projetos como [White Raven](https://github.com/silentmurdock/whiteraven)
  e [SamsungTV_IPTV](https://github.com/oasenov/SamsungTV_IPTV).
- A ideia e o visual se inspiram no [Free My TV](https://github.com/simbasjungles/free-my-tv) (MIT).
- Clima: [Open-Meteo](https://open-meteo.com/) (CC BY 4.0).
- Vídeos do PC: [Jellyfin](https://jellyfin.org) (GPL), que roda à parte no seu PC.
- Filmes e séries: este produto usa a API do TMDB, mas não é endossado nem certificado pelo
  [TMDB](https://www.themoviedb.org/).
- Não é afiliado à Samsung nem aos serviços de streaming. Os nomes dos apps pertencem aos
  seus donos.
