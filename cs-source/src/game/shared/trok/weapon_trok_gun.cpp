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
	#include "ilagcompensationmanager.h"
#endif

#include "weapon_trok_gun.h"
#include "weapon_hl2mpbasebasebludgeon.h"
#include "trok_surto.h"
#include "trok_hordas.h"
#ifdef CLIENT_DLL
#include "prediction.h"
#include "eventlist.h"
#include "cl_animevent.h"
#include "c_baseviewmodel.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


ConVar trok_recuo_fixo( "trok_recuo_fixo", "0.5", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"Fração do recuo que fica na mira (sobe e não volta; o jogador puxa para baixo). 0 = só o chute que volta." );
#ifdef CLIENT_DLL
ConVar trok_arma_balanco( "trok_arma_balanco", "1", FCVAR_ARCHIVE, "Balanço da arma na tela andando (0 = parada)." );
#endif

ConVar trok_arma_correndo_atraso( "trok_arma_correndo_atraso", "0.2", FCVAR_REPLICATED,
	"Segundos depois de parar de correr até poder atirar (armas sem \"saida_corrida\" no script)." );

// Modelo de tiro do Combat Arms (bloco "CA" do script + "modelo_ca" 1). 0 = volta ao modelo antigo
// (cone por estado e recuo por ViewPunch) em todas as armas.
ConVar trok_arma_ca( "trok_arma_ca", "1", FCVAR_REPLICATED | FCVAR_NOTIFY,
	"1 = armas com \"modelo_ca\" 1 seguem a lei de tiro do Combat Arms (cone quadrado, medidores a 80 Hz, dano em degraus, recuo de câmera)." );
ConVar trok_ca_dano_costas( "trok_ca_dano_costas", "1.3", FCVAR_REPLICATED,
	"CA: dano de tiro e faca vindo de trás (setor de 150 graus do CA). 1 = desliga." );

// Constantes da lei do CA (weapon_firing.md, "Constants"; seção global [Perturb] já com a troca Q1).
static const double CA_PASSO = 1.0 / 80.0;		// subpasso dos medidores de dispersão (s)
static const float CA_FOGO_SOBE = 3.0f;			// fire_inc_scale
static const float CA_FOGO_DESCE = 3.0f;		// fire_dec_scale
static const float CA_MOV_SOBE = 3.0f;			// move_inc_scale
static const float CA_MOV_DESCE = 9.0f;			// move_dec_scale

// Movimento do CA (trok_gamemovement.cpp): a velocidade-limite entra na fração de movimento.
extern ConVar trok_mov_ca;
extern ConVar trok_ca_escala;
extern ConVar trok_ca_corrida;
extern ConVar trok_ca_andar;
extern ConVar trok_ca_agachado;
extern ConVar trok_ca_colete;
#ifdef CLIENT_DLL
static ConVar trok_mira_ajuste( "trok_mira_ajuste", "0 0 0 0 0 0", 0,
	"Ajuste ao vivo da mira pelo cano: \"direita cima frente pitch yaw roll\" somados ao script. Depois: trok_mira_imprimir." );
// Preferência do jogador (vai para o servidor pelo userinfo): 1 = segurar o botão 2 para mirar
// (padrão do CoD), 0 = apertar para entrar e apertar de novo para sair.
ConVar trok_mira_segurar( "trok_mira_segurar", "1", FCVAR_USERINFO | FCVAR_ARCHIVE,
	"Mirar pelo cano: 1 = segurar o botão (como no CoD), 0 = alternar." );
// Calibração: deixa a arma parada na posição de mira (mesmo sem mirar) e o HUD desenha
// linhas finas no centro da tela. Mexer com trok_mira_mexer e copiar com trok_mira_imprimir.
ConVar trok_mira_calibrar( "trok_mira_calibrar", "0", 0,
	"1 = arma presa na posição de mira e linhas no centro da tela, para alinhar a alça (trok_mira_mexer, trok_mira_imprimir)." );
#endif

// Segurar ou alternar: bots sempre alternam (o código deles aperta e solta o botão 2).
static bool TrokMiraSegurar( CBasePlayer *pPlayer )
{
	if ( !pPlayer || pPlayer->IsBot() )
		return false;
#ifdef CLIENT_DLL
	return trok_mira_segurar.GetBool();
#else
	const char *pszVal = engine->GetClientConVarValue( pPlayer->entindex(), "trok_mira_segurar" );
	return ( pszVal && pszVal[ 0 ] ) ? atoi( pszVal ) != 0 : true;
#endif
}

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
	RecvPropFloat( RECVINFO( m_flCAFogo ) ),
	RecvPropFloat( RECVINFO( m_flCAMov ) ),
	RecvPropInt( RECVINFO( m_iRecargaFase ) ),
	RecvPropTime( RECVINFO( m_flRecargaProx ) ),
