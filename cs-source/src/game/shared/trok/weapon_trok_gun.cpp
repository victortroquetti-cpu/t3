//=============================================================================
// Trok: arma de fogo guiada por script (bloco "Trok" em scripts/weapon_*.txt).
//
// Uma classe só para todas as armas de fogo do Trok; cada nome de entidade
// (weapon_trok_fuzil, weapon_trok_pistola...) tem o próprio script.
//
//   Precisão estilo CS: cone por estado (parado, correndo, no ar, agachado,
//   mirando) + dispersão que cresce a cada tiro seguido e volta parada.
//   Recuo: chute vertical crescente e lateral em padrão (aprendível), pelo
//   ViewPunch do jogador (o tiro sai para onde a câmera chutou).
//   Dano por bala com queda por distância; balas por tiro (escopeta).
//   Automática ou semi (semi pede soltar o gatilho; bots não precisam).
//   Mira pelo cano (botão 2): zoom (FOV), menos dispersão e arma no centro.
//   Correndo (Shift): arma abaixada e sem atirar; ao parar, um instante até
//   poder atirar (como no CoD). Tudo previsto no cliente.
//
// Modelos e sons de protótipo vêm do CS:S montado (gameinfo); trocar pelos
// nossos é só mudar o script.
//=============================================================================

#include "cbase.h"
#include "in_buttons.h"

#ifdef CLIENT_DLL
	#include "c_hl2mp_player.h"
#else
	#include "hl2mp_player.h"
#endif

#include "weapon_trok_gun.h"
#include "weapon_hl2mpbasebasebludgeon.h"
#include "trok_surto.h"
#ifdef CLIENT_DLL
#include "prediction.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar trok_recuo_fixo( "trok_recuo_fixo", "0.5", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Fração do recuo que fica na mira (sobe e não volta; o jogador puxa para baixo). 0 = só o chute que volta." );
#ifdef CLIENT_DLL
ConVar trok_arma_balanco( "trok_arma_balanco", "1", FCVAR_ARCHIVE, "Balanço da arma na tela andando (0 = parada)." );
#endif

ConVar trok_arma_correndo_atraso( "trok_arma_correndo_atraso", "0.2", FCVAR_REPLICATED,
	"Segundos depois de parar de correr até poder atirar." );
#ifdef CLIENT_DLL
static ConVar trok_mira_ajuste( "trok_mira_ajuste", "0 0 0 0 0 0", 0,
	"Ajuste ao vivo da mira pelo cano: \"direita cima frente pitch yaw roll\" somados ao script. Depois: trok_mira_imprimir." );
#endif

ConVar trok_arma_correndo_vel( "trok_arma_correndo_vel", "230", FCVAR_REPLICATED,
	"Velocidade (u/s) a partir da qual, segurando Shift, conta como correndo (arma abaixada)." );

//-----------------------------------------------------------------------------
// Tabelas de animação do jogador (as mesmas do HL2MP, por tipo de arma).
//-----------------------------------------------------------------------------
static acttable_t s_actAR2[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_AR2,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_AR2,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_AR2,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_AR2,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_AR2,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_AR2,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_AR2,				false },
};
static acttable_t s_actSMG1[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SMG1,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SMG1,						false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SMG1,				false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SMG1,				false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SMG1,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SMG1,			false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SMG1,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SMG1,					false },
};
static acttable_t s_actPistol[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_PISTOL,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_PISTOL,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_PISTOL,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_PISTOL,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_PISTOL,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_PISTOL,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_PISTOL,				false },
};
static acttable_t s_actShotgun[] =
{
	{ ACT_HL2MP_IDLE,					ACT_HL2MP_IDLE_SHOTGUN,					false },
	{ ACT_HL2MP_RUN,					ACT_HL2MP_RUN_SHOTGUN,					false },
	{ ACT_HL2MP_IDLE_CROUCH,			ACT_HL2MP_IDLE_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_WALK_CROUCH,			ACT_HL2MP_WALK_CROUCH_SHOTGUN,			false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK,	ACT_HL2MP_GESTURE_RANGE_ATTACK_SHOTGUN,	false },
	{ ACT_HL2MP_GESTURE_RELOAD,			ACT_HL2MP_GESTURE_RELOAD_SHOTGUN,		false },
	{ ACT_HL2MP_JUMP,					ACT_HL2MP_JUMP_SHOTGUN,					false },
	{ ACT_RANGE_ATTACK1,				ACT_RANGE_ATTACK_SHOTGUN,				false },
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTrokGun, DT_WeaponTrokGun )

