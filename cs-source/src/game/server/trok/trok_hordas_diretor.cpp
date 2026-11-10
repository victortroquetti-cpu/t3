//=============================================================================
// Trok: diretor do modo Hordas (servidor). Ver shared/trok/trok_hordas.h e
// design/modo-hordas.md. Números em cfg/hordas.cfg; tipos por rodada, caixa e bônus em
// scripts/trok_hordas.txt; níveis da máquina em scripts/trok_upgrade.txt.
//
// Rodadas: intervalo -> rodada (zumbis nascem aos poucos, até o máximo vivo ao mesmo tempo)
// -> intervalo... A rodada acaba quando todos os zumbis dela morrem. Fim de jogo quando não
// sobra humano de pé (derrubados não contam); depois o mapa recomeça da rodada 1.
//
// Zumbis: bots NextBot do HL2MP no time dos zumbis, num grupo reaproveitado (morto volta a
// nascer como outro zumbi), porque cada zumbi ocupa uma vaga de jogador no servidor.
// O cérebro é o do Surto (CTrokBotZumbi: caça o humano mais perto e quebra barricada).
//=============================================================================

#include "cbase.h"
#include "trok/trok_hordas.h"
#include "trok/trok_surto.h"
#include "trok/trok_zumbis.h"
#include "trok_hordas_mapa.h"
#include "hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "team.h"
#include "bot/hl2mp_bot.h"
#include "NextBot/NextBotInterface.h"
#include "NextBot/NextBotIntentionInterface.h"
#include "NextBot/Player/NextBotPlayer.h"
#include "filesystem.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "explode.h"
#include "IEffects.h"
#include "weapon_parse.h"
#include "world.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Números (cfg/hordas.cfg). Fórmulas e tabela em design/modo-hordas.md.
//-----------------------------------------------------------------------------
// Ritmo
ConVar trok_hordas_preparo( "trok_hordas_preparo", "15", FCVAR_NOTIFY, "Hordas: segundos antes da rodada 1." );
ConVar trok_hordas_intervalo( "trok_hordas_intervalo", "10", FCVAR_NOTIFY, "Hordas: segundos entre as rodadas." );
ConVar trok_hordas_fim( "trok_hordas_fim", "12", FCVAR_NOTIFY, "Hordas: segundos mostrando o fim de jogo antes de recomeçar." );
ConVar trok_hordas_dificuldade( "trok_hordas_dificuldade", "0", FCVAR_NOTIFY | FCVAR_REPLICATED, "Hordas: 0 normal, 1 difícil, 2 extremo (como a Equipe Armada do CA)." );
ConVar trok_hordas_dif_vida( "trok_hordas_dif_vida", "1 1.2 1.5", FCVAR_NOTIFY, "Hordas: x vida dos zumbis por dificuldade (CA: 1 / 1,2 / 1,5)." );
ConVar trok_hordas_dif_dano( "trok_hordas_dif_dano", "1 1.25 1.6", FCVAR_NOTIFY, "Hordas: x dano dos zumbis nos humanos por dificuldade." );
ConVar trok_hordas_dif_quantidade( "trok_hordas_dif_quantidade", "1 1.15 1.3", FCVAR_NOTIFY, "Hordas: x zumbis por rodada por dificuldade." );
// Vida
ConVar trok_hordas_vida_inicial( "trok_hordas_vida_inicial", "150", FCVAR_NOTIFY, "Hordas: vida do zumbi normal na rodada 1." );
ConVar trok_hordas_vida_por_rodada( "trok_hordas_vida_por_rodada", "100", FCVAR_NOTIFY, "Hordas: vida somada por rodada até a rodada trok_hordas_vida_virada." );
ConVar trok_hordas_vida_virada( "trok_hordas_vida_virada", "10", FCVAR_NOTIFY, "Hordas: rodada em que a vida passa a multiplicar por trok_hordas_vida_fator." );
ConVar trok_hordas_vida_fator( "trok_hordas_vida_fator", "1.1", FCVAR_NOTIFY, "Hordas: x vida por rodada depois da virada." );
ConVar trok_hordas_vida_teto( "trok_hordas_vida_teto", "0", FCVAR_NOTIFY, "Hordas: teto da vida do zumbi normal (0 = sem teto)." );
ConVar trok_hordas_vida_jogadores( "trok_hordas_vida_jogadores", "0", FCVAR_NOTIFY, "Hordas: vida x (1 + N x log2(jogadores)). CA: 0,3. 0 = a vida não muda com jogadores." );
// Quantidade
ConVar trok_hordas_qtd_base( "trok_hordas_qtd_base", "24", FCVAR_NOTIFY, "Hordas: base de zumbis por rodada (CoD: 24)." );
ConVar trok_hordas_qtd_por_jogador( "trok_hordas_qtd_por_jogador", "6", FCVAR_NOTIFY, "Hordas: zumbis a mais por jogador (CoD: 6; sozinho conta meio)." );
ConVar trok_hordas_qtd_inicio( "trok_hordas_qtd_inicio", "0.25 0.3 0.5 0.7 0.9", FCVAR_NOTIFY, "Hordas: fração da quantidade nas primeiras rodadas." );
ConVar trok_hordas_qtd_teto( "trok_hordas_qtd_teto", "0", FCVAR_NOTIFY, "Hordas: teto de zumbis numa rodada (0 = sem teto)." );
ConVar trok_hordas_vivos_base( "trok_hordas_vivos_base", "10", FCVAR_NOTIFY, "Hordas: zumbis vivos ao mesmo tempo com 1 jogador." );
ConVar trok_hordas_vivos_por_jogador( "trok_hordas_vivos_por_jogador", "4", FCVAR_NOTIFY, "Hordas: zumbis vivos a mais por jogador." );
ConVar trok_hordas_vivos_max( "trok_hordas_vivos_max", "24", FCVAR_NOTIFY, "Hordas: teto de zumbis vivos (CoD: 24). Também limitado pelas vagas do servidor (maxplayers)." );
ConVar trok_hordas_vagas_livres( "trok_hordas_vagas_livres", "1", FCVAR_NOTIFY, "Hordas: vagas de jogador que os zumbis deixam livres (para alguém entrar)." );
ConVar trok_hordas_reuso_espera( "trok_hordas_reuso_espera", "1.5", FCVAR_NOTIFY, "Hordas: segundos morto antes de um bot zumbi voltar como outro zumbi." );
// Nascer
ConVar trok_hordas_nascer_intervalo( "trok_hordas_nascer_intervalo", "2", FCVAR_NOTIFY, "Hordas: segundos entre zumbis nascendo na rodada 1 (CoD: 2)." );
ConVar trok_hordas_nascer_fator( "trok_hordas_nascer_fator", "0.95", FCVAR_NOTIFY, "Hordas: x intervalo por rodada (CoD: 0,95)." );
ConVar trok_hordas_nascer_min( "trok_hordas_nascer_min", "0.15", FCVAR_NOTIFY, "Hordas: menor intervalo entre zumbis nascendo." );
ConVar trok_hordas_nascer_longe( "trok_hordas_nascer_longe", "450", FCVAR_NOTIFY, "Hordas: não nasce zumbi a menos disto (u) de um humano, se houver outro lugar." );
// Velocidade
ConVar trok_hordas_vel_inicial( "trok_hordas_vel_inicial", "0.6", FCVAR_NOTIFY, "Hordas: x velocidade do zumbi na rodada 1 (andando)." );
ConVar trok_hordas_vel_por_rodada( "trok_hordas_vel_por_rodada", "0.06", FCVAR_NOTIFY, "Hordas: velocidade somada por rodada." );
ConVar trok_hordas_vel_max( "trok_hordas_vel_max", "1.15", FCVAR_NOTIFY, "Hordas: teto da velocidade (correndo)." );
ConVar trok_hordas_vel_variacao( "trok_hordas_vel_variacao", "0.12", FCVAR_NOTIFY, "Hordas: sorteio +- na velocidade de cada zumbi." );
// Pontos
ConVar trok_hordas_pontos_inicio( "trok_hordas_pontos_inicio", "500", FCVAR_NOTIFY, "Hordas: pontos ao entrar (CoD: 500)." );
ConVar trok_hordas_pontos_acerto( "trok_hordas_pontos_acerto", "10", FCVAR_NOTIFY, "Hordas: pontos por acerto em zumbi." );
ConVar trok_hordas_pontos_abate( "trok_hordas_pontos_abate", "50", FCVAR_NOTIFY, "Hordas: pontos por abate no corpo." );
ConVar trok_hordas_pontos_cabeca( "trok_hordas_pontos_cabeca", "100", FCVAR_NOTIFY, "Hordas: pontos por abate na cabeça." );
ConVar trok_hordas_pontos_faca( "trok_hordas_pontos_faca", "130", FCVAR_NOTIFY, "Hordas: pontos por abate com faca." );
ConVar trok_hordas_pontos_reparo( "trok_hordas_pontos_reparo", "10", FCVAR_NOTIFY, "Hordas: pontos por tábua consertada." );
ConVar trok_hordas_pontos_reparo_teto( "trok_hordas_pontos_reparo_teto", "500", FCVAR_NOTIFY, "Hordas: teto de pontos de reparo por rodada por jogador." );
ConVar trok_hordas_pontos_bonus_todos( "trok_hordas_pontos_bonus_todos", "400", FCVAR_NOTIFY, "Hordas: pontos para cada um no Ataque aéreo." );
ConVar trok_hordas_pontos_reforco( "trok_hordas_pontos_reforco", "200", FCVAR_NOTIFY, "Hordas: pontos para cada um no Reforço." );
ConVar trok_hordas_perda_caido( "trok_hordas_perda_caido", "0.05", FCVAR_NOTIFY, "Hordas: fração dos pontos perdida ao cair." );
ConVar trok_hordas_perda_morte( "trok_hordas_perda_morte", "0.10", FCVAR_NOTIFY, "Hordas: fração dos pontos perdida ao morrer (sangrar até o fim)." );
// Humanos
ConVar trok_hordas_reserva_pentes( "trok_hordas_reserva_pentes", "6", FCVAR_NOTIFY, "Hordas: reserva cheia = pentes (sem melhoria) x isto x reserva do nível." );
ConVar trok_hordas_granadas_rodada( "trok_hordas_granadas_rodada", "2", FCVAR_NOTIFY, "Hordas: granadas ganhas no começo de cada rodada (até o máximo)." );
ConVar trok_hordas_regen_espera( "trok_hordas_regen_espera", "4", FCVAR_NOTIFY, "Hordas: segundos sem levar dano para a vida voltar." );
ConVar trok_hordas_regen( "trok_hordas_regen", "40", FCVAR_NOTIFY, "Hordas: vida por segundo que volta (0 = sem)." );
ConVar trok_hordas_caido_tempo( "trok_hordas_caido_tempo", "30", FCVAR_NOTIFY, "Hordas: segundos derrubado até morrer (CoD: 30)." );
ConVar trok_hordas_caido_velocidade( "trok_hordas_caido_velocidade", "0.35", FCVAR_NOTIFY, "Hordas: x velocidade derrubado (rastejando)." );
ConVar trok_hordas_reviver_tempo( "trok_hordas_reviver_tempo", "3", FCVAR_NOTIFY, "Hordas: segundos segurando E para reviver." );
ConVar trok_hordas_reviver_distancia( "trok_hordas_reviver_distancia", "110", FCVAR_NOTIFY, "Hordas: distância (u) para reviver." );
ConVar trok_hordas_reviver_vida( "trok_hordas_reviver_vida", "0.5", FCVAR_NOTIFY, "Hordas: fração da vida máxima ao ser revivido." );
ConVar trok_hordas_kit_tempo( "trok_hordas_kit_tempo", "4", FCVAR_NOTIFY, "Hordas: segundos até o kit de reanimação levantar quem caiu." );
ConVar trok_hordas_aliados( "trok_hordas_aliados", "0", FCVAR_NOTIFY, "Hordas: bots aliados (humanos) que entram com os jogadores." );
ConVar trok_hordas_bot_dificuldade( "trok_hordas_bot_dificuldade", "1", FCVAR_NOTIFY, "Hordas: dificuldade dos bots (0-3; zumbis e aliados)." );
// Bônus
ConVar trok_hordas_bonus_chance( "trok_hordas_bonus_chance", "0.025", FCVAR_NOTIFY, "Hordas: chance de um zumbi abatido soltar um bônus." );
ConVar trok_hordas_bonus_max( "trok_hordas_bonus_max", "4", FCVAR_NOTIFY, "Hordas: bônus por rodada (CoD: 4)." );
ConVar trok_hordas_bonus_duracao( "trok_hordas_bonus_duracao", "30", FCVAR_NOTIFY, "Hordas: segundos dos bônus com tempo (CoD: 30)." );
ConVar trok_hordas_bonus_chao( "trok_hordas_bonus_chao", "25", FCVAR_NOTIFY, "Hordas: segundos que o bônus fica no chão." );
ConVar trok_hordas_arquivo( "trok_hordas_arquivo", "scripts/trok_hordas.txt", FCVAR_NOTIFY, "Hordas: tipos por rodada, rodadas especiais, caixa e bônus." );

