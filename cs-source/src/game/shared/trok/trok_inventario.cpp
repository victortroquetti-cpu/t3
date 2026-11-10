//=============================================================================
// Trok: inventário como no Combat Arms (ver trok_inventario.h).
//
// Ganchos de 1 a 3 linhas em server/hl2mp/hl2mp_player.cpp (BumpWeapon, PreThink,
// Weapon_Drop, Event_Killed) e em client/hl2/hud_weaponselection.cpp (FastWeaponSwitch).
// Números nas ConVars trok_pegar_* / trok_arma_chao_* / trok_morte_largar (balanceamento.cfg).
//
// Munição: no Source a reserva é do jogador por tipo (SMG1, Pistol...), não da arma.
// Quando uma arma cai, a reserva daquele tipo vai junto com ela (guardada em
// SetPrimaryAmmoCount) e sai do jogador; quem pega fica com exatamente o pente e a
// reserva que estavam na arma (como no CA). Supõe uma arma carregada por tipo de munição.
//=============================================================================

#include "cbase.h"
#include "in_buttons.h"
#include "trok_inventario.h"

#ifdef CLIENT_DLL
	#include "c_basecombatweapon.h"
	#include "c_baseplayer.h"
#else
	#include "player.h"
	#include "gamerules.h"
	#include "trok_surto.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_pegar_distancia( "trok_pegar_distancia", "100", FCVAR_REPLICATED,
	"CA: distância máxima (u) do olho até a arma no chão para pegar com E." );

#ifndef CLIENT_DLL
ConVar trok_pegar_encostando( "trok_pegar_encostando", "0", 0,
	"1 = pega arma do chão andando por cima (HL2). 0 = só olhando e apertando E (CA)." );
ConVar trok_pegar_trocar( "trok_pegar_trocar", "1 2", 0,
	"Classes (1 principal, 2 secundária...) em que pegar sempre troca pela sua da mesma classe. Nas outras, a mesma arma soma munição." );
ConVar trok_morte_largar( "trok_morte_largar", "1 2", 0,
	"Classes que caem ao morrer (a selecionada por último de cada uma, com o pente e a reserva). Vazio = nenhuma." );
ConVar trok_arma_chao_tempo( "trok_arma_chao_tempo", "20", 0, "CA: segundos até uma arma largada sumir do chão." );
ConVar trok_armas_max( "trok_armas_max", "16", 0, "CA: máximo de armas carregadas." );
#endif

//-----------------------------------------------------------------------------
// Mira: raio do olho para frente; a arma conta se o raio entra na caixa dela (AABB do
// modelo) a até trok_pegar_distancia, sem parede ou objeto sólido antes.
//-----------------------------------------------------------------------------
class CTrokFiltroSemArmas : public CTraceFilterSimple
{
public:
	CTrokFiltroSemArmas( const IHandleEntity *pIgnorar ) : CTraceFilterSimple( pIgnorar, COLLISION_GROUP_NONE ) {}
	virtual bool ShouldHitEntity( IHandleEntity *pHandleEntity, int contentsMask )
	{
		CBaseEntity *pEnt = EntityFromEntityHandle( pHandleEntity );
		if ( pEnt && ( pEnt->IsBaseCombatWeapon() || dynamic_cast< CBaseCombatWeapon * >( pEnt ) ) )
			return false;
		return CTraceFilterSimple::ShouldHitEntity( pHandleEntity, contentsMask );
	}
};

// Raio contra caixa alinhada (método das placas). Devolve a distância de entrada (0 se começa dentro).
static bool RaioCaixa( const Vector &vecIni, const Vector &vecDir, const Vector &mins, const Vector &maxs, float *pDist )
{
	float tMin = 0.0f, tMax = FLT_MAX;
	for ( int i = 0; i < 3; ++i )
	{
		if ( fabsf( vecDir[ i ] ) < 1e-6f )
		{
			if ( vecIni[ i ] < mins[ i ] || vecIni[ i ] > maxs[ i ] )
				return false;
			continue;
		}
		float t1 = ( mins[ i ] - vecIni[ i ] ) / vecDir[ i ];
		float t2 = ( maxs[ i ] - vecIni[ i ] ) / vecDir[ i ];
		if ( t1 > t2 )
			V_swap( t1, t2 );
		tMin = MAX( tMin, t1 );
		tMax = MIN( tMax, t2 );
		if ( tMin > tMax )
			return false;
	}
	*pDist = tMin;
	return true;
}