#else
	SendPropFloat( SENDINFO( m_flExtraSpread ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iShotsInRow ), 8, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flLastShotTime ) ),
	SendPropBool( SENDINFO( m_bTriggerHeld ) ),
	SendPropBool( SENDINFO( m_bADS ) ),
	SendPropTime( SENDINFO( m_flSprintEndTime ) ),
	SendPropBool( SENDINFO( m_bWasSprinting ) ),
	SendPropFloat( SENDINFO( m_flCAFogo ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flCAMov ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iRecargaFase ), 3, SPROP_UNSIGNED ),
	SendPropTime( SENDINFO( m_flRecargaProx ) ),
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
	DEFINE_PRED_FIELD( m_flCAFogo, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_flCAMov, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iRecargaFase, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flRecargaProx, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
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
	m_flCoiceTempo = -100.0f;
	m_bFiresUnderwater = false;
	m_flCAFogo = 0.0f;
	m_flCAMov = 0.0f;
	m_iRecargaFase = 0;
	m_flRecargaProx = 0.0f;
	m_bCAAtirouNoTique = false;
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

// Modo Hordas: a máquina de melhoria aumenta o pente (trok_hordas.cpp; 1x fora do modo).
int CWeaponTrokGun::GetMaxClip1( void ) const
{
	return TrokHordas_Pente( this, BaseClass::GetMaxClip1() );
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

bool CWeaponTrokGun::UsaCA( void ) const
{
	return Info().m_TrokCA.bAtivo && trok_arma_ca.GetBool();
}

// Cone do CA (weapon_classes.md, "Cone integers"): n = n'min + floor((n'max - n'min) p) g,
// com n' = floor(n g), p = max(F, M). Mirando pelo cano usa o cone "mira" (Zoomed*).
int CWeaponTrokGun::CAConeAtual( void ) const
{
	const TrokArmaCA_t &ca = Info().m_TrokCA;
	int nMin = ca.iConeMin, nMax = ca.iConeMax;
	if ( m_bADS && ca.iConeMiraMax > 0 )
	{
		nMin = ca.iConeMiraMin;
		nMax = ca.iConeMiraMax;
	}
	const double g = (double)ca.flConeFator;
	const int a = (int)( nMin * g );
	const int b = (int)( nMax * g );
	if ( b <= 0 )
		return 0;
	const int lo = MIN( a, b );
	const double p = (double)MAX( m_flCAFogo.Get(), m_flCAMov.Get() );
	return (int)( lo + floor( ( b - lo ) * p ) * g );
}

float CWeaponTrokGun::CurrentSpreadDegrees( void ) const
{
	if ( UsaCA() )
		return 2.0f * RAD2DEG( atanf( CAConeAtual() / 1000.0f ) );	// cone total equivalente (mira do HUD)

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
	if ( IsOwnerSprinting() || gpGlobals->curtime < m_flSprintEndTime + CASaidaCorrida() )
		return;

	// Semi-automática: um tiro por aperto (bots seguram o botão, então não precisam soltar).
	if ( !info.m_bTrokAuto && m_bTriggerHeld && !pPlayer->IsBot() )
		return;

	// CA: pente vazio com reserva recarrega sozinho, mesmo segurando o gatilho.
	if ( m_iClip1 <= 0 && UsaCA() && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		Reload();
		return;
	}

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

#ifdef GAME_DLL
	extern void TrokRules_OnPlayerFired( CBasePlayer *pPlayer ); // trok/trok_gamerules.cpp: acaba a proteção de nascimento
	TrokRules_OnPlayerFired( pPlayer );
#endif

	WeaponSound( SINGLE );
	pPlayer->DoMuzzleFlash();
	// Mirando: a animação de tiro do modelo (do CS:S) tira a alça do centro. No lugar dela,
	// coice procedural (AddViewmodelBob) e o clarão do próprio modelo, sem mexer a arma.
	if ( m_bADS && !info.m_bTrokADSAnimTiro )
	{
#ifdef CLIENT_DLL
		if ( prediction->IsFirstTimePredicted() )
		{
			m_flCoiceTempo = gpGlobals->curtime;
			ClaraoSemAnimacao();
		}
#endif
	}
	else
		SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	pPlayer->SetAnimation( PLAYER_ATTACK1 );
	m_iClip1 -= 1;

	if ( UsaCA() )
	{
		// Lei do CA: o raio usa o cone de agora (o tiro não sente o próprio crescimento),
		// depois vem o chute da câmera (afeta o próximo tiro).
		CAAtirar( pPlayer );
		float flSinceLastCA = gpGlobals->curtime - m_flLastShotTime;
		if ( flSinceLastCA > GetFireRate() * 2.5f )
			m_iShotsInRow = 0;
		m_iShotsInRow = m_iShotsInRow + 1;
		m_flLastShotTime = gpGlobals->curtime;
		CARecuo( pPlayer );
		m_bCAAtirouNoTique = true;

		// Ciclo exato da animação de tiro (sem arredondar para o tique, Q2 do CA): segurando,
		// o próximo tiro conta a partir de quando este podia sair.
		float flBase = m_flNextPrimaryAttack;
		if ( gpGlobals->curtime - flBase > gpGlobals->interval_per_tick )
			flBase = gpGlobals->curtime;
		m_flNextPrimaryAttack = flBase + GetFireRate();
		return;
	}

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

	float flADSKick = m_bADS ? info.m_flTrokADSKick : 1.0f;
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
	if ( !HasADS() || IsOwnerSprinting() || TrokMiraSegurar( GetPlayerOwner() ) )
		return;	// segurando: quem decide é o ItemPostFrame

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
		pPlayer->SetFOV( this, bOn ? GetADSFov( pPlayer ) : 0, Info().m_flTrokADSTime );
}

// FOV mirando. Com mira_zoom, é relativo ao campo de visão que o jogador escolheu (o mesmo
// aumento em qualquer FOV, como no CoD); sem, é o mira_fov fixo do script.
int CWeaponTrokGun::GetADSFov( CBasePlayer *pPlayer ) const
{
	const CHL2MPSWeaponInfo &info = Info();
	if ( info.m_flTrokADSZoom > 1.0f && pPlayer )
	{
		const float flBase = (float)pPlayer->GetDefaultFOV();
		const float flFov = 2.0f * RAD2DEG( atanf( tanf( DEG2RAD( flBase * 0.5f ) ) / info.m_flTrokADSZoom ) );
		return clamp( (int)( flFov + 0.5f ), 10, (int)flBase );
	}
	return (int)info.m_flTrokADSFov;
}

void CWeaponTrokGun::ItemPostFrame( void )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	const bool bCA = UsaCA();
	if ( bCA )
		CARecuoVolta( pPlayer );	// CA: o recuo volta antes do tiro do quadro
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

		// Segurar para mirar (padrão do CoD): mira enquanto o botão 2 está apertado. Correr ou
		// recarregar tira; ainda segurando quando acabam, volta para a mira sozinho.
		if ( TrokMiraSegurar( pPlayer ) && HasADS() )
		{
			const bool bQuer = ( pPlayer->m_nButtons & IN_ATTACK2 ) && !bSprinting && !m_bInReload && m_iRecargaFase == 0;
			if ( bQuer != m_bADS )
				SetADS( bQuer );
		}
		if ( m_bWasSprinting && !bSprinting )
			m_flSprintEndTime = gpGlobals->curtime;
		m_bWasSprinting = bSprinting;

		// Dispersão dos tiros seguidos volta com o tempo sem atirar.
		if ( !bCA && gpGlobals->curtime - m_flLastShotTime > GetFireRate() )
			m_flExtraSpread = MAX( 0.0f, m_flExtraSpread - Info().m_flTrokRecoveryRate * gpGlobals->frametime );
	}

	m_bCAAtirouNoTique = false;

	// Recarga bala por bala (escopeta): atirar com bala no pente interrompe na hora (CA 5.5).
	if ( bCA && pPlayer && m_iRecargaFase != 0 )
	{
		if ( ( pPlayer->m_nButtons & IN_ATTACK ) && m_iClip1 > 0 && !IsOwnerSprinting() )
		{
			m_iRecargaFase = 0;
			m_flNextPrimaryAttack = gpGlobals->curtime;
		}
		else
		{
			CARecargaBalaAvanca( pPlayer );
			CATique( false );
			return;
		}
	}

	BaseClass::ItemPostFrame();

	if ( bCA )
		CATique( m_bCAAtirouNoTique );
}