extern ConVar trok_surto_cor_zumbi;
extern ConVar trok_hordas_colete;

#define HORDAS_BOT_ZUMBI	"Infectado"
#define HORDAS_BOT_ALIADO	"Aliado"

//-----------------------------------------------------------------------------
// Script (scripts/trok_hordas.txt)
//-----------------------------------------------------------------------------
struct HordasTipo_t
{
	char szNome[ 32 ];
	int iClasse;			// tipo em trok_zumbis.txt
	int iAPartir;			// primeira rodada
	float flPeso, flPesoPorRodada, flPesoMax;
	int iMaxPorRodada;		// 0 = sem limite
	float flVida, flVel, flPontos, flEscala;
};

struct HordasEspecial_t
{
	int iClasse;
	float flVida, flVel, flPontos, flEscala;
	int iPrimeira, iMin, iMax;		// matilha: primeira rodada e de quantas em quantas
	int iPorJogador;				// matilha: zumbis por jogador
	int iACada;						// chefe: a cada N rodadas (a partir de N)
	int iQuantos;					// chefe: quantos
	int iBonusPontos;				// chefe: pontos para quem matar
};

struct HordasArmaCaixa_t
{
	char szArma[ 48 ];
	float flPeso;
};

static CUtlVector< HordasTipo_t > s_Tipos;
static HordasEspecial_t s_Matilha, s_Chefe;
static CUtlVector< HordasArmaCaixa_t > s_Caixa;
static float s_aBonusPeso[ HORDAS_BONUS_NUM ];

static void LerEspecial( KeyValues *pKV, HordasEspecial_t &e, const char *pszClassePadrao )
{
	memset( &e, 0, sizeof( e ) );
	e.iClasse = MAX( 0, TrokZumbis_Achar( pKV ? pKV->GetString( "classe", pszClassePadrao ) : pszClassePadrao ) );
	e.flVida = pKV ? pKV->GetFloat( "vida", 1.0f ) : 1.0f;
	e.flVel = pKV ? pKV->GetFloat( "velocidade", 1.0f ) : 1.0f;
	e.flPontos = pKV ? pKV->GetFloat( "pontos", 1.0f ) : 1.0f;
	e.flEscala = pKV ? pKV->GetFloat( "escala", 1.0f ) : 1.0f;
	e.iPrimeira = pKV ? pKV->GetInt( "primeira", 0 ) : 0;
	e.iMin = pKV ? pKV->GetInt( "a_cada_min", 5 ) : 5;
	e.iMax = pKV ? pKV->GetInt( "a_cada_max", 7 ) : 7;
	e.iPorJogador = pKV ? pKV->GetInt( "por_jogador", 6 ) : 6;
	e.iACada = pKV ? pKV->GetInt( "a_cada", 0 ) : 0;
	e.iQuantos = pKV ? pKV->GetInt( "quantos", 1 ) : 1;
	e.iBonusPontos = pKV ? pKV->GetInt( "bonus_pontos", 0 ) : 0;
}

static void Hordas_CarregarScript( void )
{
	s_Tipos.RemoveAll();
	s_Caixa.RemoveAll();
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
		s_aBonusPeso[ b ] = 1.0f;

	KeyValues *pKV = new KeyValues( "TrokHordas" );
	const bool bOk = pKV->LoadFromFile( filesystem, trok_hordas_arquivo.GetString(), "MOD" );
	if ( !bOk )
		Warning( "Trok: não achei %s; Hordas só com o zumbi normal.\n", trok_hordas_arquivo.GetString() );

	KeyValues *pTipos = bOk ? pKV->FindKey( "tipos" ) : NULL;
	for ( KeyValues *p = pTipos ? pTipos->GetFirstTrueSubKey() : NULL; p; p = p->GetNextTrueSubKey() )
	{
		HordasTipo_t t;
		memset( &t, 0, sizeof( t ) );
		Q_strncpy( t.szNome, p->GetName(), sizeof( t.szNome ) );
		t.iClasse = TrokZumbis_Achar( p->GetString( "classe", p->GetName() ) );
		if ( t.iClasse < 0 )
		{
			Warning( "Trok: Hordas: tipo '%s' cita classe de zumbi que não existe.\n", p->GetName() );
			continue;
		}
		t.iAPartir = p->GetInt( "a_partir", 1 );
		t.flPeso = p->GetFloat( "peso", 10.0f );
		t.flPesoPorRodada = p->GetFloat( "peso_por_rodada", 0.0f );
		t.flPesoMax = p->GetFloat( "peso_max", 1000.0f );
		t.iMaxPorRodada = p->GetInt( "max_por_rodada", 0 );
		t.flVida = p->GetFloat( "vida", 1.0f );
		t.flVel = p->GetFloat( "velocidade", 1.0f );
		t.flPontos = p->GetFloat( "pontos", 1.0f );
		t.flEscala = p->GetFloat( "escala", 1.0f );
		s_Tipos.AddToTail( t );
	}
	if ( s_Tipos.Count() == 0 )
	{
		HordasTipo_t t;
		memset( &t, 0, sizeof( t ) );
		Q_strncpy( t.szNome, "normal", sizeof( t.szNome ) );
		t.iAPartir = 1;
		t.flPeso = t.flPesoMax = 100.0f;
		t.flVida = t.flVel = t.flPontos = t.flEscala = 1.0f;
		s_Tipos.AddToTail( t );
	}

	LerEspecial( bOk ? pKV->FindKey( "matilha" ) : NULL, s_Matilha, "striker" );
	LerEspecial( bOk ? pKV->FindKey( "chefe" ) : NULL, s_Chefe, "crusher_toxico" );

	KeyValues *pCaixa = bOk ? pKV->FindKey( "caixa" ) : NULL;
	for ( KeyValues *p = pCaixa ? pCaixa->GetFirstValue() : NULL; p; p = p->GetNextValue() )
	{
		HordasArmaCaixa_t a;
		Q_strncpy( a.szArma, p->GetName(), sizeof( a.szArma ) );
		a.flPeso = MAX( 0.0f, p->GetFloat() );
		s_Caixa.AddToTail( a );
	}

	static const char *s_pszBonus[ HORDAS_BONUS_NUM ] = { "municao", "abate_certo", "pontos_dobro", "ataque_aereo", "reforco", "caixa_barata" };
	KeyValues *pBonus = bOk ? pKV->FindKey( "bonus" ) : NULL;
	if ( pBonus )
		for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
			s_aBonusPeso[ b ] = MAX( 0.0f, pBonus->GetFloat( s_pszBonus[ b ], 1.0f ) );

	pKV->deleteThis();
}

// Sons, modelos e armas do modo, enquanto o mapa carrega (as regras nascem no precache do mundo).
static void Hordas_Precache( void )
{
	static const char *s_pszSons[] =
	{
		"Trok.Hordas.Compra", "Trok.Hordas.SemPontos", "Trok.Hordas.Porta", "Trok.Hordas.Melhoria",
		"Trok.Hordas.Caixa", "Trok.Hordas.Energia", "Trok.Hordas.RodadaInicio", "Trok.Hordas.RodadaFim",
		"Trok.Hordas.Bonus", "Trok.Hordas.Caido", "Trok.Hordas.Reviver",
	};
	for ( int i = 0; i < (int)ARRAYSIZE( s_pszSons ); ++i )
		CBaseEntity::PrecacheScriptSound( s_pszSons[ i ] );
	UTIL_PrecacheOther( "trok_hordas_bonus" );
	UTIL_PrecacheOther( "weapon_trok_garras" );
	FOR_EACH_VEC( s_Caixa, i )
		UTIL_PrecacheOther( s_Caixa[ i ].szArma );
}

//-----------------------------------------------------------------------------
// Estado do diretor (servidor)
//-----------------------------------------------------------------------------
static bool s_bAtivo = false;
static CHandle< CTrokHordasEstado > s_hEstado;
static CUtlVector< CHandle< CBasePlayer > > s_Pool;		// bots zumbis
static CUtlVector< CHandle< CBasePlayer > > s_Aliados;	// bots aliados
static bool s_bCriandoBot = false;
static bool s_bMatando = false;			// dano que mata derrubado / limpa zumbi (passa pelas regras)
static bool s_bAtaqueAereo = false;		// abates do Ataque aéreo não dão pontos por abate
static unsigned int s_iAreas = 0;		// bits das áreas abertas (0 e 1 sempre)

// Rodada
static int s_iParaNascer = 0;
static int s_iChefesParaNascer = 0;
static int s_iTotalRodada = 0;
static float s_flProxNascer = 0.0f;
static float s_flIntervaloNascer = 2.0f;
static int s_iProxMatilha = 0;
static int s_iBonusRodada = 0;
static CUtlVector< int > s_iTiposNaRodada;
static Vector s_vecUltimaMorte = vec3_origin;
static float s_flProxManutencao = 0.0f;
static float s_flUltimoThink = 0.0f;

// Pedido de nascimento em andamento (o Spawn do bot chama TrokHordas_PlayerSpawn)
static int s_iPedido = 0;
static int s_iPedidoTipo = -1;			// índice em s_Tipos; -2 matilha; -3 chefe
static Vector s_vecPedido;
static QAngle s_angPedido;

// Por jogador
static int s_aUserId[ MAX_PLAYERS + 1 ];
static float s_aMorte[ MAX_PLAYERS + 1 ];
static float s_aUltimoDano[ MAX_PLAYERS + 1 ];
static float s_aRegenAcum[ MAX_PLAYERS + 1 ];
static int s_aRevivendo[ MAX_PLAYERS + 1 ];			// quem este jogador está revivendo (0 = ninguém)
static float s_aAutoReviver[ MAX_PLAYERS + 1 ];		// kit: quando levanta sozinho (0 = não)
static int s_aReparoRodada[ MAX_PLAYERS + 1 ];
static CHandle< CBaseCombatWeapon > s_hArmaSlot[ MAX_PLAYERS + 1 ][ TROK_HORDAS_SLOTS ];
// Por zumbi
static int s_aZTipo[ MAX_PLAYERS + 1 ];				// como s_iPedidoTipo
static float s_aZVelBase[ MAX_PLAYERS + 1 ];
static float s_aZLentoAte[ MAX_PLAYERS + 1 ], s_aZLentoFator[ MAX_PLAYERS + 1 ];
static float s_aZFogoAte[ MAX_PLAYERS + 1 ], s_aZFogoDps[ MAX_PLAYERS + 1 ];
static CHandle< CBasePlayer > s_hZFogoDono[ MAX_PLAYERS + 1 ];
static float s_flProxFogo = 0.0f;