BEGIN_NETWORK_TABLE( CWeaponTrokGun, DT_WeaponTrokGun )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flExtraSpread ) ),
	RecvPropInt( RECVINFO( m_iShotsInRow ) ),
	RecvPropTime( RECVINFO( m_flLastShotTime ) ),
	RecvPropBool( RECVINFO( m_bTriggerHeld ) ),
	RecvPropBool( RECVINFO( m_bADS ) ),
	RecvPropTime( RECVINFO( m_flSprintEndTime ) ),
	RecvPropBool( RECVINFO( m_bWasSprinting ) ),
#else
	SendPropFloat( SENDINFO( m_flExtraSpread ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iShotsInRow ), 8, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flLastShotTime ) ),
	SendPropBool( SENDINFO( m_bTriggerHeld ) ),
	SendPropBool( SENDINFO( m_bADS ) ),
	SendPropTime( SENDINFO( m_flSprintEndTime ) ),
	SendPropBool( SENDINFO( m_bWasSprinting ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponTrokGun )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_flExtraSpread, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iShotsInRow, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flLastShotTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bTriggerHeld, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bADS, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flSprintEndTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
	DEFINE_PRED_FIELD( m_bWasSprinting, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
#endif
END_PREDICTION_DATA()

// Uma subclasse por arma (o cliente pede uma classe por nome de entidade);
// cada uma lê scripts/<nome>.txt. Arma nova = mais um bloco destes + o script.
#ifdef CLIENT_DLL
	#define CWeaponTrokFuzil C_WeaponTrokFuzil
	#define CWeaponTrokSubmetralhadora C_WeaponTrokSubmetralhadora
	#define CWeaponTrokPistola C_WeaponTrokPistola
	#define CWeaponTrokEscopeta C_WeaponTrokEscopeta
#endif

#define TROK_GUN_CLASS( ClassName, NetName, TableName, entityname )				\
	class ClassName : public CWeaponTrokGun										\
	{																			\
	public:																		\
		DECLARE_CLASS( ClassName, CWeaponTrokGun );								\
		DECLARE_NETWORKCLASS();													\
		DECLARE_PREDICTABLE();													\
	};																			\
	IMPLEMENT_NETWORKCLASS_ALIASED( NetName, TableName )						\
	BEGIN_NETWORK_TABLE( ClassName, TableName )									\
	END_NETWORK_TABLE()															\
	BEGIN_PREDICTION_DATA( ClassName )											\
	END_PREDICTION_DATA()														\
	LINK_ENTITY_TO_CLASS( entityname, ClassName );								\
	PRECACHE_WEAPON_REGISTER( entityname );

TROK_GUN_CLASS( CWeaponTrokFuzil, WeaponTrokFuzil, DT_WeaponTrokFuzil, weapon_trok_fuzil )
TROK_GUN_CLASS( CWeaponTrokSubmetralhadora, WeaponTrokSubmetralhadora, DT_WeaponTrokSubmetralhadora, weapon_trok_submetralhadora )
TROK_GUN_CLASS( CWeaponTrokPistola, WeaponTrokPistola, DT_WeaponTrokPistola, weapon_trok_pistola )
TROK_GUN_CLASS( CWeaponTrokEscopeta, WeaponTrokEscopeta, DT_WeaponTrokEscopeta, weapon_trok_escopeta )

CWeaponTrokGun::CWeaponTrokGun()
{
	m_flExtraSpread = 0.0f;
	m_iShotsInRow = 0;
	m_flLastShotTime = 0.0f;
	m_bTriggerHeld = false;
	m_bADS = false;
	m_flSprintEndTime = 0.0f;
	m_bWasSprinting = false;
	m_flViewmodelSprint = 0.0f;
	m_flBobPhase = 0.0f;
	m_flBobAmount = 0.0f;
	m_flViewmodelADS = 0.0f;
	m_bFiresUnderwater = false;
}

void CWeaponTrokGun::Precache( void )
{
	BaseClass::Precache();

	// Sons do script que são arquivos .wav diretos (sem soundscript).
	for ( int i = 0; i < NUM_SHOOT_SOUND_TYPES; ++i )
	{
		const char *pszSound = GetShootSound( i );
		if ( pszSound && pszSound[ 0 ] && ( Q_stristr( pszSound, ".wav" ) || Q_stristr( pszSound, ".mp3" ) ) )
			PrecacheSound( pszSound );
	}
}

acttable_t *CWeaponTrokGun::ActivityList( int &iActivityCount )
{
	const char *pszAnim = Info().m_szTrokAnim;
	if ( !Q_stricmp( pszAnim, "pistol" ) ) { iActivityCount = ARRAYSIZE( s_actPistol ); return s_actPistol; }
	if ( !Q_stricmp( pszAnim, "smg1" ) ) { iActivityCount = ARRAYSIZE( s_actSMG1 ); return s_actSMG1; }
	if ( !Q_stricmp( pszAnim, "shotgun" ) ) { iActivityCount = ARRAYSIZE( s_actShotgun ); return s_actShotgun; }
	iActivityCount = ARRAYSIZE( s_actAR2 );
	return s_actAR2;
}

float CWeaponTrokGun::GetFireRate( void )
{
	return 60.0f / MAX( 1.0f, Info().m_flTrokRPM );
}

//-----------------------------------------------------------------------------
// Estado do dono
//-----------------------------------------------------------------------------
bool CWeaponTrokGun::IsOwnerSprinting( void ) const
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer || !( pPlayer->m_nButtons & IN_SPEED ) || !( pPlayer->GetFlags() & FL_ONGROUND ) )
		return false;
	return pPlayer->GetAbsVelocity().Length2D() > trok_arma_correndo_vel.GetFloat();
}