// Recarregando ou sacando (o jogador não deixa a arma atirar): os medidores e o recuo seguem.
void CWeaponTrokGun::ItemBusyFrame( void )
{
	if ( UsaCA() )
	{
		CARecuoVolta( GetPlayerOwner() );
		CATique( false );
	}
	BaseClass::ItemBusyFrame();
}

bool CWeaponTrokGun::Reload( void )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	const bool bCA = UsaCA();
	if ( bCA && pPlayer )
	{
		// CA: a recarga espera a animação de tiro acabar (o bombeado da escopeta também).
		if ( gpGlobals->curtime < m_flNextPrimaryAttack || m_iRecargaFase != 0 )
			return false;

		// Bala por bala (escopeta): início, uma bala por volta, fim.
		const TrokArmaCA_t &ca = Info().m_TrokCA;
		if ( ca.flRecargaBala[ 1 ] > 0.0f )
		{
			if ( m_iClip1 >= GetMaxClip1() || pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
				return false;
			SetADS( false );
			m_iRecargaFase = 1;
			SendWeaponAnimTempo( ACT_SHOTGUN_RELOAD_START, ca.flRecargaBala[ 0 ] );
			m_flRecargaProx = gpGlobals->curtime + ca.flRecargaBala[ 0 ];
			m_flNextPrimaryAttack = m_flRecargaProx;
			pPlayer->SetAnimation( PLAYER_RELOAD );
			return true;
		}
	}

	bool bReloaded = DefaultReload( GetMaxClip1(), GetMaxClip2(), ACT_VM_RELOAD );
	if ( bReloaded )
	{
		SetADS( false );
		WeaponSound( RELOAD );
		if ( pPlayer )
			pPlayer->SetAnimation( PLAYER_RELOAD );

		// CA: tempo da animação "reload" da arma (o pente enche quando ela acaba).
		const float flTempo = bCA ? Info().m_TrokCA.flTempoRecarga : 0.0f;
		if ( flTempo > 0.0f && pPlayer )
		{
			SendWeaponAnimTempo( ACT_VM_RELOAD, flTempo );
			const float flFim = gpGlobals->curtime + flTempo;
			pPlayer->SetNextAttack( flFim );
			m_flNextPrimaryAttack = m_flNextSecondaryAttack = flFim;
		}
	}
	return bReloaded;
}

