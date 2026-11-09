//=============================================================================
// Trok: ação raiz dos bots. Ver trok_bot_main_action.h.
//=============================================================================

#include "cbase.h"
#include "trok_bot.h"
#include "trok_bot_main_action.h"
#include "trok_bot_vision.h"
#include "trok_bot_team.h"
#include "hl2mp_player.h"
#include "NextBotUtil.h"
#include "trok/weapon_trok_gun.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_bot_mira_humana( "trok_bot_mira_humana", "1", FCVAR_CHEAT,
	"Mira com erro humano (foco que melhora com o tempo). 0 = mira do HL2MP." );
static ConVar trok_bot_audicao( "trok_bot_audicao", "1", FCVAR_CHEAT,
	"Bots ouvem passos de quem corre por perto e viram para o barulho." );
static ConVar trok_bot_ritmo_tiro( "trok_bot_ritmo_tiro", "1", FCVAR_CHEAT,
	"Bots atiram em rajadas parados (contra-strafe) a média e longa distância, conforme a disciplina do nível." );
static ConVar trok_bot_checar_cantos( "trok_bot_checar_cantos", "1", FCVAR_CHEAT,
	"Bots olham, em ordem, os cantos de onde alguém pode aparecer enquanto andam (encounter spots do nav_analyze)." );
static ConVar trok_bot_debug_mira( "trok_bot_debug_mira", "0", FCVAR_CHEAT,
	"Mostra o ponto de mira e o erro atual do bot escolhido com nb_select." );

// Um engajamento novo começa se o bot ficou este tempo sem mirar no alvo.
#define AIM_ENGAGEMENT_GAP		0.6f
// Raio (u) em que se ouvem passos de quem corre / anda rápido. Agachado ou andando devagar não faz barulho.
#define HEAR_RUN_RANGE			1100.0f
#define HEAR_JOG_RANGE			600.0f
// Até esta distância, spray andando é o normal (todo mundo faz).
#define CLOSE_FIGHT_RANGE		450.0f

// Movimento de combate controlado por uma ação (ex.: peek do CTrokBotFight):
// o strafe da ação raiz fica parado para não brigar com ela.
static bool s_bCombatMoveOwned[ MAX_PLAYERS + 1 ];

void TrokBot_SetCombatMoveOwned( CHL2MPBot *me, bool bOwned )
{
	int i = me->entindex();
	if ( i > 0 && i <= MAX_PLAYERS )
		s_bCombatMoveOwned[ i ] = bOwned;
}

static bool IsCombatMoveOwned( CHL2MPBot *me )
{
	int i = me->entindex();
	return i > 0 && i <= MAX_PLAYERS && s_bCombatMoveOwned[ i ];
}

// Onde cada bot morreu da última vez (posição de quem matou).
struct DeathMemory_t { Vector killerPos; float flTime; };
static DeathMemory_t s_deathMemory[ MAX_PLAYERS + 1 ];

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
	m_flLastInjured = -100.0f;
	m_flThreatSeenSince = 0.0f;
	m_iStrafeDir = 0;
	m_iLastStrafeDir = 1;
	m_combatPhase = PHASE_MOVE;
	m_bDisciplinedCycle = true;
	m_bFireWindowOpen = true;
	m_pCornerArea = NULL;
	m_pCornerPrevArea = NULL;
	m_iCornerSpot = 0;
	m_flLastVisibleThreat = -100.0f;
	m_bSuppressArmed = false;
	m_flSuppressUntil = 0.0f;
	m_vecSuppress = vec3_origin;
}

