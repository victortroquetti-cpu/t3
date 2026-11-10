//=============================================================================
// Trok: regras de jogo do mod. Ver trok_gamerules.h e design/modo-surto.md.
//=============================================================================

#include "cbase.h"
#include "trok_gamerules.h"
#include "movevars_shared.h"
#include "in_buttons.h"
#include "trok_zumbis.h"

#ifndef CLIENT_DLL
	#include "hl2mp_player.h"
	#include "team.h"
	#include "weapon_hl2mpbase.h"
	#include "hl2mp_weapon_parse.h"
	#include "trok/weapon_trok_gun.h"
	#include "NextBot/NextBotInterface.h"
	#include "NextBot/NextBotIntentionInterface.h"
	#include "filesystem.h"
	#include "trok/trok_killstreak.h"
	#include "trok/trok_economia.h"
	#include "trok/trok_corrida.h"
	#include "trok/trok_hordas.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// O servidor cria as regras por nome em InstallGameRules() (ver
// patches/sdk.patch, hl2mp_client.cpp) e o cliente cria a classe com o
// mesmo nome quando a string chega.
REGISTER_GAMERULES_CLASS( CTrokRules );

#ifndef CLIENT_DLL
ConVar trok_vida( "trok_vida", "100", FCVAR_NOTIFY, "Vida (e vida máxima) ao nascer." );
ConVar trok_colete( "trok_colete", "0", FCVAR_NOTIFY, "Colete (armadura do HL2) ao nascer." );

// --- Modo Surto (números de partida: medir na quarentena do Combat Arms e ajustar em cfg/surto.cfg)
ConVar trok_surto_preparo( "trok_surto_preparo", "20", FCVAR_NOTIFY, "Segundos de preparo: todos humanos, antes de sortear os zumbis." );
ConVar trok_surto_tempo( "trok_surto_tempo", "160", FCVAR_NOTIFY, "Segundos de rodada depois do sorteio (2:40)." );
ConVar trok_surto_alarme( "trok_surto_alarme", "80", FCVAR_NOTIFY, "Faltando estes segundos toca o alarme (1:20)." );
ConVar trok_surto_abrigos( "trok_surto_abrigos", "60", FCVAR_NOTIFY, "Faltando estes segundos os abrigos abrem (1:00)." );
ConVar trok_surto_fim( "trok_surto_fim", "8", FCVAR_NOTIFY, "Segundos mostrando o vencedor antes da próxima rodada." );
ConVar trok_surto_min_jogadores( "trok_surto_min_jogadores", "2", FCVAR_NOTIFY, "Mínimo de jogadores (com bots) para começar uma rodada." );
ConVar trok_surto_jogadores_por_hospedeiro( "trok_surto_jogadores_por_hospedeiro", "8", FCVAR_NOTIFY, "Um zumbi hospedeiro a cada N jogadores." );
ConVar trok_surto_hospedeiros_max( "trok_surto_hospedeiros_max", "3", FCVAR_NOTIFY, "Máximo de hospedeiros sorteados." );
ConVar trok_surto_vida_hospedeiro( "trok_surto_vida_hospedeiro", "2500", FCVAR_NOTIFY, "Vida do zumbi hospedeiro (os sorteados)." );
ConVar trok_surto_vida_zumbi( "trok_surto_vida_zumbi", "1500", FCVAR_NOTIFY, "Vida do zumbi infectado (humano que virou)." );
ConVar trok_surto_virar_tempo( "trok_surto_virar_tempo", "2", FCVAR_NOTIFY, "Segundos entre o humano morrer e levantar como zumbi." );
ConVar trok_surto_virar_no_lugar( "trok_surto_virar_no_lugar", "1", FCVAR_NOTIFY, "O infectado levanta onde morreu (0 = nasce num ponto de nascimento)." );
ConVar trok_surto_zumbi_renasce( "trok_surto_zumbi_renasce", "0", FCVAR_NOTIFY, "Zumbi morto volta (0 = fica morto até a próxima rodada; humanos vencem matando todos)." );
ConVar trok_surto_regen( "trok_surto_regen", "0.10", FCVAR_NOTIFY, "Fração da vida máxima que o zumbi parado recupera a cada intervalo." );
ConVar trok_surto_regen_intervalo( "trok_surto_regen_intervalo", "1", FCVAR_NOTIFY, "Segundos entre as recuperações do zumbi parado (CA: 1)." );
ConVar trok_surto_regen_espera( "trok_surto_regen_espera", "3", FCVAR_NOTIFY, "Segundos parado, sem levar dano e sem atacar, antes da primeira recuperação (CA: 3)." );
ConVar trok_surto_zumbi_sem_critico( "trok_surto_zumbi_sem_critico", "1", FCVAR_NOTIFY, "Zumbi não leva o multiplicador de tiro na cabeça." );
ConVar trok_surto_empurrao( "trok_surto_empurrao", "1", FCVAR_NOTIFY, "Multiplica o empurrão e o 'segurar' das armas nos zumbis (0 = desliga)." );
ConVar trok_surto_empurrao_max( "trok_surto_empurrao_max", "450", FCVAR_NOTIFY, "Velocidade horizontal máxima (u/s) que o empurrão pode dar." );
// Com trok_mov_ca 1 o empurrão é o do CA (ca_empurrao e ca_empurrao_tempo no script da arma).
ConVar trok_surto_ca_granada_empurrao( "trok_surto_ca_granada_empurrao", "135", FCVAR_NOTIFY, "Movimento do CA: força K do empurrão de explosão no infectado (M67: 135)." );
ConVar trok_surto_ca_granada_tempo( "trok_surto_ca_granada_tempo", "0.4", FCVAR_NOTIFY, "Movimento do CA: duração (s) do empurrão de explosão (M67: 0,4)." );
extern ConVar trok_mov_ca;
ConVar trok_surto_zumbi_folego( "trok_surto_zumbi_folego", "2", FCVAR_NOTIFY, "Fôlego do zumbi correndo, em vezes o do humano (CA: o dobro)." );
ConVar trok_surto_assistir( "trok_surto_assistir", "3", FCVAR_NOTIFY, "Segundos depois de morrer (sem virar zumbi) para passar a assistir os vivos." );
ConVar trok_surto_cor_zumbi( "trok_surto_cor_zumbi", "185 70 62", FCVAR_NOTIFY, "Tom do zumbi enquanto não há modelo próprio (R G B; sangue, decisão do dono)." );
#endif

