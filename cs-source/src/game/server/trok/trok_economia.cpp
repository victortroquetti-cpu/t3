//=============================================================================
// Trok: economia estilo CS (servidor). Ver trok_economia.h e design/economia.md.
//=============================================================================

#include "cbase.h"
#include "trok_economia.h"
#include "trok/trok_loja.h"
#include "trok/trok_placar_extra.h"
#include "trok/trok_surto.h"
#include "hl2_player.h"
#include "hl2mp_gamerules.h"
#include "ammodef.h"
#include "collisionutils.h"
#include "in_buttons.h"
#include "trok/trok_zumbis.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_economia( "trok_economia", "0", FCVAR_NOTIFY, "Economia estilo CS: dinheiro, loja (trok_comprar / tecla B) e começar só com pistola. 0 = desligada." );
ConVar trok_economia_inicial( "trok_economia_inicial", "800", FCVAR_NOTIFY, "Dinheiro de quem entra no mapa (CS:S mp_startmoney 800)." );
ConVar trok_economia_teto( "trok_economia_teto", "16000", FCVAR_NOTIFY, "Máximo de dinheiro (CS:S 16000)." );
ConVar trok_economia_tempo_compra( "trok_economia_tempo_compra", "20", FCVAR_NOTIFY, "Segundos depois de nascer em que dá para comprar (CS:S: 6 s de congelamento + 90 s de mp_buytime)." );
ConVar trok_economia_area( "trok_economia_area", "1", FCVAR_NOTIFY, "1 = só compra dentro da zona de compra do mapa (func_buyzone) ou perto de onde nasceu; 0 = em qualquer lugar." );
ConVar trok_economia_raio( "trok_economia_raio", "600", FCVAR_NOTIFY, "Mapa sem zona de compra: distância (u) de onde nasceu em que ainda compra." );
ConVar trok_economia_aliado( "trok_economia_aliado", "-3300", FCVAR_NOTIFY, "Dinheiro por abater um aliado (CS:S -3300)." );
ConVar trok_economia_vitoria( "trok_economia_vitoria", "3000", FCVAR_NOTIFY, "Rodada ganha (CS:S por eliminação num mapa sem objetivo: 3000)." );
ConVar trok_economia_derrota( "trok_economia_derrota", "1400", FCVAR_NOTIFY, "Rodada perdida (CS:S 1400)." );
ConVar trok_economia_derrota_passo( "trok_economia_derrota_passo", "500", FCVAR_NOTIFY, "A mais por derrota seguida (CS:S 500)." );
ConVar trok_economia_derrota_max( "trok_economia_derrota_max", "3400", FCVAR_NOTIFY, "Máximo do bônus de derrota (CS:S 3400)." );
ConVar trok_economia_sobreviver( "trok_economia_sobreviver", "0", FCVAR_NOTIFY, "A mais para quem termina a rodada vivo (não zumbi). CS:S: 0." );
ConVar trok_economia_armas_iniciais( "trok_economia_armas_iniciais", "weapon_trok_pistola weapon_trok_faca", FCVAR_NOTIFY, "Com a economia: armas ao nascer (o resto se compra)." );
ConVar trok_economia_municao_inicial( "trok_economia_municao_inicial", "Pistol 30", FCVAR_NOTIFY, "Com a economia: munição ao nascer, pares \"tipo quantidade\"." );
ConVar trok_economia_largar( "trok_economia_largar", "1", FCVAR_NOTIFY, "Comprar arma no lugar de outra do mesmo slot: 1 = larga a velha no chão (CS:S), 0 = some." );
ConVar trok_economia_manter( "trok_economia_manter", "1", FCVAR_NOTIFY, "Quem sobrevive à rodada fica com o que comprou (CS:S). 0 = todo nascimento recomeça do kit." );

static const int MAX_COMPRADOS = 24;

struct TrokEcoJogador_t
{
	int iUserId;
	bool bIniciado;					// já recebeu o dinheiro inicial
	Vector vecNasceu;
	int aComprados[ MAX_COMPRADOS ];	// itens (índice na loja) desta vida, devolvidos ao renascer sem morrer
	int nComprados;
	int iDerrotas;					// derrotas seguidas
	float flProtecao;				// capacete
	float flEnergia;				// fôlego no quadro anterior
};