//-----------------------------------------------------------------------------
ActionResult< CHL2MPBot > CTrokBotMainAction::Update( CHL2MPBot *me, float interval )
{
	// O HL2MP decide o tiro (e aperta o gatilho) dentro do Update; o ritmo de
	// tiro solta o gatilho fora da janela de rajada.
	ActionResult< CHL2MPBot > result = BaseClass::Update( me, interval );

	UpdateFireWindow( me );
	if ( !m_bFireWindowOpen )
		me->ReleaseFireButton();

	UpdateSuppression( me );
	UpdateADS( me );

	if ( trok_bot_audicao.GetBool() )
		ListenForFootsteps( me );

	if ( trok_bot_checar_cantos.GetBool() )
		CheckCorners( me );

	// Avisa o time onde está o inimigo que ele está vendo (o "call" do CS).
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( true );
	if ( threat && threat->GetEntity() && m_intelTimer.IsElapsed() )
	{
		m_intelTimer.Start( 1.0f );
		TrokBot_AddIntel( me, threat->GetLastKnownPosition(), 0.0f );
	}

	// Emboscada: desde quando o alvo atual está à vista.
	CBaseEntity *pSeen = threat ? threat->GetEntity() : NULL;
	if ( pSeen != m_hThreatSeen.Get() )
	{
		m_hThreatSeen = pSeen;
		m_flThreatSeenSince = gpGlobals->curtime;
	}

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
	TrokBot_AddIntel( me, pLoudest->GetAbsOrigin(), 250.0f );	// passos dão a região, não o ponto

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
	m_flLastInjured = gpGlobals->curtime;

	// Quem atirou chama atenção na hora.
	CTrokBotVision *vision = static_cast< CTrokBotVision * >( me->GetVisionInterface() );
	if ( info.GetAttacker() )
		vision->NoteGunfire( info.GetAttacker() );

	return BaseClass::OnInjured( me, info );
}

EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnWeaponFired( CHL2MPBot *me, CBaseCombatCharacter *whoFired, CBaseCombatWeapon *weapon )
{
	if ( whoFired && whoFired != me )
	{
		static_cast< CTrokBotVision * >( me->GetVisionInterface() )->NoteGunfire( whoFired );

		// Tiro de inimigo ouvido por perto vira pista para o time (com erro: som dá a região).
		if ( me->IsEnemy( whoFired ) && me->IsRangeLessThan( whoFired, 2500.0f ) )
			TrokBot_AddIntel( me, whoFired->GetAbsOrigin(), 200.0f );
	}

	return TryContinue();
}

// Derrubou (ou viu morrer) quem estava na mira: gente leva um instante para
// trocar de alvo; o bot não emenda o próximo tiro no mesmo quadro.
EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnOtherKilled( CHL2MPBot *me, CBaseCombatCharacter *victim, const CTakeDamageInfo &info )
{
	if ( victim )
		TrokBot_NoteDeath( victim );

	if ( victim && victim == m_hAimSubject.Get() )
	{
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
		m_flRetargetUntil = gpGlobals->curtime + RandomFloat( 0.5f, 1.0f ) * ( profile.flReacaoMin + profile.flReacaoMax );
	}

	return BaseClass::OnOtherKilled( me, victim, info );
}

EventDesiredResult< CHL2MPBot > CTrokBotMainAction::OnKilled( CHL2MPBot *me, const CTakeDamageInfo &info )
{
	TrokBot_NoteDeath( me );
	TrokBot_ReleaseGoal( me );
	TrokBot_SetCombatMoveOwned( me, false );

	// Lembra de onde veio o tiro que o matou: na próxima vida, pré-mira esse lugar.
	int i = me->entindex();
	if ( i > 0 && i <= MAX_PLAYERS && info.GetAttacker() && info.GetAttacker() != me )
	{
		s_deathMemory[ i ].killerPos = info.GetAttacker()->GetAbsOrigin();
		s_deathMemory[ i ].flTime = gpGlobals->curtime;
	}
	TrokBot_ReleaseHold( me );
	return BaseClass::OnKilled( me, info );
}