//-----------------------------------------------------------------------------
// Caixa e olho do jogador no movimento do Combat Arms (design/decombat/specs/MOVEMENT_2009.md
// e as specs de movimento do Chimera): no CA o corpo tem 100u em pé e 80u agachado (só 20% mais
// baixo; no Source é 50%), olho a 83u em pé e 60u agachado, caixa de 30u de largura.
//
// ESCALA EXATA DO CA (decisão do dono): estes números são os do CA x 0,72 e o jogador nasce com
// escala de modelo trok_ca_corpo_escala (100/72). O Source multiplica a caixa e o olho pela escala
// do modelo (VEC_*_SCALED), então o resultado é exatamente o do CA: 30 x 30 x 100 em pé, 80
// agachado, olho a 83 e 60; e o boneco do HL2/CS:S (72u) fica com 100u, como o do CA. Velocidades,
// pulo e gravidade já são os do CA em unidades 1:1 (trok_ca_escala 1). Com trok_ca_corpo_escala 1
// fica o corpo proporcional ao CA na altura do Source (72u).
//-----------------------------------------------------------------------------
extern ConVar trok_mov_ca;
ConVar trok_ca_corpo_escala( "trok_ca_corpo_escala", "1.3888889", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Movimento do CA: escala do corpo do jogador. 1.3888889 = corpo exato do CA (100u em pé); 1 = altura do Source (72u). Vale ao renascer." );
static CViewVectors g_TrokCAViewVectors(
	Vector( 0, 0, 59.76f ),				// olho em pé (83 do CA x 0,72)
	Vector( -10.8f, -10.8f, 0 ),		// caixa em pé (30 x 100 do CA x 0,72)
	Vector( 10.8f, 10.8f, 72 ),
	Vector( -10.8f, -10.8f, 0 ),		// caixa agachado (80 do CA x 0,72)
	Vector( 10.8f, 10.8f, 57.6f ),
	Vector( 0, 0, 43.2f ),				// olho agachado (60 do CA x 0,72)
	Vector( -10, -10, -10 ),			// espectador
	Vector( 10, 10, 10 ),
	Vector( 0, 0, 14 ) );				// olho do morto

const CViewVectors *CTrokRules::GetViewVectors() const
{
	return trok_mov_ca.GetBool() ? &g_TrokCAViewVectors : BaseClass::GetViewVectors();
}

CTrokRules::CTrokRules()
{
#ifndef CLIENT_DLL
	m_iModo = TrokModo();
	m_bSurto = ( m_iModo == TROK_MODO_SURTO );
	m_flSurtoTick = 0.0f;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
	{
		m_flVirar[ i ] = 0.0f;
		m_vecMorte[ i ] = vec3_invalid;
		m_bHospedeiro[ i ] = false;
		m_bNaRodada[ i ] = false;
		m_flUltimoDano[ i ] = -100.0f;
		m_flProxRegen[ i ] = 0.0f;
		m_flMorte[ i ] = 0.0f;
		m_flProtegido[ i ] = 0.0f;
		m_iRodadaInfec[ i ] = 0;
		m_iRodadaZumbis[ i ] = 0;
	}
	m_flUltimoThink = 0.0f;
	TrokZumbis_Precache();	// olhos que brilham, bolhas (trok_zumbis.cpp)
	if ( m_iModo == TROK_MODO_SURTO )
	{
		// Surto é sempre em times (humanos x zumbis) e lê os números do cfg do modo.
		m_bTeamPlayEnabled = true;
		engine->ServerCommand( "exec surto.cfg\n" );
	}
	else if ( m_iModo == TROK_MODO_SOLO )
	{
		// Exército solo: todos contra todos.
		m_bTeamPlayEnabled = false;
		engine->ServerCommand( "exec solo.cfg\n" );
	}
	else if ( m_iModo == TROK_MODO_CORRIDA )
	{
		// Corrida Armada: todos contra todos com escada de armas (trok_corrida.cpp).
		m_bTeamPlayEnabled = false;
		engine->ServerCommand( "exec corrida.cfg\n" );
	}
	else if ( m_iModo == TROK_MODO_HORDAS )
	{
		// Hordas: humanos (cooperativo) contra zumbis bots, por rodadas (trok_hordas_diretor.cpp).
		m_bTeamPlayEnabled = true;
		engine->ServerCommand( "exec hordas.cfg\n" );
	}
	else
	{
		m_bTeamPlayEnabled = true;
		engine->ServerCommand( "exec equipe.cfg\n" );
	}
	TrokCorrida_Iniciar( m_iModo == TROK_MODO_CORRIDA );
	TrokHordas_Iniciar( m_iModo == TROK_MODO_HORDAS );

	// Partida criada pelo menu (CRIAR PARTIDA): meta, tempo, nome e senha escolhidos vêm por cima do cfg
	// do modo. O menu escreve este arquivo ao criar e o apaga ao jogar contra bots pelo JOGAR.
	if ( filesystem->FileExists( "cfg/partida_menu.cfg", "MOD" ) )
		engine->ServerCommand( "exec partida_menu.cfg\n" );
#endif
}

CTrokRules::~CTrokRules()
{
}

const char *CTrokRules::GetGameDescription( void )
{
	return "Rearmed"; // nome provisório do jogo (design/lore-e-arte.md)
}

#ifndef CLIENT_DLL

extern void respawn( CBaseEntity *pEdict, bool fCopyCorpse );
void TrokSurto_DispararMapa( int iEvento );	// server/trok/trok_surto_mapa.cpp

static bool IsPlaying( CBasePlayer *pPlayer )
{
	return pPlayer && pPlayer->IsConnected() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR;
}

//-----------------------------------------------------------------------------
// Nascer
//-----------------------------------------------------------------------------
// Chamado dentro de CBasePlayer::Spawn, depois da vida padrão e antes das armas.
void CTrokRules::PlayerSpawn( CBasePlayer *pPlayer )
{
	BaseClass::PlayerSpawn( pPlayer );

	int iHealth = MAX( 1, trok_vida.GetInt() );
	pPlayer->SetMaxHealth( iHealth );
	pPlayer->SetHealth( iHealth );
	pPlayer->SetArmorValue( MAX( 0, trok_colete.GetInt() ) );

	// Corpo exato do CA (ver g_TrokCAViewVectors).
	pPlayer->SetModelScale( trok_mov_ca.GetBool() ? clamp( trok_ca_corpo_escala.GetFloat(), 0.5f, 2.0f ) : 1.0f );

	TrokSerie_PlayerSpawn( pPlayer );		// série de abates (trok_killstreak.cpp)
	TrokEconomia_PlayerSpawn( pPlayer );	// prazo e lugar da compra (server/trok/trok_economia.cpp)

	if ( m_iModo == TROK_MODO_HORDAS )
	{
		// Hordas: zumbi do diretor ou humano; sem proteção ao nascer (trok_hordas_diretor.cpp).
		TrokHordas_PlayerSpawn( pPlayer );
		return;
	}
	if ( !m_bSurto )
	{
		// Equipe e solo: proteção ao nascer.
		if ( pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
			SetProtegido( pPlayer, gpGlobals->curtime + MAX( 0.0f, trok_protecao.GetFloat() ) );
		return;
	}
	if ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
		return;

	const int i = pPlayer->entindex();
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	const int iFase = pEstado ? pEstado->GetFase() : SURTO_DESLIGADO;

	// No preparo todo mundo é humano. Durante a rodada, quem chegou atrasado entra zumbi.
	if ( iFase == SURTO_JOGO && !m_bNaRodada[ i ] && m_flVirar[ i ] == 0.0f )
	{
		m_bNaRodada[ i ] = true;
		SurtoTrocarTime( pPlayer, TEAM_TROK_ZUMBIS );
	}
	else if ( iFase != SURTO_JOGO && pPlayer->GetTeamNumber() != TEAM_TROK_HUMANOS )
	{
		SurtoTrocarTime( pPlayer, TEAM_TROK_HUMANOS );
	}

	if ( pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
		SurtoAplicarZumbi( pPlayer );
	else
		SurtoAplicarHumano( pPlayer );
}

void CTrokRules::SurtoTrocarTime( CBasePlayer *pPlayer, int iTeam )
{
	if ( pPlayer->GetTeamNumber() == iTeam )
		return;
	// A troca do HL2MP mata o jogador; a do CBasePlayer só muda o time (silenciosa).
	pPlayer->CBasePlayer::ChangeTeam( iTeam, false, true );
	CHL2MP_Player *pHL2MP = dynamic_cast< CHL2MP_Player * >( pPlayer );
	if ( pHL2MP )
		pHL2MP->SetPlayerTeamModel();

	// Bot que muda de lado (hospedeiro sorteado vivo) recomeça o cérebro como zumbi/humano.
	INextBot *pBot = pPlayer->MyNextBotPointer();
	if ( pBot && pBot->GetIntentionInterface() )
		pBot->GetIntentionInterface()->Reset();
}

void CTrokRules::SurtoAplicarHumano( CBasePlayer *pPlayer )
{
	pPlayer->SetRenderColor( 255, 255, 255 );
	TrokZumbis_AoVirarHumano( pPlayer );	// tipos de zumbi (trok_zumbis.cpp)
}

// Zumbi: só garras, vida de zumbi, tom esverdeado (até haver modelo próprio), levanta onde morreu.
void CTrokRules::SurtoAplicarZumbi( CBasePlayer *pPlayer )
{
	const int i = pPlayer->entindex();

	pPlayer->RemoveAllItems( false );
	pPlayer->GiveNamedItem( "weapon_trok_garras" );
	CBaseCombatWeapon *pGarras = pPlayer->Weapon_OwnsThisType( "weapon_trok_garras" );
	if ( pGarras )
		pPlayer->Weapon_Switch( pGarras );

	// Tipo de zumbi escolhido (normal, Striker, Crusher...; scripts/trok_zumbis.txt).
	const TrokZumbiClasse_t &tipo = TrokZumbis_Classe( TrokZumbis_AoVirar( pPlayer ) );
	const float flVidaTipo = m_bHospedeiro[ i ] ? tipo.flVidaHospedeiro : tipo.flVida;
	const int iVida = MAX( 1, flVidaTipo > 0.0f ? (int)flVidaTipo : ( m_bHospedeiro[ i ] ? trok_surto_vida_hospedeiro.GetInt() : trok_surto_vida_zumbi.GetInt() ) );
	pPlayer->SetMaxHealth( iVida );
	pPlayer->SetHealth( iVida );
	pPlayer->SetArmorValue( 0 );
	if ( tipo.flEscala != 1.0f )
		pPlayer->SetModelScale( pPlayer->GetModelScale() * clamp( tipo.flEscala, 0.5f, 2.0f ) );

	int r = 150, g = 225, b = 140;
	sscanf( trok_surto_cor_zumbi.GetString(), "%d %d %d", &r, &g, &b );
	if ( tipo.iCor[ 0 ] >= 0 )
	{
		r = tipo.iCor[ 0 ];
		g = tipo.iCor[ 1 ];
		b = tipo.iCor[ 2 ];
	}
	pPlayer->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ) );

	if ( trok_surto_virar_no_lugar.GetBool() && m_vecMorte[ i ] != vec3_invalid )
	{
		pPlayer->Teleport( &m_vecMorte[ i ], NULL, &vec3_origin );
		m_vecMorte[ i ] = vec3_invalid;
	}

	m_flUltimoDano[ i ] = gpGlobals->curtime;
	m_flProxRegen[ i ] = gpGlobals->curtime + trok_surto_regen_espera.GetFloat();
}

//-----------------------------------------------------------------------------
// Rodada
//-----------------------------------------------------------------------------
void CTrokRules::Think( void )
{
	BaseClass::Think();

	if ( g_fGameOver )
		return;
	if ( m_bSurto )
		SurtoThink();
	else if ( m_iModo != TROK_MODO_HORDAS )
		ModoThink();
	if ( m_iModo == TROK_MODO_CORRIDA )
		TrokCorrida_Think();
	TrokHordas_Think();		// rodadas do Hordas (e, nos outros modos, tira bots que sobraram dele)
}

//-----------------------------------------------------------------------------
// Equipe e solo (como o Combat Arms): renasce sozinho depois da contagem, nasce
// protegido por uns segundos (acaba se atirar) e longe dos inimigos.
//-----------------------------------------------------------------------------
void CTrokRules::ModoThink( void )
{
	const float flNow = gpGlobals->curtime;
	const float flTempo = MAX( 0.0f, trok_renascer_tempo.GetFloat() );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;

		if ( !pPlayer->IsAlive() && !pPlayer->IsObserver() && m_flMorte[ i ] > 0.0f && flNow - m_flMorte[ i ] >= flTempo )
		{
			m_flMorte[ i ] = 0.0f;
			respawn( pPlayer, false );
			continue;
		}

		if ( m_flProtegido[ i ] > 0.0f && ( flNow >= m_flProtegido[ i ] || !pPlayer->IsAlive() ) )
			SetProtegido( pPlayer, 0.0f );
	}
}