// Recarga bala por bala: avança as fases (chamado a cada tique enquanto m_iRecargaFase != 0).
void CWeaponTrokGun::CARecargaBalaAvanca( CBasePlayer *pPlayer )
{
	const TrokArmaCA_t &ca = Info().m_TrokCA;
	m_flNextPrimaryAttack = MAX( m_flNextPrimaryAttack.Get(), m_flRecargaProx.Get() );
	if ( gpGlobals->curtime < m_flRecargaProx )
		return;

	if ( m_iRecargaFase == 3 )
	{
		// Fim da animação de fechar: pronta.
		m_iRecargaFase = 0;
		m_flNextPrimaryAttack = gpGlobals->curtime;
		SendWeaponAnim( ACT_VM_IDLE );
		return;
	}

	if ( m_iRecargaFase == 2 )
	{
		// Uma bala entrou (só quando a volta da animação acaba).
		m_iClip1 += 1;
		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}

	if ( m_iClip1 < GetMaxClip1() && pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) > 0 )
	{
		m_iRecargaFase = 2;
		SendWeaponAnimTempo( ACT_VM_RELOAD, ca.flRecargaBala[ 1 ] );
		WeaponSound( RELOAD );
		m_flRecargaProx = m_flRecargaProx + ca.flRecargaBala[ 1 ];
	}
	else
	{
		m_iRecargaFase = 3;
		SendWeaponAnimTempo( ACT_SHOTGUN_RELOAD_FINISH, ca.flRecargaBala[ 2 ] );
		m_flRecargaProx = m_flRecargaProx + ca.flRecargaBala[ 2 ];
	}
	m_flNextPrimaryAttack = m_flRecargaProx;
}

bool CWeaponTrokGun::Deploy( void )
{
	m_flExtraSpread = 0.0f;
	m_iShotsInRow = 0;
	m_bADS = false;
	m_iRecargaFase = 0;
	const bool bOk = BaseClass::Deploy();

	// CA: sacar é instantâneo do lado de quem sai; a nova arma só atira quando a animação
	// "select" acaba (tempo do script).
	const float flTempo = UsaCA() ? Info().m_TrokCA.flTempoSacar : 0.0f;
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( bOk && flTempo > 0.0f && pPlayer )
	{
		SendWeaponAnimTempo( ACT_VM_DRAW, flTempo );
		const float flFim = gpGlobals->curtime + flTempo;
		pPlayer->SetNextAttack( flFim );
		m_flNextPrimaryAttack = m_flNextSecondaryAttack = flFim;
	}
	return bOk;
}

bool CWeaponTrokGun::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	SetADS( false );
	m_iRecargaFase = 0;		// trocar abandona a recarga (as balas já postas ficam)
	return BaseClass::Holster( pSwitchingTo );
}

// Toca a animação e ajusta a velocidade dela para durar flTempo (0 = velocidade normal).
bool CWeaponTrokGun::SendWeaponAnimTempo( int iActivity, float flTempo )
{
	const bool bOk = BaseClass::SendWeaponAnim( iActivity );
	CBasePlayer *pPlayer = GetPlayerOwner();
	CBaseViewModel *pVM = pPlayer ? pPlayer->GetViewModel( m_nViewModelIndex ) : NULL;
	if ( pVM )
	{
		float flTaxa = 1.0f;
		const float flDur = pVM->SequenceDuration();
		if ( bOk && flTempo > 0.0f && flDur > 0.0f )
			flTaxa = clamp( flDur / flTempo, 0.25f, 4.0f );
		pVM->SetPlaybackRate( flTaxa );
	}
	return bOk;
}

bool CWeaponTrokGun::SendWeaponAnim( int iActivity )
{
	return SendWeaponAnimTempo( iActivity, 0.0f );
}

void CWeaponTrokGun::Drop( const Vector &vecVelocity )
{
	SetADS( false );
	m_iRecargaFase = 0;
	BaseClass::Drop( vecVelocity );
}

//=============================================================================
// Lei de tiro do Combat Arms (design/chimera/specs/combat_arms/weapon_firing.md)
//=============================================================================