static TrokEcoJogador_t s_Eco[ MAX_PLAYERS + 1 ];

bool TrokEconomia_Ligada( void )
{
	return trok_economia.GetBool();
}

//-----------------------------------------------------------------------------
// Zona de compra dos mapas do CS:S (func_buyzone, com TeamNum 2 = TR, 3 = CT; os números
// batem com Combine 2 e Rebeldes 3 do HL2MP, e os spawns do CS:S seguem a mesma regra).
//-----------------------------------------------------------------------------
class CTrokZonaCompra : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokZonaCompra, CBaseEntity );
	DECLARE_DATADESC();

	virtual void Spawn( void )
	{
		SetSolid( SOLID_BSP );
		AddSolidFlags( FSOLID_NOT_SOLID | FSOLID_TRIGGER );
		SetMoveType( MOVETYPE_NONE );
		SetModel( STRING( GetModelName() ) );
		AddEffects( EF_NODRAW );
	}
	int m_iTimeZona;
};

BEGIN_DATADESC( CTrokZonaCompra )
	DEFINE_KEYFIELD( m_iTimeZona, FIELD_INTEGER, "TeamNum" ),
END_DATADESC()

LINK_ENTITY_TO_CLASS( func_buyzone, CTrokZonaCompra );

//-----------------------------------------------------------------------------
// Estado por jogador
//-----------------------------------------------------------------------------
static void ZerarSlot( int i )
{
	V_memset( &s_Eco[ i ], 0, sizeof( s_Eco[ i ] ) );
	s_Eco[ i ].flEnergia = -1.0f;
}

static void ZerarEquipamento( int i )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( pPlacar )
	{
		pPlacar->m_aColete.Set( i, 0 );
		pPlacar->m_aCapacete.Set( i, 0 );
		pPlacar->m_aEquipVel.Set( i, 1.0f );
		pPlacar->m_aEquipFolego.Set( i, 1.0f );
	}
	s_Eco[ i ].flProtecao = 0.0f;
}

static int Slot( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return 0;
	const int i = pPlayer->entindex();
	if ( !CTrokPlacarExtra::Valido( i ) )
		return 0;
	if ( s_Eco[ i ].iUserId != pPlayer->GetUserID() )
	{
		ZerarSlot( i );
		s_Eco[ i ].iUserId = pPlayer->GetUserID();
		CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
		if ( pPlacar )
		{
			pPlacar->m_aDinheiro.Set( i, 0 );
			pPlacar->m_aCompraAte.Set( i, 0.0f );
		}
		ZerarEquipamento( i );
	}
	return i;
}

static int Dinheiro( int i )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	return pPlacar ? pPlacar->GetDinheiro( i ) : 0;
}

static void DarDinheiro( int i, int iValor )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !pPlacar || !CTrokPlacarExtra::Valido( i ) )
		return;
	const int iNovo = clamp( pPlacar->GetDinheiro( i ) + iValor, 0, MAX( 0, trok_economia_teto.GetInt() ) );
	pPlacar->m_aDinheiro.Set( i, iNovo );
}

static void Avisar( CBasePlayer *pPlayer, const char *pszMsg )
{
	ClientPrint( pPlayer, HUD_PRINTCENTER, pszMsg );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "%s\n", pszMsg ) );
}

static bool EZumbi( CBasePlayer *pPlayer )
{
	return TrokSurto_IsZumbi( pPlayer );
}

// Itens guardados que não estão mais com o jogador (arma largada) saem da lista.
static void Podar( CBasePlayer *pPlayer, int i )
{
	TrokEcoJogador_t &e = s_Eco[ i ];
	const CTrokLojaDados &loja = TrokLoja();
	for ( int k = e.nComprados - 1; k >= 0; --k )
	{
		const int idx = e.aComprados[ k ];
		bool bFica = loja.m_Itens.IsValidIndex( idx );
		if ( bFica && loja.m_Itens[ idx ].iTipo == LOJA_ARMA )
			bFica = pPlayer->Weapon_OwnsThisType( loja.m_Itens[ idx ].szEntidade ) != NULL;
		if ( !bFica )
		{
			e.aComprados[ k ] = e.aComprados[ e.nComprados - 1 ];
			--e.nComprados;
		}
	}
}