CBaseCombatWeapon *TrokInventario_ArmaMirada( CBasePlayer *pPlayer )
{
	if ( !pPlayer || !pPlayer->IsAlive() )
		return NULL;

	const float flDist = trok_pegar_distancia.GetFloat();
	const Vector vecOlho = pPlayer->EyePosition();
	Vector vecFrente;
	pPlayer->EyeVectors( &vecFrente );

	CTrokFiltroSemArmas filtro( pPlayer );
	trace_t tr;
	UTIL_TraceLine( vecOlho, vecOlho + vecFrente * flDist, MASK_SOLID, &filtro, &tr );
	float flMelhor = tr.fraction * flDist + 0.5f;

	CBaseEntity *pLista[ 64 ];
	const int n = UTIL_EntitiesInSphere( pLista, ARRAYSIZE( pLista ), vecOlho, flDist + 48.0f, 0 );
	CBaseCombatWeapon *pMelhor = NULL;
	for ( int i = 0; i < n; ++i )
	{
		CBaseCombatWeapon *pArma = dynamic_cast< CBaseCombatWeapon * >( pLista[ i ] );
		if ( !pArma || pArma->GetOwner() || pArma->IsEffectActive( EF_NODRAW ) )
			continue;
		Vector mins, maxs;
		pArma->CollisionProp()->WorldSpaceAABB( &mins, &maxs );
		float d;
		if ( RaioCaixa( vecOlho, vecFrente, mins, maxs, &d ) && d <= flMelhor )
		{
			flMelhor = d;
			pMelhor = pArma;
		}
	}
	return pMelhor;
}

#ifdef CLIENT_DLL
//=============================================================================
// Cliente: última arma usada em cada classe (a tecla da classe volta nela).
//=============================================================================
static CHandle< C_BaseCombatWeapon > s_hUltimaCli[ MAX_WEAPON_SLOTS ];

void TrokInventario_ClienteFrame( void )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pAtiva = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	if ( pAtiva && pAtiva->GetSlot() >= 0 && pAtiva->GetSlot() < MAX_WEAPON_SLOTS )
		s_hUltimaCli[ pAtiva->GetSlot() ] = pAtiva;
}

C_BaseCombatWeapon *TrokInventario_UltimaDaClasse( int iSlot )
{
	if ( iSlot < 0 || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	C_BaseCombatWeapon *pArma = s_hUltimaCli[ iSlot ].Get();
	if ( !pPlayer || !pArma || pArma->GetOwner() != pPlayer || pArma->GetSlot() != iSlot || !pArma->CanBeSelected() )
		return NULL;
	return pArma;
}

#else
//=============================================================================
// Servidor
//=============================================================================
static CHandle< CBaseCombatWeapon > s_hUltima[ MAX_PLAYERS + 1 ][ MAX_WEAPON_SLOTS ];

struct TrokArmaChao_t
{
	CHandle< CBaseCombatWeapon > hArma;
	float flSome;
};
static CUtlVector< TrokArmaChao_t > s_Chao;
static float s_flProxVarredura = 0.0f;
static bool s_bPermitirLargar = false;	// largada feita por este arquivo (morte ou troca)
static bool s_bEspalhar = false;		// troca: a arma cai com um pequeno deslocamento

static bool ClasseNaLista( const char *pszLista, int iClasse )
{
	CUtlVector< CUtlString > partes;
	V_SplitString( pszLista, " ", partes );
	FOR_EACH_VEC( partes, i )
		if ( atoi( partes[ i ] ) == iClasse )
			return true;
	return false;
}

static int AchaChao( CBaseCombatWeapon *pArma )
{
	FOR_EACH_VEC( s_Chao, i )
		if ( s_Chao[ i ].hArma.Get() == pArma )
			return i;
	return -1;
}

static void TirarDoChao( CBaseCombatWeapon *pArma )
{
	const int i = AchaChao( pArma );
	if ( i >= 0 )
		s_Chao.FastRemove( i );
}

// Armas largadas há mais de trok_arma_chao_tempo e ainda sem dono somem.
static void Varrer( void )
{
	if ( gpGlobals->curtime < s_flProxVarredura )
		return;
	s_flProxVarredura = gpGlobals->curtime + 0.5f;
	for ( int i = s_Chao.Count() - 1; i >= 0; --i )
	{
		CBaseCombatWeapon *pArma = s_Chao[ i ].hArma.Get();
		if ( !pArma || pArma->GetOwner() )
		{
			s_Chao.FastRemove( i );
			continue;
		}
		if ( gpGlobals->curtime >= s_Chao[ i ].flSome )
		{
			s_Chao.FastRemove( i );
			UTIL_Remove( pArma );
		}
	}
}

// A arma da classe que o jogador selecionou por último (ou a primeira que tiver dela).
static CBaseCombatWeapon *UltimaDaClasse( CBasePlayer *pPlayer, int iSlot )
{
	if ( iSlot < 0 || iSlot >= MAX_WEAPON_SLOTS )
		return NULL;
	const int idx = pPlayer->entindex();
	if ( idx >= 1 && idx <= MAX_PLAYERS )
	{
		CBaseCombatWeapon *pArma = s_hUltima[ idx ][ iSlot ].Get();
		if ( pArma && pArma->GetOwner() == pPlayer && pArma->GetSlot() == iSlot )
			return pArma;
	}
	for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
	{
		CBaseCombatWeapon *pArma = pPlayer->GetWeapon( i );
		if ( pArma && pArma->GetSlot() == iSlot )
			return pArma;
	}
	return NULL;
}

// A reserva do tipo de munição da arma sai do jogador e vai com ela.
static void ReservaVaiJunto( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma )
{
	const int iTipo = pArma->GetPrimaryAmmoType();
	const int iRes = ( iTipo >= 0 && pArma->UsesClipsForAmmo1() ) ? pPlayer->GetAmmoCount( iTipo ) : 0;
	pArma->SetPrimaryAmmoCount( iRes );
	if ( iRes > 0 )
		pPlayer->RemoveAmmo( iRes, iTipo );
}

bool TrokInventario_BloqueiaEncostar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( trok_pegar_encostando.GetBool() || !pWeapon )
		return false;
	// Arma largada por alguém, ou posta no mapa: só com E. Arma recém-criada por
	// GiveNamedItem (equipamento ao nascer, give) tem SF_NORESPAWN e passa.
	return AchaChao( pWeapon ) >= 0 || !pWeapon->HasSpawnFlags( SF_NORESPAWN );
}