// Atraso para atirar depois de correr: "saida_corrida" do script (PostRush do CA) ou a ConVar.
float CWeaponTrokGun::CASaidaCorrida( void ) const
{
	if ( UsaCA() && Info().m_TrokCA.flSaidaCorrida >= 0.0f )
		return Info().m_TrokCA.flSaidaCorrida;
	return trok_arma_correndo_atraso.GetFloat();
}

// Dano por faixa de distância: degraus, não curva (Rangedamage0..2; depois da última, x1, Q8).
float CWeaponTrokGun::CAFaixa( float flDistancia ) const
{
	const TrokArmaCA_t &ca = Info().m_TrokCA;
	for ( int i = 0; i < 3; ++i )
		if ( flDistancia <= ca.flFaixa[ i ] )
			return ca.flFaixaDano[ i ];
	return 1.0f;
}

// Setor de trás do CA (150 graus, torto de propósito: 30 a 180 graus a partir da direita da vítima).
static float TrokCA_Costas( CBaseEntity *pVitima, const Vector &vecDir )
{
	if ( !pVitima || !pVitima->IsPlayer() )
		return 1.0f;
	Vector vecF, vecR;
	AngleVectors( QAngle( 0.0f, pVitima->GetAbsAngles()[ YAW ], 0.0f ), &vecF, &vecR, NULL );
	const float th = atan2f( DotProduct( vecF, vecDir ), DotProduct( vecR, vecDir ) );
	return ( th >= M_PI_F / 6.0f && th < M_PI_F ) ? trok_ca_dano_costas.GetFloat() : 1.0f;
}

// Um tiro: cada bala (escopeta: várias) sai no quadrado [-n, n]/1000 em volta da mira, com
// sorteio próprio; dano = base x sorteio +-k x degrau da distância x costas. Alcance = Range.
void CWeaponTrokGun::CAAtirar( CBasePlayer *pPlayer )
{
	const TrokArmaCA_t &ca = Info().m_TrokCA;
	const int nCone = CAConeAtual();
	const int nBalas = clamp( Info().m_iTrokPellets, 1, 16 );

	const Vector vecSrc = pPlayer->Weapon_ShootPosition();
	Vector vecF, vecR, vecU;
	AngleVectors( pPlayer->EyeAngles() + pPlayer->GetPunchAngle(), &vecF, &vecR, &vecU );

	Vector vecDir[ 16 ];
	float flDano[ 16 ];

#ifdef GAME_DLL
	// As vítimas voltam para onde o atirador as via (o mesmo que o FireBullets faz).
	CHL2MP_Player *pHL2 = ToHL2MPPlayer( pPlayer );
	if ( pHL2 )
		lagcompensation->StartLagCompensation( pHL2, pHL2->GetCurrentCommand() );
#endif
	for ( int i = 0; i < nBalas; ++i )
	{
		const float u1 = SharedRandomFloat( "TrokCAConeX", 0.0f, 1.0f, i );
		const float u2 = SharedRandomFloat( "TrokCAConeY", 0.0f, 1.0f, i );
		vecDir[ i ] = vecF + vecR * ( ( 2.0f * u1 - 1.0f ) * nCone / 1000.0f ) + vecU * ( ( 2.0f * u2 - 1.0f ) * nCone / 1000.0f );
		VectorNormalize( vecDir[ i ] );

		trace_t tr;
		UTIL_TraceLine( vecSrc, vecSrc + vecDir[ i ] * ca.flAlcance, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );
		const float k = ca.flDanoVariacao;
		float d = ca.flDano * SharedRandomFloat( "TrokCADano", 1.0f - k, 1.0f + k, i );
		d *= CAFaixa( ( tr.endpos - vecSrc ).Length() );
		if ( tr.m_pEnt && tr.m_pEnt != pPlayer )
			d *= TrokCA_Costas( tr.m_pEnt, vecDir[ i ] );
		flDano[ i ] = d;
	}
#ifdef GAME_DLL
	if ( pHL2 )
		lagcompensation->FinishLagCompensation( pHL2 );
#endif

	for ( int i = 0; i < nBalas; ++i )
	{
		FireBulletsInfo_t bullets( 1, vecSrc, vecDir[ i ], vec3_origin, ca.flAlcance, m_iPrimaryAmmoType );
		bullets.m_flDamage = flDano[ i ];
		bullets.m_iPlayerDamage = 0;	// usa m_flDamage (sem arredondar)
		bullets.m_iTracerFreq = ( nBalas > 1 ) ? 0 : 2;
		bullets.m_pAttacker = pPlayer;
		pPlayer->FireBullets( bullets );
	}
}