static void Guardar( int i, int idx )
{
	TrokEcoJogador_t &e = s_Eco[ i ];
	for ( int k = 0; k < e.nComprados; ++k )
	{
		if ( e.aComprados[ k ] == idx )
			return;
	}
	if ( e.nComprados < MAX_COMPRADOS )
		e.aComprados[ e.nComprados++ ] = idx;
}

//-----------------------------------------------------------------------------
// Entregar um item (comprado agora ou devolvido ao renascer)
//-----------------------------------------------------------------------------
static int ReservaCheia( CBasePlayer *pPlayer, int iAmmo )
{
	if ( iAmmo < 0 )
		return 0;
	return MAX( 0, GetAmmoDef()->MaxCarry( iAmmo ) - pPlayer->GetAmmoCount( iAmmo ) );
}

// Devolve false (e o motivo) se não deu para entregar.
static bool Entregar( CBasePlayer *pPlayer, int i, const TrokLojaItem_t &it, bool bComprando, const char **ppszMotivo )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	*ppszMotivo = NULL;

	switch ( it.iTipo )
	{
	case LOJA_ARMA:
	{
		if ( !it.szEntidade[ 0 ] )
		{
			*ppszMotivo = "Item sem entidade na loja.";
			return false;
		}
		CBaseCombatWeapon *pTem = pPlayer->Weapon_OwnsThisType( it.szEntidade );
		if ( pTem )
		{
			// Arma de arremesso: compra mais uma até o máximo; o resto não repete.
			if ( it.iMax > 0 && pTem->GetPrimaryAmmoType() >= 0 )
			{
				const int iTem = MAX( 0, pTem->Clip1() ) + pPlayer->GetAmmoCount( pTem->GetPrimaryAmmoType() );
				if ( iTem >= it.iMax )
				{
					*ppszMotivo = "Você já carrega o máximo disso.";
					return false;
				}
				pPlayer->GiveAmmo( 1, pTem->GetPrimaryAmmoType() );
				return true;
			}
			*ppszMotivo = "Você já tem essa arma.";
			return false;
		}

		CBaseEntity *pEnt = pPlayer->GiveNamedItem( it.szEntidade );
		CBaseCombatWeapon *pNova = pPlayer->Weapon_OwnsThisType( it.szEntidade );
		if ( !pNova )
		{
			if ( pEnt && !pEnt->IsMarkedForDeletion() )
				UTIL_Remove( pEnt );
			*ppszMotivo = "Essa arma ainda não existe neste build.";
			return false;
		}

		// Outra arma no mesmo slot sai (no CS:S cai no chão).
		for ( int w = 0; w < pPlayer->WeaponCount(); ++w )
		{
			CBaseCombatWeapon *pOutra = pPlayer->GetWeapon( w );
			if ( !pOutra || pOutra == pNova || pOutra->GetSlot() != pNova->GetSlot() )
				continue;
			if ( it.iMax > 0 )
				continue; // granadas convivem no slot delas
			pPlayer->Weapon_Drop( pOutra, NULL, NULL );
			if ( !trok_economia_largar.GetBool() || !bComprando )
				UTIL_Remove( pOutra );
		}

		// Vem com a reserva cheia (CS:S) ou com o que o script mandar.
		const int iAmmo = pNova->GetPrimaryAmmoType();
		if ( iAmmo >= 0 )
		{
			const int iDar = ( it.iMunicao < 0 ) ? ReservaCheia( pPlayer, iAmmo ) : it.iMunicao;
			if ( iDar > 0 )
				pPlayer->GiveAmmo( iDar, iAmmo, true );
		}
		if ( bComprando )
			pPlayer->Weapon_Switch( pNova );
		return true;
	}

	case LOJA_COLETE:
	{
		if ( bComprando && pPlacar && pPlacar->GetColete( i ) >= it.iNivel && pPlayer->ArmorValue() >= MIN( 255, it.iArmadura ) )
		{
			*ppszMotivo = "Você já tem um colete igual ou melhor.";
			return false;
		}
		pPlayer->SetArmorValue( MAX( pPlayer->ArmorValue(), MIN( 255, it.iArmadura ) ) );
		if ( pPlacar && it.iNivel >= pPlacar->GetColete( i ) )
		{
			pPlacar->m_aColete.Set( i, clamp( it.iNivel, 0, 15 ) );
			pPlacar->m_aEquipVel.Set( i, it.flVelocidade );
			pPlacar->m_aEquipFolego.Set( i, it.flFolego );
		}
		return true;
	}

	case LOJA_CAPACETE:
	{
		if ( bComprando && pPlacar && pPlacar->GetCapacete( i ) > 0 && s_Eco[ i ].flProtecao >= it.flProtecao )
		{
			*ppszMotivo = "Você já tem um capacete.";
			return false;
		}
		s_Eco[ i ].flProtecao = it.flProtecao;
		if ( pPlacar )
			pPlacar->m_aCapacete.Set( i, it.iGolpes > 0 ? MIN( it.iGolpes, 254 ) : 255 );
		return true;
	}

	case LOJA_MUNICAO:
	{
		bool bDeu = false;
		for ( int w = 0; w < pPlayer->WeaponCount(); ++w )
		{
			CBaseCombatWeapon *pW = pPlayer->GetWeapon( w );
			if ( !pW || ( it.iSlot >= 0 && pW->GetSlot() != it.iSlot ) )
				continue;
			const int iAmmo = pW->GetPrimaryAmmoType();
			const int iFalta = ReservaCheia( pPlayer, iAmmo );
			if ( iFalta > 0 && pW->UsesClipsForAmmo1() )
			{
				pPlayer->GiveAmmo( it.iMunicao > 0 ? MIN( it.iMunicao, iFalta ) : iFalta, iAmmo );
				bDeu = true;
			}
		}
		if ( !bDeu )
			*ppszMotivo = "A munição já está cheia.";
		return bDeu;
	}

	case LOJA_MEDICO:
	{
		if ( pPlayer->GetHealth() >= pPlayer->GetMaxHealth() )
		{
			*ppszMotivo = "A vida já está cheia.";
			return false;
		}
		pPlayer->SetHealth( MIN( pPlayer->GetMaxHealth(), pPlayer->GetHealth() + MAX( 1, it.iVida ) ) );
		return true;
	}
	}
	return false;
}