QueryResultType CTrokBotMainAction::ShouldAttack( const INextBot *meBot, const CKnownEntity *them ) const
{
	if ( gpGlobals->curtime < m_flRetargetUntil )
		return ANSWER_NO;

	// Emboscada (âncora segurando posição): mira e espera. Atira quando o
	// inimigo chega perto, vira de costas, já está à vista há um tempo
	// (vai acabar vendo) ou quando o próprio bot leva tiro.
	CHL2MPBot *me = (CHL2MPBot *)meBot->GetEntity();
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	if ( profile.bEmboscada && TrokBot_IsHolding( me ) && them && them->GetEntity() &&
		 gpGlobals->curtime - m_flLastInjured > 3.0f )
	{
		CBaseEntity *pThem = them->GetEntity();
		bool bClose = me->IsRangeLessThan( pThem, 700.0f );

		Vector theirForward;
		AngleVectors( pThem->EyeAngles(), &theirForward );
		Vector toMe = me->EyePosition() - pThem->EyePosition();
		toMe.NormalizeInPlace();
		bool bLookingAway = DotProduct( theirForward, toMe ) < 0.6f;

		bool bSeenLong = gpGlobals->curtime - m_flThreatSeenSince > 2.5f;

		if ( !bClose && !bLookingAway && !bSeenLong )
			return ANSWER_NO;
	}

	return BaseClass::ShouldAttack( meBot, them );
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

	// Tamanho do alvo na tela (graus da meia largura do tronco, ~13u).
	// Gente mira mais fino em alvo pequeno (longe) e leva mais tempo para isso
	// (lei de Fitts): o erro mínimo acompanha o tamanho do alvo e a meia-vida
	// cresce com a dificuldade do alvo.
	float flTargetHalfWidth = RAD2DEG( atanf( 13.0f / flRange ) );
	float flErrorFloor = MIN( profile.flMiraErroMinimo, profile.flMiraPrecisaoAlvo * flTargetHalfWidth );
	float flIndex = log2f( 1.0f + m_flAimInitialError / MAX( 0.05f, flTargetHalfWidth ) );
	float flHalfLife = MAX( 0.05f, profile.flMiraMeiaVida ) * ( 0.6f + 0.2f * flIndex );

	// Foco: o erro cai pela metade a cada meia-vida, até o mínimo.
	float t = flNow - m_flAimStart;
	float flError = flErrorFloor + ( m_flAimInitialError - flErrorFloor ) * powf( 0.5f, t / flHalfLife );

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
// Ritmo de combate: strafe, contra-strafe e rajadas
//-----------------------------------------------------------------------------

// Distância até o alvo visível atual (ou -1 se não há).
static float VisibleThreatRange( CHL2MPBot *me, const CKnownEntity **ppThreat = NULL )
{
	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat();
	if ( ppThreat )
		*ppThreat = threat;
	if ( !threat || !threat->GetEntity() || !threat->IsVisibleRecently() )
		return -1.0f;
	return me->GetRangeTo( threat->GetLastKnownPosition() );
}

// Decide se o gatilho pode ficar apertado agora.
//   Perto (< 450u) ou ciclo "indisciplinado": spray à vontade (andando).
//   Média/longa distância e ciclo disciplinado: atira só na fase de rajada,
//   parado; rajada de 0,6-1 s até 1200u, 0,3-0,5 s mais longe (valores
//   escolhidos no simulador tools/botsim: rajadas longas ganham mais duelos).
void CTrokBotMainAction::UpdateFireWindow( CHL2MPBot *me )
{
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	float flRange = VisibleThreatRange( me );

	if ( !trok_bot_ritmo_tiro.GetBool() || flRange < 0.0f || flRange < CLOSE_FIGHT_RANGE ||
		 !m_bDisciplinedCycle || profile.flDisciplina <= 0.0f || CHL2MPBot::IsBludgeon( me->GetActiveWeapon() ) )
	{
		m_bFireWindowOpen = true;
		return;
	}

	m_bFireWindowOpen = ( m_combatPhase == PHASE_BURST );
}

//-----------------------------------------------------------------------------
// Mira pelo cano: de média/longa distância, sem pressa, quem tem disciplina mira.
//-----------------------------------------------------------------------------
static ConVar trok_bot_mira_cano( "trok_bot_mira_cano", "1", FCVAR_CHEAT, "Bots usam a mira pelo cano." );
static ConVar trok_bot_mira_cano_dist( "trok_bot_mira_cano_dist", "700", FCVAR_CHEAT, "Distância (u) a partir da qual os bots miram pelo cano." );
static ConVar trok_bot_supressao( "trok_bot_supressao", "1", FCVAR_CHEAT, "Bots atiram onde o inimigo sumiu por um instante (fogo de supressão)." );

void CTrokBotMainAction::UpdateADS( CHL2MPBot *me )
{
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( me->GetActiveWeapon() );
	if ( !pGun || !pGun->HasADS() )
		return;

	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	const float flRange = VisibleThreatRange( me );
	if ( flRange >= 0.0f )
		m_flLastVisibleThreat = gpGlobals->curtime;

	const bool bSkilled = profile.flDisciplina >= 0.3f;
	bool bWant = false;
	if ( trok_bot_mira_cano.GetBool() && bSkilled )
	{
		if ( flRange >= trok_bot_mira_cano_dist.GetFloat() && me->GetIntentionInterface()->ShouldHurry( me ) != ANSWER_YES )
			bWant = true;
		if ( gpGlobals->curtime < m_flSuppressUntil )
			bWant = true;
		// Perdeu de vista agora: segura a mira um instante (não fica entrando e saindo).
		if ( pGun->IsADS() && gpGlobals->curtime - m_flLastVisibleThreat < 1.0f )
			bWant = true;
	}

	if ( pGun->IsADS() != bWant && m_adsTimer.IsElapsed() )
	{
		me->PressAltFireButton( 0.05f ); // o botão 2 alterna a mira nas armas do Trok
		m_adsTimer.Start( 0.5f );
	}
}

//-----------------------------------------------------------------------------
// Fogo de supressão: o inimigo que estava à vista sumiu atrás de algo; por 1,5 a
// 2,5 s atira onde ele estava, varrendo de um lado a outro, sem atirar em colega.
// Erro de 10 graus na direção e 20% na distância, como no NT;RE.
//-----------------------------------------------------------------------------
void CTrokBotMainAction::UpdateSuppression( CHL2MPBot *me )
{
	if ( !trok_bot_supressao.GetBool() )
		return;

	const CKnownEntity *threat = me->GetVisionInterface()->GetPrimaryKnownThreat( false );
	if ( !threat || !threat->GetEntity() || !threat->GetEntity()->IsAlive() )
	{
		m_flSuppressUntil = 0.0f;
		m_bSuppressArmed = false;
		return;
	}

	if ( threat->IsVisibleRecently() && threat->GetTimeSinceLastSeen() < 0.1f )
	{
		m_bSuppressArmed = true;
		m_flSuppressUntil = 0.0f;
		return;
	}

	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( me->GetActiveWeapon() );
	const bool bGunOk = pGun && pGun->IsAutomatic() && pGun->Clip1() >= MAX( 1, pGun->GetMaxClip1() / 4 );

	if ( m_bSuppressArmed && threat->WasEverVisible() )
	{
		m_bSuppressArmed = false;
		const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
		if ( bGunOk && threat->GetTimeSinceLastSeen() < 1.0f && RandomFloat() < profile.flDisciplina )
		{
			const Vector eye = me->EyePosition();
			Vector target = threat->GetLastKnownPosition() + Vector( 0, 0, 40.0f );
			Vector dir = target - eye;
			float flDist = dir.NormalizeInPlace();
			QAngle ang;
			VectorAngles( dir, ang );
			ang.y += RandomFloat( -10.0f, 10.0f );
			ang.x += RandomFloat( -3.0f, 3.0f );
			AngleVectors( ang, &dir );
			m_vecSuppress = eye + dir * flDist * RandomFloat( 0.8f, 1.2f );
			m_flSuppressUntil = gpGlobals->curtime + RandomFloat( 1.5f, 2.5f );
		}
	}

	if ( gpGlobals->curtime >= m_flSuppressUntil || !bGunOk )
		return;

	// Varre ±64u de lado.
	const Vector eye = me->EyePosition();
	Vector toTarget = m_vecSuppress - eye;
	Vector side( -toTarget.y, toTarget.x, 0.0f );
	side.NormalizeInPlace();
	const Vector aim = m_vecSuppress + side * ( 64.0f * sinf( gpGlobals->curtime * 2.5f ) );
	me->GetBodyInterface()->AimHeadTowards( aim, IBody::IMPORTANT, 0.2f, NULL, "Fogo de supressão" );

	// Só atira olhando para lá e sem colega na frente (rajadas curtas para reconferir).
	Vector forward;
	me->EyeVectors( &forward );
	Vector toAim = aim - eye;
	toAim.NormalizeInPlace();
	if ( DotProduct( forward, toAim ) < 0.996f ) // ~5 graus
		return;

	trace_t tr;
	UTIL_TraceLine( eye, aim, MASK_SHOT, me, COLLISION_GROUP_NONE, &tr );
	if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() && tr.m_pEnt->GetTeamNumber() == me->GetTeamNumber() && me->GetTeamNumber() > TEAM_SPECTATOR )
		return;

	me->PressFireButton( 0.1f );
}