static float ValorDaLista( const char *pszLista, int i, float flPadrao )
{
	CUtlVector< char * > partes;
	V_SplitString( pszLista, " ", partes );
	float f = flPadrao;
	if ( partes.Count() > 0 )
		f = (float)atof( partes[ clamp( i, 0, partes.Count() - 1 ) ] );
	partes.PurgeAndDeleteElements();
	return f;
}

static float Dif( const ConVar &cv )
{
	return ValorDaLista( cv.GetString(), clamp( trok_hordas_dificuldade.GetInt(), 0, 2 ), 1.0f );
}

static bool EhDoPool( CBasePlayer *p )
{
	if ( !p )
		return false;
	FOR_EACH_VEC( s_Pool, i )
		if ( s_Pool[ i ].Get() == p )
			return true;
	return false;
}

static bool EhAliadoBot( CBasePlayer *p )
{
	FOR_EACH_VEC( s_Aliados, i )
		if ( s_Aliados[ i ].Get() == p )
			return true;
	return false;
}

static bool Jogando( CBasePlayer *p )
{
	return p && p->IsConnected() && p->GetTeamNumber() != TEAM_SPECTATOR && p->GetTeamNumber() != TEAM_UNASSIGNED;
}

// Humano do modo: jogando, não é zumbi do grupo.
static bool EhHumano( CBasePlayer *p )
{
	return Jogando( p ) && !EhDoPool( p ) && p->GetTeamNumber() == TEAM_TROK_HUMANOS;
}

static bool DePe( CBasePlayer *p )
{
	return EhHumano( p ) && p->IsAlive() && !TrokHordas_Caido( p );
}

static int ContarHumanos( bool bSoDePe = false )
{
	int n = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( bSoDePe ? DePe( p ) : EhHumano( p ) )
			++n;
	}
	return n;
}

static int ContarZumbisVivos( void )
{
	int n = 0;
	FOR_EACH_VEC( s_Pool, i )
	{
		CBasePlayer *p = s_Pool[ i ].Get();
		if ( p && p->IsAlive() )
			++n;
	}
	return n;
}

static void TrocarTime( CBasePlayer *pPlayer, int iTeam )
{
	if ( pPlayer->GetTeamNumber() == iTeam )
		return;
	pPlayer->CBasePlayer::ChangeTeam( iTeam, false, true );	// sem matar (a do HL2MP mata)
	CHL2MP_Player *pHL2MP = dynamic_cast< CHL2MP_Player * >( pPlayer );
	if ( pHL2MP )
		pHL2MP->SetPlayerTeamModel();
	INextBot *pBot = pPlayer->MyNextBotPointer();
	if ( pBot && pBot->GetIntentionInterface() )
		pBot->GetIntentionInterface()->Reset();
}

static CTrokHordasEstado *Estado( void )
{
	return s_hEstado.Get();
}

//-----------------------------------------------------------------------------
// Fórmulas da rodada (design/modo-hordas.md, "Curvas")
//-----------------------------------------------------------------------------
static int Jogadores( void )
{
	return MAX( 1, ContarHumanos() );
}

static float VidaDaRodada( int R )
{
	const float ini = trok_hordas_vida_inicial.GetFloat();
	const float por = trok_hordas_vida_por_rodada.GetFloat();
	const int iVirada = MAX( 2, trok_hordas_vida_virada.GetInt() );
	float v;
	if ( R < iVirada )
		v = ini + por * ( R - 1 );
	else
		v = ( ini + por * ( iVirada - 2 ) ) * powf( MAX( 1.0f, trok_hordas_vida_fator.GetFloat() ), (float)( R - iVirada + 1 ) );
	v *= Dif( trok_hordas_dif_vida );
	const int J = Jogadores();
	if ( J > 1 && trok_hordas_vida_jogadores.GetFloat() > 0.0f )
		v *= 1.0f + trok_hordas_vida_jogadores.GetFloat() * ( logf( (float)J ) / logf( 2.0f ) );
	if ( trok_hordas_vida_teto.GetFloat() > 0.0f )
		v = MIN( v, trok_hordas_vida_teto.GetFloat() );
	return MAX( 1.0f, v );
}

static int QuantidadeDaRodada( int R )
{
	const int J = Jogadores();
	float mult = MAX( 1.0f, R / 5.0f );
	if ( R >= 10 )
		mult *= R * 0.15f;
	const float por = trok_hordas_qtd_por_jogador.GetFloat();
	const float extra = ( J <= 1 ) ? 0.5f * por * mult : ( J - 1 ) * por * mult;
	float total = trok_hordas_qtd_base.GetFloat() + extra;
	if ( R <= 5 )
		total *= ValorDaLista( trok_hordas_qtd_inicio.GetString(), R - 1, 1.0f );
	total *= Dif( trok_hordas_dif_quantidade );
	if ( trok_hordas_qtd_teto.GetInt() > 0 )
		total = MIN( total, trok_hordas_qtd_teto.GetFloat() );
	return MAX( 1, (int)total );
}

static float VelocidadeDaRodada( int R )
{
	const float v = MIN( trok_hordas_vel_max.GetFloat(), trok_hordas_vel_inicial.GetFloat() + trok_hordas_vel_por_rodada.GetFloat() * ( R - 1 ) );
	const float k = clamp( trok_hordas_vel_variacao.GetFloat(), 0.0f, 0.9f );
	return v * RandomFloat( 1.0f - k, 1.0f + k );
}

static int MaxVivos( void )
{
	int n = trok_hordas_vivos_base.GetInt() + trok_hordas_vivos_por_jogador.GetInt() * ( Jogadores() - 1 );
	return clamp( n, 1, MAX( 1, trok_hordas_vivos_max.GetInt() ) );
}

static int SortearTipo( int R )
{
	float flTotal = 0.0f;
	CUtlVector< float > pesos;
	FOR_EACH_VEC( s_Tipos, i )
	{
		const HordasTipo_t &t = s_Tipos[ i ];
		float p = 0.0f;
		const int iJa = s_iTiposNaRodada.IsValidIndex( i ) ? s_iTiposNaRodada[ i ] : 0;
		if ( R >= t.iAPartir && ( t.iMaxPorRodada <= 0 || iJa < t.iMaxPorRodada ) )
			p = MIN( t.flPesoMax, t.flPeso + t.flPesoPorRodada * ( R - t.iAPartir ) );
		pesos.AddToTail( MAX( 0.0f, p ) );
		flTotal += MAX( 0.0f, p );
	}
	if ( flTotal <= 0.0f )
		return 0;
	float r = RandomFloat( 0.0f, flTotal );
	FOR_EACH_VEC( pesos, i )
	{
		r -= pesos[ i ];
		if ( r <= 0.0f )
			return i;
	}
	return 0;
}

//-----------------------------------------------------------------------------
// Pontos
//-----------------------------------------------------------------------------
void TrokHordas_DarPontos( CBasePlayer *pPlayer, int iPontos, bool bDobro )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado || !pPlayer || iPontos == 0 )
		return;
	if ( bDobro && iPontos > 0 && pEstado->BonusAtivo( HORDAS_BONUS_PONTOS_DOBRO ) )
		iPontos *= 2;
	const int i = pPlayer->entindex();
	pEstado->m_aPontos.Set( i, clamp( pEstado->m_aPontos[ i ] + iPontos, 0, ( 1 << 24 ) - 1 ) );
}

bool TrokHordas_Pagar( CBasePlayer *pPlayer, int iPreco )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado || !pPlayer )
		return false;
	const int i = pPlayer->entindex();
	if ( pEstado->m_aPontos[ i ] < iPreco )
	{
		ClientPrint( pPlayer, HUD_PRINTCENTER, "PONTOS INSUFICIENTES" );
		pPlayer->EmitSound( "Trok.Hordas.SemPontos" );
		return false;
	}
	pEstado->m_aPontos.Set( i, pEstado->m_aPontos[ i ] - MAX( 0, iPreco ) );
	return true;
}

void TrokHordas_PontosReparo( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;
	const int i = pPlayer->entindex();
	const int iPontos = trok_hordas_pontos_reparo.GetInt();
	if ( s_aReparoRodada[ i ] + iPontos > trok_hordas_pontos_reparo_teto.GetInt() )
		return;
	s_aReparoRodada[ i ] += iPontos;
	TrokHordas_DarPontos( pPlayer, iPontos );
}

//-----------------------------------------------------------------------------
// Mapa: áreas e energia
//-----------------------------------------------------------------------------
bool TrokHordas_AreaAberta( int iArea )
{
	return iArea <= 1 || iArea >= 32 || ( s_iAreas & ( 1u << iArea ) ) != 0;
}

void TrokHordas_AbrirArea( int iArea )
{
	if ( iArea <= 1 || iArea >= 32 || TrokHordas_AreaAberta( iArea ) )
		return;
	s_iAreas |= ( 1u << iArea );
	TrokHordas_DispararMapa( HORDAS_EV_AREA, iArea );
}

void TrokHordas_LigarEnergia( void )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado || pEstado->IsEnergia() )
		return;
	pEstado->m_bEnergia = true;
	CBroadcastRecipientFilter filter;
	CBaseEntity::EmitSound( filter, 0, "Trok.Hordas.Energia" );
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "A ENERGIA FOI LIGADA" );
	TrokHordas_DispararMapa( HORDAS_EV_ENERGIA, 0 );
}

//-----------------------------------------------------------------------------
// Armas, munição e melhoria
//-----------------------------------------------------------------------------
static void DefinirNivel( CBasePlayer *pPlayer, int iSlot, int iNivel )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado || !pPlayer || iSlot < 0 || iSlot >= TROK_HORDAS_SLOTS )
		return;
	pEstado->m_aNivel.Set( pPlayer->entindex() * TROK_HORDAS_SLOTS + iSlot, clamp( iNivel, 0, TROK_UPGRADE_MAX ) );
}

void TrokHordas_EncherMunicao( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma )
{
	if ( !pPlayer || !pArma )
		return;
	const int iTipo = pArma->GetPrimaryAmmoType();
	if ( pArma->UsesClipsForAmmo1() )
	{
		pArma->m_iClip1 = pArma->GetMaxClip1();
		if ( iTipo >= 0 )
		{
			const int iNivel = TrokHordas_NivelArma( pArma );
			const float flRes = TrokUpgrade().niveis[ iNivel ].flReserva;
			const int iCheia = (int)( pArma->GetWpnData().iMaxClip1 * MAX( 1, trok_hordas_reserva_pentes.GetInt() ) * flRes );
			pPlayer->SetAmmoCount( MAX( pPlayer->GetAmmoCount( iTipo ), MIN( iCheia, 999 ) ), iTipo );
		}
	}
	else if ( iTipo >= 0 )
	{
		// Explosivos e armas sem pente: o máximo do tipo (trok_max_*).
		pPlayer->SetAmmoCount( MAX( pPlayer->GetAmmoCount( iTipo ), GetAmmoDef()->MaxCarry( iTipo ) ), iTipo );
	}
}

void TrokHordas_DarArma( CBasePlayer *pPlayer, const char *pszArma )
{
	if ( !pPlayer || !pszArma || !pszArma[ 0 ] )
		return;
	CBaseCombatWeapon *pTem = pPlayer->Weapon_OwnsThisType( pszArma );
	if ( pTem )
	{
		TrokHordas_EncherMunicao( pPlayer, pTem );
		pPlayer->Weapon_Switch( pTem );
		return;
	}
	pPlayer->GiveNamedItem( pszArma );
	CBaseCombatWeapon *pNova = pPlayer->Weapon_OwnsThisType( pszArma );
	if ( !pNova )
		return;

	// Inventário do CA: uma arma por classe (principal, secundária...). A velha da mesma classe sai.
	const int iSlot = pNova->GetSlot();
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pW = pPlayer->GetWeapon( i );
		if ( !pW || pW == pNova || pW->GetSlot() != iSlot )
			continue;
		if ( pPlayer->GetActiveWeapon() == pW )
			pW->Holster( pNova );
		pPlayer->Weapon_Detach( pW );
		UTIL_Remove( pW );
	}
	if ( iSlot >= 0 && iSlot < TROK_HORDAS_SLOTS )
	{
		DefinirNivel( pPlayer, iSlot, 0 );
		s_hArmaSlot[ pPlayer->entindex() ][ iSlot ] = pNova;
	}
	TrokHordas_EncherMunicao( pPlayer, pNova );
	pPlayer->Weapon_Switch( pNova );
}

