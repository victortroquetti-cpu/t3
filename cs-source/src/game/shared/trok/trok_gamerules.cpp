//=============================================================================
// Trok: regras de jogo do mod. Ver trok_gamerules.h e design/modo-surto.md.
//=============================================================================

#include "cbase.h"
#include "trok_gamerules.h"
#include "movevars_shared.h"

#ifndef CLIENT_DLL
	#include "hl2mp_player.h"
	#include "team.h"
	#include "weapon_hl2mpbase.h"
	#include "hl2mp_weapon_parse.h"
	#include "trok/weapon_trok_gun.h"
	#include "NextBot/NextBotInterface.h"
	#include "NextBot/NextBotIntentionInterface.h"
	#include "filesystem.h"
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
ConVar trok_surto_regen_intervalo( "trok_surto_regen_intervalo", "3", FCVAR_NOTIFY, "Segundos entre as recuperações do zumbi parado." );
ConVar trok_surto_regen_espera( "trok_surto_regen_espera", "3", FCVAR_NOTIFY, "Segundos sem levar dano antes de começar a recuperar." );
ConVar trok_surto_zumbi_sem_critico( "trok_surto_zumbi_sem_critico", "1", FCVAR_NOTIFY, "Zumbi não leva o multiplicador de tiro na cabeça." );
ConVar trok_surto_empurrao( "trok_surto_empurrao", "1", FCVAR_NOTIFY, "Multiplica o empurrão e o 'segurar' das armas nos zumbis (0 = desliga)." );
ConVar trok_surto_empurrao_max( "trok_surto_empurrao_max", "450", FCVAR_NOTIFY, "Velocidade horizontal máxima (u/s) que o empurrão pode dar." );
ConVar trok_surto_zumbi_folego( "trok_surto_zumbi_folego", "2", FCVAR_NOTIFY, "Fôlego do zumbi correndo, em vezes o do humano (CA: o dobro)." );
ConVar trok_surto_assistir( "trok_surto_assistir", "3", FCVAR_NOTIFY, "Segundos depois de morrer (sem virar zumbi) para passar a assistir os vivos." );
ConVar trok_surto_cor_zumbi( "trok_surto_cor_zumbi", "150 225 140", FCVAR_NOTIFY, "Tom do zumbi enquanto não há modelo próprio (R G B)." );
#endif

//-----------------------------------------------------------------------------
// Caixa e olho do jogador no movimento do Combat Arms (design/decombat/specs/MOVEMENT_2009.md
// e as specs de movimento do Chimera): no CA o corpo tem 100u em pé e 80u agachado (só 20% mais
// baixo; no Source é 50%), olho a 83u em pé e 60u agachado. Aqui vão as proporções sobre a caixa de
// 72u do Source (os mapas são do Source); largura também a do Source.
//-----------------------------------------------------------------------------
extern ConVar trok_mov_ca;
static CViewVectors g_TrokCAViewVectors(
	Vector( 0, 0, 60 ),			// olho em pé (83% da altura)
	Vector( -16, -16, 0 ),		// caixa em pé
	Vector( 16, 16, 72 ),
	Vector( -16, -16, 0 ),		// caixa agachado (80% da altura)
	Vector( 16, 16, 58 ),
	Vector( 0, 0, 43 ),			// olho agachado
	Vector( -10, -10, -10 ),	// espectador
	Vector( 10, 10, 10 ),
	Vector( 0, 0, 14 ) );		// olho do morto

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
	else
	{
		m_bTeamPlayEnabled = true;
		engine->ServerCommand( "exec equipe.cfg\n" );
	}

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

	const int iVida = MAX( 1, m_bHospedeiro[ i ] ? trok_surto_vida_hospedeiro.GetInt() : trok_surto_vida_zumbi.GetInt() );
	pPlayer->SetMaxHealth( iVida );
	pPlayer->SetHealth( iVida );
	pPlayer->SetArmorValue( 0 );

	int r = 150, g = 225, b = 140;
	sscanf( trok_surto_cor_zumbi.GetString(), "%d %d %d", &r, &g, &b );
	pPlayer->SetRenderColor( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ) );

	if ( trok_surto_virar_no_lugar.GetBool() && m_vecMorte[ i ] != vec3_invalid )
	{
		pPlayer->Teleport( &m_vecMorte[ i ], NULL, &vec3_origin );
		m_vecMorte[ i ] = vec3_invalid;
	}

	m_flUltimoDano[ i ] = gpGlobals->curtime;
	m_flProxRegen[ i ] = 0.0f;
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
	else
		ModoThink();
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

	TrokSurto_DispararMapa( iVencedor == SURTO_HUMANOS ? 4 : 5 );
}

