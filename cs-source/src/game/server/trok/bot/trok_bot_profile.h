//=============================================================================
// Trok: perfil (dificuldade + personalidade) de cada bot.
//
// Os quatro níveis (facil, normal, dificil, expert) vêm de
// scripts/trok_bot_perfis.txt; o que faltar no arquivo usa os valores
// padrão deste código. Cada bot sorteia uma pequena variação em cima do
// nível (personalidade), para dois bots do mesmo nível não jogarem igual.
//
// Regra de equilíbrio: o nível muda reação, mira, giro, atenção e tática.
// Nenhum nível vê através de parede ou sabe onde está quem ele não percebeu.
//=============================================================================

#ifndef TROK_BOT_PROFILE_H
#define TROK_BOT_PROFILE_H
#ifdef _WIN32
#pragma once
#endif

// Estilo de jogo (papéis do CS competitivo). Sorteado por bot com os pesos de
// trok_bot_estilos; trok_bot_estilo força um para testar.
enum TrokBotStyle_t
{
	TROK_ESTILO_EQUILIBRADO = 0,	// faz um pouco de tudo
	TROK_ESTILO_ENTRY,				// entra primeiro, corre, procura a luta
	TROK_ESTILO_LURKER,				// sozinho por rotas longe do time, pega por trás
	TROK_ESTILO_ANCORA,				// segura um lugar por muito tempo e embosca
	TROK_ESTILO_SUPORTE,			// anda atrás de um colega para trocar o abate

	TROK_ESTILO_COUNT
};

const char *TrokBot_StyleName( int iStyle );

struct TrokBotProfile_t
{
	char	szNivel[ 16 ];

	// --- Percepção ---
	float	flReacaoMin;			// s: do "percebi" até poder reagir (tempo de reação visual)
	float	flReacaoMax;
	float	flGanhoPercepcao;		// quão rápido a "barra de percepção" enche (1 = humano médio)
	float	flCampoVisao;			// graus: campo total em que ainda dá para perceber algo (periferia)
	float	flAudicao;				// multiplicador do raio em que ouve passos

	// --- Mira ---
	float	flMiraErroInicial;		// graus: erro ao começar a mirar num alvo novo
	float	flMiraErroMinimo;		// graus: erro que sobra mesmo mirando há muito tempo
	float	flMiraPrecisaoAlvo;		// o erro mínimo também não passa disto × meia largura do alvo (alvo longe = mira mais fina, como gente faz)
	float	flMiraMeiaVida;			// s: tempo para o erro cair pela metade (foco)
	float	flMiraRastreio;			// graus de erro por 100 graus/s de movimento angular do alvo
	float	flMiraTremorFerido;		// graus somados ao erro quando leva dano
	float	flChanceCabeca;			// 0..1: chance de mirar na cabeça em vez do peito (perto)
	float	flGiroMax;				// graus/s: velocidade máxima de giro da cabeça
	float	flIntervaloRastreio;	// s: de quanto em quanto tempo reavalia para onde o alvo está indo

	// --- Movimento e tática ---
	float	flStrafeMin;			// s: duração mínima de um "lado" do strafe em combate
	float	flStrafeMax;
	float	flChanceAgachar;		// 0..1: chance de agachar no meio do tiroteio
	float	flAgressividade;		// 0..1: 0 = segura posição boa, 1 = caça o inimigo
	float	flUsoTatico;			// 0..1: quanto usa o mapa tático (lugares seguros, entradas)

	// --- Estilo (preenchido por TrokBot_BuildProfile, não vem do arquivo) ---
	int		iEstilo;				// TrokBotStyle_t
	float	flRotaVariedade;		// multiplica trok_bot_rota_variedade
	float	flRotaColegas;			// multiplica trok_bot_rota_colegas (lurker evita muito o time)
	float	flTempoSegurar;			// multiplica o tempo que segura um lugar
	bool	bEmboscada;				// segurando posição, espera o inimigo chegar perto ou se distrair para atirar
};

// Perfil final de um bot (nível + personalidade sorteada).
void TrokBot_BuildProfile( int iDificuldade, TrokBotProfile_t *pOut );

// Recarrega scripts/trok_bot_perfis.txt (também roda na primeira vez que um bot nasce).
void TrokBot_ReloadProfiles( void );

#endif // TROK_BOT_PROFILE_H