bool TrokHordas_Melhorar( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma )
{
	if ( !pPlayer || !pArma )
		return false;
	const TrokUpgrade_t &up = TrokUpgrade();
	const int iNivel = TrokHordas_NivelArma( pArma ) + 1;
	if ( iNivel > up.nNiveis )
		return false;
	const int iSlot = pArma->GetSlot();
	DefinirNivel( pPlayer, iSlot, iNivel );
	if ( iSlot >= 0 && iSlot < TROK_HORDAS_SLOTS )
		s_hArmaSlot[ pPlayer->entindex() ][ iSlot ] = pArma;
	TrokHordas_EncherMunicao( pPlayer, pArma );
	const TrokUpgradeNivel_t &n = up.niveis[ iNivel ];
	if ( n.iCor[ 0 ] >= 0 )
		pArma->SetRenderColor( clamp( n.iCor[ 0 ], 0, 255 ), clamp( n.iCor[ 1 ], 0, 255 ), clamp( n.iCor[ 2 ], 0, 255 ) );
	ClientPrint( pPlayer, HUD_PRINTCENTER, UTIL_VarArgs( "ARMA MELHORADA: %s", n.szNome ) );
	return true;
}

const char *TrokHordas_SortearArmaCaixa( CBasePlayer *pPlayer )
{
	if ( s_Caixa.Count() == 0 )
		Hordas_CarregarScript();
	float flTotal = 0.0f;
	FOR_EACH_VEC( s_Caixa, i )
	{
		// Não oferece a arma que o jogador já tem.
		if ( pPlayer && pPlayer->Weapon_OwnsThisType( s_Caixa[ i ].szArma ) )
			continue;
		flTotal += s_Caixa[ i ].flPeso;
	}
	if ( flTotal <= 0.0f )
		return s_Caixa.Count() ? s_Caixa[ 0 ].szArma : NULL;
	float r = RandomFloat( 0.0f, flTotal );
	FOR_EACH_VEC( s_Caixa, i )
	{
		if ( pPlayer && pPlayer->Weapon_OwnsThisType( s_Caixa[ i ].szArma ) )
			continue;
		r -= s_Caixa[ i ].flPeso;
		if ( r <= 0.0f )
			return s_Caixa[ i ].szArma;
	}
	return s_Caixa.Tail().szArma;
}

void TrokHordas_DarKit( CBasePlayer *pPlayer )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( pEstado && pPlayer )
		pEstado->m_aKit.Set( pPlayer->entindex(), 1 );
}

// Rodada nova: granadas (até o máximo do tipo), como o CoD dá a cada rodada.
static void DarGranadas( CBasePlayer *pPlayer )
{
	const int n = trok_hordas_granadas_rodada.GetInt();
	if ( n <= 0 )
		return;
	CBaseCombatWeapon *pGranada = pPlayer->Weapon_OwnsThisType( "weapon_trok_granada" );
	if ( !pGranada )
		return;
	const int iTipo = pGranada->GetPrimaryAmmoType();
	if ( iTipo >= 0 )
		pPlayer->SetAmmoCount( MIN( GetAmmoDef()->MaxCarry( iTipo ), pPlayer->GetAmmoCount( iTipo ) + n ), iTipo );
}

//-----------------------------------------------------------------------------
// Bônus que caem (entidade trok_hordas_bonus)
//-----------------------------------------------------------------------------
static const char *s_pszNomeBonus[ HORDAS_BONUS_NUM ] =
{
	"REABASTECIMENTO", "ABATE CERTO", "PONTOS EM DOBRO", "ATAQUE AÉREO", "REFORÇO", "CAIXA BARATA"
};
static const int s_aCorBonus[ HORDAS_BONUS_NUM ][ 3 ] =
{
	{ 120, 200, 255 }, { 255, 80, 60 }, { 255, 210, 60 }, { 255, 140, 40 }, { 140, 255, 120 }, { 200, 120, 255 }
};

static void AplicarBonus( int iBonus, CBasePlayer *pQuem );

class CTrokHordasBonus : public CBaseAnimating
{
public:
	DECLARE_CLASS( CTrokHordasBonus, CBaseAnimating );
	DECLARE_DATADESC();

	int m_iBonus;
	float m_flSome;

	virtual void Precache( void )
	{
		PrecacheModel( "models/items/boxmrounds.mdl" );	// TROCAR (HL2): modelo próprio por bônus
		PrecacheScriptSound( "Trok.Hordas.Bonus" );
	}
	virtual void Spawn( void )
	{
		Precache();
		SetModel( "models/items/boxmrounds.mdl" );
		SetMoveType( MOVETYPE_NONE );
		SetSolid( SOLID_BBOX );
		AddSolidFlags( FSOLID_TRIGGER | FSOLID_NOT_SOLID );
		UTIL_SetSize( this, Vector( -20, -20, -8 ), Vector( 20, 20, 40 ) );
		const int b = clamp( m_iBonus, 0, HORDAS_BONUS_NUM - 1 );
		SetRenderColor( s_aCorBonus[ b ][ 0 ], s_aCorBonus[ b ][ 1 ], s_aCorBonus[ b ][ 2 ] );
		SetRenderMode( kRenderTransColor );
		m_nRenderFX = kRenderFxPulseSlow;
		m_flSome = gpGlobals->curtime + MAX( 5.0f, trok_hordas_bonus_chao.GetFloat() );
		SetTouch( &CTrokHordasBonus::Tocar );
		SetThink( &CTrokHordasBonus::Pensar );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}
	void Pensar( void )
	{
		if ( gpGlobals->curtime >= m_flSome )
		{
			UTIL_Remove( this );
			return;
		}
		QAngle ang = GetLocalAngles();
		ang.y = anglemod( ang.y + 6.0f );
		SetLocalAngles( ang );
		// Últimos 5 s piscando.
		const bool bPisca = ( m_flSome - gpGlobals->curtime ) < 5.0f && ( (int)( gpGlobals->curtime * 4.0f ) % 2 ) == 0;
		if ( bPisca )
			AddEffects( EF_NODRAW );
		else
			RemoveEffects( EF_NODRAW );
		SetNextThink( gpGlobals->curtime + 0.05f );
	}
	void Tocar( CBaseEntity *pOther )
	{
		CBasePlayer *pPlayer = ToBasePlayer( pOther );
		if ( !DePe( pPlayer ) )
			return;
		SetTouch( NULL );
		AplicarBonus( m_iBonus, pPlayer );
		UTIL_Remove( this );
	}
};

BEGIN_DATADESC( CTrokHordasBonus )
	DEFINE_THINKFUNC( Pensar ),
	DEFINE_ENTITYFUNC( Tocar ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( trok_hordas_bonus, CTrokHordasBonus );

static void SoltarBonus( const Vector &vecPos, int iBonus )
{
	CTrokHordasBonus *p = (CTrokHordasBonus *)CreateEntityByName( "trok_hordas_bonus" );
	if ( !p )
		return;
	p->m_iBonus = iBonus;
	p->SetAbsOrigin( vecPos + Vector( 0, 0, 12 ) );
	DispatchSpawn( p );
}

static int SortearBonus( void )
{
	float pesos[ HORDAS_BONUS_NUM ];
	float flTotal = 0.0f;
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
	{
		pesos[ b ] = s_aBonusPeso[ b ];
		if ( b == HORDAS_BONUS_REFORCO && TrokHordas_NumGrades() == 0 )
			pesos[ b ] = 0.0f;
		if ( b == HORDAS_BONUS_CAIXA_BARATA && !gEntList.FindEntityByClassname( NULL, "func_trok_caixa_misteriosa" ) )
			pesos[ b ] = 0.0f;
		flTotal += pesos[ b ];
	}
	if ( flTotal <= 0.0f )
		return HORDAS_BONUS_MUNICAO;
	float r = RandomFloat( 0.0f, flTotal );
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
	{
		r -= pesos[ b ];
		if ( r <= 0.0f )
			return b;
	}
	return HORDAS_BONUS_MUNICAO;
}

static void MatarZumbi( CBasePlayer *pZumbi, CBaseEntity *pAtacante, int iTipoDano )
{
	if ( !pZumbi || !pZumbi->IsAlive() )
		return;
	s_bMatando = true;
	CTakeDamageInfo info( pAtacante ? pAtacante : GetWorldEntity(), pAtacante ? pAtacante : GetWorldEntity(), (float)( pZumbi->GetHealth() + 100000 ), iTipoDano );
	pZumbi->TakeDamage( info );
	s_bMatando = false;
}

static void AplicarBonus( int iBonus, CBasePlayer *pQuem )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;
	iBonus = clamp( iBonus, 0, HORDAS_BONUS_NUM - 1 );
	const float flAgora = gpGlobals->curtime;
	pEstado->m_iUltimoBonus = iBonus;
	pEstado->m_flUltimoBonus = flAgora;
	CBroadcastRecipientFilter filter;
	CBaseEntity::EmitSound( filter, 0, "Trok.Hordas.Bonus" );

	switch ( iBonus )
	{
	case HORDAS_BONUS_MUNICAO:
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *p = UTIL_PlayerByIndex( i );
			if ( !EhHumano( p ) || !p->IsAlive() )
				continue;
			for ( int k = 0; k < p->WeaponCount(); ++k )
			{
				CBaseCombatWeapon *pW = p->GetWeapon( k );
				if ( pW && !FClassnameIs( pW, "weapon_trok_faca" ) )
				{
					// Reabastecimento enche a reserva; o pente fica como está (CoD).
					const int iClip = pW->m_iClip1;
					TrokHordas_EncherMunicao( p, pW );
					if ( pW->UsesClipsForAmmo1() )
						pW->m_iClip1 = iClip;
				}
			}
		}
		break;
	case HORDAS_BONUS_ABATE_CERTO:
	case HORDAS_BONUS_PONTOS_DOBRO:
	case HORDAS_BONUS_CAIXA_BARATA:
		pEstado->m_aBonusAte.Set( iBonus, flAgora + MAX( 1.0f, trok_hordas_bonus_duracao.GetFloat() ) );
		break;
	case HORDAS_BONUS_ATAQUE_AEREO:
		s_bAtaqueAereo = true;
		FOR_EACH_VEC( s_Pool, i )
		{
			CBasePlayer *p = s_Pool[ i ].Get();
			if ( !p || !p->IsAlive() || s_aZTipo[ p->entindex() ] == -3 )
				continue;	// o chefe não morre com o ataque aéreo
			ExplosionCreate( p->WorldSpaceCenter(), vec3_angle, NULL, 0, 0, false );
			MatarZumbi( p, pQuem, DMG_BLAST );
		}
		s_bAtaqueAereo = false;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			if ( EhHumano( UTIL_PlayerByIndex( i ) ) )
				TrokHordas_DarPontos( UTIL_PlayerByIndex( i ), trok_hordas_pontos_bonus_todos.GetInt() );
		break;
	case HORDAS_BONUS_REFORCO:
		TrokHordas_GradesReparar();
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
			if ( EhHumano( UTIL_PlayerByIndex( i ) ) )
				TrokHordas_DarPontos( UTIL_PlayerByIndex( i ), trok_hordas_pontos_reforco.GetInt() );
		break;
	}
}