//-----------------------------------------------------------------------------
// Onde e quando dá para comprar
//-----------------------------------------------------------------------------
static bool NaAreaDeCompra( CBasePlayer *pPlayer, int i )
{
	if ( !trok_economia_area.GetBool() )
		return true;
	const bool bTimes = HL2MPRules() && HL2MPRules()->IsTeamplay();
	const Vector vecPe = pPlayer->GetAbsOrigin() + Vector( 0, 0, 4 );
	bool bTemZona = false;
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, "func_buyzone" ) ) != NULL )
	{
		bTemZona = true;
		CTrokZonaCompra *pZona = static_cast< CTrokZonaCompra * >( pEnt );
		if ( bTimes && pZona->m_iTimeZona >= FIRST_GAME_TEAM && pZona->m_iTimeZona != pPlayer->GetTeamNumber() )
			continue;
		Vector mins, maxs;
		pZona->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		if ( IsPointInBox( vecPe, mins, maxs ) )
			return true;
	}
	if ( bTemZona )
		return false;
	return ( pPlayer->GetAbsOrigin() - s_Eco[ i ].vecNasceu ).Length() <= trok_economia_raio.GetFloat();
}

static void Comprar( CBasePlayer *pPlayer, const char *pszId )
{
	if ( !TrokEconomia_Ligada() )
	{
		Avisar( pPlayer, "A economia está desligada (trok_economia 0)." );
		return;
	}
	const int i = Slot( pPlayer );
	if ( !i || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR || EZumbi( pPlayer ) )
	{
		Avisar( pPlayer, "Agora você não pode comprar." );
		return;
	}
	const CTrokLojaDados &loja = TrokLoja();
	const int idx = loja.ItemIndice( pszId );
	if ( idx < 0 )
	{
		Avisar( pPlayer, UTIL_VarArgs( "Item desconhecido: %s (lista: trok_loja_lista)", pszId ) );
		return;
	}
	const TrokLojaItem_t &it = loja.m_Itens[ idx ];
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !pPlacar || gpGlobals->curtime > pPlacar->GetCompraAte( i ) )
	{
		Avisar( pPlayer, "O tempo de compra acabou." );
		return;
	}
	if ( !NaAreaDeCompra( pPlayer, i ) )
	{
		Avisar( pPlayer, "Fora da zona de compra." );
		return;
	}
	if ( it.iTime >= FIRST_GAME_TEAM && HL2MPRules() && HL2MPRules()->IsTeamplay() && it.iTime != pPlayer->GetTeamNumber() )
	{
		Avisar( pPlayer, "Seu time não compra isso." );
		return;
	}
	if ( Dinheiro( i ) < it.iPreco )
	{
		Avisar( pPlayer, "Dinheiro insuficiente." );
		return;
	}
	const char *pszMotivo = NULL;
	if ( !Entregar( pPlayer, i, it, true, &pszMotivo ) )
	{
		Avisar( pPlayer, pszMotivo ? pszMotivo : "Não deu para comprar." );
		return;
	}
	DarDinheiro( i, -it.iPreco );
	if ( it.iTipo == LOJA_ARMA || it.iTipo == LOJA_COLETE || it.iTipo == LOJA_CAPACETE )
		Guardar( i, idx );
	Podar( pPlayer, i );
	pPlayer->EmitSound( it.iTipo == LOJA_ARMA ? "Player.PickupWeapon" : "BaseCombatCharacter.AmmoPickup" );
	ClientPrint( pPlayer, HUD_PRINTCONSOLE, UTIL_VarArgs( "Comprou %s por $%d. Sobrou $%d.\n", it.szNome, it.iPreco, Dinheiro( i ) ) );
}

