/*
 * Dados de fora: tudo passa pelo servidor do PC (servidor/servidor.py), que fala com
 * Open-Meteo e TMDB por HTTPS moderno, coisa que a TV de 2013 nem sempre consegue.
 * Se o PC estiver desligado, o clima ainda tenta direto no Open-Meteo.
 *
 * A TV tem pouca memória: cada "canal" reaproveita um XMLHttpRequest só e chama
 * destroy() no anterior, como pede o guia da Samsung.
 */
var Dados = (function () {
    var canais = {};

    function servidor() {
        return (window.CONFIG && CONFIG.servidor) ? CONFIG.servidor : '';
    }

    function url(caminho) {
        return servidor() + caminho;
    }

    function liberar(canal) {
        var antigo = canais[canal];
        if (!antigo) {
            return;
        }
        if (antigo.timer) {
            clearTimeout(antigo.timer);
        }
        antigo.xhr.onreadystatechange = function () {};
        try { antigo.xhr.abort(); } catch (e1) {}
        if (antigo.xhr.destroy) {
            try { antigo.xhr.destroy(); } catch (e2) {}
        }
        canais[canal] = null;
    }

    // Busca JSON. retorno(erro, dados): erro é null quando deu certo.
    function json(canal, endereco, limiteMs, retorno) {
        liberar(canal);
        var xhr = new XMLHttpRequest();
        var atual = { xhr: xhr, timer: null, fim: false };
        canais[canal] = atual;

        function terminar(erro, dados) {
            if (atual.fim) {
                return;
            }
            atual.fim = true;
            if (atual.timer) {
                clearTimeout(atual.timer);
            }
            if (canais[canal] === atual) {
                canais[canal] = null;
            }
            retorno(erro, dados);
        }

        xhr.onreadystatechange = function () {
            if (xhr.readyState !== 4) {
                return;
            }
            if (xhr.status < 200 || xhr.status >= 300) {
                var mensagem = 'HTTP ' + xhr.status;
                try {
                    var corpoErro = JSON.parse(xhr.responseText);
                    if (corpoErro && corpoErro.erro) {
                        mensagem = corpoErro.erro;
                    }
                } catch (e1) {}
                terminar(xhr.status === 0 ? 'sem resposta' : mensagem, null);
                return;
            }
            var dados = null;
            try {
                dados = JSON.parse(xhr.responseText);
            } catch (e2) {
                terminar('resposta inválida', null);
                return;
            }
            terminar(null, dados);
        };

        atual.timer = setTimeout(function () {
            atual.timer = null;
            terminar('tempo esgotado', null);
            try { xhr.abort(); } catch (e) {}
        }, limiteMs || 10000);

        try {
            xhr.open('GET', endereco, true);
            xhr.send(null);
        } catch (e3) {
            terminar('falha ao conectar', null);
        }
    }

    // Parâmetros do Open-Meteo (iguais aos que o servidor usa).
    function consultaClima(lat, lon) {
        return 'latitude=' + lat + '&longitude=' + lon +
            '&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day' +
            '&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset' +
            '&timezone=auto&forecast_days=5';
    }

    function status(retorno) {
        json('status', url('/api/status'), 6000, retorno);
    }

    function configuracao(retorno) {
        json('config', url('/api/config'), 6000, retorno);
    }

    function clima(retorno) {
        json('clima', url('/api/clima'), 12000, function (erro, dados) {
            if (!erro || !window.CONFIG || CONFIG.latitude === undefined) {
                retorno(erro, dados);
                return;
            }
            // PC fora do ar: tenta direto (HTTP simples, que a TV antiga aceita).
            var direto = 'http://api.open-meteo.com/v1/forecast?' + consultaClima(CONFIG.latitude, CONFIG.longitude);
            json('clima', direto, 12000, retorno);
        });
    }

    function emAlta(tipo, retorno) {
        json('alta-' + tipo, url('/api/em-alta?tipo=' + tipo), 15000, retorno);
    }

    function titulo(tipo, id, retorno) {
        json('titulo', url('/api/titulo?tipo=' + tipo + '&id=' + id), 15000, retorno);
    }

    // Endereço completo de uma imagem que o servidor do PC entrega (/api/img/...).
    function imagem(caminho) {
        return caminho ? url(caminho) : '';
    }

    return {
        servidor: servidor,
        status: status,
        configuracao: configuracao,
        clima: clima,
        emAlta: emAlta,
        titulo: titulo,
        imagem: imagem
    };
})();