bool TrokInventario_BloqueiaLargar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	// Morrendo, o motor tenta largar a arma da mão; no CA só cai o que AoMorrer escolhe.
	return !s_bPermitirLargar && pPlayer && !pPlayer->IsAlive();
}

void TrokInventario_AoLargar( CBasePlayer *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pPlayer || !pWeapon || pWeapon->GetOwner() )
		return;

	// No pé esquerdo, parada, com um giro de 0 a 1 rad (CA). Sem esqueleto do CA: base do
	// corpo, um pouco à esquerda; na troca, deslocamento aleatório de até meia caixa.
	Vector vecFrente, vecDir;
	const float flYaw = pPlayer->EyeAngles()[ YAW ];
	AngleVectors( QAngle( 0.0f, flYaw, 0.0f ), &vecFrente, &vecDir, NULL );
	Vector vecPos = pPlayer->GetAbsOrigin() - vecDir * 6.0f + Vector( 0.0f, 0.0f, 8.0f );
	if ( s_bEspalhar )
		vecPos += vecDir * RandomFloat( -7.5f, 7.5f ) + vecFrente * RandomFloat( -7.5f, 7.5f );
	QAngle angArma( 0.0f, flYaw + RAD2DEG( RandomFloat( 0.0f, 1.0f ) ), 0.0f );
	pWeapon->Teleport( &vecPos, &angArma, &vec3_origin );
	IPhysicsObject *pObj = pWeapon->VPhysicsGetObject();
	if ( pObj )
	{
		Vector vZero( 0.0f, 0.0f, 0.0f );
		AngularImpulse aZero( 0.0f, 0.0f, 0.0f );
		pObj->SetVelocity( &vZero, &aZero );
	}

	TirarDoChao( pWeapon );
	TrokArmaChao_t chao;
	chao.hArma = pWeapon;
	chao.flSome = gpGlobals->curtime + trok_arma_chao_tempo.GetFloat();
	s_Chao.AddToTail( chao );
}

void TrokInventario_AoMorrer( CBasePlayer *pPlayer )
{
	if ( !pPlayer || TrokSurto_IsZumbi( pPlayer ) )
		return;

	// Escolhe antes de largar (largar a da mão troca a arma ativa).
	CBaseCombatWeapon *pCair[ MAX_WEAPON_SLOTS ] = {};
	for ( int iSlot = 0; iSlot < MAX_WEAPON_SLOTS; ++iSlot )
		if ( ClasseNaLista( trok_morte_largar.GetString(), iSlot + 1 ) )
			pCair[ iSlot ] = UltimaDaClasse( pPlayer, iSlot );

	s_bPermitirLargar = true;
	for ( int iSlot = 0; iSlot < MAX_WEAPON_SLOTS; ++iSlot )
	{
		CBaseCombatWeapon *pArma = pCair[ iSlot ];
		if ( !pArma || pArma->GetOwner() != pPlayer || FClassnameIs( pArma, "weapon_trok_garras" ) )
			continue;
		ReservaVaiJunto( pPlayer, pArma );
		pPlayer->Weapon_Drop( pArma, NULL, NULL );
	}
	s_bPermitirLargar = false;
}