//-----------------------------------------------------------------------------
// Ganchos
//-----------------------------------------------------------------------------
void TrokEconomia_PlayerSpawn( CBasePlayer *pPlayer )
{
	const int i = Slot( pPlayer );
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !i || !pPlacar )
		return;
	pPlacar->m_bEconomia = TrokEconomia_Ligada();
	if ( !TrokEconomia_Ligada() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR )
	{
		pPlacar->m_aCompraAte.Set( i, 0.0f );
		return;
	}
	if ( !s_Eco[ i ].bIniciado )
	{
		s_Eco[ i ].bIniciado = true;
		pPlacar->m_aDinheiro.Set( i, clamp( trok_economia_inicial.GetInt(), 0, MAX( 0, trok_economia_teto.GetInt() ) ) );
	}
	s_Eco[ i ].vecNasceu = pPlayer->GetAbsOrigin();
	s_Eco[ i ].flEnergia = -1.0f;
	if ( EZumbi( pPlayer ) )
	{
		// Zumbi não compra nem leva o equipamento de humano.
		s_Eco[ i ].nComprados = 0;
		ZerarEquipamento( i );
		pPlacar->m_aCompraAte.Set( i, 0.0f );
		return;
	}
	pPlacar->m_aCompraAte.Set( i, gpGlobals->curtime + MAX( 0.0f, trok_economia_tempo_compra.GetFloat() ) );
}

bool TrokEconomia_DarEquipamento( CBasePlayer *pPlayer )
{
	if ( !TrokEconomia_Ligada() || !pPlayer || EZumbi( pPlayer ) )
		return false;
	const int i = Slot( pPlayer );
	if ( !i )
		return false;

	CUtlVector< CUtlString > ammo;
	V_SplitString( trok_economia_municao_inicial.GetString(), " ", ammo );
	for ( int k = 0; k + 1 < ammo.Count(); k += 2 )
		pPlayer->GiveAmmo( atoi( ammo[ k + 1 ] ), ammo[ k ] );

	CBaseCombatWeapon *pMelhor = NULL;
	CUtlVector< CUtlString > weapons;
	V_SplitString( trok_economia_armas_iniciais.GetString(), " ", weapons );
	FOR_EACH_VEC( weapons, k )
	{
		CBaseEntity *pEnt = pPlayer->GiveNamedItem( weapons[ k ] );
		if ( !pMelhor && pEnt )
			pMelhor = dynamic_cast< CBaseCombatWeapon * >( pEnt );
	}

	// O que comprou e não perdeu (sobreviveu à rodada) volta sem pagar.
	ZerarEquipamento( i );
	if ( trok_economia_manter.GetBool() )
	{
		const CTrokLojaDados &loja = TrokLoja();
		TrokEcoJogador_t &e = s_Eco[ i ];
		for ( int k = 0; k < e.nComprados; ++k )
		{
			if ( !loja.m_Itens.IsValidIndex( e.aComprados[ k ] ) )
				continue;
			const TrokLojaItem_t &it = loja.m_Itens[ e.aComprados[ k ] ];
			const char *pszMotivo = NULL;
			if ( Entregar( pPlayer, i, it, false, &pszMotivo ) && it.iTipo == LOJA_ARMA )
			{
				CBaseCombatWeapon *pW = pPlayer->Weapon_OwnsThisType( it.szEntidade );
				if ( pW && ( !pMelhor || pW->GetSlot() < pMelhor->GetSlot() ) )
					pMelhor = pW;
			}
		}
	}
	else
	{
		s_Eco[ i ].nComprados = 0;
	}

	if ( pMelhor )
		pPlayer->Weapon_Switch( pMelhor );
	return true;
}