//-----------------------------------------------------------------------------
// Nascer zumbi
//-----------------------------------------------------------------------------
static const char *s_pszSpawnsJogador[] =
{
	"info_player_deathmatch", "info_player_combine", "info_player_rebel",
	"info_player_terrorist", "info_player_counterterrorist", "info_player_start",
};

static bool LugarLivre( const Vector &vecPos )
{
	trace_t tr;
	UTIL_TraceHull( vecPos + Vector( 0, 0, 2 ), vecPos + Vector( 0, 0, 2 ), Vector( -17, -17, 0 ), Vector( 17, 17, 104 ),
		MASK_PLAYERSOLID, NULL, COLLISION_GROUP_PLAYER_MOVEMENT, &tr );
	return !tr.startsolid && !tr.allsolid;
}

static bool EscolherLugar( bool bChefe, Vector *pPos, QAngle *pAng )
{
	CUtlVector< CBaseEntity * > lugares;
	TrokHordas_Spawns( &lugares, bChefe );
	if ( lugares.Count() == 0 )
	{
		// Mapa sem nascedouro do Hordas (ex.: mapas do CS:S): pontos de nascimento de jogador.
		for ( int k = 0; k < (int)ARRAYSIZE( s_pszSpawnsJogador ); ++k )
		{
			CBaseEntity *pEnt = NULL;
			while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, s_pszSpawnsJogador[ k ] ) ) != NULL )
				lugares.AddToTail( pEnt );
		}
	}
	if ( lugares.Count() == 0 )
		return false;

	// Perto de um humano de pé sorteado (CoD: zumbis vêm das zonas onde os jogadores estão),
	// mas não colado em ninguém nem à vista curta.
	CUtlVector< CBasePlayer * > humanos;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		if ( DePe( UTIL_PlayerByIndex( i ) ) )
			humanos.AddToTail( UTIL_PlayerByIndex( i ) );
	const Vector vecAlvo = humanos.Count() ? humanos[ RandomInt( 0, humanos.Count() - 1 ) ]->GetAbsOrigin() : lugares[ 0 ]->GetAbsOrigin();
	const float flLonge = trok_hordas_nascer_longe.GetFloat();

	struct Cand_t { CBaseEntity *p; float d; };
	CUtlVector< Cand_t > cands;
	for ( int pass = 0; pass < 2 && cands.Count() == 0; ++pass )
	{
		FOR_EACH_VEC( lugares, i )
		{
			const Vector &o = lugares[ i ]->GetAbsOrigin();
			bool bPerto = false;
			FOR_EACH_VEC( humanos, h )
				if ( ( humanos[ h ]->GetAbsOrigin() - o ).LengthSqr() < flLonge * flLonge )
					bPerto = true;
			if ( bPerto && pass == 0 )
				continue;
			Cand_t c = { lugares[ i ], ( o - vecAlvo ).LengthSqr() };
			cands.AddToTail( c );
		}
	}
	// Os 4 mais perto do alvo; sorteia um.
	for ( int a = 0; a < cands.Count(); ++a )
		for ( int b = a + 1; b < cands.Count(); ++b )
			if ( cands[ b ].d < cands[ a ].d )
			{
				Cand_t t = cands[ a ];
				cands[ a ] = cands[ b ];
				cands[ b ] = t;
			}
	const int nEscolha = MIN( 4, cands.Count() );
	for ( int tentativa = 0; tentativa < nEscolha * 2; ++tentativa )
	{
		CBaseEntity *pLugar = cands[ RandomInt( 0, nEscolha - 1 ) ].p;
		const Vector o = pLugar->GetAbsOrigin();
		// No ponto ou em volta (vários zumbis no mesmo nascedouro).
		for ( int k = 0; k < 17; ++k )
		{
			Vector v = o;
			if ( k > 0 )
			{
				const float flAng = ( k - 1 ) * ( 2.0f * M_PI / 8.0f );
				const float flRaio = ( k <= 8 ) ? 40.0f : 80.0f;
				v += Vector( cosf( flAng ) * flRaio, sinf( flAng ) * flRaio, 0.0f );
			}
			if ( LugarLivre( v ) )
			{
				*pPos = v;
				*pAng = QAngle( 0, pLugar->GetAbsAngles().y, 0 );
				return true;
			}
		}
	}
	return false;
}

static int VagasLivres( void )
{
	int iUsadas = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( p && p->IsConnected() )
			++iUsadas;
	}
	return gpGlobals->maxClients - iUsadas - MAX( 0, trok_hordas_vagas_livres.GetInt() );
}

static CBasePlayer *CriarBot( const char *pszPrefixo, int iTeam )
{
	if ( VagasLivres() <= 0 )
		return NULL;
	static int s_iNumBot = 0;
	char szNome[ 64 ];
	V_snprintf( szNome, sizeof( szNome ), "%s %02d", pszPrefixo, ( ++s_iNumBot % 99 ) + 1 );
	s_bCriandoBot = true;
	CHL2MPBot *pBot = NextBotCreatePlayerBot< CHL2MPBot >( szNome );
	s_bCriandoBot = false;
	if ( !pBot )
		return NULL;
	pBot->SetDifficulty( (CHL2MPBot::DifficultyType)clamp( trok_hordas_bot_dificuldade.GetInt(), 0, 3 ) );
	TrocarTime( pBot, iTeam );
	return pBot;
}

static CBasePlayer *BotLivre( void )
{
	const float flEspera = MAX( 0.1f, trok_hordas_reuso_espera.GetFloat() );
	FOR_EACH_VEC( s_Pool, i )
	{
		CBasePlayer *p = s_Pool[ i ].Get();
		if ( p && p->IsConnected() && !p->IsAlive() && gpGlobals->curtime - s_aMorte[ p->entindex() ] >= flEspera )
			return p;
	}
	// Grupo pequeno demais: cria outro (até o teto de vivos + uma folga para o reuso).
	if ( s_Pool.Count() < MaxVivos() + 4 )
	{
		CBasePlayer *p = CriarBot( HORDAS_BOT_ZUMBI, TEAM_TROK_ZUMBIS );
		if ( p )
		{
			s_Pool.AddToTail( p );
			return p;
		}
	}
	return NULL;
}

static bool NascerZumbi( int iTipo )
{
	Vector vecPos;
	QAngle angPos;
	if ( !EscolherLugar( iTipo == -3, &vecPos, &angPos ) )
		return false;
	CBasePlayer *pBot = BotLivre();
	if ( !pBot )
		return false;

	s_iPedido = pBot->entindex();
	s_iPedidoTipo = iTipo;
	s_vecPedido = vecPos;
	s_angPedido = angPos;
	TrocarTime( pBot, TEAM_TROK_ZUMBIS );
	if ( pBot->IsObserver() )
		pBot->StopObserverMode();
	pBot->Spawn();		// -> CTrokRules::PlayerSpawn -> TrokHordas_PlayerSpawn -> AplicarZumbi
	s_iPedido = 0;
	return pBot->IsAlive();
}

static void AplicarZumbi( CBasePlayer *pPlayer, int iTipo, const Vector *pPos, const QAngle *pAng )
{
	CTrokHordasEstado *pEstado = Estado();
	const int i = pPlayer->entindex();
	const int R = pEstado ? MAX( 1, pEstado->GetRodada() ) : 1;

	pPlayer->RemoveAllItems( false );
	pPlayer->GiveNamedItem( "weapon_trok_garras" );
	CBaseCombatWeapon *pGarras = pPlayer->Weapon_OwnsThisType( "weapon_trok_garras" );
	if ( pGarras )
		pPlayer->Weapon_Switch( pGarras );

	// Tipo: da tabela da rodada, da matilha (-2) ou o chefe (-3).
	int iClasse = 0;
	float flVida = 1.0f, flVel = 1.0f, flEscala = 1.0f;
	if ( iTipo == -2 )
	{
		iClasse = s_Matilha.iClasse; flVida = s_Matilha.flVida; flVel = s_Matilha.flVel; flEscala = s_Matilha.flEscala;
	}
	else if ( iTipo == -3 )
	{
		iClasse = s_Chefe.iClasse; flVida = s_Chefe.flVida; flVel = s_Chefe.flVel; flEscala = s_Chefe.flEscala;
	}
	else if ( s_Tipos.IsValidIndex( iTipo ) )
	{
		const HordasTipo_t &t = s_Tipos[ iTipo ];
		iClasse = t.iClasse; flVida = t.flVida; flVel = t.flVel; flEscala = t.flEscala;
	}
	TrokZumbis_DefinirClasse( pPlayer, iClasse );
	s_aZTipo[ i ] = iTipo;

	const TrokZumbiClasse_t &classe = TrokZumbis_Classe( iClasse );
	const int iVida = MAX( 1, (int)( VidaDaRodada( R ) * flVida ) );
	pPlayer->SetMaxHealth( iVida );
	pPlayer->SetHealth( iVida );
	pPlayer->SetArmorValue( 0 );
	const float flEscalaTotal = clamp( classe.flEscala * flEscala, 0.5f, 2.0f );
	if ( flEscalaTotal != 1.0f )
		pPlayer->SetModelScale( pPlayer->GetModelScale() * flEscalaTotal );

	int r = 150, g = 225, b = 140;
	sscanf( trok_surto_cor_zumbi.GetString(), "%d %d %d", &r, &g, &b );
	if ( classe.iCor[ 0 ] >= 0 )
	{
		r = classe.iCor[ 0 ]; g = classe.iCor[ 1 ]; b = classe.iCor[ 2 ];
	}
	pPlayer->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ) );

	s_aZVelBase[ i ] = VelocidadeDaRodada( R ) * flVel;
	s_aZLentoAte[ i ] = 0.0f;
	s_aZFogoAte[ i ] = 0.0f;
	if ( pEstado )
		pEstado->m_aVelZumbi.Set( i, s_aZVelBase[ i ] );

	if ( pPos )
		pPlayer->Teleport( pPos, pAng, &vec3_origin );
}

static void AplicarHumano( CBasePlayer *pPlayer )
{
	CTrokHordasEstado *pEstado = Estado();
	const int i = pPlayer->entindex();
	pPlayer->SetRenderColor( 255, 255, 255 );
	pPlayer->SetLaggedMovementValue( 1.0f );
	TrokZumbis_AoVirarHumano( pPlayer );
	s_aUltimoDano[ i ] = -100.0f;
	s_aRegenAcum[ i ] = 0.0f;
	s_aRevivendo[ i ] = 0;
	s_aAutoReviver[ i ] = 0.0f;
	if ( pEstado )
	{
		pEstado->m_aCaidoAte.Set( i, 0.0f );
		pEstado->m_aReviver.Set( i, 0.0f );
		pEstado->m_aVelZumbi.Set( i, 1.0f );
		// Nasceu de novo: armas novas, sem melhoria.
		for ( int s = 0; s < TROK_HORDAS_SLOTS; ++s )
		{
			pEstado->m_aNivel.Set( i * TROK_HORDAS_SLOTS + s, 0 );
			s_hArmaSlot[ i ][ s ] = NULL;
		}
		// Jogador novo no mapa: pontos iniciais.
		if ( s_aUserId[ i ] != pPlayer->GetUserID() )
		{
			s_aUserId[ i ] = pPlayer->GetUserID();
			pEstado->m_aPontos.Set( i, trok_hordas_pontos_inicio.GetInt() );
			pEstado->m_aAbates.Set( i, 0 );
			pEstado->m_aKit.Set( i, 0 );
			s_aReparoRodada[ i ] = 0;
		}
	}
}