void CTrokRules::SetProtegido( CBasePlayer *pPlayer, float flAte )
{
	const int i = pPlayer->entindex();
	m_flProtegido[ i ] = flAte;
	CHL2MP_Player *pHL2MP = dynamic_cast< CHL2MP_Player * >( pPlayer );
	if ( pHL2MP )
		pHL2MP->m_flTrokProtegidoAte = flAte;
	// Os outros veem o protegido translúcido.
	if ( flAte > gpGlobals->curtime )
	{
		pPlayer->SetRenderMode( kRenderTransAlpha );
		pPlayer->SetRenderColorA( 130 );
	}
	else
	{
		pPlayer->SetRenderColorA( 255 );
		pPlayer->SetRenderMode( kRenderNormal );
	}
}

void CTrokRules::OnPlayerFired( CBasePlayer *pPlayer )
{
	if ( pPlayer && m_flProtegido[ pPlayer->entindex() ] > 0.0f )
		SetProtegido( pPlayer, 0.0f );
}

bool CTrokRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info )
{
	if ( m_iModo == TROK_MODO_HORDAS && !TrokHordas_PodeLevarDano( pPlayer, pAttacker ) )
		return false;	// derrubado e fogo amigo (trok_hordas_diretor.cpp)
	if ( !m_bSurto && pPlayer && m_flProtegido[ pPlayer->entindex() ] > gpGlobals->curtime &&
		 pAttacker && pAttacker->IsPlayer() && pAttacker != pPlayer )
		return false;
	return BaseClass::FPlayerCanTakeDamage( pPlayer, pAttacker, info );
}