bool CWeaponTrokGun::IsAutomatic( void ) const
{
	return Info().m_bTrokAuto;
}

bool CWeaponTrokGun::HasADS( void ) const
{
	return Info().m_flTrokADSFov > 0.0f;
}

float CWeaponTrokGun::CurrentSpreadDegrees( void ) const
{
	const CHL2MPSWeaponInfo &info = Info();
	CBasePlayer *pPlayer = GetPlayerOwner();
	float flSpread = info.m_flTrokSpreadStill;

	if ( pPlayer )
	{
		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
		{
			flSpread = info.m_flTrokSpreadAir;
		}
		else
		{
			// Andando devagar quase não atrapalha; correndo, cone cheio.
			float flSpeed = pPlayer->GetAbsVelocity().Length2D();
			float t = clamp( ( flSpeed - 40.0f ) / 150.0f, 0.0f, 1.0f );
			flSpread = Lerp( t, info.m_flTrokSpreadStill, info.m_flTrokSpreadMove );

			if ( pPlayer->GetFlags() & FL_DUCKING )
				flSpread *= info.m_flTrokSpreadCrouch;
		}
	}

	if ( m_bADS )
		flSpread *= info.m_flTrokSpreadADS;

	return flSpread + m_flExtraSpread;
}

const Vector &CWeaponTrokGun::GetBulletSpread( void )
{
	static Vector s_cone;
	float s = sinf( DEG2RAD( 0.5f * CurrentSpreadDegrees() ) );
	s_cone.Init( s, s, s );
	return s_cone;
}