// Zumbi parado e sem levar dano recupera vida aos poucos.
void CTrokRules::SurtoRegenerar( void )
{
	const float flNow = gpGlobals->curtime;
	const float flFrac = trok_surto_regen.GetFloat();
	if ( flFrac <= 0.0f )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !IsPlaying( pPlayer ) || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_TROK_ZUMBIS )
			continue;

		const bool bParado = pPlayer->GetAbsVelocity().Length2D() < 10.0f;
		if ( !bParado || flNow - m_flUltimoDano[ i ] < trok_surto_regen_espera.GetFloat() )
		{
			m_flProxRegen[ i ] = flNow + trok_surto_regen_intervalo.GetFloat();
			continue;
		}
		if ( flNow < m_flProxRegen[ i ] )
			continue;
		m_flProxRegen[ i ] = flNow + trok_surto_regen_intervalo.GetFloat();
		const int iMax = pPlayer->GetMaxHealth();
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
	BaseClass::PlayerKilled( pVictim, info );
	m_flMorte[ pVictim->entindex() ] = gpGlobals->curtime;

	CTrokSurtoEstado *pEstado = m_hEstado.Get();
	if ( !m_bSurto || !pEstado || pEstado->GetFase() != SURTO_JOGO )
		return;

	const int i = pVictim->entindex();

	// Estatísticas: infecção (zumbi matou humano) e zumbi abatido (humano matou zumbi).
	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	if ( pAttacker && pAttacker != pVictim )
	{
		const int a = pAttacker->entindex();
		if ( pAttacker->GetTeamNumber() == TEAM_TROK_ZUMBIS && pVictim->GetTeamNumber() == TEAM_TROK_HUMANOS )
		{
			pEstado->m_aInfeccoes.Set( a, pEstado->m_aInfeccoes[ a ] + 1 );
			++m_iRodadaInfec[ a ];
		}
		else if ( pAttacker->GetTeamNumber() == TEAM_TROK_HUMANOS && pVictim->GetTeamNumber() == TEAM_TROK_ZUMBIS )
		{
			pEstado->m_aZumbisAbatidos.Set( a, pEstado->m_aZumbisAbatidos[ a ] + 1 );
			++m_iRodadaZumbis[ a ];
		}
	}

	if ( pVictim->GetTeamNumber() == TEAM_TROK_HUMANOS )
	{
		// Humano que morre na rodada (garra, queda, o que for) levanta zumbi.
		m_vecMorte[ i ] = pVictim->GetAbsOrigin();
		m_flVirar[ i ] = gpGlobals->curtime + MAX( 0.1f, trok_surto_virar_tempo.GetFloat() );
	}
	else if ( pVictim->GetTeamNumber() == TEAM_TROK_ZUMBIS && trok_surto_zumbi_renasce.GetBool() )
	{
		m_vecMorte[ i ] = vec3_invalid;
		m_flVirar[ i ] = gpGlobals->curtime + 5.0f;
	}
}

bool CTrokRules::FPlayerCanRespawn( CBasePlayer *pPlayer )
{
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
	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

bool CTrokRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	// No Surto o time é decidido pela rodada.
	if ( m_bSurto && ( FStrEq( args[ 0 ], "jointeam" ) || FStrEq( args[ 0 ], "changeteam" ) ) )
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
	if ( !m_bSurto || !pVictim )
		return;
	m_flUltimoDano[ pVictim->entindex() ] = gpGlobals->curtime;

	const float flScale = trok_surto_empurrao.GetFloat();
	if ( flScale <= 0.0f || !TrokSurto_IsZumbi( pVictim ) || !( info.GetDamageType() & DMG_BULLET ) )
		return;

	CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
	if ( !pAttacker || !TrokSurto_IsHumano( pAttacker ) )
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

#endif // !CLIENT_DLL