void CTrokBotMainAction::Dodge( CHL2MPBot *me )
{
	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );

	const CKnownEntity *threat = NULL;
	float flRange = VisibleThreatRange( me, &threat );

	bool bFighting = flRange >= 0.0f && me->IsCombatWeapon( MY_CURRENT_GUN ) && !CHL2MPBot::IsBludgeon( me->GetActiveWeapon() ) &&
		me->IsLineOfFireClear( threat->GetLastKnownPosition() );

	if ( !bFighting || me->HasAttribute( CHL2MPBot::DISABLE_DODGE ) )
	{
		m_iStrafeDir = 0;
		m_combatPhase = PHASE_MOVE;
		m_phaseTimer.Invalidate();
		return;
	}

	// Uma ação (peek) está movendo o bot: não mexe nos pés, só no ritmo do tiro.
	bool bMoveOwned = IsCombatMoveOwned( me );
	bool bCanStrafe = profile.flStrafeMax > 0.0f && !bMoveOwned &&
		me->GetIntentionInterface()->ShouldHurry( me ) != ANSWER_YES;

	bool bRhythm = trok_bot_ritmo_tiro.GetBool() && flRange >= CLOSE_FIGHT_RANGE && profile.flDisciplina > 0.0f;

	if ( m_phaseTimer.IsElapsed() )
	{
		switch ( m_combatPhase )
		{
		case PHASE_MOVE:
			if ( bRhythm && m_bDisciplinedCycle )
			{
				// Contra-strafe: um toque para o lado oposto para parar rápido.
				m_combatPhase = PHASE_STOP;
				m_phaseTimer.Start( 0.08f );
				m_iStrafeDir = 0;
				break;
			}
			// sem ritmo: continua strafe (troca de lado como antes)
			m_phaseTimer.Start( RandomFloat( profile.flStrafeMin, MAX( profile.flStrafeMin, profile.flStrafeMax ) ) );
			m_iStrafeDir = ( m_iStrafeDir == 1 ) ? -1 : 1;
			if ( RandomFloat() < 0.2f )
				m_iStrafeDir = 0;
			m_bDisciplinedCycle = RandomFloat() < profile.flDisciplina;
			break;

		case PHASE_STOP:
		{
			m_combatPhase = PHASE_BURST;
			// Rajada: mais curta quanto mais longe (tiro a tiro de longe).
			// Valores escolhidos no simulador (tools/botsim): rajadas longas e pouco
			// tempo andando ganham mais duelos que rajadas curtas.
			float flBurst = ( flRange < 1200.0f ) ? RandomFloat( 0.6f, 1.0f ) : RandomFloat( 0.3f, 0.5f );
			m_phaseTimer.Start( flBurst );
			m_iStrafeDir = 0;
			if ( RandomFloat() < profile.flChanceAgachar )
				me->PressCrouchButton( flBurst + 0.3f );
			break;
		}

		case PHASE_BURST:
			// Recupera a mira (o recuo assenta) antes de voltar a andar.
			m_combatPhase = PHASE_RECOVER;
			m_phaseTimer.Start( ( flRange < 1200.0f ) ? RandomFloat( 0.05f, 0.1f ) : RandomFloat( 0.1f, 0.2f ) );
			break;

		case PHASE_RECOVER:
		default:
			m_combatPhase = PHASE_MOVE;
			m_bDisciplinedCycle = RandomFloat() < profile.flDisciplina;
			// Lado novo, segmento curto em luta de longe (sai da mira e volta).
			m_iStrafeDir = ( m_iLastStrafeDir == 1 ) ? -1 : 1;
			m_phaseTimer.Start( RandomFloat( profile.flStrafeMin, MAX( profile.flStrafeMin, profile.flStrafeMax ) ) * 0.3f );
			break;
		}
	}

	// Contra-strafe: toque no lado oposto ao último movimento.
	if ( m_combatPhase == PHASE_STOP )
	{
		if ( !bMoveOwned )
		{
			if ( m_iLastStrafeDir < 0 )
				me->PressRightButton( 0.08f );
			else
				me->PressLeftButton( 0.08f );
		}
		return;
	}

	if ( m_combatPhase != PHASE_MOVE || !bCanStrafe || m_iStrafeDir == 0 )
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

	m_iLastStrafeDir = m_iStrafeDir;
	if ( m_iStrafeDir < 0 )
		me->PressLeftButton();
	else
		me->PressRightButton();
}