// Pega a arma do chão (o E já foi conferido).
static void Pegar( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma )
{
	const int iSlot = pArma->GetSlot();
	CBaseCombatWeapon *pVelha = UltimaDaClasse( pPlayer, iSlot );
	const bool bTroca = ClasseNaLista( trok_pegar_trocar.GetString(), iSlot + 1 );
	const bool bLargada = AchaChao( pArma ) >= 0;	// largada por alguém: traz a reserva
	const int iPente = pArma->m_iClip1;
	const int iReserva = bLargada ? pArma->GetPrimaryAmmoCount() : -1;

	// Classe sem troca e mesma arma: soma a munição e enche o pente (CA, item não principal).
	if ( pVelha && !bTroca && FClassnameIs( pVelha, pArma->GetClassname() ) )
	{
		const int iTipo = pVelha->GetPrimaryAmmoType();
		if ( iTipo >= 0 )
		{
			int iTotal = MAX( 0, iPente ) + ( bLargada ? MAX( 0, iReserva ) : MAX( 0, pArma->GetDefaultClip1() - MAX( 0, iPente ) ) );
			pPlayer->GiveAmmo( iTotal, iTipo, true );
			if ( pVelha->UsesClipsForAmmo1() )
			{
				const int n = MIN( pVelha->GetMaxClip1() - pVelha->m_iClip1, pPlayer->GetAmmoCount( iTipo ) );
				if ( n > 0 )
				{
					pVelha->m_iClip1 += n;
					pPlayer->RemoveAmmo( n, iTipo );
				}
			}
		}
		TirarDoChao( pArma );
		UTIL_Remove( pArma );
		pPlayer->Weapon_Switch( pVelha );
		return;
	}

	if ( !pVelha && pPlayer->WeaponCount() > 0 )
	{
		int iTem = 0;
		for ( int i = 0; i < pPlayer->WeaponCount(); ++i )
			if ( pPlayer->GetWeapon( i ) )
				++iTem;
		if ( iTem >= trok_armas_max.GetInt() )
			return;
	}

	if ( !bLargada )
		pArma->CheckRespawn();	// arma do mapa: o mapa decide se volta

	// A da mesma classe cai no lugar, com o pente e a reserva.
	if ( pVelha )
	{
		ReservaVaiJunto( pPlayer, pVelha );
		s_bPermitirLargar = true;
		s_bEspalhar = true;
		pPlayer->Weapon_Drop( pVelha, NULL, NULL );
		s_bEspalhar = false;
		s_bPermitirLargar = false;
	}

	TirarDoChao( pArma );
	pPlayer->Weapon_Equip( pArma );
	if ( pArma->UsesClipsForAmmo1() && iPente >= 0 )
		pArma->m_iClip1 = iPente;
	if ( bLargada && pArma->GetPrimaryAmmoType() >= 0 )
		pPlayer->SetAmmoCount( MAX( 0, iReserva ), pArma->GetPrimaryAmmoType() );
	pArma->SetPrimaryAmmoCount( 0 );
	pArma->OnPickedUp( pPlayer );
	pPlayer->Weapon_Switch( pArma );
}

void TrokInventario_PreThink( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;
	Varrer();

	if ( !pPlayer->IsAlive() )
		return;

	// Lembra a última arma de cada classe (a que cai ao morrer e a que é trocada).
	CBaseCombatWeapon *pAtiva = pPlayer->GetActiveWeapon();
	const int idx = pPlayer->entindex();
	if ( pAtiva && idx >= 1 && idx <= MAX_PLAYERS && pAtiva->GetSlot() >= 0 && pAtiva->GetSlot() < MAX_WEAPON_SLOTS )
		s_hUltima[ idx ][ pAtiva->GetSlot() ] = pAtiva;

	// E mirando numa arma no chão. Sem arma na mira, o E segue para o PlayerUse (grades do Surto).
	if ( !( pPlayer->m_afButtonPressed & IN_USE ) || TrokSurto_IsZumbi( pPlayer ) )
		return;
	CBaseCombatWeapon *pArma = TrokInventario_ArmaMirada( pPlayer );
	if ( !pArma || !pPlayer->Weapon_CanUse( pArma ) || !g_pGameRules->CanHavePlayerItem( pPlayer, pArma ) )
		return;
	pPlayer->m_afButtonPressed &= ~IN_USE;
	Pegar( pPlayer, pArma );
}
#endif
