/*
 * Camada da TV: tudo o que depende da Samsung 2013 (plataforma Orsay) fica aqui.
 * Na pré-visualização no navegador do PC os módulos da TV não existem, e as mesmas
 * funções usam substitutos (teclado do PC no lugar do controle).
 *
 * Regra do projeto: JavaScript ES5 puro, para o navegador antigo da TV (veja CLAUDE.md).
 */
var TV = (function () {
    var widget = null;
    var teclas = null;
    var plugin = null;

    try { widget = new Common.API.Widget(); } catch (e1) { widget = null; }
    try { teclas = new Common.API.TVKeyValue(); } catch (e2) { teclas = null; }
    try { plugin = new Common.API.Plugin(); } catch (e3) { plugin = null; }

    var naTV = !!(widget && teclas);

    // Código da tecla -> ação ('esquerda', 'ok', 'voltar'...).
    var acoes = {};
    function liga(codigo, acao) {
        if (typeof codigo === 'number' && !acoes.hasOwnProperty(codigo)) {
            acoes[codigo] = acao;
        }
    }

    if (naTV) {
        liga(teclas.KEY_LEFT, 'esquerda');
        liga(teclas.KEY_RIGHT, 'direita');
        liga(teclas.KEY_UP, 'cima');
        liga(teclas.KEY_DOWN, 'baixo');
        liga(teclas.KEY_ENTER, 'ok');
        liga(teclas.KEY_PANEL_ENTER, 'ok');
        liga(teclas.KEY_RETURN, 'voltar');
        liga(teclas.KEY_PANEL_RETURN, 'voltar');
        liga(teclas.KEY_EXIT, 'sair');
        liga(teclas.KEY_RED, 'vermelho');
        liga(teclas.KEY_GREEN, 'verde');
        liga(teclas.KEY_YELLOW, 'amarelo');
        liga(teclas.KEY_BLUE, 'azul');
        liga(teclas.KEY_INFO, 'info');
        liga(teclas.KEY_CH_UP, 'canal+');
        liga(teclas.KEY_CH_DOWN, 'canal-');
        liga(teclas.KEY_PLAY, 'play');
        liga(teclas.KEY_PAUSE, 'pausa');
        liga(teclas.KEY_STOP, 'parar');
        liga(teclas.KEY_FF, 'avancar');
        liga(teclas.KEY_RW, 'retroceder');
    } else {
        // Pré-visualização: setas, Enter, Backspace (RETURN), Esc (EXIT), R/G/Y/B (cores),
        // PageUp/PageDown (canal), P (play/pausa), S (parar), vírgula e ponto (voltar/avançar).
        liga(37, 'esquerda');
        liga(39, 'direita');
        liga(38, 'cima');
        liga(40, 'baixo');
        liga(13, 'ok');
        liga(8, 'voltar');
        liga(27, 'sair');
        liga(82, 'vermelho');
        liga(71, 'verde');
        liga(89, 'amarelo');
        liga(66, 'azul');
        liga(73, 'info');
        liga(33, 'canal+');
        liga(34, 'canal-');
        liga(80, 'play');
        liga(83, 'parar');
        liga(190, 'avancar');
        liga(188, 'retroceder');
    }

    function acao(codigo) {
        return acoes.hasOwnProperty(codigo) ? acoes[codigo] : null;
    }

    // Avisa a TV que o app carregou (sem isso a TV fecha o app).
    function pronto() {
        if (widget) {
            try { widget.sendReadyEvent(); } catch (e) {}
        }
    }

    // Chamado pela TV quando o app aparece na tela (window.onShow).
    // Devolve as teclas de volume e mudo para a TV, com o aviso de volume na tela.
    function aoMostrar() {
        if (!plugin || !teclas) {
            return;
        }
        try {
            plugin.unregistKey(teclas.KEY_VOL_UP);
            plugin.unregistKey(teclas.KEY_VOL_DOWN);
            plugin.unregistKey(teclas.KEY_MUTE);
            plugin.SetBannerState(1);
        } catch (e) {}
    }

    // Impede a tecla RETURN/EXIT de fechar o app.
    function segurarTecla(evento) {
        if (widget && evento) {
            try { widget.blockNavigation(evento); } catch (e) {}
        }
        if (evento && evento.preventDefault) {
            evento.preventDefault();
        }
    }

    // Abre outro app da TV pelo ID (ex.: YouTube 111299001912). Devolve false se não deu.
    function abrirApp(id) {
        if (!naTV) {
            TV.abertos.push(String(id));
            return true;
        }
        try {
            widget.runSearchWidget(String(id), '');
            return true;
        } catch (e) {
            return false;
        }
    }

    // Volta para o Smart Hub.
    function voltarAoHub() {
        if (widget) {
            try { widget.sendReturnEvent(); return; } catch (e) {}
        }
        TV.abertos.push('smarthub');
    }

    // Fecha o app e volta para a imagem da TV (antena/HDMI).
    function sair() {
        if (widget) {
            try { widget.sendExitEvent(); return; } catch (e) {}
        }
        TV.abertos.push('sair');
    }

    // Parâmetros que a TV passa ao abrir o app (?country=BR&language=...&modelid=...).
    function parametros() {
        var saida = {};
        var busca = (window.location && window.location.search) ? window.location.search.substring(1) : '';
        var partes = busca ? busca.split('&') : [];
        for (var i = 0; i < partes.length; i++) {
            var par = partes[i].split('=');
            if (par[0]) {
                try { saida[par[0]] = decodeURIComponent(par[1] || ''); } catch (e) { saida[par[0]] = par[1] || ''; }
            }
        }
        return saida;
    }

    return {
        naTV: naTV,
        acao: acao,
        pronto: pronto,
        aoMostrar: aoMostrar,
        segurarTecla: segurarTecla,
        abrirApp: abrirApp,
        voltarAoHub: voltarAoHub,
        sair: sair,
        parametros: parametros,
        abertos: [] // só na pré-visualização: o que teria sido aberto
    };
})();