#ifdef CLIENT_DLL
// Recuo de câmera do CA: é do JOGADOR, não da arma (Corrections 1): um acumulador vertical
// e um lateral, a direção do lado e a hora do último chute. Nunca zera (só volta).
// Os acumuladores guardam metade do que a vista andou (Q4).
struct TrokCARecuo_t
{
	float J;		// vertical (rad)
	float Jy;		// lateral (rad, com sinal)
	bool bDir;		// lado do próximo chute: true = WagR
	float flChute;	// hora do último chute
	bool bSorteado;
};
static TrokCARecuo_t s_CARecuo = { 0.0f, 0.0f, false, -100.0f, false };

static void TrokCA_MexerVista( float flPitch, float flYaw )
{
	QAngle ang;
	engine->GetViewAngles( ang );
	ang.x = clamp( ang.x + flPitch, -89.0f, 89.0f );
	ang.y += flYaw;
	engine->SetViewAngles( ang );
}
#endif

// Chute por tiro (depois do raio; afeta o próximo tiro).
void CWeaponTrokGun::CARecuo( CBasePlayer *pPlayer )
{
	const TrokArmaCA_t &ca = Info().m_TrokCA;
#ifdef GAME_DLL
	// Bots: o chute pelo ViewPunch (a mira deles compensa).
	if ( pPlayer->IsBot() )
	{
		const float flLado = SharedRandomFloat( "TrokCABotLado", ca.flRecuoLadoDir, ca.flRecuoLadoEsq );
		pPlayer->ViewPunch( QAngle( -ca.flRecuo, -0.5f * flLado, 0.0f ) );
	}
#else
	if ( !prediction->IsFirstTimePredicted() || !pPlayer->IsLocalPlayer() )
		return;
	TrokCARecuo_t &R = s_CARecuo;
	if ( !R.bSorteado )
	{
		R.bDir = RandomInt( 0, 1 ) != 0;	// sorteado uma vez ao entrar no mapa
		R.bSorteado = true;
	}
	const float Jmax = DEG2RAD( ca.flRecuoMax );
	if ( Jmax <= 0.0f )
		return;
	R.J = clamp( R.J, 0.0f, Jmax );
	const float b = DEG2RAD( ca.flRecuo );
	float k = b * ( 1.0f - R.J / Jmax );
	if ( k > 0.0f )
		k += ca.flRecuoAcel * R.J;
	const float h = MAX( 0.0f, MIN( 0.5f * k, Jmax - R.J ) );
	R.J += h;
	float flPitch = -RAD2DEG( 2.0f * h );	// sobe a vista 2h

	// Lado: depois que a subida passa do limiar, chuta para o lado e pode trocar de lado.
	float flYaw = 0.0f;
	if ( fabsf( R.J ) > ca.flRecuoLadoInicio )
	{
		const float Wmax = DEG2RAD( ca.flRecuoLadoMax );
		const float q = ( Wmax > 0.0f ) ? 1.0f - fabsf( R.Jy ) / Wmax : 0.0f;
		const float w = q * DEG2RAD( R.bDir ? ca.flRecuoLadoDir : ca.flRecuoLadoEsq );
		R.Jy += 0.5f * w;
		flYaw = RAD2DEG( w );	// yaw do CA cresce para a direita; no Source, para a esquerda
		if ( fabsf( R.Jy ) > ca.flRecuoLadoTroca && RandomInt( 0, 1 ) )
			R.bDir = !R.bDir;
	}
	R.flChute = gpGlobals->curtime;
	TrokCA_MexerVista( flPitch, flYaw );
#endif
}

// Volta do recuo, todo tique, antes do tiro: puxa a vista de volta mesmo que o jogador já
// tenha corrigido com o mouse (como no CA). Usa os números da arma na mão.
void CWeaponTrokGun::CARecuoVolta( CBasePlayer *pPlayer )
{
#ifdef CLIENT_DLL
	if ( !pPlayer || !pPlayer->IsLocalPlayer() || !prediction->IsFirstTimePredicted() )
		return;
	TrokCARecuo_t &R = s_CARecuo;
	if ( R.J <= 0.0f && R.Jy == 0.0f )
		return;
	const TrokArmaCA_t &ca = Info().m_TrokCA;
	const float dt = MIN( gpGlobals->frametime, 0.1f );
	const float flTaxa = ( gpGlobals->curtime - R.flChute < ca.flRecuoVoltaEspera ) ? ca.flRecuoVoltaLenta : ca.flRecuoVoltaRapida;
	float flPitch = 0.0f, flYaw = 0.0f;

	const float Jmax = DEG2RAD( ca.flRecuoMax );
	if ( R.J > 0.0f && Jmax > 0.0f )
	{
		const float v = MIN( R.J, dt * flTaxa * ( 1.0f + R.J / Jmax ) );
		R.J -= 0.5f * v;
		flPitch = RAD2DEG( v );
		if ( R.J < 1e-6f )
			R.J = 0.0f;
	}
	const float Wmax = DEG2RAD( ca.flRecuoLadoMax );
	if ( R.Jy != 0.0f && Wmax > 0.0f )
	{
		const float a = fabsf( R.Jy );
		const float s = ( R.Jy > 0.0f ) ? 1.0f : -1.0f;
		const float v = MIN( a, dt * flTaxa * ( 1.0f + a / Wmax ) );
		R.Jy -= s * 0.5f * v;
		flYaw = -s * RAD2DEG( v );
		if ( fabsf( R.Jy ) < 1e-6f )
			R.Jy = 0.0f;
	}
	if ( flPitch != 0.0f || flYaw != 0.0f )
		TrokCA_MexerVista( flPitch, flYaw );
#else
	NOTE_UNUSED( pPlayer );
#endif
}