float CWeaponTrokGun::DamageAtDistance( float flDistance ) const
{
	const CHL2MPSWeaponInfo &info = Info();
	if ( flDistance <= info.m_flTrokFalloffStart || info.m_flTrokFalloffEnd <= info.m_flTrokFalloffStart )
		return (float)info.m_iTrokDamage;

	float t = clamp( ( flDistance - info.m_flTrokFalloffStart ) / ( info.m_flTrokFalloffEnd - info.m_flTrokFalloffStart ), 0.0f, 1.0f );
	return info.m_iTrokDamage * Lerp( t, 1.0f, info.m_flTrokFalloffMin );
}

//-----------------------------------------------------------------------------
// Tiro
//-----------------------------------------------------------------------------
void CWeaponTrokGun::PrimaryAttack( void )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;

	const CHL2MPSWeaponInfo &info = Info();

	// Correndo ou acabou de parar de correr: não atira.
	if ( IsOwnerSprinting() || gpGlobals->curtime < m_flSprintEndTime + trok_arma_correndo_atraso.GetFloat() )
		return;

	// Semi-automática: um tiro por aperto (bots seguram o botão, então não precisam soltar).
	if ( !info.m_bTrokAuto && m_bTriggerHeld && !pPlayer->IsBot() )
		return;

	if ( m_iClip1 <= 0 )
	{
		if ( !m_bFireOnEmpty )
		{
			WeaponSound( EMPTY );
			m_bFireOnEmpty = true;
		}
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.25f;
		return;
	}

	m_bTriggerHeld = true;

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();
	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_iClip1 -= 1;

	// Dano pela distância até onde a mira aponta (estimativa antes do disparo).
	Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecDir = pPlayer->GetAutoaimVector( AUTOAIM_SCALE_DEFAULT );
	trace_t tr;
	UTIL_TraceLine( vecSrc, vecSrc + vecDir * MAX_TRACE_LENGTH, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
	float flDamage = DamageAtDistance( ( tr.endpos - vecSrc ).Length() );

	FireBulletsInfo_t bullets( info.m_iTrokPellets, vecSrc, vecDir, GetBulletSpread(), MAX_TRACE_LENGTH, m_iPrimaryAmmoType );
	bullets.m_flDamage = flDamage;
	bullets.m_iPlayerDamage = (int)flDamage;
	bullets.m_iTracerFreq = 2;
	bullets.m_pAttacker = pPlayer;
	pPlayer->FireBullets( bullets );

	// Recuo: dispersão cresce e a câmera chuta (sobe e anda de lado num padrão).
	float flSinceLast = gpGlobals->curtime - m_flLastShotTime;
	if ( flSinceLast > GetFireRate() * 2.5f )
		m_iShotsInRow = 0;
	m_iShotsInRow = m_iShotsInRow + 1;
	m_flLastShotTime = gpGlobals->curtime;
	m_flExtraSpread = MIN( m_flExtraSpread + info.m_flTrokSpreadPerShot, info.m_flTrokSpreadMaxShots );

	float flADSKick = m_bADS ? 0.7f : 1.0f;
	float flUp = info.m_flTrokKickUp * ( 1.0f + 0.1f * MIN( m_iShotsInRow.Get(), 10 ) ) * flADSKick;
	float flSide = info.m_flTrokKickSide * sinf( m_iShotsInRow * 0.9f ) * flADSKick +
		SharedRandomFloat( "TrokGunKick", -0.25f, 0.25f ) * info.m_flTrokKickSide;

	// Parte do recuo fica na mira (sobe e não volta, como no CoD: o jogador puxa para
	// baixo) e parte é o chute que volta sozinho. A parte que fica mexe nos ângulos de
	// visão do jogador local, uma vez por tiro previsto (receita do NEOTOKYO Rebuild e do
	// Double Action); bots recebem o chute inteiro (a mira deles já compensa).
	float flFixo = clamp( trok_recuo_fixo.GetFloat(), 0.0f, 1.0f );
#ifdef GAME_DLL
	if ( pPlayer->IsBot() )
		flFixo = 0.0f;
#endif
	pPlayer->ViewPunch( QAngle( -flUp * ( 1.0f - flFixo ), flSide * ( 1.0f - flFixo ), 0.0f ) );
#ifdef CLIENT_DLL
	if ( flFixo > 0.0f && prediction->IsFirstTimePredicted() && pPlayer->IsLocalPlayer() )
	{
		QAngle angView;
		engine->GetViewAngles( angView );
		angView.x = MAX( angView.x - flUp * flFixo, -89.0f );
		angView.y += flSide * flFixo;
		engine->SetViewAngles( angView );
	}
#endif

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	if ( !info.m_bTrokAuto )
		m_flNextPrimaryAttack = gpGlobals->curtime + MAX( GetFireRate(), 0.08f );
}