//-----------------------------------------------------------------------------
// Checar cantos
//-----------------------------------------------------------------------------

// Andando sem inimigo à vista: olha, em ordem, os pontos de onde alguém pode
// aparecer no trecho que está cruzando (encounter spots do nav_analyze, os
// mesmos que os bots do CS usavam). Sem esses dados, olha os esconderijos da
// área seguinte. E se lembra de onde levou o tiro que o matou na última vida.
void CTrokBotMainAction::CheckCorners( CHL2MPBot *me )
{
	if ( !m_cornerTimer.IsElapsed() )
		return;

	const TrokBotProfile_t &profile = TrokBot_GetProfile( me );
	m_cornerTimer.Start( RandomFloat( 0.45f, 0.8f ) );

	if ( me->GetVisionInterface()->GetPrimaryKnownThreat( true ) )
		return;

	Vector velocity = me->GetAbsVelocity();
	velocity.z = 0.0f;
	if ( velocity.Length() < 60.0f )
		return;

	// Bot pouco tático esquece de checar às vezes.
	if ( RandomFloat() > 0.35f + 0.65f * profile.flUsoTatico )
		return;

	// Memória da última morte: se está perto de onde veio o tiro, olha para lá.
	int iMe = me->entindex();
	if ( iMe > 0 && iMe <= MAX_PLAYERS && gpGlobals->curtime - s_deathMemory[ iMe ].flTime < 90.0f &&
		 me->IsRangeLessThan( s_deathMemory[ iMe ].killerPos, 1800.0f ) && RandomFloat() < 0.5f &&
		 me->GetVisionInterface()->IsLineOfSightClear( s_deathMemory[ iMe ].killerPos + Vector( 0, 0, HumanEyeHeight ) ) )
	{
		me->GetBodyInterface()->AimHeadTowards( s_deathMemory[ iMe ].killerPos + Vector( 0, 0, HumanEyeHeight - 8.0f ),
			IBody::INTERESTING, 0.8f, NULL, "Pré-mirando onde morreu" );
		return;
	}

	CNavArea *area = me->GetLastKnownArea();
	if ( !area )
		return;

	if ( area != m_pCornerArea )
	{
		m_pCornerPrevArea = m_pCornerArea;
		m_pCornerArea = area;
		m_iCornerSpot = 0;
	}

	// Próxima área na direção do movimento.
	Vector dir = velocity;
	dir.NormalizeInPlace();
	float flAhead = 0.5f * MAX( area->GetSizeX(), area->GetSizeY() ) + 60.0f;
	CNavArea *next = TheNavMesh->GetNearestNavArea( me->GetAbsOrigin() + flAhead * dir, false, 200.0f );

	Vector lookAt;
	bool bHave = false;

	if ( m_pCornerPrevArea && next && next != area )
	{
		SpotEncounter *encounter = area->GetSpotEncounter( m_pCornerPrevArea, next );
		if ( encounter && m_iCornerSpot < encounter->spots.Count() )
		{
			HidingSpot *spot = encounter->spots[ m_iCornerSpot ].spot;
			++m_iCornerSpot;
			if ( spot )
			{
				lookAt = spot->GetPosition() + Vector( 0, 0, HumanEyeHeight - 8.0f );
				bHave = true;
			}
		}
	}

	if ( !bHave && next && next != area )
	{
		const HidingSpotVector *spots = next->GetHidingSpots();
		if ( spots && spots->Count() )
		{
			lookAt = ( *spots )[ RandomInt( 0, spots->Count() - 1 ) ]->GetPosition() + Vector( 0, 0, HumanEyeHeight - 8.0f );
			bHave = true;
		}
	}

	if ( bHave )
		me->GetBodyInterface()->AimHeadTowards( lookAt, IBody::INTERESTING, 0.6f, NULL, "Checando canto" );
}
