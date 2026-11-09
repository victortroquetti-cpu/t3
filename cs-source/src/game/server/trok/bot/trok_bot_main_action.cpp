//=============================================================================
// Trok: ação raiz dos bots. Ver trok_bot_main_action.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_main_action.h"
#include "trok_bot_vision.h"
#include "hl2mp_player.h"
#include "NextBotUtil.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_mira_humana( "trok_bot_mira_humana", "1", FCVAR_CHEAT,
	"Mira com erro humano (foco que melhora com o tempo). 0 = mira do HL2MP." );
static ConVar trok_bot_audicao( "trok_bot_audicao", "1", FCVAR_CHEAT,
	"Bots ouvem passos de quem corre por perto e viram para o barulho." );
static ConVar trok_bot_debug_mira( "trok_bot_debug_mira", "0", FCVAR_CHEAT,
	"Mostra o ponto de mira e o erro atual do bot escolhido com nb_select." );

// Um engajamento novo começa se o bot ficou este tempo sem mirar no alvo.
#define AIM_ENGAGEMENT_GAP		0.6f
// Raio (u) em que se ouvem passos de quem corre / anda rápido. Agachado ou andando devagar não faz barulho.
#define HEAR_RUN_RANGE			1100.0f
#define HEAR_JOG_RANGE			600.0f

CTrokBotMainAction::CTrokBotMainAction()
{
	m_flAimStart = 0.0f;
	m_flAimLastQuery = -100.0f;
	m_flAimInitialError = 0.0f;
	m_flAimPhase = 0.0f;
	m_flAimWobble = 0.0f;
	m_bAimHead = false;
	m_flFlinchError = 0.0f;
	m_flFlinchTime = 0.0f;
	m_flRetargetUntil = 0.0f;
	m_iStrafeDir = 0;
}

//-----------------------------------------------------------------------------
ActionResult< CHL2MPBot > CTrokBotMainAction::Update( CHL2MPBot *me, float interval )
{
	ActionResult< CHL2MPBot > result = BaseClass::Update( me, interval );

	if ( trok_bot_audicao.GetBool() )
		ListenForFootsteps( me );

	return result;
}

// Passos: quem corre faz barulho; o bot "sente" a direção e vira para olhar,
// mas não ganha a posição exata (a percepção nunca passa do limite só por som).
void CTrokBotMainAction::ListenForFootsteps( CHL2MPBot *me )
{
	if ( !m_listenTimer.IsElapsed() )
		return;
	m_listenTimer.Start( 0.25f );

	CTrokBotVision *vision = static_cast< CTrokBotVision * >( me->GetVisionInterface() );
	const TrokBotProfile_t &profile = vision->GetProfile();

	// Já está vendo um inimigo: o som não muda nada.
	const CKnownEntity *threat = vision->GetPrimaryKnownThreat( true );
	if ( threat )
		return;

	CBasePlayer *pLoudest = NULL;
	float flLoudest = 0.0f;

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || !pPlayer->IsAlive() || !me->IsEnemy( pPlayer ) )
			continue;

		if ( !( pPlayer->GetFlags() & FL_ONGROUND ) || ( pPlayer->GetFlags() & FL_DUCKING ) )
			continue;

		float flSpeed = pPlayer->GetAbsVelocity().Length2D();
		float flRange = ( flSpeed > 200.0f ) ? HEAR_RUN_RANGE : ( flSpeed > 160.0f ) ? HEAR_JOG_RANGE : 0.0f;
		flRange *= profile.flAudicao;
		if ( flRange <= 0.0f )
			continue;

		float flDist = me->GetRangeTo( pPlayer );
		if ( flDist > flRange )
			continue;

		// Parede no meio abafa (sem simular propagação de som ainda).
		if ( !me->IsLineOfSightClear( pPlayer, CBaseCombatCharacter::IGNORE_ACTORS ) )
		{
			flRange *= 0.6f;
			if ( flDist > flRange )
				continue;
		}

		float flLoudness = 1.0f - flDist / flRange;
		if ( flLoudness > flLoudest )
		{
			flLoudest = flLoudness;
			pLoudest = pPlayer;
		}
	}

	if ( !pLoudest )
		return;

	vision->NoteNoise( pLoudest, 0.15f + 0.35f * flLoudest );

	// Olha na direção do barulho com erro: sabe de onde veio, não o ponto exato.
	Vector toNoise = pLoudest->GetAbsOrigin() - me->GetAbsOrigin();
	float flError = toNoise.Length() * 0.25f * ( 1.0f - 0.5f * flLoudest );
	Vector lookAt = pLoudest->WorldSpaceCenter() + Vector( RandomFloat( -flError, flError ), RandomFloat( -flError, flError ), 0.0f );
	me->GetBodyInterface()->AimHeadTowards( lookAt, IBody::INTERESTING, 1.0f, NULL, "Ouviu passos" );
}

