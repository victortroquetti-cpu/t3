# Minha TV: notas para o Claude

App de tela inicial para uma **Samsung UN46F6400 (série F, 2013)**: firmware
`T-MST12UABC-2142.0`, Smart Hub 5.3351. A plataforma é **Orsay**, o "Samsung Smart TV
Legacy Platform". Não é Tizen: nada do Free My TV/Tizen/TizenBrew/sdb serve aqui. A TV está
no Wi-Fi e o PC do usuário está no cabo, no mesmo roteador. O usuário fala português
(pt-BR) e não é técnico: respostas curtas e diretas, e quem mexe na TV é ele.

## Como chega na TV

1. A TV fica logada como `develop` (Menu → Recursos Smart → Conta Samsung, senha vazia).
2. No PC roda `servidor/servidor.py` (stdlib pura, porta **80**: a TV só sincroniza por ela).
3. Na TV: Smart Hub → Apps → Mais Apps → Opções → Configuração de IP = IP do PC → Opções →
   Iniciar sincronização de apps. A TV baixa `http://IP/widgetlist.xml` e depois `MinhaTV.zip`.
4. O servidor monta o zip na hora, a partir de `app/`, e injeta um `js/config.js` gerado
   (endereço do PC, cidade, apps e se há chave do TMDB). **Nunca** coloque a chave do TMDB
   no zip nem em `/api/config`.
5. Mudou código ou visual? É preciso sincronizar de novo. Mudou só o `config.json` (cidade,
   apps)? Basta reiniciar o servidor: o app lê `/api/config` toda vez que abre.

Uma sessão na nuvem não alcança a rede do usuário. Instalar e testar na TV de verdade é com
ele (ou com um Claude rodando no PC dele).

## Regras da TV antiga (não quebre)

- **JavaScript ES5 puro**: sem `let`/`const`, arrow, crase, `class`, spread. Também sem
  `.bind`, `classList`, `fetch`, `Promise`, `addEventListener`, `requestAnimationFrame`,
  `.trim()`, `toLocale*`, `localStorage`. O `ferramentas/verificar.py` confere (acorn
  `--ecma5` via npx quando houver Node).
- **CSS simples**: posições absolutas, `rgba`, `border-radius` com `-webkit-`. Sem flexbox,
  grid, `var(--)`, transições nem sombras pesadas. Tela fixa de **1280x720** (`config.xml`
  e `widget.info`).
- **Acentos**: pode escrever normalmente. No zip, o servidor troca os acentos por `\uXXXX`
  no JS e por `&#N;` no HTML, e tira os comentários do CSS. O CSS deve ter só ASCII fora
  dos comentários.
- **Memória**: um XHR por "canal", com `destroy()` no anterior (`dados.js`); pôsteres só
  perto do foco; `innerHTML` só quando os dados mudam. O app é fechado se travar por mais
  de 20 s (watchdog), então nada pode ser síncrono e longo.
- **Teclas**: vêm de `Common.API.TVKeyValue` na âncora `#ancora` (`onkeydown`). Use
  `TV.acao(keyCode)`, que já devolve `esquerda/direita/cima/baixo/ok/voltar/sair/vermelho...`.
  RETURN precisa de `TV.segurarTecla(evento)` (`blockNavigation`) para não fechar o app.
  Volume e mudo são devolvidos à TV em `window.onShow`.
- **Abrir outro app**: `widgetAPI.runSearchWidget(id, '')` (`TV.abrirApp`). IDs conhecidos
  da era Orsay: YouTube `111299001912`, Netflix `11101200001`. Os IDs `3201...` são do
  Tizen e não servem. Não invente IDs: pesquise e diga ao usuário quando um ID não estiver
  confirmado.
- **Hora**: o relógio usa `utc_ms` e `fuso_min` de `/api/status` (o PC), depois o
  `utc_offset_seconds` do clima, e só por último o `Date` da TV, que no Orsay pode vir em UTC.
- `$MANAGER_WIDGET/Common/API/*.js` só existem na TV. Fora dela, o `tv.js` usa o teclado
  do PC (setas, Enter, Backspace = RETURN, R = vermelho).

## Rodar e testar

```
python3 servidor/servidor.py              # servidor completo (porta 80; no Linux, com sudo)
python3 servidor/servidor.py --porta 8080 # só para a pré-visualização: http://localhost:8080/app/
python3 servidor/servidor.py --gerar      # dist/MinhaTV.zip + dist/widgetlist.xml
python3 ferramentas/verificar.py          # ES5, CSS e pacote, antes de mandar sincronizar
python3 ferramentas/gerar_imagens.py      # redesenha os ícones (precisa do Pillow)
```

O Playwright (quando houver) abre a pré-visualização em 1280x720. Para simular a TV,
injete um `window.Common.API` falso (Widget, TVKeyValue com os códigos da TV, Plugin) e
dispare `keydown` com o `keyCode` na `#ancora`.

## Arquivos

- `app/`: o que vai para a TV (`config.xml`, `index.html`, `css/`, `js/`, `img/`, `icon/`).
- `servidor/servidor.py`: instalação (`/widgetlist.xml`, `/MinhaTV.zip`), pré-visualização
  (`/app/`) e API (`/api/status`, `/api/config`, `/api/clima`, `/api/em-alta`,
  `/api/titulo`, `/api/img/<tamanho>/<arquivo>`, com cache em `.cache/`).
- `config.exemplo.json` vira `config.json` (fora do git) na primeira execução.
- Ao mudar o app, suba a `<ver>` do `config.xml` e a `VERSAO` do servidor.