void CWeaponTrokGun::SecondaryAttack( void )
{
	if ( Info().m_flTrokADSFov <= 0.0f || IsOwnerSprinting() )
		return;

	SetADS( !m_bADS );
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.25f;
}

void CWeaponTrokGun::SetADS( bool bOn )
{
	if ( m_bADS == bOn )
		return;
	m_bADS = bOn;

	// Nos dois lados: m_iFOV, m_flFOVTime e m_hZoomOwner são previstos, então o zoom
	// começa no mesmo tique no cliente (só no servidor chegaria um ping depois).
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
		pPlayer->SetFOV( this, bOn ? (int)Info().m_flTrokADSFov : 0, Info().m_flTrokADSTime );
}

void CWeaponTrokGun::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( pPlayer )
	{
		// Soltou o gatilho: libera o próximo tiro semi e o aviso de sem munição.
		if ( !( pPlayer->m_nButtons & IN_ATTACK ) )
		{
			m_bTriggerHeld = false;
			m_bFireOnEmpty = false;
		}

		// Correr tira da mira; ao parar de correr, marca a hora.
		bool bSprinting = IsOwnerSprinting();
		if ( bSprinting && m_bADS )
			SetADS( false );
		if ( m_bWasSprinting && !bSprinting )
			m_flSprintEndTime = gpGlobals->curtime;
		m_bWasSprinting = bSprinting;

		// Dispersão dos tiros seguidos volta com o tempo sem atirar.
		if ( gpGlobals->curtime - m_flLastShotTime > GetFireRate() )
			m_flExtraSpread = MAX( 0.0f, m_flExtraSpread - Info().m_flTrokRecoveryRate * gpGlobals->frametime );
	}

	BaseClass::ItemPostFrame();
}

bool CWeaponTrokGun::Reload( void )
{
	bool bReloaded = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( bReloaded )
	{
		SetADS( false );
		WeaponSound( RELOAD );
		CBasePlayer *pPlayer = GetPlayerOwner();
		if ( pPlayer )
			pPlayer->SetAnimation( PLAYER_RELOAD );
	}
	return bReloaded;
}

bool CWeaponTrokGun::Deploy( void )
{
	m_flExtraSpread = 0.0f;
	m_iShotsInRow = 0;
	m_bADS = false;
	return BaseClass::Deploy();
}

bool CWeaponTrokGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	SetADS( false );
	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponTrokGun::Drop( const Vector &vecVelocity )
{
	SetADS( false );
	BaseClass::Drop( vecVelocity );
}

//-----------------------------------------------------------------------------
// Animação procedural da arma na tela: abaixada correndo, centrada mirando.
//-----------------------------------------------------------------------------
void CWeaponTrokGun::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	BaseClass::AddViewmodelBob( viewmodel, origin, angles );