static const char *ArmaDoAbate( CBasePlayer *pKiller, const CTakeDamageInfo &info )
{
	CBaseEntity *pInflictor = info.GetInflictor();
	if ( pInflictor && pInflictor != pKiller )
		return pInflictor->GetClassname();
	CBaseCombatWeapon *pW = pKiller->GetActiveWeapon();
	return pW ? pW->GetClassname() : "";
}

void TrokEconomia_PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	const int v = Slot( pVictim );
	if ( v )
	{
		// Morreu: perde o que comprou.
		s_Eco[ v ].nComprados = 0;
		ZerarEquipamento( v );
		CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
		if ( pPlacar )
			pPlacar->m_aCompraAte.Set( v, 0.0f );
	}
	if ( !TrokEconomia_Ligada() )
		return;

	CBasePlayer *pKiller = ToBasePlayer( info.GetAttacker() );
	if ( !pKiller || pKiller == pVictim )
		return;
	const int a = Slot( pKiller );
	if ( !a )
		return;
	if ( g_pGameRules && g_pGameRules->PlayerRelationship( pKiller, pVictim ) == GR_TEAMMATE )
		DarDinheiro( a, trok_economia_aliado.GetInt() );
	else
		DarDinheiro( a, TrokLoja().GanhoAbate( ArmaDoAbate( pKiller, info ) ) );
}

void TrokEconomia_FimRodada( int iTimeVencedor )
{
	if ( !TrokEconomia_Ligada() || iTimeVencedor <= 0 )
		return;	// empate: ninguém ganha (CS:S)
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->GetTeamNumber() == TEAM_SPECTATOR || !Slot( pPlayer ) )
			continue;
		TrokEcoJogador_t &e = s_Eco[ i ];
		if ( pPlayer->GetTeamNumber() == iTimeVencedor )
		{
			DarDinheiro( i, trok_economia_vitoria.GetInt() );
			e.iDerrotas = 0;
		}
		else
		{
			const int iBonus = MIN( trok_economia_derrota.GetInt() + trok_economia_derrota_passo.GetInt() * e.iDerrotas, trok_economia_derrota_max.GetInt() );
			DarDinheiro( i, iBonus );
			e.iDerrotas++;
		}
		const bool bVivoHumano = pPlayer->IsAlive() && !EZumbi( pPlayer );
		if ( bVivoHumano )
		{
			DarDinheiro( i, trok_economia_sobreviver.GetInt() );
			Podar( pPlayer, i );
		}
		else
		{
			e.nComprados = 0;
		}
	}
}

bool TrokEconomia_ClientCommand( CBasePlayer *pPlayer, const CCommand &args )
{
	if ( !pPlayer )
		return false;
	if ( FStrEq( args[ 0 ], "trok_comprar" ) )
	{
		if ( args.ArgC() < 2 )
			Avisar( pPlayer, "Uso: trok_comprar <item> (lista: trok_loja_lista)" );
		for ( int k = 1; k < args.ArgC(); ++k )
			Comprar( pPlayer, args[ k ] );
		return true;
	}
	if ( FStrEq( args[ 0 ], "trok_dinheiro_teste" ) )
	{
		static ConVarRef s_cheats( "sv_cheats" );
		if ( s_cheats.IsValid() && s_cheats.GetBool() )
			DarDinheiro( Slot( pPlayer ), args.ArgC() > 1 ? atoi( args[ 1 ] ) : 10000 );
		return true;
	}
	return false;
}