//-----------------------------------------------------------------------------
// Derrubado e reviver
//-----------------------------------------------------------------------------
static void Derrubar( CBasePlayer *pPlayer )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;
	const int i = pPlayer->entindex();
	pEstado->m_aCaidoAte.Set( i, gpGlobals->curtime + MAX( 1.0f, trok_hordas_caido_tempo.GetFloat() ) );
	pEstado->m_aReviver.Set( i, 0.0f );
	pPlayer->SetHealth( 1 );
	pPlayer->SetArmorValue( 0 );
	pPlayer->SetLaggedMovementValue( clamp( trok_hordas_caido_velocidade.GetFloat(), 0.05f, 1.0f ) );
	TrokHordas_DarPontos( pPlayer, -(int)( pEstado->m_aPontos[ i ] * clamp( trok_hordas_perda_caido.GetFloat(), 0.0f, 1.0f ) ), false );
	// Kit de reanimação: levanta sozinho daqui a pouco (e o kit acaba).
	if ( pEstado->TemKit( i ) )
	{
		pEstado->m_aKit.Set( i, 0 );
		s_aAutoReviver[ i ] = gpGlobals->curtime + MAX( 0.5f, trok_hordas_kit_tempo.GetFloat() );
	}
	// Derrubado: só a pistola (classe 2 do CA = slot 1).
	CBaseCombatWeapon *pPistola = pPlayer->Weapon_GetSlot( 1 );
	if ( pPistola )
		pPlayer->Weapon_Switch( pPistola );
	pPlayer->EmitSound( "Trok.Hordas.Caido" );
	UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "%s caiu! Segure E perto dele para reviver.\n", pPlayer->GetPlayerName() ) );
}

static void Reviver( CBasePlayer *pPlayer )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;
	const int i = pPlayer->entindex();
	pEstado->m_aCaidoAte.Set( i, 0.0f );
	pEstado->m_aReviver.Set( i, 0.0f );
	s_aAutoReviver[ i ] = 0.0f;
	pPlayer->SetLaggedMovementValue( 1.0f );
	pPlayer->SetHealth( MAX( 1, (int)( pPlayer->GetMaxHealth() * clamp( trok_hordas_reviver_vida.GetFloat(), 0.05f, 1.0f ) ) ) );
	s_aUltimoDano[ i ] = gpGlobals->curtime;
	pPlayer->EmitSound( "Trok.Hordas.Reviver" );
}

static void Sangrar( CBasePlayer *pPlayer )
{
	s_bMatando = true;
	pPlayer->SetArmorValue( 0 );
	CTakeDamageInfo info( GetWorldEntity(), GetWorldEntity(), (float)( pPlayer->GetHealth() + 1000 ), DMG_GENERIC );
	pPlayer->TakeDamage( info );
	s_bMatando = false;
}

static void PensarCaidos( float dt )
{
	CTrokHordasEstado *pEstado = Estado();
	const float flAgora = gpGlobals->curtime;
	const float flDist = trok_hordas_reviver_distancia.GetFloat();
	const float flTempo = MAX( 0.1f, trok_hordas_reviver_tempo.GetFloat() );

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !EhHumano( p ) || !p->IsAlive() )
		{
			s_aRevivendo[ i ] = 0;
			continue;
		}

		if ( TrokHordas_Caido( p ) )
		{
			if ( s_aAutoReviver[ i ] > 0.0f && flAgora >= s_aAutoReviver[ i ] )
				Reviver( p );
			else if ( flAgora >= pEstado->GetCaidoAte( i ) && s_aAutoReviver[ i ] <= 0.0f )
				Sangrar( p );
			continue;
		}

		// De pé segurando E: revive o derrubado mais perto.
		int iAlvo = 0;
		if ( p->m_nButtons & IN_USE )
		{
			float flMelhor = flDist * flDist;
			for ( int j = 1; j <= gpGlobals->maxClients; ++j )
			{
				CBasePlayer *c = UTIL_PlayerByIndex( j );
				if ( !c || c == p || !c->IsAlive() || !TrokHordas_Caido( c ) )
					continue;
				const float d = ( c->GetAbsOrigin() - p->GetAbsOrigin() ).LengthSqr();
				if ( d < flMelhor )
				{
					flMelhor = d;
					iAlvo = j;
				}
			}
		}
		if ( s_aRevivendo[ i ] && s_aRevivendo[ i ] != iAlvo )
			pEstado->m_aReviver.Set( s_aRevivendo[ i ], 0.0f );
		s_aRevivendo[ i ] = iAlvo;
		if ( iAlvo )
		{
			const float flProg = pEstado->GetReviver( iAlvo ) + dt / flTempo;
			if ( flProg >= 1.0f )
			{
				Reviver( UTIL_PlayerByIndex( iAlvo ) );
				s_aRevivendo[ i ] = 0;
			}
			else
			{
				pEstado->m_aReviver.Set( iAlvo, flProg );
			}
		}

		// Vida volta sozinha (como no CoD) depois de um tempo sem levar dano.
		const float flRegen = trok_hordas_regen.GetFloat();
		if ( flRegen > 0.0f && p->GetHealth() < p->GetMaxHealth() && flAgora - s_aUltimoDano[ i ] >= trok_hordas_regen_espera.GetFloat() )
		{
			s_aRegenAcum[ i ] += flRegen * dt;
			const int n = (int)s_aRegenAcum[ i ];
			if ( n > 0 )
			{
				s_aRegenAcum[ i ] -= n;
				p->SetHealth( MIN( p->GetMaxHealth(), p->GetHealth() + n ) );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Zumbis: fogo, lentidão (efeitos das armas melhoradas)
//-----------------------------------------------------------------------------
static void PensarZumbis( void )
{
	CTrokHordasEstado *pEstado = Estado();
	const float flAgora = gpGlobals->curtime;
	const bool bFogo = flAgora >= s_flProxFogo;
	if ( bFogo )
		s_flProxFogo = flAgora + 0.5f;
	FOR_EACH_VEC( s_Pool, k )
	{
		CBasePlayer *p = s_Pool[ k ].Get();
		if ( !p || !p->IsAlive() )
			continue;
		const int i = p->entindex();
		float flVel = s_aZVelBase[ i ];
		if ( flAgora < s_aZLentoAte[ i ] )
			flVel *= clamp( s_aZLentoFator[ i ], 0.05f, 1.0f );
		if ( pEstado && fabsf( pEstado->GetVelZumbi( i ) - flVel ) > 0.01f )
			pEstado->m_aVelZumbi.Set( i, flVel );

		if ( bFogo && flAgora < s_aZFogoAte[ i ] )
		{
			CBasePlayer *pDono = s_hZFogoDono[ i ].Get();
			CTakeDamageInfo info( pDono ? (CBaseEntity *)pDono : GetWorldEntity(), pDono ? (CBaseEntity *)pDono : GetWorldEntity(),
				MAX( 1.0f, s_aZFogoDps[ i ] * 0.5f ), DMG_BURN );
			p->TakeDamage( info );
			g_pEffects->Sparks( p->WorldSpaceCenter(), 1, 1 );
		}
	}
}

// Efeito do nível da arma num acerto (atordoante, incendiária, explosiva).
static void EfeitoDaArma( CBasePlayer *pAtacante, CBasePlayer *pZumbi, const TrokUpgradeNivel_t &n, float flDano )
{
	if ( !n.szEfeito[ 0 ] || RandomFloat() > n.flEfeitoChance )
		return;
	const int i = pZumbi->entindex();
	if ( !Q_stricmp( n.szEfeito, "atordoante" ) )
	{
		s_aZLentoAte[ i ] = gpGlobals->curtime + MAX( 0.1f, n.flEfeitoTempo );
		s_aZLentoFator[ i ] = clamp( n.flEfeitoForca, 0.05f, 1.0f );
	}
	else if ( !Q_stricmp( n.szEfeito, "incendiaria" ) )
	{
		s_aZFogoAte[ i ] = gpGlobals->curtime + MAX( 0.5f, n.flEfeitoTempo );
		s_aZFogoDps[ i ] = pZumbi->GetMaxHealth() * MAX( 0.0f, n.flEfeitoForca );
		s_hZFogoDono[ i ] = pAtacante;
	}
	else if ( !Q_stricmp( n.szEfeito, "explosiva" ) )
	{
		const Vector vecCentro = pZumbi->WorldSpaceCenter();
		const float flRaio = MAX( 16.0f, n.flEfeitoRaio );
		ExplosionCreate( vecCentro, vec3_angle, pAtacante, 0, 0, false );
		// Só zumbis em volta (sem dano em humano nem no atirador).
		FOR_EACH_VEC( s_Pool, k )
		{
			CBasePlayer *z = s_Pool[ k ].Get();
			if ( !z || z == pZumbi || !z->IsAlive() || ( z->WorldSpaceCenter() - vecCentro ).LengthSqr() > flRaio * flRaio )
				continue;
			CTakeDamageInfo info( pAtacante, pAtacante, flDano * MAX( 0.0f, n.flEfeitoForca ), DMG_BLAST );
			z->TakeDamage( info );
		}
	}
}

//-----------------------------------------------------------------------------
// Rodadas
//-----------------------------------------------------------------------------
static void Espectadores( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !EhHumano( p ) || p->IsAlive() || p->IsObserver() || gpGlobals->curtime - s_aMorte[ i ] < 3.0f )
			continue;
		if ( p->StartObserverMode( OBS_MODE_CHASE ) )
		{
			for ( int j = 1; j <= gpGlobals->maxClients; ++j )
			{
				CBasePlayer *a = UTIL_PlayerByIndex( j );
				if ( a && a != p && DePe( a ) )
				{
					p->SetObserverTarget( a );
					break;
				}
			}
		}
	}
}

// Rodada nova: quem morreu volta (com o kit inicial), granadas para quem está vivo.
static void ComecarRodada( int R )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;

	int iTipo = HORDAS_RODADA_NORMAL;
	if ( s_Matilha.iPrimeira > 0 && s_iProxMatilha <= 0 )
		s_iProxMatilha = s_Matilha.iPrimeira + RandomInt( 0, MAX( 0, s_Matilha.iMax - s_Matilha.iMin ) );
	if ( s_Matilha.iPrimeira > 0 && R >= s_iProxMatilha )
		iTipo = HORDAS_RODADA_MATILHA;
	else if ( s_Chefe.iACada > 0 && R % s_Chefe.iACada == 0 )
		iTipo = HORDAS_RODADA_CHEFE;

	const int J = Jogadores();
	if ( iTipo == HORDAS_RODADA_MATILHA )
		s_iParaNascer = MAX( 1, (int)( J * s_Matilha.iPorJogador * Dif( trok_hordas_dif_quantidade ) ) );
	else
		s_iParaNascer = QuantidadeDaRodada( R );
	s_iChefesParaNascer = ( iTipo == HORDAS_RODADA_CHEFE ) ? MAX( 1, s_Chefe.iQuantos ) : 0;
	s_iTotalRodada = s_iParaNascer + s_iChefesParaNascer;
	s_flIntervaloNascer = MAX( trok_hordas_nascer_min.GetFloat(), trok_hordas_nascer_intervalo.GetFloat() * powf( trok_hordas_nascer_fator.GetFloat(), (float)( R - 1 ) ) );
	if ( iTipo == HORDAS_RODADA_MATILHA )
		s_flIntervaloNascer = MAX( trok_hordas_nascer_min.GetFloat(), 1.0f );
	s_flProxNascer = gpGlobals->curtime + 1.0f;
	s_iBonusRodada = 0;
	s_iTiposNaRodada.SetCount( s_Tipos.Count() );
	FOR_EACH_VEC( s_iTiposNaRodada, i )
		s_iTiposNaRodada[ i ] = 0;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		s_aReparoRodada[ i ] = 0;
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !EhHumano( p ) )
			continue;
		if ( !p->IsAlive() )
		{
			if ( p->IsObserver() )
				p->StopObserverMode();
			p->RemoveAllItems( true );
			p->Spawn();
		}
		else
		{
			DarGranadas( p );
		}
	}

	pEstado->m_iFase = HORDAS_RODADA;
	pEstado->m_iRodada = R;
	pEstado->m_iRodadaTipo = iTipo;
	pEstado->m_iRestantes = s_iTotalRodada;
	pEstado->m_flFimFase = 0.0f;
	if ( R > pEstado->GetRecorde() )
		pEstado->m_iRecorde = R;

	CBroadcastRecipientFilter filter;
	CBaseEntity::EmitSound( filter, 0, "Trok.Hordas.RodadaInicio" );
	TrokHordas_DispararMapa( HORDAS_EV_RODADA_INICIO, R );
	if ( iTipo == HORDAS_RODADA_CHEFE )
		TrokHordas_DispararMapa( HORDAS_EV_CHEFE, R );
}