//-----------------------------------------------------------------------------
EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnInjured( CHL2MPBot *me, const CTakeDamageInfo &info )
{
	// Levar tiro atrapalha a mira por um instante.
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	m_flFlinchError = MIN( m_flFlinchError + profile.flMiraTremorFerido, 3.0f * profile.flMiraTremorFerido );
	m_flFlinchTime = gpGlobals->curtime;

	// Quem atirou chama atenção na hora.
	CTrokBotVision *vision = static_cast< CTrokBotVision * >( me->GetVisionInterface() );
	if ( info.GetAttacker() )
		vision->NoteGunfire( info.GetAttacker() );

	return BaseClass::OnInjured( me, info );
}

EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnWeaponFired( CHL2MPBot *me, CBaseCombatCharacter *whoFired, CBaseCombatWeapon *weapon )
{
	if ( whoFired && whoFired != me )
		static_cast< CTrokBotVision * >( me->GetVisionInterface() )->NoteGunfire( whoFired );

	return TryContinue();
}

// Derrubou (ou viu morrer) quem estava na mira: gente leva um instante para
// trocar de alvo; o bot não emenda o próximo tiro no mesmo quadro.
EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnOtherKilled( CHL2MPBot *me, CBaseCombatCharacter *victim, const CTakeDamageInfo &info )
{
	if ( victim && victim == m_hAimSubject.Get() )
	{
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
		m_flRetargetUntil = gpGlobals->curtime + RandomFloat( 0.5f, 1.0f ) * ( profile.flReacaoMin + profile.flReacaoMax );
	}

	return BaseClass::OnOtherKilled( me, victim, info );
}

QueryResultType CTrokBotMainAction::ShouldAttack( const INextBot *me, const CKnownEntity *them ) const
{
	if ( gpGlobals->curtime < m_flRetargetUntil )
		return ANSWER_NO;

	return BaseClass::ShouldAttack( me, them );
}

//-----------------------------------------------------------------------------
// Mira humana
//-----------------------------------------------------------------------------
Vector CTrokBotMainAction::SelectTargetPoint( const INextBot *meBot, const CBaseCombatCharacter *subject ) const
{
	CHL2MPBot *me = (CHL2MPBot *)meBot->GetEntity();

	if ( !trok_bot_mira_humana.GetBool() || !subject || !subject->IsPlayer() )
		return BaseClass::SelectTargetPoint( meBot, subject );

	// Armas com mira especial (foguete, granada, física) e arma branca: do HL2MP.
	CBaseCombatWeapon *pWeapon = me->GetActiveWeapon();
	if ( !pWeapon || CHL2MPBot::IsBludgeon( pWeapon ) ||
		 pWeapon->ClassMatches( "weapon_rpg" ) || pWeapon->ClassMatches( "weapon_frag" ) || pWeapon->ClassMatches( "weapon_physcannon" ) )
		return BaseClass::SelectTargetPoint( meBot, subject );

	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	float flNow = gpGlobals->curtime;
	const Vector &eye = me->EyePosition();
	float flRange = MAX( 1.0f, ( subject->WorldSpaceCenter() - eye ).Length() );

	// Engajamento novo: alvo diferente, ou ficou um tempo sem mirar nele.
	if ( m_hAimSubject.Get() != subject || flNow - m_flAimLastQuery > AIM_ENGAGEMENT_GAP )
	{
		m_hAimSubject = const_cast< CBaseCombatCharacter * >( subject );
		m_flAimStart = flNow;

		// Mais longe = menos chance de arriscar a cabeça.
		float flHeadChance = profile.flChanceCabeca * RemapValClamped( flRange, 600.0f, 2000.0f, 1.0f, 0.3f );
		m_bAimHead = RandomFloat() < flHeadChance;

		// O erro de partida cresce com o quanto ele teve que girar (puxada longa erra mais).
		Vector toSubject = subject->WorldSpaceCenter() - eye;
		toSubject.NormalizeInPlace();
		float flTurn = RAD2DEG( acos( clamp( DotProduct( me->GetBodyInterface()->GetViewVector(), toSubject ), -1.0f, 1.0f ) ) );
		m_flAimInitialError = profile.flMiraErroInicial * RandomFloat( 0.7f, 1.3f ) * ( 1.0f + flTurn / 90.0f );

		m_flAimPhase = RandomFloat( 0.0f, 2.0f * M_PI );
		m_flAimWobble = RandomFloat( 0.0f, 2.0f * M_PI );
	}
	m_flAimLastQuery = flNow;

	// Foco: o erro cai pela metade a cada meia-vida, até o mínimo.
	float t = flNow - m_flAimStart;
	float flError = profile.flMiraErroMinimo +
		( m_flAimInitialError - profile.flMiraErroMinimo ) * powf( 0.5f, t / MAX( 0.05f, profile.flMiraMeiaVida ) );

	// Alvo atravessando a visão rápido é difícil de acompanhar.
	Vector toSubject = subject->WorldSpaceCenter() - eye;
	Vector relVel = subject->GetAbsVelocity() - me->GetAbsVelocity();
	Vector dir = toSubject;
	dir.NormalizeInPlace();
	Vector perpVel = relVel - DotProduct( relVel, dir ) * dir;
	float flAngularSpeed = RAD2DEG( perpVel.Length() / flRange );	// graus/s
	flError += profile.flMiraRastreio * flAngularSpeed / 100.0f;

	// Correndo enquanto atira, erra mais.
	if ( me->GetAbsVelocity().Length2D() > 120.0f )
		flError *= 1.25f;

	// Tremor de quem levou dano, sumindo em ~0.5 s.
	if ( m_flFlinchError > 0.0f )
	{
		float flFlinch = m_flFlinchError * powf( 0.5f, ( flNow - m_flFlinchTime ) / 0.25f );
		flError += flFlinch;
	}

	// Ponto base: cabeça ou peito.
	Vector target = m_bAimHead ? ( subject->EyePosition() - Vector( 0, 0, 2 ) ) : ( subject->WorldSpaceCenter() + Vector( 0, 0, 6 ) );

	// Desvio no plano do alvo, numa direção que passeia devagar (não treme).
	Vector right( -dir.y, dir.x, 0.0f );
	right.NormalizeInPlace();
	Vector up = CrossProduct( right, dir );
	float flAngle = m_flAimPhase + 0.9f * sinf( 1.3f * flNow + m_flAimWobble );
	float flOffset = tanf( DEG2RAD( flError ) ) * flRange;
	target += flOffset * ( cosf( flAngle ) * right + 0.6f * sinf( flAngle ) * up );	// erro vertical menor (gente mira na linha do horizonte)

	if ( trok_bot_debug_mira.GetBool() && TheNextBots().GetSelected() == meBot )
	{
		NDebugOverlay::Cross3D( target, 3.0f, 255, 0, 0, true, 0.1f );
		NDebugOverlay::Text( target + Vector( 0, 0, 6 ), CFmtStr( "erro %.1f° %s", flError, m_bAimHead ? "cabeça" : "peito" ), true, 0.1f );
	}

	return target;
}