#ifdef CLIENT_DLL
	float flTargetSprint = IsOwnerSprinting() ? 1.0f : 0.0f;
	float flTargetADS = m_bADS ? 1.0f : 0.0f;
	float flRate = gpGlobals->frametime * 8.0f;
	m_flViewmodelSprint = Approach( flTargetSprint, m_flViewmodelSprint, flRate );
	m_flViewmodelADS = Approach( flTargetADS, m_flViewmodelADS, gpGlobals->frametime / MAX( 0.05f, Info().m_flTrokADSTime ) );

	Vector forward, right, up;
	AngleVectors( angles, &forward, &right, &up );

	// Passo: a fase anda com a distância percorrida (parado contra a parede não balança).
	CBasePlayer *pOwner = GetPlayerOwner();
	float flSpeed = 0.0f;
	bool bOnGround = false;
	if ( pOwner )
	{
		flSpeed = pOwner->GetAbsVelocity().Length2D();
		bOnGround = pOwner->GetGroundEntity() != NULL;
	}
	m_flBobPhase += flSpeed * gpGlobals->frametime * ( 2.0f * M_PI / 160.0f ); // um ciclo a cada ~160u (dois passos)
	if ( m_flBobPhase > 2.0f * M_PI * 100.0f )
		m_flBobPhase -= 2.0f * M_PI * 100.0f;
	const float flBobTarget = bOnGround ? clamp( flSpeed / 220.0f, 0.0f, 1.0f ) : 0.0f;
	m_flBobAmount = Approach( flBobTarget, m_flBobAmount, gpGlobals->frametime * 4.0f );

	// Andando: balanço leve em "oito", quase nada mirando, nada correndo (o sprint tem o dele).
	if ( m_flBobAmount > 0.0f && trok_arma_balanco.GetFloat() > 0.0f )
	{
		const float k = m_flBobAmount * trok_arma_balanco.GetFloat() * ( 1.0f - m_flViewmodelSprint ) * ( 1.0f - 0.9f * m_flViewmodelADS );
		const float s1 = sinf( m_flBobPhase );
		const float s2 = sinf( m_flBobPhase * 2.0f );
		origin += k * ( 0.45f * s1 * right - 0.35f * fabsf( s1 ) * up + 0.15f * s2 * forward );
		angles.z += k * 0.8f * s1;
		angles.x += k * 0.5f * fabsf( s2 );
	}

	// Correndo: arma desce e gira para o lado, com um balanço no ritmo da passada.
	if ( m_flViewmodelSprint > 0.0f )
	{
		float b = SimpleSpline( m_flViewmodelSprint );
		float flStep = sinf( m_flBobPhase * 2.0f );
		origin += b * ( -4.0f * up + 1.5f * right + 1.2f * flStep * up );
		angles.x += b * ( 14.0f + 2.0f * flStep );
		angles.y += b * 18.0f;
		angles.z += b * -6.0f;
	}

	// Mirando pelo cano: a arma vai para a posição e o ângulo do script (mais o
	// ajuste ao vivo de trok_mira_ajuste), para a alça de mira cair no centro.
	if ( m_flViewmodelADS > 0.0f )
	{
		float a = SimpleSpline( m_flViewmodelADS );
		Vector pos = Info().m_vecTrokADSPos;
		QAngle ang = Info().m_angTrokADSAng;
		float adj[ 6 ] = {};
		sscanf( trok_mira_ajuste.GetString(), "%f %f %f %f %f %f", &adj[ 0 ], &adj[ 1 ], &adj[ 2 ], &adj[ 3 ], &adj[ 4 ], &adj[ 5 ] );
		pos += Vector( adj[ 0 ], adj[ 1 ], adj[ 2 ] );
		ang += QAngle( adj[ 3 ], adj[ 4 ], adj[ 5 ] );

		// Na mira, quase sem balanço de passo: mira confortável.
		origin += a * ( pos.x * right + pos.y * up + pos.z * forward );
		angles += a * ang;
	}
#endif
}