static void FimDaRodada( void )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;
	const int R = pEstado->GetRodada();
	// Rodada da matilha: prêmio de munição onde o último caiu (CoD: Max Ammo no último cão).
	if ( pEstado->GetRodadaTipo() == HORDAS_RODADA_MATILHA )
	{
		SoltarBonus( s_vecUltimaMorte, HORDAS_BONUS_MUNICAO );
		s_iProxMatilha = R + RandomInt( MAX( 1, s_Matilha.iMin ), MAX( s_Matilha.iMin, s_Matilha.iMax ) );
	}
	pEstado->m_iFase = HORDAS_INTERVALO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 1.0f, trok_hordas_intervalo.GetFloat() );
	pEstado->m_iRestantes = 0;
	CBroadcastRecipientFilter filter;
	CBaseEntity::EmitSound( filter, 0, "Trok.Hordas.RodadaFim" );
	TrokHordas_DispararMapa( HORDAS_EV_RODADA_FIM, R );
}

static void FimDeJogo( void )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado || pEstado->GetFase() == HORDAS_FIM )
		return;
	pEstado->m_iFase = HORDAS_FIM;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 3.0f, trok_hordas_fim.GetFloat() );
	TrokHordas_DispararMapa( HORDAS_EV_FIM_DE_JOGO, pEstado->GetRodada() );
	UTIL_ClientPrintAll( HUD_PRINTTALK, UTIL_VarArgs( "Fim de jogo: vocês sobreviveram até a rodada %d.\n", pEstado->GetRodada() ) );
}

static void CriarEstado( void )
{
	// Os tipos de zumbi (classe, lentidão) moram na entidade do Surto: cria desligada.
	if ( !TrokSurto_Estado() )
	{
		CBaseEntity *pSurto = CreateEntityByName( "trok_surto_estado" );
		if ( pSurto )
			DispatchSpawn( pSurto );
	}
	CTrokHordasEstado *pEstado = static_cast< CTrokHordasEstado * >( CreateEntityByName( "trok_hordas_estado" ) );
	if ( !pEstado )
		return;
	DispatchSpawn( pEstado );
	s_hEstado = pEstado;
}

// Fim de jogo -> mapa limpo (portas, barricadas, caixa voltam), pontos do começo, rodada 1.
static void Recomecar( void )
{
	const int iRecorde = Estado() ? Estado()->GetRecorde() : 0;
	FOR_EACH_VEC( s_Pool, i )
		MatarZumbi( s_Pool[ i ].Get(), NULL, DMG_GENERIC );
	if ( HL2MPRules() )
		HL2MPRules()->CleanUpMap();
	s_iAreas = 0;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
		s_aUserId[ i ] = -1;	// todos voltam com os pontos iniciais
	s_hEstado = NULL;
	CriarEstado();
	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
		return;
	pEstado->m_iRecorde = iRecorde;
	s_iProxMatilha = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !EhHumano( p ) )
			continue;
		if ( p->IsObserver() )
			p->StopObserverMode();
		if ( p->GetActiveWeapon() )
			p->GetActiveWeapon()->Holster();
		p->RemoveAllItems( true );
		p->Spawn();
	}
	pEstado->m_iFase = HORDAS_INTERVALO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 1.0f, trok_hordas_preparo.GetFloat() );
}

// Bots que sobraram de um mapa anterior (o motor mantém os bots na troca de mapa): os do
// prefixo voltam ao grupo; fora do Hordas, saem do servidor.
static void Manutencao( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || !p->IsConnected() || !p->IsBot() || EhDoPool( p ) || EhAliadoBot( p ) )
			continue;
		const char *pszNome = p->GetPlayerName();
		const bool bZumbi = !Q_strncmp( pszNome, HORDAS_BOT_ZUMBI, V_strlen( HORDAS_BOT_ZUMBI ) );
		const bool bAliado = !Q_strncmp( pszNome, HORDAS_BOT_ALIADO, V_strlen( HORDAS_BOT_ALIADO ) );
		if ( !bZumbi && !bAliado )
			continue;
		if ( !s_bAtivo )
		{
			engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", p->GetUserID() ) );
			continue;
		}
		if ( bZumbi )
		{
			s_Pool.AddToTail( p );
			TrocarTime( p, TEAM_TROK_ZUMBIS );
			MatarZumbi( p, NULL, DMG_GENERIC );		// fica na reserva até ser chamado
		}
		else
		{
			s_Aliados.AddToTail( p );
		}
	}
	// Grupos sem quem saiu.
	for ( int i = s_Pool.Count() - 1; i >= 0; --i )
		if ( !s_Pool[ i ].Get() )
			s_Pool.Remove( i );
	for ( int i = s_Aliados.Count() - 1; i >= 0; --i )
		if ( !s_Aliados[ i ].Get() )
			s_Aliados.Remove( i );

	// Aliados pedidos (só com alguém de verdade jogando).
	if ( s_bAtivo && s_Aliados.Count() < trok_hordas_aliados.GetInt() )
	{
		bool bGente = false;
		for ( int i = 1; i <= gpGlobals->maxClients && !bGente; ++i )
		{
			CBasePlayer *p = UTIL_PlayerByIndex( i );
			bGente = p && p->IsConnected() && !p->IsBot() && p->GetTeamNumber() == TEAM_TROK_HUMANOS;
		}
		if ( bGente )
		{
			CBasePlayer *p = CriarBot( HORDAS_BOT_ALIADO, TEAM_TROK_HUMANOS );
			if ( p )
				s_Aliados.AddToTail( p );
		}
	}
}

