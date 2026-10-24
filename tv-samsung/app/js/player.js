/*
 * O tocador de vídeo: canais ao vivo e vídeos do Jellyfin, em tela cheia.
 *
 * Na TV usa o AVPlay da Samsung (webapis.avplay, do $MANAGER_WIDGET/Common/webapi).
 * O vídeo é desenhado por baixo da página: enquanto toca, a tela do app some e o fundo
 * fica transparente; só o painel de informações (OSD) aparece por cima.
 * Na pré-visualização do PC não há vídeo: o tocador só simula o tempo passando.
 */
var Tocador = (function () {
    var avplay = null;
    var atual = null;          // o que está tocando (opções de tocar())
    var tempoMs = 0;           // posição dentro do que foi aberto
    var pausado = false;
    var osdTimer = null;
    var relatorioTimer = null;
    var simulacaoTimer = null;
    var abrirTimer = null;
    var historico = [];        // endereços abertos (usado nos testes)

    function $(id) {
        return document.getElementById(id);
    }

    function texto(id, valor) {
        var el = $(id);
        if (el) {
            el.innerHTML = String(valor === undefined || valor === null ? '' : valor)
                .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
        }
    }

    function naTV() {
        return typeof webapis !== 'undefined' && webapis && webapis.avplay;
    }

    function relogio(ms) {
        var s = Math.max(0, Math.floor(ms / 1000));
        var h = Math.floor(s / 3600);
        var m = Math.floor((s % 3600) / 60);
        s = s % 60;
        return (h ? h + ':' + (m < 10 ? '0' : '') : '') + m + ':' + (s < 10 ? '0' : '') + s;
    }

    function posicaoMs() {
        return atual ? (atual.inicioMs || 0) + tempoMs : 0;
    }

    // ---------- AVPlay da TV ----------

    function aoTempo(t) {
        var ms = (t && typeof t === 'object') ? Number(t.millisecond) : Number(t);
        if (!isNaN(ms)) {
            tempoMs = ms;
            desenharProgresso();
        }
    }

    function preparar(pronto) {
        if (avplay || !naTV()) {
            pronto();
            return;
        }
        try {
            webapis.avplay.getAVPlay(function (objeto) {
                avplay = objeto;
                try {
                    avplay.init({
                        containerID: 'tocador-video',
                        displayRect: { top: 0, left: 0, width: 1280, height: 720 },
                        autoRatio: true,
                        bufferingCallback: {
                            onbufferingstart: function () { estado('Carregando...'); },
                            onbufferingcomplete: function () { estado(''); }
                        },
                        playCallback: {
                            oncurrentplaytime: aoTempo,
                            onstreamcompleted: function () { fechar(true); },
                            onerror: function (erro) { falhou(erro); }
                        }
                    });
                } catch (e1) {}
                pronto();
            }, function () {
                falhou({ code: 'sem tocador' });
            });
        } catch (e2) {
            falhou({ code: 'sem tocador' });
        }
    }

    function pararVideo() {
        if (abrirTimer) {
            clearTimeout(abrirTimer);
            abrirTimer = null;
        }
        if (simulacaoTimer) {
            clearInterval(simulacaoTimer);
            simulacaoTimer = null;
        }
        if (relatorioTimer) {
            clearInterval(relatorioTimer);
            relatorioTimer = null;
        }
        if (avplay) {
            try { avplay.stop(); } catch (e) {}
        }
    }

    // ---------- tela ----------

    function mostrarTela(visivel) {
        document.body.style.backgroundColor = visivel ? 'transparent' : '';
        $('tela').style.display = visivel ? 'none' : 'block';
        $('tocador').style.display = visivel ? 'block' : 'none';
        $('tocador').className = naTV() ? '' : 'simulado';
    }

    function estado(mensagem) {
        texto('tocador-estado', mensagem);
        $('tocador-estado').style.display = mensagem ? 'block' : 'none';
    }

    function desenharProgresso() {
        if (!atual || atual.aoVivo) {
            return;
        }
        var total = atual.duracaoMs || 0;
        var pos = posicaoMs();
        texto('tocador-tempo', relogio(pos) + (total ? ' / ' + relogio(total) : ''));
        $('tocador-progresso').style.width = total ? Math.min(100, Math.round(pos * 100 / total)) + '%' : '0%';
    }

    function desenharOsd() {
        texto('tocador-titulo', atual.titulo);
        texto('tocador-sub', atual.subtitulo || '');
        $('tocador-selo').style.display = atual.aoVivo ? 'inline-block' : 'none';
        $('tocador-barra').style.display = atual.aoVivo ? 'none' : 'block';
        texto('tocador-tempo', '');
        texto('tocador-dicas', atual.aoVivo ?
            'Cima/baixo ou CH: trocar de canal  ·  RETURN: voltar' :
            'OK: pausar  ·  Esquerda/direita: -10s/+30s  ·  RETURN: voltar');
        desenharProgresso();
    }

    function mostrarOsd(fixo) {
        $('tocador-osd').style.display = 'block';
        if (osdTimer) {
            clearTimeout(osdTimer);
            osdTimer = null;
        }
        if (!fixo) {
            osdTimer = setTimeout(function () {
                if (!pausado) {
                    $('tocador-osd').style.display = 'none';
                }
            }, 5000);
        }
    }

    function falhou(erro) {
        var codigo = erro && erro.code !== undefined ? erro.code : '?';
        estado('Não consegui tocar (erro ' + codigo + ').' + (atual && atual.aoVivo ? ' Troque de canal ou aperte RETURN.' : ' Aperte RETURN.'));
        mostrarOsd(true);
    }

    // ---------- comandos ----------

    /*
     * opcoes: titulo, subtitulo, url, hls (bool), aoVivo (bool), inicioMs, duracaoMs,
     *         trocarCanal(passo), aoComecar(), aoProgresso(posMs, pausado), aoFechar(posMs, terminou)
     */
    function tocar(opcoes) {
        var trocando = !!atual;
        pararVideo();
        atual = opcoes;
        tempoMs = 0;
        pausado = false;
        mostrarTela(true);
        desenharOsd();
        mostrarOsd(false);
        estado('Carregando...');

        var endereco = opcoes.url + (opcoes.hls ? '|COMPONENT=HLS' : '');
        preparar(function () {
            if (atual !== opcoes) {
                return;
            }
            // A TV precisa de um respiro entre parar um vídeo e abrir outro.
            abrirTimer = setTimeout(function () {
                abrirTimer = null;
                if (atual !== opcoes) {
                    return;
                }
                historico.push(endereco);
                if (avplay) {
                    try {
                        avplay.open(endereco);
                        avplay.play(function () { estado(''); }, falhou);
                    } catch (e) {
                        falhou({ code: 'abrir' });
                        return;
                    }
                } else {
                    estado('');
                    simulacaoTimer = setInterval(function () {
                        if (!pausado) {
                            aoTempo(tempoMs + 1000);
                        }
                    }, 1000);
                }
                if (opcoes.aoComecar) {
                    opcoes.aoComecar();
                }
                if (opcoes.aoProgresso) {
                    relatorioTimer = setInterval(function () {
                        opcoes.aoProgresso(posicaoMs(), pausado);
                    }, 30000);
                }
            }, trocando ? 600 : 50);
        });
    }

    function alternarPausa() {
        if (!atual || atual.aoVivo) {
            mostrarOsd(false);
            return;
        }
        pausado = !pausado;
        if (avplay) {
            try {
                if (pausado) { avplay.pause(); } else { avplay.resume(); }
            } catch (e) {}
        }
        estado(pausado ? 'Pausado' : '');
        mostrarOsd(pausado);
        if (atual.aoProgresso) {
            atual.aoProgresso(posicaoMs(), pausado);
        }
    }

    function pular(segundos) {
        if (!atual || atual.aoVivo) {
            mostrarOsd(false);
            return;
        }
        if (avplay) {
            try {
                if (segundos > 0) { avplay.jumpForward(segundos); } else { avplay.jumpBackward(-segundos); }
            } catch (e) {}
        } else {
            tempoMs = Math.max(0, tempoMs + segundos * 1000);
        }
        mostrarOsd(false);
        desenharProgresso();
    }

    // Para o vídeo e volta para a tela inicial.
    function fechar(terminou) {
        if (!atual) {
            return;
        }
        var opcoes = atual;
        var pos = terminou && opcoes.duracaoMs ? opcoes.duracaoMs : posicaoMs();
        pararVideo();
        atual = null;
        pausado = false;
        if (osdTimer) {
            clearTimeout(osdTimer);
            osdTimer = null;
        }
        estado('');
        mostrarTela(false);
        if (opcoes.aoFechar) {
            opcoes.aoFechar(pos, !!terminou);
        }
    }

    function tecla(acao) {
        if (!atual) {
            return;
        }
        if (acao === 'voltar' || acao === 'parar') {
            fechar(false);
        } else if (acao === 'ok' || acao === 'play' || acao === 'pausa') {
            alternarPausa();
        } else if (acao === 'esquerda') {
            pular(-10);
        } else if (acao === 'direita') {
            pular(30);
        } else if (acao === 'retroceder') {
            pular(-60);
        } else if (acao === 'avancar') {
            pular(60);
        } else if (acao === 'cima' || acao === 'canal+' || acao === 'baixo' || acao === 'canal-') {
            if (atual.aoVivo && atual.trocarCanal) {
                atual.trocarCanal(acao === 'cima' || acao === 'canal+' ? 1 : -1);
            } else {
                mostrarOsd(false);
            }
        } else {
            mostrarOsd(false);
        }
    }

    return {
        tocar: tocar,
        tecla: tecla,
        fechar: fechar,
        ativo: function () { return !!atual; },
        mensagem: function (texto) {
            estado(texto);
            mostrarOsd(true);
        },
        historico: historico
    };
})();