float TrokEquip_MultCabeca( CBasePlayer *pVictim, float flMult )
{
	if ( !TrokEconomia_Ligada() || !pVictim || EZumbi( pVictim ) || flMult <= 1.0f )
		return flMult;
	const int i = Slot( pVictim );
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !i || !pPlacar || pPlacar->GetCapacete( i ) <= 0 )
		return flMult;
	const float flNovo = 1.0f + ( flMult - 1.0f ) * ( 1.0f - s_Eco[ i ].flProtecao );
	const int iGolpes = pPlacar->GetCapacete( i );
	if ( iGolpes < 255 )
	{
		pPlacar->m_aCapacete.Set( i, iGolpes - 1 );	// chega a 0: quebrou
		if ( iGolpes - 1 <= 0 )
			s_Eco[ i ].flProtecao = 0.0f;
	}
	return flNovo;
}

//-----------------------------------------------------------------------------
// Por quadro: liga/desliga na rede e fôlego do colete (devolve parte do que a corrida gasta,
// como o fôlego do zumbi em trok_gamerules.cpp).
//-----------------------------------------------------------------------------
class CTrokEconomiaSistema : public CAutoGameSystemPerFrame
{
public:
	CTrokEconomiaSistema() : CAutoGameSystemPerFrame( "CTrokEconomiaSistema" ) {}

	virtual void LevelInitPreEntity( void )
	{
		for ( int i = 0; i <= MAX_PLAYERS; ++i )
			ZerarSlot( i );
		TrokLoja().Carregar();	// relê os preços a cada mapa
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
		if ( !pPlacar )
			return;
		if ( pPlacar->m_bEconomia != TrokEconomia_Ligada() )
			pPlacar->m_bEconomia = TrokEconomia_Ligada();
		if ( !TrokEconomia_Ligada() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CHL2_Player *pHL2 = dynamic_cast< CHL2_Player * >( UTIL_PlayerByIndex( i ) );
			if ( !pHL2 || !pHL2->IsAlive() || s_Eco[ i ].iUserId != pHL2->GetUserID() )
				continue;
			const float flMult = pPlacar->GetEquipFolego( i );
			const float flAgora = pHL2->SuitPower_GetCurrentPercentage();
			if ( flMult > 1.0f && !EZumbi( pHL2 ) && pHL2->IsSprinting() && s_Eco[ i ].flEnergia > flAgora )
				pHL2->SuitPower_Charge( ( s_Eco[ i ].flEnergia - flAgora ) * ( 1.0f - 1.0f / flMult ) );
			s_Eco[ i ].flEnergia = pHL2->SuitPower_GetCurrentPercentage();
		}
	}
};
static CTrokEconomiaSistema s_TrokEconomiaSistema;

//-----------------------------------------------------------------------------
// Bônus da série de abates (decisão do dono: a série pode dar dinheiro, equilibrado).
// Conta: um abate de fuzil dá $300 (CS); um ABATE DUPLO soma +$100, um INACREDITÁVEL +$300, e a
// série da vida paga +$200 aos 5, +$400 aos 10, +$600 a cada 5 depois. Teto de $2000 de bônus
// por vida, para quem domina não disparar na frente. Zumbi no Surto recebe em Fúria (metade).
//-----------------------------------------------------------------------------
ConVar trok_economia_serie( "trok_economia_serie", "1", FCVAR_NOTIFY, "A série de abates dá dinheiro (humano) e Fúria (zumbi no Surto)." );
ConVar trok_economia_serie_multi( "trok_economia_serie_multi", "0 0 100 150 200 250 300 350", FCVAR_NOTIFY,
	"Bônus por multi-abate: valores para 0,1,2(duplo),3,4,5,6,7+ abates seguidos." );
ConVar trok_economia_serie_vida( "trok_economia_serie_vida", "5 200 10 400 15 600", FCVAR_NOTIFY,
	"Bônus da série da vida: pares 'abates valor'; depois do último, repete o último valor a cada 5." );