//-----------------------------------------------------------------------------
// Strafe em combate
//-----------------------------------------------------------------------------
void CTrokBotMainAction::Dodge( CHL2MPBot *me )
{
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

	if ( profile.flStrafeMax <= 0.0f || me->HasAttribute( CHL2MPBot::DISABLE_DODGE ) )
		return;

	if ( !me->IsCombatWeapon( MY_CURRENT_GUN ) )
		return;

	if ( me->GetIntentionInterface()->ShouldHurry( me ) == ANSWER_YES )
		return;

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( !threat || !threat->IsVisibleRecently() || !me->IsLineOfFireClear( threat->GetLastKnownPosition() ) )
	{
		m_iStrafeDir = 0;
		return;
	}

	if ( m_strafeTimer.IsElapsed() )
	{
		// Troca de lado quase sempre; às vezes para um instante para atirar
		// parado (mais preciso), como um jogador faz.
		float flRoll = RandomFloat();
		if ( flRoll < 0.2f )
			m_iStrafeDir = 0;
		else if ( m_iStrafeDir == 0 || flRoll < 0.85f )
			m_iStrafeDir = ( m_iStrafeDir == 1 ) ? -1 : 1;

		float flDuration = RandomFloat( profile.flStrafeMin, profile.flStrafeMax );
		if ( m_iStrafeDir == 0 )
			flDuration *= 0.5f;
		m_strafeTimer.Start( flDuration );

		if ( RandomFloat() < profile.flChanceAgachar )
			me->PressCrouchButton( RandomFloat( 0.4f, 1.0f ) );
	}

	if ( m_iStrafeDir == 0 )
		return;

	Vector forward;
	me->EyeVectors( &forward );
	Vector left( -forward.y, forward.x, 0.0f );
	left.NormalizeInPlace();

	const float flStep = 40.0f;
	Vector side = ( m_iStrafeDir < 0 ) ? left : -left;

	// Não cai de borda nem fica empurrando parede: troca de lado.
	trace_t tr;
	CTraceFilterSimple filter( me, COLLISION_GROUP_PLAYER_MOVEMENT );
	UTIL_TraceHull( me->GetAbsOrigin() + Vector( 0, 0, 18 ), me->GetAbsOrigin() + Vector( 0, 0, 18 ) + flStep * side,
		Vector( -16, -16, 0 ), Vector( 16, 16, 54 ), MASK_PLAYERSOLID, &filter, &tr );

	if ( tr.DidHit() || me->GetLocomotionInterface()->HasPotentialGap( me->GetAbsOrigin(), me->GetAbsOrigin() + flStep * side ) )
	{
		m_iStrafeDir = -m_iStrafeDir;
		return;
	}

	if ( m_iStrafeDir < 0 )
		me->PressLeftButton();
	else
		me->PressRightButton();
}
