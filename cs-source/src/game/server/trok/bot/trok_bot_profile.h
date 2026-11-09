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
};

// Perfil final de um bot (nível + personalidade sorteada).
void TrokBot_BuildProfile( int iDificuldade, TrokBotProfile_t *pOut );

// Recarrega scripts/trok_bot_perfis.txt (também roda na primeira vez que um bot nasce).
void TrokBot_ReloadProfiles( void );

#endif // TROK_BOT_PROFILE_H