// Nota de um ponto de nascimento: longe do inimigo vivo mais perto, e pior se algum
// inimigo enxerga o ponto.
float CTrokRules::SpawnScore( CBasePlayer *pPlayer, CBaseEntity *pSpot )
{
	const Vector spot = pSpot->GetAbsOrigin() + Vector( 0, 0, 64 );
	float flNearest = 4000.0f;
	bool bVisto = false;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pPlayer || !p->IsAlive() || p->GetTeamNumber() == TEAM_SPECTATOR )
			continue;
		if ( IsTeamplay() && p->GetTeamNumber() == pPlayer->GetTeamNumber() )
			continue;
		const float d = ( p->GetAbsOrigin() - pSpot->GetAbsOrigin() ).Length();
		flNearest = MIN( flNearest, d );
		if ( !bVisto && d < 2500.0f )
		{
			trace_t tr;
			UTIL_TraceLine( p->EyePosition(), spot, MASK_VISIBLE, p, COLLISION_GROUP_NONE, &tr );
			bVisto = tr.fraction > 0.99f;
		}
	}
	return flNearest - ( bVisto ? 1500.0f : 0.0f ) + RandomFloat( 0.0f, 200.0f );
}

CBaseEntity *CTrokRules::GetPlayerSpawnSpot( CBasePlayer *pPlayer )
{
	if ( m_bSurto )
		return BaseClass::GetPlayerSpawnSpot( pPlayer );

	// Sorteia alguns pontos válidos pelo próprio HL2MP e fica com o mais seguro.
	CBaseEntity *pBest = NULL;
	float flBest = -FLT_MAX;
	for ( int k = 0; k < 6; ++k )
	{
		CBaseEntity *pSpot = pPlayer->EntSelectSpawnPoint();
		if ( !pSpot )
			continue;
		const float flScore = SpawnScore( pPlayer, pSpot );
		if ( flScore > flBest )
		{
			flBest = flScore;
			pBest = pSpot;
		}
	}
	if ( !pBest )
		return BaseClass::GetPlayerSpawnSpot( pPlayer );

	// O mesmo que CGameRules::GetPlayerSpawnSpot faz com o ponto escolhido.
	pPlayer->SetLocalOrigin( pBest->GetAbsOrigin() + Vector( 0, 0, 1 ) );
	pPlayer->SetAbsVelocity( vec3_origin );
	pPlayer->SetLocalAngles( pBest->GetLocalAngles() );
	pPlayer->m_Local.m_vecPunchAngle = vec3_angle;
	pPlayer->m_Local.m_vecPunchAngleVel = vec3_angle;
	pPlayer->SnapEyeAngles( pBest->GetLocalAngles() );
	return pBest;
}

void CTrokRules::SurtoContar( int &iHumanos, int &iZumbis )
{
	iHumanos = iZumbis = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		if ( m_flVirar[ i ] > 0.0f )
			++iZumbis; // morto, vai levantar zumbi
		else if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_HUMANOS )
			++iHumanos;
		else if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
			++iZumbis;
	}
}

void CTrokRules::SurtoThink( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
	{
		pEstado = static_cast< CTrokSurtoEstado * >( CreateEntityByName( "trok_surto_estado" ) );
		if ( !pEstado )
			return;
		DispatchSpawn( pEstado );
		m_hEstado = pEstado;
		SurtoPreparo();
		return;
	}

	const float flNow = gpGlobals->curtime;

	// Quem morreu levanta zumbi na hora marcada.
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( m_flVirar[ i ] <= 0.0f || flNow < m_flVirar[ i ] )
			continue;
		m_flVirar[ i ] = 0.0f;
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( IsPlaying( pPlayer ) && pEstado->GetFase() == SURTO_JOGO )
			SurtoVirarZumbi( pPlayer, false );
	}

	int iHumanos, iZumbis;
	SurtoContar( iHumanos, iZumbis );
	pEstado->m_iHumanos = iHumanos;
	pEstado->m_iZumbis = iZumbis;

	switch ( pEstado->GetFase() )
	{
	case SURTO_PREPARO:
		if ( flNow >= pEstado->GetFimFase() )
		{
			if ( iHumanos >= MAX( 2, trok_surto_min_jogadores.GetInt() ) )
				SurtoComecar();
			else
				pEstado->m_flFimFase = flNow + 5.0f; // esperando gente
		}
		break;

	case SURTO_JOGO:
	{
		const float flLeft = pEstado->GetFimFase() - flNow;
		if ( !pEstado->IsAlarme() && flLeft <= trok_surto_alarme.GetFloat() )
		{
			pEstado->m_bAlarme = true;
			CBroadcastRecipientFilter filter;
			CBaseEntity::EmitSound( filter, 0, "Trok.Surto.Alarme" );
			TrokSurto_DispararMapa( 2 );
		}
		if ( !pEstado->IsAbrigos() && flLeft <= trok_surto_abrigos.GetFloat() )
		{
			pEstado->m_bAbrigos = true;
			TrokSurto_DispararMapa( 3 );
		}

		SurtoRegenerar();
		TrokZumbis_Think();		// fôlego infinito, aura, bolhas (trok_zumbis.cpp)
		SurtoFolegoZumbi();
		SurtoEspectadores();

		if ( iHumanos == 0 )
			SurtoFim( SURTO_ZUMBIS );
		else if ( iZumbis == 0 )
			SurtoFim( SURTO_HUMANOS );
		else if ( flLeft <= 0.0f )
			SurtoFim( SURTO_HUMANOS );
		break;
	}

	case SURTO_FIM:
		if ( flNow >= pEstado->GetFimFase() )
			SurtoPreparo();
		break;

	default:
		SurtoPreparo();
		break;
	}
}

// Rodada nova: mapa limpo (grades e portas voltam), todos humanos, preparo.
void CTrokRules::SurtoPreparo( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	// O CleanUpMap recria as entidades do mapa; o estado é nosso e fica (está na lista de
	// classes preservadas do HL2MP? não: por isso ele é recriado se sumir, no SurtoThink).
	CleanUpMap();
	TrokSerie_RodadaNova();	// primeiro abate da rodada (trok_killstreak.cpp)
	pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		m_flVirar[ i ] = 0.0f;
		m_vecMorte[ i ] = vec3_invalid;
		m_bHospedeiro[ i ] = false;
		m_bNaRodada[ i ] = false;
		m_iRodadaInfec[ i ] = 0;
		m_iRodadaZumbis[ i ] = 0;

		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		SurtoTirarDeEspectador( pPlayer );
		SurtoTrocarTime( pPlayer, TEAM_TROK_HUMANOS );
		if ( pPlayer->GetActiveWeapon() )
			pPlayer->GetActiveWeapon()->Holster();
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false );
	}

	pEstado->m_iFase = SURTO_PREPARO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 1.0f, trok_surto_preparo.GetFloat() );
	pEstado->m_iVencedor = SURTO_NINGUEM;
	pEstado->m_iDestaque = 0;
	pEstado->m_iDestaqueTipo = SURTO_DESTAQUE_NENHUM;
	pEstado->m_iDestaqueValor = 0;
	pEstado->m_bAlarme = false;
	pEstado->m_bAbrigos = false;
	TrokSurto_DispararMapa( 0 );
}