ConVar trok_economia_serie_vinganca( "trok_economia_serie_vinganca", "100", FCVAR_NOTIFY, "Bônus de vingança." );
ConVar trok_economia_serie_primeiro( "trok_economia_serie_primeiro", "100", FCVAR_NOTIFY, "Bônus do primeiro abate da partida/rodada." );
ConVar trok_economia_serie_teto_vida( "trok_economia_serie_teto_vida", "2000", FCVAR_NOTIFY, "Máximo de bônus de série numa vida." );
ConVar trok_economia_serie_furia( "trok_economia_serie_furia", "0.5", FCVAR_NOTIFY, "Zumbi no Surto: fração do bônus que vira Fúria." );

static int s_aBonusVida[ MAX_PLAYERS + 1 ];

void TrokEconomia_SerieNovaVida( CBasePlayer *pPlayer )
{
	if ( pPlayer && pPlayer->entindex() > 0 && pPlayer->entindex() <= MAX_PLAYERS )
		s_aBonusVida[ pPlayer->entindex() ] = 0;
}

static int TrokBonusVida( int iSerie )
{
	int aPar[ 16 ] = {};
	const int n = sscanf( trok_economia_serie_vida.GetString(), "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		&aPar[ 0 ], &aPar[ 1 ], &aPar[ 2 ], &aPar[ 3 ], &aPar[ 4 ], &aPar[ 5 ], &aPar[ 6 ], &aPar[ 7 ],
		&aPar[ 8 ], &aPar[ 9 ], &aPar[ 10 ], &aPar[ 11 ], &aPar[ 12 ], &aPar[ 13 ], &aPar[ 14 ], &aPar[ 15 ] ) / 2;
	if ( n <= 0 )
		return 0;
	for ( int k = 0; k < n; ++k )
		if ( iSerie == aPar[ k * 2 ] )
			return aPar[ k * 2 + 1 ];
	const int iUltimo = aPar[ ( n - 1 ) * 2 ];
	if ( iSerie > iUltimo && ( iSerie - iUltimo ) % 5 == 0 )
		return aPar[ ( n - 1 ) * 2 + 1 ];
	return 0;
}

void TrokEconomia_BonusSerie( CBasePlayer *pKiller, int iMulti, int iSerie, bool bVinganca, bool bPrimeiro )
{
	if ( !pKiller || !trok_economia_serie.GetBool() )
		return;
	const int i = pKiller->entindex();
	if ( i <= 0 || i > MAX_PLAYERS )
		return;

	int aMulti[ 8 ] = {};
	sscanf( trok_economia_serie_multi.GetString(), "%d %d %d %d %d %d %d %d",
		&aMulti[ 0 ], &aMulti[ 1 ], &aMulti[ 2 ], &aMulti[ 3 ], &aMulti[ 4 ], &aMulti[ 5 ], &aMulti[ 6 ], &aMulti[ 7 ] );
	int iBonus = aMulti[ clamp( iMulti, 0, 7 ) ] + TrokBonusVida( iSerie );
	if ( bVinganca )
		iBonus += trok_economia_serie_vinganca.GetInt();
	if ( bPrimeiro )
		iBonus += trok_economia_serie_primeiro.GetInt();
	iBonus = MIN( iBonus, MAX( 0, trok_economia_serie_teto_vida.GetInt() - s_aBonusVida[ i ] ) );
	if ( iBonus <= 0 )
		return;
	s_aBonusVida[ i ] += iBonus;

	if ( TrokSurto_IsZumbi( pKiller ) )
	{
		const int iFuria = (int)( iBonus * trok_economia_serie_furia.GetFloat() );
		TrokZumbis_Ganhar( pKiller, iFuria );
		ClientPrint( pKiller, HUD_PRINTCONSOLE, UTIL_VarArgs( "Série: +%d de Fúria\n", iFuria ) );
		return;
	}
	if ( !TrokEconomia_Ligada() )
		return;
	DarDinheiro( i, iBonus );
	ClientPrint( pKiller, HUD_PRINTCONSOLE, UTIL_VarArgs( "Série: +$%d\n", iBonus ) );
}