// Medidores de dispersão, em subpassos fixos de 1/80 s contados pelo relógio do jogo (o
// resto não se perde: recomendação do Q2). Ordem do CA: primeiro o de movimento, depois o
// de tiro; o tiro só conta no 1º subpasso do quadro em que saiu.
void CWeaponTrokGun::CATique( bool bAtirou )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
	const TrokArmaCA_t &ca = Info().m_TrokCA;

	if ( ca.iConeMax <= 0 )
	{
		m_flCAFogo = 0.0f;
		m_flCAMov = 0.0f;
		return;
	}

	const double t1 = gpGlobals->curtime;
	const double t0 = t1 - gpGlobals->frametime;
	int n = (int)( floor( t1 / CA_PASSO ) - floor( t0 / CA_PASSO ) );
	if ( n <= 0 && bAtirou )
		n = 1;
	n = MIN( n, 100 );
	if ( n <= 0 )
		return;

	const float flPasso = (float)CA_PASSO;
	const bool bMira = m_bADS;
	const float flSobe = ( bMira && ca.flConeMiraSobe > 0.0f ) ? ca.flConeMiraSobe : ca.flConeSobe;
	const float flDesce = ( bMira && ca.flConeMiraDesce > 0.0f ) ? ca.flConeMiraDesce : ca.flConeDesce;

	const int iBot = pPlayer->m_nButtons;
	const bool bAgachado = ( pPlayer->GetFlags() & FL_DUCKING ) || ( iBot & IN_DUCK );
	const bool bMovendo = ( iBot & ( IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT ) ) != 0;
	const float C = ca.flTeto[ bMovendo ? ( bAgachado ? 3 : 1 ) : ( bAgachado ? 2 : 0 ) ];

	// "Em um pulo": pulou e ainda não pousou (cair de uma borda não conta).
	bool bNoPulo = false;
	CHL2MP_Player *pHL2 = ToHL2MPPlayer( pPlayer );
	if ( pHL2 && !( pPlayer->GetFlags() & FL_ONGROUND ) )
		bNoPulo = pHL2->m_flTrokJumpTime > pHL2->m_flTrokGroundTime;
	const bool bEscada = pPlayer->GetMoveType() == MOVETYPE_LADDER;

	// Alvo do movimento: correr (sem agachar e sem andar devagar) ou andar; pulo; escada = correr.
	const bool bCorre = !bAgachado && !( iBot & IN_WALK );
	float S = bNoPulo ? ca.flAlvoPulo : ( bCorre ? ca.flAlvoCorrer : ca.flAlvoAndar );
	if ( bEscada )
		S = ca.flAlvoCorrer;

	// Fração de movimento: velocidade / velocidade-limite do momento (correndo firme = 1).
	float V;
	if ( trok_mov_ca.GetBool() )
	{
		const float flBase = bAgachado ? trok_ca_agachado.GetFloat() : ( ( iBot & IN_WALK ) ? trok_ca_andar.GetFloat() : trok_ca_corrida.GetFloat() );
		V = flBase * MAX( 0.0f, 1.0f - trok_ca_colete.GetFloat() ) * GetMoveSpeedScale() * MAX( 0.01f, trok_ca_escala.GetFloat() );
	}
	else
		V = pPlayer->MaxSpeed();
	const float mu = ( V > 1.0f ) ? MIN( 1.0f, pPlayer->GetAbsVelocity().Length() / V ) : 0.0f;
	const float muS = bEscada ? S : mu * S;
	const float A = muS;	// o termo de virar o mouse não sobe a dispersão (Q11); fica de fora

	float F = m_flCAFogo, M = m_flCAMov;
	const bool bEsperando = ( gpGlobals->curtime - m_flLastShotTime ) < ca.flEsperaRecuperar;
	for ( int i = 0; i < n; ++i )
	{
		// Movimento
		if ( A > M )
		{
			M += MIN( flPasso * CA_MOV_SOBE, A - M ) * flSobe;
			M = MIN( M, MIN( S, muS ) );
		}
		else if ( A < M )
		{
			if ( bNoPulo )
				M += MIN( flPasso * CA_MOV_DESCE, M - A ) * flSobe;	// no ar depois do pulo ela sobe (Q12)
			else
				M -= MIN( flPasso * CA_MOV_DESCE, M - A ) * flDesce;
			M = clamp( M, 0.0f, ca.flAlvoPulo );
		}

		// Tiro
		if ( bAtirou && i == 0 )
			F = MIN( C, F + flPasso * CA_FOGO_SOBE * flSobe );
		else if ( !( bAtirou || bEsperando ) && F > 0.0f )
			F = MAX( 0.0f, F - flPasso * CA_FOGO_DESCE * flDesce );
	}
	m_flCAFogo = F;
	m_flCAMov = M;
}

