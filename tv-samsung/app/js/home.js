/*
 * A tela inicial: relógio, clima, apps, canais ao vivo, vídeos do Jellyfin, filmes e séries
 * em alta e previsão. O vídeo em si fica com o js/player.js.
 * Tela fixa de 1280x720. Navegação por linhas (cima/baixo) e itens (esquerda/direita).
 *
 * ES5 puro e DOM simples: a TV de 2013 tem um navegador antigo e pouca memória.
 */
var Home = (function () {
    var LARGURA_UTIL = 1160;      // 1280 menos as margens laterais (60 de cada lado)

    var DIAS = ['domingo', 'segunda-feira', 'terça-feira', 'quarta-feira', 'quinta-feira', 'sexta-feira', 'sábado'];
    var DIAS_CURTOS = ['Dom', 'Seg', 'Ter', 'Qua', 'Qui', 'Sex', 'Sáb'];
    var MESES = ['janeiro', 'fevereiro', 'março', 'abril', 'maio', 'junho', 'julho', 'agosto', 'setembro', 'outubro', 'novembro', 'dezembro'];

    // Tamanho dos itens de cada tipo de linha: largura, altura e espaço entre eles.
    var GEOMETRIA = {
        apps: { largura: 220, altura: 124, espaco: 24 },
        canais: { largura: 220, altura: 124, espaco: 24 },
        posters: { largura: 150, altura: 225, espaco: 22 },
        previsao: { largura: 190, altura: 150, espaco: 22 },
        aviso: { largura: 760, altura: 110, espaco: 0 }
    };
    var CABECALHO_LINHA = 44;
    var RODAPE_LINHA = 34;

    var estado = {
        linhas: [],
        linha: 0,
        colunas: {},
        rolagens: {},
        detalhe: null,
        relogio: { correcao: 0, fuso: null, origem: null },
        textoRelogio: '',
        pcOnline: null,
        clima: null,
        erroClima: null,
        filmes: null,
        series: null,
        erroFilmes: null,
        canais: null,
        erroCanais: null,
        jf: null,
        erroJf: null,
        fundoAtual: '',
        fundoTimer: null,
        avisoTimer: null,
        timers: []
    };

    // ---------- utilidades ----------

    function $(id) {
        return document.getElementById(id);
    }

    function esc(texto) {
        return String(texto === undefined || texto === null ? '' : texto)
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;');
    }

    function doisDigitos(n) {
        return n < 10 ? '0' + n : String(n);
    }

    function temClasse(el, nome) {
        return (' ' + el.className + ' ').indexOf(' ' + nome + ' ') !== -1;
    }

    function classe(el, nome, ligada) {
        if (!el) {
            return;
        }
        var tem = temClasse(el, nome);
        if (ligada && !tem) {
            el.className = el.className ? el.className + ' ' + nome : nome;
        } else if (!ligada && tem) {
            el.className = (' ' + el.className + ' ').replace(' ' + nome + ' ', ' ').replace(/^\s+|\s+$/g, '');
        }
    }

    function mostrar(el, visivel) {
        if (el) {
            el.style.display = visivel ? 'block' : 'none';
        }
    }

    function cfg() {
        return window.CONFIG || {};
    }

    function corValida(cor, padrao) {
        return (typeof cor === 'string' && /^#[0-9a-fA-F]{3}([0-9a-fA-F]{3})?$/.test(cor)) ? cor : padrao;
    }

    function aviso(texto, ms) {
        var el = $('aviso');
        el.innerHTML = esc(texto);
        mostrar(el, true);
        if (estado.avisoTimer) {
            clearTimeout(estado.avisoTimer);
        }
        estado.avisoTimer = setTimeout(function () {
            mostrar(el, false);
        }, ms || 3500);
    }

    function numero(valor) {
        return Math.round(Number(valor));
    }

    // 3725000 -> "1:02:05"
    function relogioVideo(ms) {
        var s = Math.floor(ms / 1000);
        var h = Math.floor(s / 3600);
        var m = Math.floor((s % 3600) / 60);
        s = s % 60;
        return (h ? h + ':' + doisDigitos(m) : String(m)) + ':' + doisDigitos(s);
    }

    // 6300000 -> "1h 45min"
    function duracaoVideo(ms) {
        var minutos = Math.round(ms / 60000);
        var h = Math.floor(minutos / 60);
        return h ? h + 'h ' + doisDigitos(minutos % 60) + 'min' : minutos + ' min';
    }

    // ---------- relógio ----------

    // Hora certa: usa a hora e o fuso do PC (ou do clima). Sem eles, a hora da própria TV.
    function agora() {
        var r = estado.relogio;
        if (r.fuso !== null) {
            var d = new Date(new Date().getTime() + r.correcao + r.fuso * 60000);
            return { h: d.getUTCHours(), m: d.getUTCMinutes(), dia: d.getUTCDate(), mes: d.getUTCMonth(), semana: d.getUTCDay(), iso: isoData(d.getUTCFullYear(), d.getUTCMonth(), d.getUTCDate()) };
        }
        var l = new Date();
        return { h: l.getHours(), m: l.getMinutes(), dia: l.getDate(), mes: l.getMonth(), semana: l.getDay(), iso: isoData(l.getFullYear(), l.getMonth(), l.getDate()) };
    }

    function isoData(ano, mes, dia) {
        return ano + '-' + doisDigitos(mes + 1) + '-' + doisDigitos(dia);
    }

    function atualizarRelogio() {
        var t = agora();
        var texto = doisDigitos(t.h) + ':' + doisDigitos(t.m);
        if (texto === estado.textoRelogio) {
            return;
        }
        var virouODia = estado.textoRelogio && texto === '00:00';
        estado.textoRelogio = texto;
        $('relogio').innerHTML = texto;
        $('data').innerHTML = esc(DIAS[t.semana] + ', ' + t.dia + ' de ' + MESES[t.mes]);
        if (virouODia && estado.clima) {
            desenharClima();
            desenharLinhas();
        }
    }

    // ---------- clima ----------

    function tempo(codigo, dia) {
        var c = Number(codigo);
        if (c === 0) { return { texto: 'Céu limpo', icone: dia ? 'sol' : 'lua' }; }
        if (c === 1) { return { texto: 'Poucas nuvens', icone: dia ? 'parcial' : 'parcial-noite' }; }
        if (c === 2) { return { texto: 'Parcialmente nublado', icone: dia ? 'parcial' : 'parcial-noite' }; }
        if (c === 3) { return { texto: 'Nublado', icone: 'nublado' }; }
        if (c === 45 || c === 48) { return { texto: 'Neblina', icone: 'neblina' }; }
        if (c >= 51 && c <= 57) { return { texto: 'Garoa', icone: 'chuva' }; }
        if (c === 61) { return { texto: 'Chuva fraca', icone: 'chuva' }; }
        if (c === 63) { return { texto: 'Chuva', icone: 'chuva' }; }
        if (c === 65) { return { texto: 'Chuva forte', icone: 'chuva' }; }
        if (c === 66 || c === 67) { return { texto: 'Chuva congelante', icone: 'chuva' }; }
        if ((c >= 71 && c <= 77) || c === 85 || c === 86) { return { texto: 'Neve', icone: 'neve' }; }
        if (c >= 80 && c <= 82) { return { texto: 'Pancadas de chuva', icone: 'chuva' }; }
        if (c === 95) { return { texto: 'Trovoadas', icone: 'tempestade' }; }
        if (c === 96 || c === 99) { return { texto: 'Trovoadas com granizo', icone: 'tempestade' }; }
        return { texto: '', icone: 'nublado' };
    }

    function iconeClima(nome, tamanho) {
        return '<img class="icone-clima" src="img/clima/' + nome + '.png" width="' + tamanho + '" height="' + tamanho + '">';
    }

    function desenharClima() {
        var c = estado.clima;
        if (!c || !c.current) {
            $('clima-icone').innerHTML = '';
            $('clima-temp').innerHTML = '';
            $('clima-texto').innerHTML = esc(cfg().cidade || '');
            $('clima-extra').innerHTML = estado.erroClima ? 'Clima indisponível' : 'Carregando clima...';
            return;
        }
        var atual = c.current;
        var t = tempo(atual.weather_code, atual.is_day !== 0);
        $('clima-icone').innerHTML = iconeClima(t.icone, 64);
        $('clima-temp').innerHTML = numero(atual.temperature_2m) + '&deg;';
        $('clima-texto').innerHTML = esc((c.cidade || cfg().cidade || '') + (t.texto ? ' · ' + t.texto : ''));
        var extra = [];
        var hoje = indiceHoje(c);
        if (c.daily && hoje >= 0) {
            extra.push('Máx ' + numero(c.daily.temperature_2m_max[hoje]) + '°');
            extra.push('Mín ' + numero(c.daily.temperature_2m_min[hoje]) + '°');
            var chuva = c.daily.precipitation_probability_max ? c.daily.precipitation_probability_max[hoje] : null;
            if (chuva !== null && chuva !== undefined) {
                extra.push('Chuva ' + numero(chuva) + '%');
            }
        }
        $('clima-extra').innerHTML = esc(extra.join('  ·  '));
    }

    // Posição de "hoje" na previsão diária (pela data, para não errar depois da meia-noite).
    function indiceHoje(c) {
        if (!c.daily || !c.daily.time) {
            return -1;
        }
        var hoje = agora().iso;
        for (var i = 0; i < c.daily.time.length; i++) {
            if (c.daily.time[i] === hoje) {
                return i;
            }
        }
        return 0;
    }

    // ---------- linhas ----------

    function itensApps() {
        var lista = [];
        var apps = cfg().apps || [];
        for (var i = 0; i < apps.length; i++) {
            if (apps[i] && apps[i].nome && apps[i].id) {
                lista.push({ nome: apps[i].nome, id: String(apps[i].id), cor: corValida(apps[i].cor, '#2a3140') });
            }
        }
        lista.push({ nome: 'Smart Hub', acao: 'hub', cor: '#1d2a44' });
        lista.push({ nome: 'Ver TV', acao: 'sair', cor: '#26303d' });
        return lista;
    }

    function itensPrevisao() {
        var c = estado.clima;
        if (!c || !c.daily || !c.daily.time) {
            return null;
        }
        var lista = [];
        var hoje = indiceHoje(c);
        for (var i = hoje; i < c.daily.time.length && lista.length < 5; i++) {
            var partes = String(c.daily.time[i]).split('-');
            var data = new Date(Date.UTC(Number(partes[0]), Number(partes[1]) - 1, Number(partes[2])));
            var nome = i === hoje ? 'Hoje' : (i === hoje + 1 ? 'Amanhã' : DIAS_CURTOS[data.getUTCDay()]);
            var chuva = c.daily.precipitation_probability_max ? c.daily.precipitation_probability_max[i] : null;
            lista.push({
                nome: nome,
                tempo: tempo(c.daily.weather_code[i], true),
                max: numero(c.daily.temperature_2m_max[i]),
                min: numero(c.daily.temperature_2m_min[i]),
                chuva: (chuva === null || chuva === undefined) ? null : numero(chuva)
            });
        }
        return lista;
    }

    function linhaAviso(id, titulo, texto) {
        return { id: id, titulo: titulo, tipo: 'aviso', itens: [{ texto: texto }] };
    }

    function linhaTitulos(id, titulo, lista, nomeTipo) {
        if (lista && lista.length) {
            return { id: id, titulo: titulo, tipo: 'posters', itens: lista };
        }
        if (estado.pcOnline === false) {
            return linhaAviso(id, titulo, 'O PC está desligado ou fora da rede. Ligue o servidor no PC para ver ' + nomeTipo + ' em alta.');
        }
        if (estado.erroFilmes) {
            return linhaAviso(id, titulo, 'Não deu para carregar ' + nomeTipo + ': ' + estado.erroFilmes);
        }
        return linhaAviso(id, titulo, 'Carregando ' + nomeTipo + ' em alta...');
    }

    function linhaCanais() {
        var canais = estado.canais;
        if (canais && canais.length) {
            return { id: 'canais', titulo: 'Ao vivo', tipo: 'canais', itens: canais };
        }
        if (estado.pcOnline === false) {
            return linhaAviso('canais', 'Ao vivo', 'O PC está desligado ou fora da rede: os canais passam por ele.');
        }
        if (canais) {
            return linhaAviso('canais', 'Ao vivo', 'Nenhum canal ainda. Coloque os canais no config.json do PC (veja "Canais ao vivo" no README).');
        }
        return linhaAviso('canais', 'Ao vivo', estado.erroCanais ? 'Não deu para carregar os canais: ' + estado.erroCanais : 'Carregando os canais...');
    }

    function linhasJellyfin() {
        if (!cfg().jellyfin) {
            return [linhaAviso('jf', 'Meus vídeos', 'Para ver aqui os filmes e séries do PC, instale o Jellyfin no PC e coloque o usuário e a senha no config.json.')];
        }
        var jf = estado.jf;
        if (!jf) {
            if (estado.pcOnline === false) {
                return [linhaAviso('jf', 'Meus vídeos', 'O PC está desligado ou fora da rede.')];
            }
            return [linhaAviso('jf', 'Meus vídeos', estado.erroJf ? 'Não deu para abrir o Jellyfin: ' + estado.erroJf : 'Carregando seus vídeos...')];
        }
        var linhas = [];
        if (jf.continuar && jf.continuar.length) {
            linhas.push({ id: 'jf-continuar', titulo: 'Continuar assistindo', tipo: 'posters', itens: jf.continuar });
        }
        if (jf.proximos && jf.proximos.length) {
            linhas.push({ id: 'jf-proximos', titulo: 'Próximos episódios', tipo: 'posters', itens: jf.proximos });
        }
        if (jf.filmes && jf.filmes.length) {
            linhas.push({ id: 'jf-filmes', titulo: 'Filmes adicionados', tipo: 'posters', itens: jf.filmes });
        }
        if (!linhas.length) {
            linhas.push(linhaAviso('jf', 'Meus vídeos', 'A biblioteca do Jellyfin ainda está vazia.'));
        }
        return linhas;
    }

    function montarLinhas() {
        var linhas = [];
        linhas.push({ id: 'apps', titulo: 'Apps', tipo: 'apps', itens: itensApps() });
        linhas.push(linhaCanais());
        linhas = linhas.concat(linhasJellyfin());

        if (cfg().filmes) {
            linhas.push(linhaTitulos('filmes', 'Em alta: filmes', estado.filmes, 'filmes'));
            linhas.push(linhaTitulos('series', 'Em alta: séries', estado.series, 'séries'));
        } else {
            linhas.push(linhaAviso('filmes', 'Em alta', 'Para ver filmes e séries em alta, coloque sua chave do TMDB no arquivo config.json do PC e reinicie o servidor.'));
        }

        var previsao = itensPrevisao();
        if (previsao && previsao.length) {
            linhas.push({ id: 'previsao', titulo: 'Previsão: ' + (estado.clima.cidade || cfg().cidade || ''), tipo: 'previsao', itens: previsao });
        } else {
            linhas.push(linhaAviso('previsao', 'Previsão', estado.erroClima ? 'Clima indisponível agora (' + estado.erroClima + ').' : 'Carregando a previsão...'));
        }

        var y = 0;
        for (var i = 0; i < linhas.length; i++) {
            var g = GEOMETRIA[linhas[i].tipo];
            linhas[i].topo = y;
            linhas[i].altura = CABECALHO_LINHA + g.altura + RODAPE_LINHA;
            y += linhas[i].altura;
        }
        return linhas;
    }

    function htmlItem(linha, item, i) {
        var g = GEOMETRIA[linha.tipo];
        var pos = 'left:' + (i * (g.largura + g.espaco)) + 'px;';
        var id = ' id="item-' + linha.id + '-' + i + '"';
        if (linha.tipo === 'apps') {
            return '<div class="item app"' + id + ' style="' + pos + 'background-color:' + item.cor + ';">' +
                '<div class="app-nome">' + esc(item.nome) + '</div></div>';
        }
        if (linha.tipo === 'canais') {
            return '<div class="item canal"' + id + ' style="' + pos + '">' +
                '<div class="canal-nome">' + esc(item.nome) + '</div><div class="canal-selo">AO VIVO</div></div>';
        }
        if (linha.tipo === 'posters') {
            var progresso = '';
            if (item.posicao_ms && item.duracao_ms) {
                progresso = '<div class="poster-progresso" style="width:' + Math.min(100, Math.round(item.posicao_ms * 100 / item.duracao_ms)) + '%"></div>';
            }
            return '<div class="item poster"' + id + ' style="' + pos + '">' +
                '<div class="poster-nome">' + esc(item.titulo) + '</div>' +
                '<img class="poster-img" id="img-' + linha.id + '-' + i + '" width="142" height="217" style="visibility:hidden" onload="this.style.visibility=\'visible\'">' +
                progresso + '</div>';
        }
        if (linha.tipo === 'previsao') {
            return '<div class="item previsao"' + id + ' style="' + pos + '">' +
                '<div class="previsao-dia">' + esc(item.nome) + '</div>' +
                iconeClima(item.tempo.icone, 48) +
                '<div class="previsao-temp">' + item.max + '&deg; <span class="previsao-min">' + item.min + '&deg;</span></div>' +
                '<div class="previsao-chuva">' + (item.chuva === null ? '' : 'Chuva ' + item.chuva + '%') + '</div></div>';
        }
        return '<div class="item aviso-card"' + id + ' style="' + pos + '">' + esc(item.texto) + '</div>';
    }

    function desenharLinhas() {
        var idFoco = estado.linhas.length ? estado.linhas[estado.linha].id : 'apps';
        estado.linhas = montarLinhas();
        estado.linha = 0;
        for (var k = 0; k < estado.linhas.length; k++) {
            if (estado.linhas[k].id === idFoco) {
                estado.linha = k;
            }
        }

        var html = [];
        for (var i = 0; i < estado.linhas.length; i++) {
            var linha = estado.linhas[i];
            var g = GEOMETRIA[linha.tipo];
            html.push('<div class="linha linha-' + linha.tipo + '" style="top:' + linha.topo + 'px;height:' + linha.altura + 'px">');
            html.push('<div class="linha-titulo">' + esc(linha.titulo) + '<span class="linha-sub" id="sub-' + linha.id + '"></span></div>');
            html.push('<div class="trilho" id="trilho-' + linha.id + '" style="height:' + (g.altura + 12) + 'px">');
            for (var j = 0; j < linha.itens.length; j++) {
                html.push(htmlItem(linha, linha.itens[j], j));
            }
            html.push('</div></div>');
            var coluna = estado.colunas[linha.id] || 0;
            estado.colunas[linha.id] = Math.min(coluna, linha.itens.length - 1);
        }
        $('linhas').innerHTML = html.join('');
        aplicarFoco(true);
    }

    function itemAtual() {
        var linha = estado.linhas[estado.linha];
        return linha ? linha.itens[estado.colunas[linha.id] || 0] : null;
    }

    // Marca o item com foco e rola a linha (horizontal) e a lista (vertical) até ele.
    function aplicarFoco(tudo) {
        var anterior = document.querySelector ? document.querySelector('#linhas .foco') : null;
        if (anterior) {
            classe(anterior, 'foco', false);
        }
        var linha = estado.linhas[estado.linha];
        if (!linha) {
            return;
        }
        var coluna = estado.colunas[linha.id] || 0;
        classe($('item-' + linha.id + '-' + coluna), 'foco', true);

        var g = GEOMETRIA[linha.tipo];
        var passo = g.largura + g.espaco;
        var rolagem = estado.rolagens[linha.id] || 0;
        var esquerda = coluna * passo;
        if (esquerda - rolagem < 0) {
            rolagem = esquerda;
        } else if (esquerda + g.largura - rolagem > LARGURA_UTIL) {
            rolagem = esquerda + g.largura - LARGURA_UTIL;
        }
        estado.rolagens[linha.id] = rolagem;
        $('trilho-' + linha.id).style.left = (-rolagem) + 'px';

        if (tudo) {
            for (var i = 0; i < estado.linhas.length; i++) {
                var outra = estado.linhas[i];
                var trilho = $('trilho-' + outra.id);
                if (trilho) {
                    trilho.style.left = (-(estado.rolagens[outra.id] || 0)) + 'px';
                }
                if (outra.tipo === 'posters') {
                    carregarImagens(outra);
                }
            }
        }

        // A linha com foco fica sempre no alto da área das linhas, com o título à vista.
        $('linhas').style.top = (-linha.topo) + 'px';

        if (linha.tipo === 'posters') {
            carregarImagens(linha);
            var item = linha.itens[coluna];
            var sub = '';
            if (item) {
                sub = item.subtitulo ? item.titulo + ' \u00b7 ' + item.subtitulo : item.titulo + (item.ano ? ' (' + item.ano + ')' : '');
            }
            $('sub-' + linha.id).innerHTML = sub ? esc('  \u00b7  ' + sub) : '';
            trocarFundoDepois(item ? item.fundo : '');
        }
        limparSubtitulos(linha.id);
    }

    function limparSubtitulos(exceto) {
        for (var i = 0; i < estado.linhas.length; i++) {
            if (estado.linhas[i].id !== exceto) {
                var sub = $('sub-' + estado.linhas[i].id);
                if (sub && sub.innerHTML) {
                    sub.innerHTML = '';
                }
            }
        }
    }

    // Só baixa os pôsteres perto do foco (a TV tem pouca memória).
    function carregarImagens(linha) {
        var coluna = estado.colunas[linha.id] || 0;
        var ate = Math.min(linha.itens.length, coluna + 10);
        for (var i = 0; i < ate; i++) {
            var img = $('img-' + linha.id + '-' + i);
            if (img && !img.getAttribute('src') && linha.itens[i].poster) {
                img.setAttribute('src', Dados.imagem(linha.itens[i].poster));
            }
        }
    }

    function trocarFundoDepois(caminho) {
        if (estado.fundoTimer) {
            clearTimeout(estado.fundoTimer);
        }
        estado.fundoTimer = setTimeout(function () {
            trocarFundo(caminho);
        }, 450);
    }

    function trocarFundo(caminho) {
        if (!caminho || caminho === estado.fundoAtual) {
            return;
        }
        estado.fundoAtual = caminho;
        $('fundo').style.backgroundImage = 'url(' + Dados.imagem(caminho) + ')';
    }

    function mover(dLinha, dColuna) {
        if (dLinha) {
            var nova = estado.linha + dLinha;
            if (nova < 0 || nova >= estado.linhas.length) {
                return;
            }
            estado.linha = nova;
        } else {
            var linha = estado.linhas[estado.linha];
            var coluna = (estado.colunas[linha.id] || 0) + dColuna;
            if (coluna < 0 || coluna >= linha.itens.length) {
                return;
            }
            estado.colunas[linha.id] = coluna;
        }
        aplicarFoco(false);
    }

    function ativar() {
        var linha = estado.linhas[estado.linha];
        var item = itemAtual();
        if (!linha || !item) {
            return;
        }
        if (linha.tipo === 'apps') {
            abrirApp(item);
        } else if (linha.tipo === 'canais') {
            assistirCanal(estado.colunas[linha.id] || 0);
        } else if (linha.tipo === 'posters' && item.tipo === 'jellyfin') {
            abrirDetalheJellyfin(item);
        } else if (linha.tipo === 'posters') {
            abrirDetalhe(item);
        } else {
            atualizarTudo();
        }
    }

    function abrirApp(item) {
        if (item.acao === 'hub') {
            TV.voltarAoHub();
            return;
        }
        if (item.acao === 'sair') {
            TV.sair();
            return;
        }
        aviso('Abrindo ' + item.nome + '...');
        if (!TV.abrirApp(item.id)) {
            aviso('Não consegui abrir ' + item.nome + ' (ID ' + item.id + ').', 5000);
        }
    }

    // ---------- vídeo: canais e Jellyfin ----------

    function assistirCanal(indice) {
        var canais = estado.canais || [];
        if (!canais.length) {
            return;
        }
        indice = (indice + canais.length) % canais.length;
        var canal = canais[indice];
        estado.colunas.canais = indice;
        Dados.canal(canal.id, function (erro, r) {
            if (erro) {
                if (Tocador.ativo()) {
                    Tocador.mensagem('Não consegui abrir ' + canal.nome + ': ' + erro);
                } else {
                    aviso('Não consegui abrir ' + canal.nome + ': ' + erro, 6000);
                }
                return;
            }
            Tocador.tocar({
                titulo: canal.nome,
                subtitulo: 'Canal ' + (indice + 1) + ' de ' + canais.length,
                url: Dados.endereco(r.url),
                hls: !!r.hls,
                aoVivo: true,
                trocarCanal: function (passo) {
                    assistirCanal(indice + passo);
                },
                aoFechar: function () {
                    focarAncora();
                    aplicarFoco(true);
                }
            });
        });
    }

    function abrirDetalheJellyfin(item) {
        var botoes = [];
        if (item.posicao_ms > 0) {
            botoes.push({ nome: 'Continuar de ' + relogioVideo(item.posicao_ms), acao: 'jellyfin', doInicio: false });
            botoes.push({ nome: 'Do início', acao: 'jellyfin', doInicio: true });
        } else {
            botoes.push({ nome: 'Assistir', acao: 'jellyfin', doInicio: true });
        }
        botoes.push({ nome: 'Voltar', acao: 'voltar' });
        estado.detalhe = { item: item, botoes: botoes, foco: 0 };
        $('detalhe-fundo').style.backgroundImage = item.fundo ? 'url(' + Dados.imagem(item.fundo) + ')' : 'none';
        $('detalhe-titulo').innerHTML = esc(item.titulo);
        desenharDetalhe({
            extra: item.subtitulo,
            ano: item.ano,
            duracao: item.duracao_ms ? duracaoVideo(item.duracao_ms) : '',
            sinopse: item.sinopse,
            onde: 'pc'
        });
        mostrar($('detalhe'), true);
    }

    function assistirJellyfin(item, doInicio) {
        aviso('Abrindo ' + item.titulo + '...');
        Dados.jellyfinTocar(item.id, doInicio, function (erro, r) {
            if (erro) {
                aviso('Não consegui abrir o vídeo: ' + erro, 6000);
                return;
            }
            fecharDetalhe();
            var sessao = { id: item.id, fonte: r.fonte, sessao: r.sessao };
            Tocador.tocar({
                titulo: r.titulo || item.titulo,
                subtitulo: r.subtitulo || item.subtitulo,
                url: Dados.endereco(r.url),
                hls: true,
                aoVivo: false,
                inicioMs: r.inicio_ms || 0,
                duracaoMs: r.duracao_ms || item.duracao_ms || 0,
                aoComecar: function () {
                    Dados.jellyfinRelatorio('inicio', sessao, r.inicio_ms || 0, false);
                },
                aoProgresso: function (ms, pausado) {
                    Dados.jellyfinRelatorio('progresso', sessao, ms, pausado);
                },
                aoFechar: function (ms) {
                    focarAncora();
                    // Primeiro o Jellyfin guarda onde parou, depois a lista "continuar" é atualizada.
                    Dados.jellyfinRelatorio('fim', sessao, ms, false, buscarJellyfin);
                }
            });
        });
    }

    // ---------- detalhe de um filme/série ----------

    function appsDoTitulo(onde) {
        var apps = itensApps();
        var achados = [];
        for (var i = 0; i < apps.length; i++) {
            if (!apps[i].id) {
                continue;
            }
            var nome = apps[i].nome.toLowerCase();
            for (var j = 0; j < onde.length; j++) {
                if (String(onde[j]).toLowerCase().indexOf(nome) !== -1) {
                    achados.push(apps[i]);
                    break;
                }
            }
        }
        return achados;
    }

    function abrirDetalhe(item) {
        estado.detalhe = { item: item, botoes: [{ nome: 'Voltar', acao: 'voltar' }], foco: 0 };
        $('detalhe-fundo').style.backgroundImage = item.fundo ? 'url(' + Dados.imagem(item.fundo) + ')' : 'none';
        $('detalhe-titulo').innerHTML = esc(item.titulo);
        desenharDetalhe({ ano: item.ano, nota: item.nota, sinopse: item.sinopse, onde: null });
        mostrar($('detalhe'), true);

        Dados.titulo(item.tipo, item.id, function (erro, dados) {
            if (!estado.detalhe || estado.detalhe.item !== item) {
                return;
            }
            if (erro) {
                desenharDetalhe({ ano: item.ano, nota: item.nota, sinopse: item.sinopse, onde: [], erro: erro });
                return;
            }
            var botoes = [];
            var apps = appsDoTitulo(dados.onde || []);
            for (var i = 0; i < apps.length; i++) {
                botoes.push({ nome: 'Abrir ' + apps[i].nome, app: apps[i] });
            }
            botoes.push({ nome: 'Voltar', acao: 'voltar' });
            estado.detalhe.botoes = botoes;
            estado.detalhe.foco = 0;
            desenharDetalhe(dados);
        });
    }

    function desenharDetalhe(d) {
        var info = [];
        if (d.extra) { info.push(d.extra); }
        if (d.ano) { info.push(d.ano); }
        if (d.nota) { info.push('Nota ' + String(d.nota).replace('.', ',')); }
        if (d.duracao) { info.push(d.duracao); }
        if (d.generos && d.generos.length) { info.push(d.generos.slice(0, 3).join(', ')); }
        $('detalhe-info').innerHTML = esc(info.join('  ·  '));

        var sinopse = d.sinopse || 'Sem sinopse em português.';
        if (sinopse.length > 480) {
            sinopse = sinopse.substring(0, 477) + '...';
        }
        $('detalhe-sinopse').innerHTML = esc(sinopse);

        var onde;
        if (d.erro) {
            onde = 'Não deu para ver onde assistir (' + d.erro + ').';
        } else if (d.onde === 'pc') {
            onde = 'No seu PC (Jellyfin)';
        } else if (d.onde === null) {
            onde = 'Procurando onde assistir...';
        } else if (d.onde.length) {
            onde = 'Onde assistir: ' + d.onde.join(', ');
        } else {
            onde = 'Não está em nenhum streaming por assinatura no Brasil agora.';
        }
        $('detalhe-onde').innerHTML = esc(onde);
        desenharBotoes();
    }

    function desenharBotoes() {
        var det = estado.detalhe;
        var html = [];
        for (var i = 0; i < det.botoes.length; i++) {
            html.push('<div class="botao' + (i === det.foco ? ' foco' : '') + '">' + esc(det.botoes[i].nome) + '</div>');
        }
        $('detalhe-botoes').innerHTML = html.join('');
    }

    function fecharDetalhe() {
        estado.detalhe = null;
        mostrar($('detalhe'), false);
    }

    function teclaDetalhe(acao) {
        var det = estado.detalhe;
        if (acao === 'esquerda' && det.foco > 0) {
            det.foco--;
            desenharBotoes();
        } else if (acao === 'direita' && det.foco < det.botoes.length - 1) {
            det.foco++;
            desenharBotoes();
        } else if (acao === 'ok') {
            var botao = det.botoes[det.foco];
            if (botao.app) {
                abrirApp(botao.app);
            } else if (botao.acao === 'jellyfin') {
                assistirJellyfin(det.item, botao.doInicio);
            } else {
                fecharDetalhe();
            }
        }
    }

    // ---------- dados ----------

    function aplicarStatus(erro, dados) {
        var estava = estado.pcOnline;
        estado.pcOnline = !erro;
        if (!erro && dados) {
            if (typeof dados.utc_ms === 'number') {
                estado.relogio.correcao = dados.utc_ms - new Date().getTime();
            }
            if (typeof dados.fuso_min === 'number') {
                estado.relogio.fuso = dados.fuso_min;
                estado.relogio.origem = 'pc';
            }
            estado.textoRelogio = '';
            atualizarRelogio();
        }
        var avisoPc = $('aviso-pc');
        if (erro) {
            avisoPc.innerHTML = esc('Sem conexão com o PC' + (Dados.servidor() ? ' (' + Dados.servidor().replace('http://', '') + ')' : ''));
            mostrar(avisoPc, true);
        } else {
            mostrar(avisoPc, false);
        }
        if (estava !== estado.pcOnline) {
            if (estava === false && estado.pcOnline) {
                // O PC voltou: busca de novo o que dependia dele.
                buscarConfiguracao();
                buscarClima();
                buscarFilmes();
                buscarCanais();
                buscarJellyfin();
            }
            desenharLinhas();
        }
    }

    function buscarStatus() {
        Dados.status(aplicarStatus);
    }

    // O PC manda a configuração atual (cidade, apps): muda sem reinstalar o app.
    function buscarConfiguracao() {
        Dados.configuracao(function (erro, dados) {
            if (erro || !dados) {
                return;
            }
            var atual = cfg();
            var mudouCidade = dados.latitude !== atual.latitude || dados.longitude !== atual.longitude;
            var mudouFilmes = !!dados.filmes !== !!atual.filmes;
            var mudouVideo = !!dados.jellyfin !== !!atual.jellyfin || !!dados.ao_vivo !== !!atual.ao_vivo;
            for (var chave in dados) {
                if (dados.hasOwnProperty(chave)) {
                    atual[chave] = dados[chave];
                }
            }
            window.CONFIG = atual;
            if (mudouCidade) {
                buscarClima();
            }
            if (mudouFilmes) {
                buscarFilmes();
            }
            if (mudouVideo) {
                buscarCanais();
                buscarJellyfin();
            }
            desenharLinhas();
        });
    }

    function buscarClima() {
        Dados.clima(function (erro, dados) {
            if (erro) {
                estado.erroClima = erro;
            } else {
                estado.erroClima = null;
                estado.clima = dados;
                if (estado.relogio.origem !== 'pc' && typeof dados.utc_offset_seconds === 'number') {
                    estado.relogio.fuso = Math.round(dados.utc_offset_seconds / 60);
                    estado.relogio.origem = 'clima';
                    estado.textoRelogio = '';
                    atualizarRelogio();
                }
            }
            desenharClima();
            desenharLinhas();
        });
    }

    function buscarFilmes() {
        if (!cfg().filmes) {
            return;
        }
        Dados.emAlta('movie', function (erro, dados) {
            if (erro) {
                estado.erroFilmes = erro;
            } else {
                estado.erroFilmes = null;
                estado.filmes = dados.itens || [];
                if (!estado.fundoAtual && estado.filmes.length) {
                    trocarFundo(estado.filmes[0].fundo);
                }
            }
            desenharLinhas();
            Dados.emAlta('tv', function (erro2, dados2) {
                if (!erro2) {
                    estado.series = dados2.itens || [];
                }
                desenharLinhas();
            });
        });
    }

    function buscarCanais() {
        Dados.canais(function (erro, dados) {
            if (erro) {
                estado.erroCanais = erro;
            } else {
                estado.erroCanais = null;
                estado.canais = dados.canais || [];
            }
            desenharLinhas();
        });
    }

    function buscarJellyfin() {
        if (!cfg().jellyfin) {
            return;
        }
        Dados.jellyfin(function (erro, dados) {
            if (erro) {
                estado.erroJf = erro;
            } else {
                estado.erroJf = null;
                estado.jf = dados;
            }
            desenharLinhas();
        });
    }

    function atualizarTudo() {
        aviso('Atualizando...');
        buscarStatus();
        buscarConfiguracao();
        buscarClima();
        buscarFilmes();
        buscarCanais();
        buscarJellyfin();
    }

    // ---------- teclas ----------

    function tecla(evento) {
        var e = evento || window.event;
        var acao = TV.acao(e.keyCode);
        if (!acao || acao === 'sair') {
            return; // EXIT fecha o app normalmente; volume e mudo ficam com a TV
        }
        if (Tocador.ativo()) {
            if (acao === 'voltar') {
                TV.segurarTecla(e);
            } else if (e.preventDefault) {
                e.preventDefault();
            }
            Tocador.tecla(acao);
            return;
        }
        if (acao === 'voltar') {
            TV.segurarTecla(e);
            if (estado.detalhe) {
                fecharDetalhe();
            } else {
                aviso('Para ir ao Smart Hub, use o botão SMART HUB do controle.');
            }
            return;
        }
        if (e.preventDefault) {
            e.preventDefault();
        }
        if (estado.detalhe) {
            teclaDetalhe(acao);
            return;
        }
        switch (acao) {
            case 'esquerda': mover(0, -1); break;
            case 'direita': mover(0, 1); break;
            case 'cima': mover(-1, 0); break;
            case 'baixo': mover(1, 0); break;
            case 'ok': ativar(); break;
            case 'vermelho': atualizarTudo(); break;
            case 'info': aviso('Minha TV ' + (cfg().versao || '') + ' · PC: ' + (Dados.servidor() || 'mesmo endereço'), 6000); break;
        }
    }

    function focarAncora() {
        var ancora = $('ancora');
        if (ancora && ancora.focus) {
            ancora.focus();
        }
    }

    // ---------- ciclo de vida ----------

    function iniciar() {
        window.onShow = function () { TV.aoMostrar(); };
        window.onshow = window.onShow;
        TV.pronto();
        focarAncora();

        if (!TV.naTV) {
            // Pré-visualização no PC: teclado em qualquer lugar da página.
            document.onkeydown = function (evento) {
                var e = evento || window.event;
                if (!e.target || e.target.id !== 'ancora') {
                    tecla(e);
                }
            };
            document.onclick = focarAncora;
        }

        atualizarRelogio();
        desenharClima();
        desenharLinhas();

        buscarStatus();
        buscarConfiguracao();
        buscarClima();
        buscarFilmes();
        buscarCanais();
        buscarJellyfin();

        estado.timers.push(setInterval(atualizarRelogio, 1000));
        estado.timers.push(setInterval(buscarStatus, 5 * 60 * 1000));
        estado.timers.push(setInterval(buscarClima, 15 * 60 * 1000));
        estado.timers.push(setInterval(buscarFilmes, 60 * 60 * 1000));
        estado.timers.push(setInterval(buscarJellyfin, 10 * 60 * 1000));
    }

    function encerrar() {
        // A Samsung pede: nunca fechar o app com vídeo tocando.
        Tocador.fechar(false);
        for (var i = 0; i < estado.timers.length; i++) {
            clearInterval(estado.timers[i]);
        }
        estado.timers = [];
    }

    return {
        iniciar: iniciar,
        encerrar: encerrar,
        tecla: tecla,
        estado: estado
    };
})();