// Sorteio dos hospedeiros (1 a cada N jogadores, até o máximo, sempre sobra humano).
void CTrokRules::SurtoComecar( void )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;

	CUtlVector< CBasePlayer * > humanos;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		m_bNaRodada[ i ] = true;
		if ( pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_HUMANOS )
			humanos.AddToTail( pPlayer );
	}

	const int iPor = MAX( 1, trok_surto_jogadores_por_hospedeiro.GetInt() );
	int iHosts = ( humanos.Count() + iPor - 1 ) / iPor;
	iHosts = clamp( iHosts, 1, MAX( 1, trok_surto_hospedeiros_max.GetInt() ) );
	iHosts = MIN( iHosts, humanos.Count() - 1 );

	for ( int k = 0; k < iHosts && humanos.Count() > 0; ++k )
	{
		int j = RandomInt( 0, humanos.Count() - 1 );
		CBasePlayer *pHost = humanos[ j ];
		humanos.FastRemove( j );
		m_bHospedeiro[ pHost->entindex() ] = true;
		m_vecMorte[ pHost->entindex() ] = pHost->GetAbsOrigin();
		SurtoVirarZumbi( pHost, true );
	}

	pEstado->m_iFase = SURTO_JOGO;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 10.0f, trok_surto_tempo.GetFloat() );
	TrokSurto_DispararMapa( 1 );
}

void CTrokRules::SurtoVirarZumbi( CBasePlayer *pPlayer, bool bHospedeiro )
{
	const int i = pPlayer->entindex();
	if ( bHospedeiro )
		m_bHospedeiro[ i ] = true;
	SurtoTrocarTime( pPlayer, TEAM_TROK_ZUMBIS );

	if ( pPlayer->IsAlive() )
	{
		// Hospedeiro sorteado: vira na hora, no lugar.
		m_vecMorte[ i ] = vec3_invalid;
		SurtoAplicarZumbi( pPlayer );
	}
	else
	{
		SurtoTirarDeEspectador( pPlayer );
		if ( pPlayer->GetActiveWeapon() )
			pPlayer->GetActiveWeapon()->Holster();
		pPlayer->RemoveAllItems( true );
		respawn( pPlayer, false ); // Spawn -> PlayerSpawn -> SurtoAplicarZumbi
	}

	pPlayer->EmitSound( "Trok.Surto.Virou" );
}

void CTrokRules::SurtoFim( int iVencedor )
{
	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !pEstado )
		return;
	pEstado->m_iFase = SURTO_FIM;
	pEstado->m_iVencedor = iVencedor;
	pEstado->m_flFimFase = gpGlobals->curtime + MAX( 2.0f, trok_surto_fim.GetFloat() );

	CTeam *pTeam = GetGlobalTeam( iVencedor == SURTO_HUMANOS ? TEAM_TROK_HUMANOS : TEAM_TROK_ZUMBIS );
	if ( pTeam )
		pTeam->AddScore( 1 );

	// Sobreviventes e destaque da rodada (quem mais infectou ou mais abateu zumbis).
	int iBest = 0, iBestVal = 0, iBestTipo = SURTO_DESTAQUE_NENHUM;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) )
			continue;
		if ( iVencedor == SURTO_HUMANOS && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_TROK_HUMANOS )
			pEstado->m_aSobreviveu.Set( i, pEstado->m_aSobreviveu[ i ] + 1 );
		const int iVal = ( iVencedor == SURTO_HUMANOS ) ? m_iRodadaZumbis[ i ] : m_iRodadaInfec[ i ];
		if ( iVal > iBestVal )
		{
			iBestVal = iVal;
			iBest = i;
			iBestTipo = ( iVencedor == SURTO_HUMANOS ) ? SURTO_DESTAQUE_ZUMBIS : SURTO_DESTAQUE_INFECCOES;
		}
	}
	pEstado->m_iDestaque = iBest;
	pEstado->m_iDestaqueTipo = iBestTipo;
	pEstado->m_iDestaqueValor = iBestVal;

	// Economia: bônus de vitória e derrota (server/trok/trok_economia.cpp).
	TrokEconomia_FimRodada( iVencedor == SURTO_HUMANOS ? TEAM_TROK_HUMANOS : TEAM_TROK_ZUMBIS );
	TrokZumbis_FimRodada( iVencedor == SURTO_ZUMBIS );	// dinheiro de infectado (trok_zumbis.cpp)

	TrokSurto_DispararMapa( iVencedor == SURTO_HUMANOS ? 4 : 5 );
}

// Zumbi parado recupera vida (CA: 10% da vida máxima por segundo depois de 3 s parado).
// Andar (qualquer mudança de posição), atacar ou levar dano recomeça a espera; a primeira
// recuperação vem assim que a espera acaba, depois uma a cada intervalo.
void CTrokRules::SurtoRegenerar( void )
{
	const float flNow = gpGlobals->curtime;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_TROK_ZUMBIS )
			continue;
		// Regeneração do tipo de zumbi (Crusher: 20%) ou a do modo.
		const float flRegenTipo = TrokZumbis_Classe( TrokZumbis_ClasseDe( pPlayer ) ).flRegen;
		const float flFrac = flRegenTipo >= 0.0f ? flRegenTipo : trok_surto_regen.GetFloat();
		if ( flFrac <= 0.0f )
			continue;

		const bool bParado = pPlayer->GetAbsVelocity().Length() < 1.0f;
		const bool bAtacando = ( pPlayer->m_nButtons & ( IN_ATTACK | IN_ATTACK2 ) ) != 0;
		if ( !bParado || bAtacando )
		{
			m_flProxRegen[ i ] = flNow + trok_surto_regen_espera.GetFloat();
			continue;
		}
		if ( flNow < m_flProxRegen[ i ] )
			continue;
		m_flProxRegen[ i ] = flNow + trok_surto_regen_intervalo.GetFloat();
		const int iMax = pPlayer->GetMaxHealth();
		if ( pPlayer->GetHealth() < iMax )
			pPlayer->SetHealth( MIN( iMax, pPlayer->GetHealth() + MAX( 1, (int)( iMax * flFrac ) ) ) );
	}
}

// Morto que não vai virar zumbi passa a assistir quem está vivo (câmera atrás de alguém).
void CTrokRules::SurtoEspectadores( void )
{
	const float flEspera = MAX( 0.5f, trok_surto_assistir.GetFloat() );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) || pPlayer->IsAlive() || pPlayer->IsObserver() || m_flVirar[ i ] > 0.0f )
			continue;
		if ( gpGlobals->curtime - m_flMorte[ i ] < flEspera )
			continue;
		if ( pPlayer->StartObserverMode( OBS_MODE_CHASE ) )
		{
			// De preferência alguém do próprio time.
			CBasePlayer *pAlvo = NULL;
			for ( int j = 1; j <= gpGlobals->maxClients && !pAlvo; ++j )
			{
				CBasePlayer *p = UTIL_PlayerByIndex( j );
				if ( p && p != pPlayer && p->IsAlive() && p->GetTeamNumber() == pPlayer->GetTeamNumber() )
					pAlvo = p;
			}
			for ( int j = 1; j <= gpGlobals->maxClients && !pAlvo; ++j )
			{
				CBasePlayer *p = UTIL_PlayerByIndex( j );
				if ( p && p != pPlayer && p->IsAlive() )
					pAlvo = p;
			}
			if ( pAlvo )
				pPlayer->SetObserverTarget( pAlvo );
		}
	}
}