#ifdef CLIENT_DLL
// Ajuste ao vivo da mira pelo cano: mexe com trok_mira_ajuste até a alça cair
// no centro e rode trok_mira_imprimir para copiar os valores para o script.
CON_COMMAND( trok_mira_imprimir, "Mostra a posição e o ângulo de mira da arma atual (script + ajuste) para colar no script da arma." )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	CWeaponTrokGun *pGun = pPlayer ? dynamic_cast< CWeaponTrokGun * >( pPlayer->GetActiveWeapon() ) : NULL;
	if ( !pGun )
	{
		Msg( "Segure uma arma do Trok.\n" );
		return;
	}
	float adj[ 6 ] = {};
	sscanf( trok_mira_ajuste.GetString(), "%f %f %f %f %f %f", &adj[ 0 ], &adj[ 1 ], &adj[ 2 ], &adj[ 3 ], &adj[ 4 ], &adj[ 5 ] );
	const Vector &p = pGun->Info().m_vecTrokADSPos;
	const QAngle &q = pGun->Info().m_angTrokADSAng;
	Msg( "%s:\n\t\t\"mira_posicao\"\t\"%.2f %.2f %.2f\"\n\t\t\"mira_angulos\"\t\"%.2f %.2f %.2f\"\n",
		pGun->GetClassname(), p.x + adj[ 0 ], p.y + adj[ 1 ], p.z + adj[ 2 ], q.x + adj[ 3 ], q.y + adj[ 4 ], q.z + adj[ 5 ] );
}
#endif

//-----------------------------------------------------------------------------
float CWeaponTrokGun::GetMoveSpeedScale( void ) const
{
	const CHL2MPSWeaponInfo &info = Info();
	return info.m_flTrokMoveSpeed * ( m_bADS ? info.m_flTrokADSMoveSpeed : 1.0f );
}

// Atraso da arma ao virar e deslocamento pelo pitch (CBaseViewModel::CalcViewModelLag):
// some mirando, para a alça ficar no centro olhando para cima ou para baixo.
float TrokWeapon_ViewmodelLagScale( CBaseCombatWeapon *pWeapon )
{
#ifdef CLIENT_DLL
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pWeapon );
	if ( pGun )
		return 1.0f - SimpleSpline( pGun->GetADSBlend() );
#endif
	NOTE_UNUSED( pWeapon );
	return 1.0f;
}

#ifdef CLIENT_DLL
// FOV da arma na tela (ClientModeShared::GetViewModelFOV): mirando, vai para o
// mira_fov_arma do script (0 = não muda).
float TrokWeapon_ViewmodelFOV( float flDefault )
{
	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	CWeaponTrokGun *pGun = pPlayer ? dynamic_cast< CWeaponTrokGun * >( pPlayer->GetActiveWeapon() ) : NULL;
	if ( !pGun || pGun->Info().m_flTrokADSViewmodelFov <= 0.0f )
		return flDefault;
	return Lerp( SimpleSpline( pGun->GetADSBlend() ), flDefault, pGun->Info().m_flTrokADSViewmodelFov );
}
#endif

ConVar trok_faca_velocidade( "trok_faca_velocidade", "1.0", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Multiplicador de velocidade com arma branca (faca) na mão. O Combat Arms corre mais com a faca." );

float TrokWeapon_MoveSpeedScale( CBasePlayer *pPlayer )
{
	CBaseCombatWeapon *pWeapon = pPlayer ? pPlayer->GetActiveWeapon() : NULL;
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pWeapon );
	if ( pGun )
		return pGun->GetMoveSpeedScale();

	// Garras do zumbi (modo Surto).
	if ( pWeapon && FClassnameIs( pWeapon, "weapon_trok_garras" ) )
		return trok_surto_zumbi_velocidade.GetFloat();

	// Arma branca (pé de cabra/cassetete hoje; a faca do Trok depois).
	if ( dynamic_cast< CBaseHL2MPBludgeonWeapon * >( pWeapon ) )
		return trok_faca_velocidade.GetFloat();

	return 1.0f;
}