//-----------------------------------------------------------------------------
// Animação procedural da arma na tela: abaixada correndo, centrada mirando.
//-----------------------------------------------------------------------------
void CWeaponTrokGun::AddViewmodelBob( CBaseViewModel *viewmodel, Vector &origin, QAngle &angles )
{
	BaseClass::AddViewmodelBob( viewmodel, origin, angles );

#ifdef CLIENT_DLL
	float flTargetSprint = IsOwnerSprinting() ? 1.0f : 0.0f;
	float flTargetADS = ( m_bADS || trok_mira_calibrar.GetBool() ) ? 1.0f : 0.0f;
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

		// Coice mirando: a arma vem para trás e levanta um pouco e volta em ~0,1 s,
		// sem tirar a alça do centro por mais que um instante.
		const float flCoiceDur = 0.1f;
		const float tc = gpGlobals->curtime - m_flCoiceTempo;
		if ( tc >= 0.0f && tc < flCoiceDur )
		{
			const float c = a * ( 1.0f - tc / flCoiceDur ) * ( 1.0f - tc / flCoiceDur );
			origin -= c * Info().m_flTrokADSCoice * forward;
			angles.x -= c * Info().m_flTrokADSCoiceAng;
		}
	}
#endif
}

#ifdef CLIENT_DLL
// Mirando sem a animação de tiro: dispara os eventos de clarão que a sequência de tiro do
// modelo teria (mesmo tipo e mesmo anexo), sem tocar a sequência.
void CWeaponTrokGun::ClaraoSemAnimacao( void )
{
	CBasePlayer *pPlayer = GetPlayerOwner();
	C_BaseViewModel *pVM = pPlayer ? pPlayer->GetViewModel( m_nViewModelIndex ) : NULL;
	if ( !pVM )
		return;
	CStudioHdr *pHdr = pVM->GetModelPtr();
	if ( !pHdr )
		return;
	const int iSeq = pVM->SelectWeightedSequence( ACT_VM_PRIMARYATTACK );
	if ( iSeq < 0 || iSeq >= pHdr->GetNumSeq() )
		return;
	mstudioseqdesc_t &seq = pHdr->pSeqdesc( iSeq );
	for ( int i = 0; i < seq.numevents; ++i )
	{
		const mstudioevent_t *pEv = seq.pEvent( i );
		const int ev = pEv->event;
		const bool bClarao = ev == AE_MUZZLEFLASH || ( ev >= CL_EVENT_MUZZLEFLASH0 && ev <= CL_EVENT_MUZZLEFLASH3 );
		if ( bClarao )
			pVM->FireEvent( pVM->GetAbsOrigin(), pVM->GetAbsAngles(), ev, pEv->pszOptions() );
	}
}
#endif

#ifdef CLIENT_DLL
// Calibração: soma um valor a um eixo do trok_mira_ajuste. Bom para bind nas setas:
//   bind UPARROW "trok_mira_mexer cima 0.05"; bind DOWNARROW "trok_mira_mexer cima -0.05"
CON_COMMAND( trok_mira_mexer, "Uso: trok_mira_mexer <direita|cima|frente|pitch|yaw|roll> <valor>. Soma ao trok_mira_ajuste." )
{
	static const char *s_pszEixos[ 6 ] = { "direita", "cima", "frente", "pitch", "yaw", "roll" };
	if ( args.ArgC() < 3 )
	{
		Msg( "Uso: trok_mira_mexer <direita|cima|frente|pitch|yaw|roll> <valor>\n" );
		return;
	}
	int iEixo = -1;
	for ( int i = 0; i < 6; ++i )
		if ( !Q_stricmp( args[ 1 ], s_pszEixos[ i ] ) )
			iEixo = i;
	if ( iEixo < 0 )
	{
		Msg( "Eixo desconhecido: %s\n", args[ 1 ] );
		return;
	}
	float adj[ 6 ] = {};
	sscanf( trok_mira_ajuste.GetString(), "%f %f %f %f %f %f", &adj[ 0 ], &adj[ 1 ], &adj[ 2 ], &adj[ 3 ], &adj[ 4 ], &adj[ 5 ] );
	adj[ iEixo ] += atof( args[ 2 ] );
	char szVal[ 128 ];
	V_snprintf( szVal, sizeof( szVal ), "%.3f %.3f %.3f %.3f %.3f %.3f", adj[ 0 ], adj[ 1 ], adj[ 2 ], adj[ 3 ], adj[ 4 ], adj[ 5 ] );
	trok_mira_ajuste.SetValue( szVal );
	Msg( "trok_mira_ajuste \"%s\"\n", szVal );
}

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