// Melhoria vai com a arma: se a arma de uma classe trocou (largou, pegou outra), o nível zera.
static void PensarArmas( void )
{
	CTrokHordasEstado *pEstado = Estado();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !EhHumano( p ) || !p->IsAlive() )
			continue;
		for ( int s = 0; s < TROK_HORDAS_SLOTS; ++s )
		{
			CBaseCombatWeapon *w = p->Weapon_GetSlot( s );
			if ( w != s_hArmaSlot[ i ][ s ].Get() )
			{
				s_hArmaSlot[ i ][ s ] = w;
				if ( pEstado->GetNivel( i, s ) != 0 )
					pEstado->m_aNivel.Set( i * TROK_HORDAS_SLOTS + s, 0 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Ganchos das regras (trok_gamerules.cpp)
//-----------------------------------------------------------------------------
void TrokHordas_Iniciar( bool bAtivo )
{
	s_bAtivo = bAtivo;
	s_hEstado = NULL;
	s_Pool.RemoveAll();
	s_Aliados.RemoveAll();
	s_iAreas = 0;
	s_iParaNascer = s_iChefesParaNascer = s_iTotalRodada = 0;
	s_iProxMatilha = 0;
	s_iPedido = 0;
	s_flProxManutencao = 0.0f;
	s_flUltimoThink = 0.0f;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		s_aUserId[ i ] = -1;
		s_aMorte[ i ] = -100.0f;
		s_aUltimoDano[ i ] = -100.0f;
		s_aRegenAcum[ i ] = 0.0f;
		s_aRevivendo[ i ] = 0;
		s_aAutoReviver[ i ] = 0.0f;
		s_aReparoRodada[ i ] = 0;
		s_aZTipo[ i ] = 0;
		s_aZVelBase[ i ] = 1.0f;
		s_aZLentoAte[ i ] = s_aZFogoAte[ i ] = 0.0f;
		for ( int s = 0; s < TROK_HORDAS_SLOTS; ++s )
			s_hArmaSlot[ i ][ s ] = NULL;
	}
	if ( bAtivo )
	{
		Hordas_CarregarScript();
		TrokUpgrade( true );
		Hordas_Precache();
	}
}

void TrokHordas_Think( void )
{
	const float flAgora = gpGlobals->curtime;
	if ( flAgora >= s_flProxManutencao )
	{
		s_flProxManutencao = flAgora + 1.0f;
		Manutencao();
	}
	if ( !s_bAtivo )
		return;

	CTrokHordasEstado *pEstado = Estado();
	if ( !pEstado )
	{
		CriarEstado();
		pEstado = Estado();
		if ( !pEstado )
			return;
	}
	const float dt = clamp( flAgora - s_flUltimoThink, 0.0f, 0.5f );
	s_flUltimoThink = flAgora;

	PensarArmas();
	PensarCaidos( dt );
	PensarZumbis();
	TrokZumbis_Think();		// fôlego infinito, aura, bolhas dos tipos de zumbi (trok_zumbis.cpp)
	Espectadores();

	const int iHumanos = ContarHumanos();
	const int iDePe = ContarHumanos( true );
	int iEsperandoKit = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		if ( s_aAutoReviver[ i ] > 0.0f )
			++iEsperandoKit;

	switch ( pEstado->GetFase() )
	{
	case HORDAS_ESPERA:
		if ( iHumanos > 0 )
		{
			pEstado->m_iFase = HORDAS_INTERVALO;
			pEstado->m_iRodada = 0;
			pEstado->m_flFimFase = flAgora + MAX( 1.0f, trok_hordas_preparo.GetFloat() );
		}
		break;

	case HORDAS_INTERVALO:
		if ( iHumanos == 0 )
		{
			pEstado->m_iFase = HORDAS_ESPERA;
			break;
		}
		if ( flAgora >= pEstado->GetFimFase() )
			ComecarRodada( pEstado->GetRodada() + 1 );
		break;

	case HORDAS_RODADA:
	{
		if ( iHumanos == 0 )
		{
			// Todos saíram: volta a esperar, mapa limpo quando alguém entrar.
			Recomecar();
			if ( Estado() )
				Estado()->m_iFase = HORDAS_ESPERA;
			break;
		}
		if ( iDePe == 0 && iEsperandoKit == 0 )
		{
			FimDeJogo();
			break;
		}
		const int iVivos = ContarZumbisVivos();
		if ( ( s_iParaNascer > 0 || s_iChefesParaNascer > 0 ) && iVivos < MaxVivos() && flAgora >= s_flProxNascer )
		{
			// Chefe nasce quando metade da rodada já nasceu.
			const bool bChefe = s_iChefesParaNascer > 0 && ( s_iParaNascer <= s_iTotalRodada / 2 || s_iParaNascer == 0 );
			int iTipo;
			if ( bChefe )
				iTipo = -3;
			else if ( pEstado->GetRodadaTipo() == HORDAS_RODADA_MATILHA )
				iTipo = -2;
			else
				iTipo = SortearTipo( pEstado->GetRodada() );
			if ( NascerZumbi( iTipo ) )
			{
				if ( bChefe )
				{
					--s_iChefesParaNascer;
					UTIL_ClientPrintAll( HUD_PRINTCENTER, "UM COLOSSO SE APROXIMA" );
				}
				else
				{
					--s_iParaNascer;
					if ( s_iTiposNaRodada.IsValidIndex( iTipo ) )
						++s_iTiposNaRodada[ iTipo ];
				}
				s_flProxNascer = flAgora + s_flIntervaloNascer;
			}
			else
			{
				s_flProxNascer = flAgora + 0.5f;	// sem lugar ou sem vaga: tenta de novo logo
			}
		}
		pEstado->m_iRestantes = s_iParaNascer + s_iChefesParaNascer + iVivos;
		if ( s_iParaNascer <= 0 && s_iChefesParaNascer <= 0 && iVivos == 0 )
			FimDaRodada();
		break;
	}

	case HORDAS_FIM:
		if ( flAgora >= pEstado->GetFimFase() )
			Recomecar();
		break;
	}
}

void TrokHordas_PlayerSpawn( CBasePlayer *pPlayer )
{
	if ( !s_bAtivo || s_bCriandoBot || !pPlayer || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return;
	if ( EhDoPool( pPlayer ) )
	{
		if ( s_iPedido == pPlayer->entindex() )
			AplicarZumbi( pPlayer, s_iPedidoTipo, &s_vecPedido, &s_angPedido );
		else
			AplicarZumbi( pPlayer, 0, NULL, NULL );
		return;
	}
	// Todo o resto é humano (no Hordas não se escolhe time).
	if ( pPlayer->GetTeamNumber() != TEAM_TROK_HUMANOS )
		TrocarTime( pPlayer, TEAM_TROK_HUMANOS );
	AplicarHumano( pPlayer );
}

void TrokHordas_Abate( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !s_bAtivo || !pEstado || !pVictim )
		return;
	const int i = pVictim->entindex();
	s_aMorte[ i ] = gpGlobals->curtime;

	if ( EhDoPool( pVictim ) )
	{
		s_vecUltimaMorte = pVictim->GetAbsOrigin();
		CBasePlayer *pAtacante = ToBasePlayer( info.GetAttacker() );
		const int iTipo = s_aZTipo[ i ];
		if ( pAtacante && EhHumano( pAtacante ) && !s_bAtaqueAereo )
		{
			int iPontos;
			if ( info.GetDamageType() & ( DMG_CLUB | DMG_SLASH ) )
				iPontos = trok_hordas_pontos_faca.GetInt();
			else if ( pVictim->LastHitGroup() == HITGROUP_HEAD )
				iPontos = trok_hordas_pontos_cabeca.GetInt();
			else
				iPontos = trok_hordas_pontos_abate.GetInt();
			float flMult = 1.0f;
			if ( iTipo == -2 )
				flMult = s_Matilha.flPontos;
			else if ( iTipo == -3 )
				flMult = s_Chefe.flPontos;
			else if ( s_Tipos.IsValidIndex( iTipo ) )
				flMult = s_Tipos[ iTipo ].flPontos;
			TrokHordas_DarPontos( pAtacante, (int)( iPontos * flMult ) );
			if ( iTipo == -3 && s_Chefe.iBonusPontos > 0 )
				TrokHordas_DarPontos( pAtacante, s_Chefe.iBonusPontos );
			pEstado->m_aAbates.Set( pAtacante->entindex(), pEstado->m_aAbates[ pAtacante->entindex() ] + 1 );
		}
		// Bônus: chance por abate (não na matilha, que solta o dela no fim; o chefe sempre solta).
		const bool bMatilha = pEstado->GetRodadaTipo() == HORDAS_RODADA_MATILHA;
		if ( iTipo == -3 )
			SoltarBonus( pVictim->GetAbsOrigin(), HORDAS_BONUS_MUNICAO );
		else if ( !bMatilha && !s_bAtaqueAereo && s_iBonusRodada < trok_hordas_bonus_max.GetInt() && RandomFloat() < trok_hordas_bonus_chance.GetFloat() )
		{
			++s_iBonusRodada;
			SoltarBonus( pVictim->GetAbsOrigin(), SortearBonus() );
		}
		s_aZFogoAte[ i ] = 0.0f;
		s_aZLentoAte[ i ] = 0.0f;
		return;
	}

	if ( EhHumano( pVictim ) )
	{
		// Morreu de vez (sangrou ou morte direta): perde parte dos pontos, o kit e as melhorias.
		TrokHordas_DarPontos( pVictim, -(int)( pEstado->m_aPontos[ i ] * clamp( trok_hordas_perda_morte.GetFloat(), 0.0f, 1.0f ) ), false );
		pEstado->m_aCaidoAte.Set( i, 0.0f );
		pEstado->m_aReviver.Set( i, 0.0f );
		pEstado->m_aKit.Set( i, 0 );
		s_aAutoReviver[ i ] = 0.0f;
		pVictim->SetLaggedMovementValue( 1.0f );
	}
}

void TrokHordas_Dano( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !s_bAtivo || !pEstado || !pVictim || s_bMatando )
		return;
	CBasePlayer *pAtacante = ToBasePlayer( info.GetAttacker() );

	if ( EhDoPool( pVictim ) )
	{
		if ( !pAtacante || !EhHumano( pAtacante ) )
			return;
		int iVida = pVictim->GetHealth();

		// Arma melhorada: o dano a mais sai da vida aqui (o motor só vê o dano da arma).
		if ( info.GetDamageType() & DMG_BULLET )
		{
			CBaseCombatWeapon *pArma = pAtacante->GetActiveWeapon();
			const int iNivel = TrokHordas_NivelArma( pArma );
			if ( iNivel > 0 )
			{
				const TrokUpgradeNivel_t &n = TrokUpgrade().niveis[ iNivel ];
				iVida -= (int)( info.GetDamage() * MAX( 0.0f, n.flDano - 1.0f ) );
				EfeitoDaArma( pAtacante, pVictim, n, info.GetDamage() * n.flDano );
			}
		}
		// Abate certo: qualquer acerto derruba (menos o chefe, que leva 10x).
		if ( pEstado->BonusAtivo( HORDAS_BONUS_ABATE_CERTO ) )
		{
			if ( s_aZTipo[ pVictim->entindex() ] == -3 )
				iVida -= (int)( info.GetDamage() * 9.0f );
			else
				iVida = 0;
		}
		pVictim->SetHealth( iVida );	// <= 0: o motor mata logo depois deste gancho
		TrokHordas_DarPontos( pAtacante, trok_hordas_pontos_acerto.GetInt() );
		return;
	}

	if ( !EhHumano( pVictim ) )
		return;
	const int i = pVictim->entindex();
	s_aUltimoDano[ i ] = gpGlobals->curtime;
	s_aRegenAcum[ i ] = 0.0f;
	if ( pAtacante && EhDoPool( pAtacante ) )
	{
		// Dificuldade: dano dos zumbis (o golpe é o das garras, trok_surto_garra_dano x tipo).
		const float f = Dif( trok_hordas_dif_dano );
		if ( f != 1.0f )
			pVictim->SetHealth( pVictim->GetHealth() - (int)( info.GetDamage() * ( f - 1.0f ) ) );
		TrokZumbis_AoAcertar( pVictim, info );	// golpe que paralisa (Splicer)
	}
	if ( pVictim->GetHealth() <= 0 && !TrokHordas_Caido( pVictim ) )
		Derrubar( pVictim );	// em vez de morrer, cai (o motor vê vida 1 e não mata)
}

bool TrokHordas_PodeLevarDano( CBasePlayer *pPlayer, CBaseEntity *pAttacker )
{
	if ( !s_bAtivo || !pPlayer )
		return true;
	// Derrubado não leva dano (só o "sangrar" do diretor), e humano não fere humano.
	if ( TrokHordas_Caido( pPlayer ) )
		return s_bMatando;
	CBasePlayer *pAtacante = ToBasePlayer( pAttacker );
	if ( pAtacante && pAtacante != pPlayer && EhHumano( pAtacante ) && EhHumano( pPlayer ) )
		return false;
	return true;
}

bool TrokHordas_PodeRenascer( CBasePlayer *pPlayer )
{
	if ( !s_bAtivo || !pPlayer )
		return true;
	if ( EhDoPool( pPlayer ) )
		return false;	// zumbi só nasce pelo diretor
	CTrokHordasEstado *pEstado = Estado();
	// Humano morto espera a próxima rodada (o diretor traz de volta); antes da 1ª rodada, normal.
	return !pEstado || pEstado->GetFase() == HORDAS_ESPERA || ( pEstado->GetFase() == HORDAS_INTERVALO && pEstado->GetRodada() == 0 );
}

bool TrokHordas_PodeTerArma( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	if ( !s_bAtivo || !pPlayer || !pItem )
		return true;
	const bool bGarras = FClassnameIs( pItem, "weapon_trok_garras" );
	if ( EhDoPool( pPlayer ) || pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
		return bGarras;
	return !bGarras;
}

bool TrokHordas_ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	if ( !s_bAtivo || !pPlayer )
		return false;
	// No Hordas todo mundo é humano: sem escolher time.
	if ( FStrEq( args[ 0 ], "jointeam" ) || FStrEq( args[ 0 ], "changeteam" ) )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Comandos de teste (cheats)
//-----------------------------------------------------------------------------
CON_COMMAND_F( trok_hordas_info, "Hordas: mostra a rodada, a vida, a quantidade e a velocidade das rodadas 1 a 30.", 0 )
{
	CTrokHordasEstado *pEstado = Estado();
	Msg( "Hordas %s. Jogadores %d, vivos max %d, grupo %d bots. Fase %d, rodada %d, faltam %d.\n",
		s_bAtivo ? "ligado" : "desligado", Jogadores(), MaxVivos(), s_Pool.Count(),
		pEstado ? pEstado->GetFase() : -1, pEstado ? pEstado->GetRodada() : 0, pEstado ? pEstado->GetRestantes() : 0 );
	Msg( "Rodada  Vida  Zumbis  Vel(média)  Intervalo\n" );
	for ( int R = 1; R <= 30; ++R )
	{
		const float v = MIN( trok_hordas_vel_max.GetFloat(), trok_hordas_vel_inicial.GetFloat() + trok_hordas_vel_por_rodada.GetFloat() * ( R - 1 ) );
		const float it = MAX( trok_hordas_nascer_min.GetFloat(), trok_hordas_nascer_intervalo.GetFloat() * powf( trok_hordas_nascer_fator.GetFloat(), (float)( R - 1 ) ) );
		Msg( "%6d %6d %7d %10.2f %10.2f\n", R, (int)VidaDaRodada( R ), QuantidadeDaRodada( R ), v, it );
	}
}

CON_COMMAND_F( trok_hordas_rodada, "Hordas: pula para a rodada N (mata os zumbis vivos).", FCVAR_CHEAT )
{
	CTrokHordasEstado *pEstado = Estado();
	if ( !s_bAtivo || !pEstado || args.ArgC() < 2 )
		return;
	FOR_EACH_VEC( s_Pool, i )
		MatarZumbi( s_Pool[ i ].Get(), NULL, DMG_GENERIC );
	s_iParaNascer = s_iChefesParaNascer = 0;
	pEstado->m_iRodada = MAX( 0, atoi( args[ 1 ] ) - 1 );
	pEstado->m_iFase = HORDAS_INTERVALO;
	pEstado->m_flFimFase = gpGlobals->curtime + 1.0f;
}

CON_COMMAND_F( trok_hordas_pontos, "Hordas: dá N pontos a quem digitou.", FCVAR_CHEAT )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if ( pPlayer && args.ArgC() >= 2 )
		TrokHordas_DarPontos( pPlayer, atoi( args[ 1 ] ), false );
}

CON_COMMAND_F( trok_hordas_dar_bonus, "Hordas: aplica um bônus (0 munição, 1 abate certo, 2 pontos em dobro, 3 ataque aéreo, 4 reforço, 5 caixa barata).", FCVAR_CHEAT )
{
	if ( s_bAtivo && args.ArgC() >= 2 )
		AplicarBonus( atoi( args[ 1 ] ), UTIL_GetCommandClient() );
}

CON_COMMAND_F( trok_hordas_ligar_energia, "Hordas: liga a energia do mapa.", FCVAR_CHEAT )
{
	TrokHordas_LigarEnergia();
}

CON_COMMAND_F( trok_hordas_recarregar, "Hordas: lê de novo scripts/trok_hordas.txt e scripts/trok_upgrade.txt.", FCVAR_CHEAT )
{
	Hordas_CarregarScript();
	TrokUpgrade( true );
	Msg( "Trok: Hordas com %d tipos, %d armas na caixa, %d níveis de melhoria.\n", s_Tipos.Count(), s_Caixa.Count(), TrokUpgrade().nNiveis );
}