void CTrokRules::SurtoTirarDeEspectador( CBasePlayer *pPlayer )
{
	if ( pPlayer->IsObserver() && pPlayer->GetTeamNumber() != TEAM_SPECTATOR )
		pPlayer->StopObserverMode();
}

// Fôlego do zumbi: devolve parte do gasto da corrida (o HL2MP gasta 25/s correndo).
void CTrokRules::SurtoFolegoZumbi( void )
{
	const float flNow = gpGlobals->curtime;
	const float dt = clamp( flNow - m_flUltimoThink, 0.0f, 0.5f );
	m_flUltimoThink = flNow;
	const float flMult = MAX( 1.0f, trok_surto_zumbi_folego.GetFloat() );
	if ( dt <= 0.0f || flMult <= 1.0f )
		return;
	const float flRefund = 25.0f * ( 1.0f - 1.0f / flMult ) * dt;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CHL2_Player *pHL2 = dynamic_cast< CHL2_Player * >( UTIL_PlayerByIndex( i ) );
		if ( pHL2 && pHL2->IsAlive() && pHL2->GetTeamNumber() == TEAM_TROK_ZUMBIS && pHL2->IsSprinting() )
			pHL2->SuitPower_Charge( flRefund );
	}
}

//-----------------------------------------------------------------------------
// Morte, dano, itens
//-----------------------------------------------------------------------------
void CTrokRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	TrokZumbis_AoMorrer( pVictim );		// zumbi que explode / solta nuvem tóxica (trok_zumbis.cpp)
	const int iFragsAntes = pVictim->FragCount();
	BaseClass::PlayerKilled( pVictim, info );
	m_flMorte[ pVictim->entindex() ] = gpGlobals->curtime;
	if ( m_iModo == TROK_MODO_CORRIDA )
		TrokCorrida_Abate( pVictim, info, iFragsAntes );	// escada de armas (trok_corrida.cpp)
	if ( m_iModo == TROK_MODO_HORDAS )
		TrokHordas_Abate( pVictim, info );		// pontos, bônus, humano que morreu (trok_hordas_diretor.cpp)
	TrokSerie_PlayerKilled( pVictim, info );		// série de abates (trok_killstreak.cpp)
	TrokEconomia_PlayerKilled( pVictim, info );		// dinheiro do abate (server/trok/trok_economia.cpp)

	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !m_bSurto || !pEstado || pEstado->GetFase() != SURTO_JOGO )
		return;

	const int i = pVictim->entindex();

	// Infecção, como no CA (Chimera M8): só o golpe de garra do infectado infecta. Humano que morre
	// de outro jeito (queda, granada, tiro, gás, explosão do Charger) sai da rodada e só assiste.
	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	const bool bInfeccao = pAttacker && pAttacker != pVictim && pAttacker->GetTeamNumber() == TEAM_TROK_ZUMBIS &&
		pVictim->GetTeamNumber() == TEAM_TROK_HUMANOS && ( info.GetDamageType() & ( DMG_CLUB | DMG_SLASH ) ) != 0;

	// Estatísticas: infecção (zumbi matou humano) e zumbi abatido (humano matou zumbi).
	if ( pAttacker && pAttacker != pVictim )
	{
		const int a = pAttacker->entindex();
		if ( bInfeccao )
		{
			pEstado->m_aInfeccoes.Set( a, pEstado->m_aInfeccoes[ a ] + 1 );
			++m_iRodadaInfec[ a ];
			TrokZumbis_Infectou( pAttacker );	// dinheiro de infectado (trok_zumbis.cpp)
		}
		else if ( pAttacker->GetTeamNumber() == TEAM_TROK_HUMANOS && pVictim->GetTeamNumber() == TEAM_TROK_ZUMBIS )
		{
			pEstado->m_aZumbisAbatidos.Set( a, pEstado->m_aZumbisAbatidos[ a ] + 1 );
			++m_iRodadaZumbis[ a ];
		}
	}

	if ( pVictim->GetTeamNumber() == TEAM_TROK_HUMANOS && bInfeccao )
	{
		// Infectado pela garra: levanta zumbi onde morreu.
		m_vecMorte[ i ] = pVictim->GetAbsOrigin();
		m_flVirar[ i ] = gpGlobals->curtime + MAX( 0.1f, trok_surto_virar_tempo.GetFloat() );
	}
	// Morte sem infecção: fica morto até a próxima rodada (SurtoEspectadores o põe para assistir).
	else if ( pVictim->GetTeamNumber() == TEAM_TROK_ZUMBIS && trok_surto_zumbi_renasce.GetBool() )
	{
		m_vecMorte[ i ] = vec3_invalid;
		m_flVirar[ i ] = gpGlobals->curtime + 5.0f;
	}
}

bool CTrokRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
	if ( m_iModo == TROK_MODO_HORDAS )
		return TrokHordas_PodeRenascer( pPlayer ) && BaseClass::FPlayerCanRespawn( pPlayer );	// o diretor traz de volta
	if ( m_bSurto )
	{
		CTrokSurtoEstado *pEstado = m_hEstado.Get();
		// Na rodada e no fim, quem renasce é o diretor (virar zumbi); no preparo, normal.
		if ( pEstado && ( pEstado->GetFase() == SURTO_JOGO || pEstado->GetFase() == SURTO_FIM ) )
			return false;
	}
	else if ( gpGlobals->curtime - m_flMorte[ pPlayer->entindex() ] < trok_renascer_tempo.GetFloat() )
	{
		return false; // a contagem ainda não acabou (o ModoThink renasce sozinho no fim)
	}
	return BaseClass::FPlayerCanRespawn( pPlayer );
}

// Dano de queda do CA: nada até trok_ca_queda_min de altura, depois sobe em linha reta até
// trok_ca_queda_dano em trok_ca_queda_max (alturas em unidades do CA, convertidas pela escala).
static ConVar trok_ca_queda_min( "trok_ca_queda_min", "300", FCVAR_REPLICATED, "CA: altura (u do CA) a partir da qual a queda machuca." );
static ConVar trok_ca_queda_max( "trok_ca_queda_max", "700", FCVAR_REPLICATED, "CA: altura (u do CA) que dá trok_ca_queda_dano." );
static ConVar trok_ca_queda_dano( "trok_ca_queda_dano", "200", FCVAR_REPLICATED, "CA: dano na altura trok_ca_queda_max (linear, sem teto)." );
extern ConVar trok_ca_escala;

float CTrokRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	if ( TrokSurto_IsZumbi( pPlayer ) )
		return 0.0f;
	if ( !trok_mov_ca.GetBool() )
		return BaseClass::FlPlayerFallDamage( pPlayer );

	const float flGravity = MAX( 1.0f, sv_gravity.GetFloat() * ( pPlayer->GetGravity() > 0.0f ? pPlayer->GetGravity() : 1.0f ) );
	const float flVel = pPlayer->m_Local.m_flFallVelocity;
	const float flAltura = ( flVel * flVel ) / ( 2.0f * flGravity );
	const float flEscala = MAX( 0.01f, trok_ca_escala.GetFloat() );
	const float flMin = trok_ca_queda_min.GetFloat() * flEscala;
	const float flMax = MAX( flMin + 1.0f, trok_ca_queda_max.GetFloat() * flEscala );
	if ( flAltura <= flMin )
		return 0.0f;
	return ( flAltura - flMin ) / ( flMax - flMin ) * trok_ca_queda_dano.GetFloat();
}

bool CTrokRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	if ( m_bSurto && pItem )
	{
		const bool bGarras = FClassnameIs( pItem, "weapon_trok_garras" );
		if ( pPlayer->GetTeamNumber() == TEAM_TROK_ZUMBIS )
			return bGarras;
		if ( bGarras )
			return false;
	}
	if ( m_iModo == TROK_MODO_CORRIDA && !TrokCorrida_PodeTerArma( pPlayer, pItem ) )
		return false; // Corrida Armada: só a escada dá arma
	if ( m_iModo == TROK_MODO_HORDAS && !TrokHordas_PodeTerArma( pPlayer, pItem ) )
		return false; // Hordas: zumbi só com garras
	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

bool CTrokRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	// No Surto o time é decidido pela rodada.
	if ( m_bSurto && ( FStrEq( args[ 0 ], "jointeam" ) || FStrEq( args[ 0 ], "changeteam" ) ) )
		return true;
	// Hordas: todos humanos (trok_hordas_diretor.cpp)
	if ( TrokHordas_ClientCommand( ToBasePlayer( pEdict ), args ) )
		return true;
	// trok_comprar (server/trok/trok_economia.cpp)
	if ( TrokEconomia_ClientCommand( ToBasePlayer( pEdict ), args ) )
		return true;
	// trok_zumbi_classe (trok_zumbis.cpp)
	if ( TrokZumbis_ClientCommand( ToBasePlayer( pEdict ), args ) )
		return true;
	return BaseClass::ClientCommand( pEdict, args );
}

bool CTrokRules::IgnoresHeadshot( CBasePlayer *pVictim ) const
{
	return m_bSurto && trok_surto_zumbi_sem_critico.GetBool() && TrokSurto_IsZumbi( pVictim );
}

// Depois de cada dano em jogador (patch em CBasePlayer::OnTakeDamage_Alive).
// No Surto, bala de humano em zumbi "segura" (tira velocidade) e empurra, conforme a arma.
void CTrokRules::OnPlayerDamaged( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( m_iModo == TROK_MODO_HORDAS )
	{
		// Pontos por acerto, arma melhorada, bônus, derrubado (trok_hordas_diretor.cpp).
		TrokHordas_Dano( pVictim, info );
		return;
	}
	if ( !m_bSurto || !pVictim )
		return;
	m_flUltimoDano[ pVictim->entindex() ] = gpGlobals->curtime;
	m_flProxRegen[ pVictim->entindex() ] = gpGlobals->curtime + trok_surto_regen_espera.GetFloat();
	TrokZumbis_AoAcertar( pVictim, info );	// golpe que paralisa (Splicer)

	const float flScale = trok_surto_empurrao.GetFloat();
	if ( flScale <= 0.0f || !TrokSurto_IsZumbi( pVictim ) )
		return;

	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	if ( !pAttacker || !TrokSurto_IsHumano( pAttacker ) )
		return;

	if ( trok_mov_ca.GetBool() )
	{
		SurtoEmpurraoCA( pVictim, pAttacker, info, flScale );
		return;
	}
	if ( !( info.GetDamageType() & DMG_BULLET ) )
		return;
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pAttacker->GetActiveWeapon() );
	if ( !pGun )
		return;
	const CHL2MPSWeaponInfo &wi = pGun->Info();

	Vector vecVel = pVictim->GetAbsVelocity();
	const float flStagger = clamp( wi.m_flTrokStagger * flScale, 0.0f, 0.9f );
	vecVel.x *= 1.0f - flStagger;
	vecVel.y *= 1.0f - flStagger;

	Vector vecDir = pVictim->WorldSpaceCenter() - pAttacker->EyePosition();
	vecDir.z = 0.0f;
	VectorNormalize( vecDir );
	vecVel += vecDir * ( wi.m_flTrokKnockback * flScale );

	const float flMax = trok_surto_empurrao_max.GetFloat();
	const float flSpeed = vecVel.Length2D();
	if ( flMax > 0.0f && flSpeed > flMax )
	{
		vecVel.x *= flMax / flSpeed;
		vecVel.y *= flMax / flSpeed;
	}
	pVictim->SetAbsVelocity( vecVel );
}

// Empurrão do CA (movimento do CA ligado): liga o tempo no jogador e o
// trok_gamemovement.cpp troca a aceleração por um empurrão de K² (1 - t) na direção do
// dano, sem correr nem frear. Força e tempo vêm da arma (bala) ou das cvars (explosão).
void CTrokRules::SurtoEmpurraoCA( CBasePlayer *pVictim, CBasePlayer *pAttacker, const CTakeDamageInfo &info, float flScale )
{
	CHL2MP_Player *pTrok = dynamic_cast< CHL2MP_Player * >( pVictim );
	if ( !pTrok )
		return;

	float K = 0.0f, T = 0.0f;
	if ( info.GetDamageType() & DMG_BLAST )
	{
		K = trok_surto_ca_granada_empurrao.GetFloat();
		T = trok_surto_ca_granada_tempo.GetFloat();
	}
	else if ( info.GetDamageType() & DMG_BULLET )
	{
		CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pAttacker->GetActiveWeapon() );
		if ( !pGun )
			return;
		K = pGun->Info().m_flTrokCAEmpurrao;
		T = pGun->Info().m_flTrokCAEmpurraoTempo;
	}
	K *= flScale;
	if ( K <= 0.0f || T <= 0.0f )
		return;

	// Direção em que o dano andou (para longe de quem causou).
	Vector vecDir = info.GetDamageForce();
	vecDir.z = 0.0f;
	if ( vecDir.LengthSqr() < 0.001f )
	{
		CBaseEntity *pFonte = info.GetInflictor() ? info.GetInflictor() : pAttacker;
		vecDir = pVictim->WorldSpaceCenter() - pFonte->WorldSpaceCenter();
		vecDir.z = 0.0f;
	}
	if ( vecDir.LengthSqr() > 0.001f )
		VectorNormalize( vecDir );
	else
		vecDir.Init();

	// O tempo só recomeça se o novo for mais longo que o que falta; a força nova sempre vale.
	const float flNow = gpGlobals->curtime;
	if ( flNow + T > pTrok->m_flTrokEmpFim )
	{
		pTrok->m_flTrokEmpInicio = flNow;
		pTrok->m_flTrokEmpFim = flNow + T;
	}
	pTrok->m_flTrokEmpForca = K;
	pTrok->m_flTrokEmpDirX = vecDir.x;
	pTrok->m_flTrokEmpDirY = vecDir.y;
}

// Pontes para o código da Valve (patches).
void TrokRules_OnPlayerDamaged( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	if ( TrokRules() )
		TrokRules()->OnPlayerDamaged( pVictim, info );
}

void TrokRules_OnPlayerFired( CBasePlayer *pPlayer )
{
	if ( TrokRules() )
		TrokRules()->OnPlayerFired( pPlayer );
}

bool TrokRules_IgnoresHeadshot( CBasePlayer *pVictim )
{
	return TrokRules() && TrokRules()->IgnoresHeadshot( pVictim );
}

//-----------------------------------------------------------------------------
// Explosões do Combat Arms (Chimera, specs/combat_arms/grenades.md, "Cover"):
//   - dano cheio até trok_explosao_ca_nucleo x raio, depois cai em linha reta até 0 no raio;
//   - só o mapa (paredes, pilastras, caixas do BSP) e portas/brushes sólidos protegem; outro
//     jogador, prop solto ou granada no caminho NÃO protegem;
//   - em jogador, dois traços (explosão -> centro e centro -> explosão); se algum bater,
//     tenta de novo até o alto da cabeça; ainda bloqueado = sem dano. Cobertura que deixa a
//     cabeça de fora não protege. Objetos (props, quebráveis) não têm teste de cobertura.
// Isso faz o "olé" do CA: jogar a granada e se esconder atrás da pilastra na hora.
// Com trok_explosao_ca 0 vale a explosão do HL2 (que contorna quinas perto do centro).
//-----------------------------------------------------------------------------
ConVar trok_explosao_ca( "trok_explosao_ca", "1", FCVAR_NOTIFY, "Explosões como no Combat Arms: parede bloqueia, gente e objetos soltos não. 0 = as do HL2." );
ConVar trok_explosao_ca_nucleo( "trok_explosao_ca_nucleo", "0.25", FCVAR_NOTIFY, "Fração do raio com dano cheio (CA: 0,25)." );
ConVar trok_granada_dano( "trok_granada_dano", "120", FCVAR_NOTIFY, "Dano da granada de mão (CA: M67, 60 aplicado duas vezes = 120). 0 = o do script." );
ConVar trok_granada_raio( "trok_granada_raio", "450", FCVAR_NOTIFY, "Raio (u) da granada de mão (CA: M67 450). 0 = o do HL2 (250)." );

// Só o mapa e brushes sólidos (portas, paredes que se mexem, quebráveis ainda inteiros).
class CTrokTraceFilterExplosao : public CTraceFilterSimple
{
public:
	CTrokTraceFilterExplosao( const IHandleEntity *pIgnore ) : CTraceFilterSimple( pIgnore, COLLISION_GROUP_NONE ) {}
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEnt = EntityFromEntityHandle( pHandleEntity );
		if ( !pEnt )
			return false;
		if ( pEnt->IsWorld() )
			return true;
		if ( !pEnt->IsSolid() )
			return false;
		if ( pEnt->IsBSPModel() || FClassnameIs( pEnt, "prop_door_rotating" ) )
			return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
		return false;
	}
};

// Também usada pelo clarão e pela claymore (server/trok/trok_projetil.cpp).
bool TrokExplosaoBloqueada( const Vector &a, const Vector &b, CBaseEntity *pInflictor )
{
	if ( ( a - b ).LengthSqr() < 0.01f )
		return false;	// traço de comprimento zero nunca bloqueia
	CTrokTraceFilterExplosao filter( pInflictor );
	trace_t tr;
	UTIL_TraceLine( a, b, MASK_SOLID, &filter, &tr );
	if ( tr.fraction < 1.0f || tr.startsolid )
		return true;
	UTIL_TraceLine( b, a, MASK_SOLID, &filter, &tr );
	return tr.fraction < 1.0f || tr.startsolid;
}

void CTrokRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	if ( !trok_explosao_ca.GetBool() )
	{
		BaseClass::RadiusDamage( info, vecSrcIn, flRadius, iClassIgnore, pEntityIgnore );
		return;
	}

	CTakeDamageInfo infoBase = info;
	CBaseEntity *pInflictor = info.GetInflictor();
	if ( pInflictor && FClassnameIs( pInflictor, "npc_grenade_frag" ) )
	{
		if ( trok_granada_dano.GetFloat() > 0.0f )
			infoBase.SetDamage( trok_granada_dano.GetFloat() );
		if ( trok_granada_raio.GetFloat() > 0.0f )
			flRadius = trok_granada_raio.GetFloat();
	}
	if ( flRadius <= 0.0f || infoBase.GetDamage() <= 0.0f )
		return;

	Vector vecSrc = vecSrcIn;
	vecSrc.z += 1.0f;	// granada parada no chão
	const bool bSrcNaAgua = ( UTIL_PointContents( vecSrc + Vector( 0, 0, 24 ) ) & MASK_WATER ) != 0;
	const float flNucleo = clamp( trok_explosao_ca_nucleo.GetFloat(), 0.0f, 0.99f ) * flRadius;

	CBaseEntity *pEntity = NULL;
	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( pEntity == pEntityIgnore || pEntity->m_takedamage == DAMAGE_NO )
			continue;
		if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			continue;
		// Explosão não entra nem sai da água funda.
		if ( bSrcNaAgua != ( pEntity->GetWaterLevel() == 3 ) )
			continue;

		const Vector vecCentro = pEntity->WorldSpaceCenter();
		const float d = ( vecCentro - vecSrc ).Length();
		if ( d > flRadius )
			continue;

		// Cobertura: só para quem luta (jogadores e NPCs).
		if ( pEntity->MyCombatCharacterPointer() && TrokExplosaoBloqueada( vecSrc, vecCentro, pInflictor ) )
		{
			Vector vecCabeca = vecCentro;
			vecCabeca.z = pEntity->GetAbsOrigin().z + pEntity->CollisionProp()->OBBMaxs().z - 2.0f;
			if ( TrokExplosaoBloqueada( vecSrc, vecCabeca, pInflictor ) )
				continue;
		}

		const float m = ( d <= flNucleo ) ? 1.0f : 1.0f - ( d - flNucleo ) / ( flRadius - flNucleo );
		if ( m <= 0.0f )
			continue;

		CTakeDamageInfo adjustedInfo = infoBase;
		adjustedInfo.SetDamage( infoBase.GetDamage() * m );

		Vector dir = vecCentro - vecSrc;
		VectorNormalize( dir );
		if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
		{
			if ( !( adjustedInfo.GetDamageType() & DMG_PREVENT_PHYSICS_FORCE ) )
				CalculateExplosiveDamageForce( &adjustedInfo, dir, vecSrc );
		}
		else
		{
			adjustedInfo.SetDamageForce( dir * adjustedInfo.GetDamageForce().Length() * m );
			adjustedInfo.SetDamagePosition( vecSrc );
		}

		pEntity->TakeDamage( adjustedInfo );
	}
}

#endif // !CLIENT_DLL
