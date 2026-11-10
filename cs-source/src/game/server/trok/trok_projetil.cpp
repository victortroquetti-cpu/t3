//=============================================================================
// Trok: o que voa ou fica no chão depois de um explosivo do Combat Arms
// (Chimera, specs/combat_arms/grenades.md e special_weapons.md). Ver trok_explosivos.h.
//
// Voo do CA (grenades.md seções 5 e 6):
//   - é um PONTO (não é física VPhysics): atravessa vão de qualquer tamanho, ignora clip
//     brush, quica em parede, chão, porta e brush sólido;
//   - gravidade de objeto de 1600 u/s² (não a do jogador) e o "quarto de gravidade" do CA:
//     cada passo anda v·dt + ¼·g·dt² (não ½). Por isso o voo roda em passos fixos de
//     trok_explosivo_passo_ca (0,05 s = os 20 quadros/s do servidor do CA) e a posição
//     desenhada é interpolada entre os dois últimos passos: o arco, o quique e o rolar
//     ficam iguais aos do CA em qualquer tickrate;
//   - quique: espelha a velocidade e multiplica tudo pela dureza da superfície (0,65 na
//     maioria); no chão (normal > 0,75) com menos de 50 u/s na horizontal, para seco;
//     não tem atrito de rolar (rolar = quiques pequenos);
//   - corpo de jogador não para a granada: cada passo que cruza um corpo tira 35% da
//     velocidade duas vezes (caixa e hitbox do CA), sem espelhar;
//   - pavio fixo desde que saiu da mão; não explode no impacto.
// Impacto (foguete): explode no primeiro toque, 1 u antes da parede.
// Mina, remota e torreta: arremessadas a 600 u/s, escorregam em parede e grudam no
// primeiro chão. Claymore: plantada. Explosões: CTrokRules::RadiusDamage (cobertura do CA).
//=============================================================================

#include "cbase.h"
#include "basegrenade_shared.h"
#include "trok_explosivos.h"
#include "explode.h"
#include "soundent.h"
#include "ammodef.h"
#include "particle_smokegrenade.h"
#include "decals.h"
#include "gamerules.h"
#include "IEffects.h"
#include "collisionutils.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar trok_explosivo_gravidade( "trok_explosivo_gravidade", "1600", FCVAR_NOTIFY, "Gravidade de objeto do CA (u/s², x trok_ca_escala) para granadas, minas e lança-granadas." );
ConVar trok_explosivo_passo_ca( "trok_explosivo_passo_ca", "0.05", FCVAR_NOTIFY, "Passo fixo (s) do voo com gravidade: 0,05 = servidor de 20 quadros/s do CA (o arco e o rolar dependem disso). 0 = um passo por tick." );
ConVar trok_explosivo_dureza( "trok_explosivo_dureza", "0.65", FCVAR_NOTIFY, "Dureza (fração da velocidade que fica no quique) da maioria das superfícies (CA: 0,65)." );
ConVar trok_explosivo_dureza_grade( "trok_explosivo_dureza_grade", "0.5", FCVAR_NOTIFY, "Dureza de grade/tela de arame (CA: 0,5)." );
ConVar trok_explosivo_dureza_areia( "trok_explosivo_dureza_areia", "0.65", FCVAR_NOTIFY, "Dureza da superfície 'sand' (CA: areia do deserto 0,1; os mapas do CS:S usam 'sand' em quase todo chão, então fica 0,65)." );
ConVar trok_explosivo_dureza_agua( "trok_explosivo_dureza_agua", "0.1", FCVAR_NOTIFY, "Dureza de superfície de líquido/lama (CA: 0,1)." );
ConVar trok_explosivo_parada( "trok_explosivo_parada", "50", FCVAR_NOTIFY, "Velocidade horizontal (u/s) abaixo da qual a granada para ao tocar o chão (CA GrenadeMinVelMag 50)." );
ConVar trok_explosivo_chao( "trok_explosivo_chao", "0.75", FCVAR_NOTIFY, "Normal (z) acima da qual uma superfície conta como chão (CA: 0,75, ~41 graus)." );
ConVar trok_explosivo_corpo_dureza( "trok_explosivo_corpo_dureza", "0.65", FCVAR_NOTIFY, "Granada que atravessa um corpo: fração da velocidade que fica a cada toque (CA: carne 0,65)." );
ConVar trok_explosivo_corpo_toques( "trok_explosivo_corpo_toques", "2", FCVAR_NOTIFY, "Toques por passo ao cruzar um corpo (CA: 2, a caixa do personagem e a das hitboxes)." );
ConVar trok_explosivo_agua_gravidade( "trok_explosivo_agua_gravidade", "200", FCVAR_NOTIFY, "Gravidade (u/s²) dentro da água (CA: 200)." );
ConVar trok_explosivo_agua_viscosidade( "trok_explosivo_agua_viscosidade", "0.035", FCVAR_NOTIFY, "Viscosidade da água: perde 5000 x isto u/s por segundo (CA: 0,035 padrão)." );
ConVar trok_explosivo_debug( "trok_explosivo_debug", "0", 0, "1 = desenha o voo dos explosivos e escreve quiques e explosões no console." );

extern short g_sModelIndexWExplosion;	// basecombatweapon.cpp
extern ConVar trok_granada_dano;
extern ConVar trok_granada_raio;
extern ConVar trok_explosao_ca_nucleo;

//-----------------------------------------------------------------------------
// Filtro do voo: só o mapa, brushes sólidos (portas, plataformas, quebráveis) e portas
// de modelo. Gente, props soltos e armas no chão não contam (grenades.md seção 5).
//-----------------------------------------------------------------------------
class CTrokFiltroVoo : public CTraceFilterSimple
{
public:
	CTrokFiltroVoo( const IHandleEntity *pIgnore ) : CTraceFilterSimple( pIgnore, COLLISION_GROUP_NONE ) {}
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

// Inimigo para minas, torreta e clarão: vivo, não é o dono e não é do time dele.
static bool TrokExplosivo_Inimigo( CBasePlayer *pDono, CBasePlayer *pOutro )
{
	if ( !pOutro || !pOutro->IsAlive() || pOutro->IsObserver() || pOutro == pDono )
		return false;
	if ( pDono && g_pGameRules->IsTeamplay() && g_pGameRules->PlayerRelationship( pDono, pOutro ) == GR_TEAMMATE )
		return false;
	return true;
}

// Queda do CA: cheio até núcleo x raio, depois linear até 0 no raio.
static float TrokExplosivo_Queda( float d, float R )
{
	if ( R <= 0.0f || d > R )
		return 0.0f;
	const float r0 = clamp( trok_explosao_ca_nucleo.GetFloat(), 0.0f, 0.99f ) * R;
	return ( d <= r0 ) ? 1.0f : 1.0f - ( d - r0 ) / ( R - r0 );
}

//=============================================================================
// Clarão (XM84): uma entidade por vítima, mandada só para ela. O cliente
// (client/trok/c_trok_clarao.cpp) desenha a tela branca que some como cos(pi t / T).
//=============================================================================
class CTrokClarao : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokClarao, CBaseEntity );
	DECLARE_SERVERCLASS();

	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_FULLCHECK ); }
	virtual int ShouldTransmit( const CCheckTransmitInfo *pInfo )
	{
		CBasePlayer *pVitima = m_hVitima.Get();
		if ( pVitima && pInfo->m_pClientEnt == pVitima->edict() )
			return FL_EDICT_ALWAYS;
		return FL_EDICT_DONTSEND;
	}

	CNetworkVar( float, m_flInicio );		// tempo do servidor em que a tela fica branca
	CNetworkVar( float, m_flDuracao );		// T do CA (o branco some em T/2)
	CHandle< CBasePlayer > m_hVitima;
};

IMPLEMENT_SERVERCLASS_ST( CTrokClarao, DT_TrokClarao )
	SendPropFloat( SENDINFO( m_flInicio ), 0, SPROP_NOSCALE ),
	SendPropFloat( SENDINFO( m_flDuracao ), 0, SPROP_NOSCALE ),
END_SEND_TABLE()

LINK_ENTITY_TO_CLASS( trok_clarao, CTrokClarao );

static CHandle< CTrokClarao > s_hClarao[ MAX_PLAYERS + 1 ];

// Um clarão novo substitui o anterior da mesma vítima, mesmo que seja mais fraco (CA).
static void TrokClarao_Criar( CBasePlayer *pVitima, float flInicio, float flDuracao )
{
	const int idx = pVitima->entindex();
	if ( idx < 1 || idx > MAX_PLAYERS )
		return;
	if ( s_hClarao[ idx ].Get() )
		UTIL_Remove( s_hClarao[ idx ].Get() );

	CTrokClarao *pClarao = dynamic_cast< CTrokClarao * >( CBaseEntity::Create( "trok_clarao", pVitima->EyePosition(), vec3_angle, NULL ) );
	if ( !pClarao )
		return;
	pClarao->m_hVitima = pVitima;
	pClarao->m_flInicio = flInicio;
	pClarao->m_flDuracao = flDuracao;
	pClarao->SetThink( &CBaseEntity::SUB_Remove );
	pClarao->SetNextThink( flInicio + flDuracao * 0.5f + 1.0f );
	s_hClarao[ idx ] = pClarao;
}

//=============================================================================
// O objeto
//=============================================================================
class CTrokProjetil : public CBaseGrenade
{
public:
	DECLARE_CLASS( CTrokProjetil, CBaseGrenade );
	DECLARE_DATADESC();

	CTrokProjetil();

	void Configurar( const TrokExplosivoInfo_t &info, CBasePlayer *pDono, const Vector &vecVel, const QAngle &ang );
	virtual void Spawn( void );
	virtual int OnTakeDamage( const CTakeDamageInfo &info );
	virtual void Event_Killed( const CTakeDamageInfo &info ) {}
	virtual int ObjectCaps( void ) { return CBaseEntity::ObjectCaps() & ~( FCAP_IMPULSE_USE | FCAP_USE_IN_RADIUS ); }
	virtual void Detonate( void ) { Explodir(); }

	void Pensar( void );
	void MarcarDetonacao( float flAtraso );

	const TrokExplosivoInfo_t &Info( void ) const { return m_Info; }
	CBasePlayer *Dono( void ) const { return m_hDono.Get(); }

private:
	bool UsaPassoFixo( void ) const { return m_Info.bGravidade && trok_explosivo_passo_ca.GetFloat() > 0.0f; }
	void Passo( float dt );
	void Quicar( const trace_t &tr, const Vector &vecVel, const Vector &vecInicio );
	void Pousar( const trace_t &tr );
	void Corpos( const Vector &vecDe, const Vector &vecAte, Vector &vecVel );
	float Dureza( const trace_t &tr ) const;
	void SomQuique( float flVolume );
	void Pousado( void );
	void Torreta( void );
	void Explodir( void );
	void ExplosaoComum( const Vector &pos, float flDano, float flRaio );
	void ExplosaoClaymore( const Vector &pos );
	void Clarao( const Vector &pos );
	void Fumaca( const Vector &pos );
	void Sumir( void );

	TrokExplosivoInfo_t m_Info;
	CHandle< CBasePlayer > m_hDono;
	float m_flCriado;
	float m_flUltimo;
	float m_flAcumulado;
	Vector m_vecPos;			// posição do passo atual (lógica)
	Vector m_vecPosAnt;			// posição do passo anterior (para interpolar o desenho)
	Vector m_vecVel;
	float m_flDetonarEm;		// 0 = nada marcado
	float m_flVidaAtual;
	bool m_bParado;				// granada parada no chão
	bool m_bEmpurrar;			// recuo de quando para (grenades.md seção 6), no passo seguinte
	Vector m_vecDirParada;
	float m_flDurezaParada;
	bool m_bPousou;				// mina, remota, torreta ou claymore no chão
	bool m_bAtivada;			// mina pisada
	bool m_bExplodiu;
	QAngle m_angGiro;			// graus/s, só visual
	int m_iMortesDono;
	float m_flProximoAjuste;
	float m_flTorretaPronta;
	float m_flTorretaTiro;
	int m_iTorretaBalas;
};

BEGIN_DATADESC( CTrokProjetil )
	DEFINE_THINKFUNC( Pensar ),
END_DATADESC()

// Os nomes que a arma cria (campo "entidade" do script). O frag usa trok_granada.
LINK_ENTITY_TO_CLASS( trok_granada, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_granada_luz, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_granada_fumaca, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_foguete, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_mina, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_claymore, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_granada_remota, CTrokProjetil );
LINK_ENTITY_TO_CLASS( trok_torreta, CTrokProjetil );

CTrokProjetil::CTrokProjetil()
{
	Q_memset( &m_Info, 0, sizeof( m_Info ) );
	m_flCriado = m_flUltimo = m_flAcumulado = 0.0f;
	m_vecPos.Init();
	m_vecPosAnt.Init();
	m_vecVel.Init();
	m_flDetonarEm = 0.0f;
	m_flVidaAtual = 1.0f;
	m_bParado = m_bEmpurrar = m_bPousou = m_bAtivada = m_bExplodiu = false;
	m_vecDirParada.Init();
	m_flDurezaParada = 0.65f;
	m_angGiro.Init();
	m_iMortesDono = 0;
	m_flProximoAjuste = 0.0f;
	m_flTorretaPronta = m_flTorretaTiro = 0.0f;
	m_iTorretaBalas = 0;
}

static QAngle TrokExplosivo_GiroAleatorio( void )
{
	// grenades.md, Constants: pitch ±π, yaw e roll ±2π rad/s.
	return QAngle( random->RandomFloat( -180.0f, 180.0f ), random->RandomFloat( -360.0f, 360.0f ), random->RandomFloat( -360.0f, 360.0f ) );
}

void CTrokProjetil::Configurar( const TrokExplosivoInfo_t &info, CBasePlayer *pDono, const Vector &vecVel, const QAngle &ang )
{
	m_Info = info;
	m_hDono = pDono;
	m_vecVel = vecVel;
	if ( pDono )
		m_iMortesDono = pDono->DeathCount();
	if ( info.iTipo == TROK_EXP_GRANADA )
		m_angGiro = TrokExplosivo_GiroAleatorio();
}

void CTrokProjetil::Spawn( void )
{
	BaseClass::Spawn();

	if ( m_Info.szModelo[ 0 ] )
		SetModel( m_Info.szModelo );

	const float flEsc = TrokExplosivo_Escala();
	const float flLado = MAX( m_Info.vecCaixa.x, m_Info.vecCaixa.y ) * flEsc;
	const Vector vecMeia( flLado, flLado, m_Info.vecCaixa.z * flEsc );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_STANDABLE );
	SetCollisionGroup( COLLISION_GROUP_WEAPON );	// tiro acerta, jogador não esbarra
	SetSize( -vecMeia, vecMeia );
	SetMoveType( MOVETYPE_NONE );

	CBasePlayer *pDono = Dono();
	SetThrower( pDono );
	SetOwnerEntity( pDono );
	m_takedamage = DAMAGE_YES;
	m_bIsLive = true;

	m_flCriado = m_flUltimo = gpGlobals->curtime;
	m_vecPos = m_vecPosAnt = GetAbsOrigin();
	// O CA dá o primeiro passo no mesmo quadro em que cria o objeto.
	m_flAcumulado = UsaPassoFixo() ? trok_explosivo_passo_ca.GetFloat() : 0.0f;

	// Mina, remota e torreta têm 1 de vida até pousar; o resto já nasce com a do script.
	const int t = m_Info.iTipo;
	m_flVidaAtual = ( t == TROK_EXP_MINA || t == TROK_EXP_REMOTA || t == TROK_EXP_TORRETA ) ? 1.0f : m_Info.flVida;

	if ( t == TROK_EXP_CLAYMORE )
	{
		m_bPousou = true;
		m_vecVel.Init();
	}
	SetAbsVelocity( m_vecVel );

	SetThink( &CTrokProjetil::Pensar );
	SetNextThink( gpGlobals->curtime );
}

void CTrokProjetil::MarcarDetonacao( float flAtraso )
{
	const float flEm = gpGlobals->curtime + MAX( 0.0f, flAtraso );
	if ( m_flDetonarEm <= 0.0f || flEm < m_flDetonarEm )
		m_flDetonarEm = flEm;
}

// Tiro e explosão detonam (grenades.md seção 13). O crédito é de quem jogou.
int CTrokProjetil::OnTakeDamage( const CTakeDamageInfo &info )
{
	if ( m_bExplodiu || m_takedamage != DAMAGE_YES || info.GetDamage() <= 0.0f )
		return 0;
	const int t = m_Info.iTipo;
	// Mina/remota/torreta em voo: 1 de vida, mas não detonam (special_weapons.md Quirk 7).
	if ( ( t == TROK_EXP_MINA || t == TROK_EXP_REMOTA || t == TROK_EXP_TORRETA ) && !m_bPousou )
		return 0;

	float flDano = info.GetDamage();
	if ( t == TROK_EXP_TORRETA && ( info.GetDamageType() & DMG_BLAST ) )
		flDano *= 3.0f;		// a torreta toma 3x de explosão
	m_flVidaAtual -= flDano;
	if ( m_flVidaAtual > 1.0f )
		return 1;

	if ( t == TROK_EXP_TORRETA )
		Sumir();			// sem explosão (a munição dela não tem dano de área)
	else if ( t == TROK_EXP_MINA )
	{
		if ( !m_bAtivada )
		{
			m_bAtivada = true;
			MarcarDetonacao( m_Info.flMinaAtraso );
		}
	}
	else
		MarcarDetonacao( 0.0f );	// no próximo tick (não explode dentro de outra explosão)
	return 1;
}

void CTrokProjetil::Pensar( void )
{
	if ( m_bExplodiu )
		return;

	const float flAgora = gpGlobals->curtime;
	float dt = flAgora - m_flUltimo;
	m_flUltimo = flAgora;
	SetNextThink( flAgora );

	CBasePlayer *pDono = Dono();
	const int t = m_Info.iTipo;

	// Dono saiu do jogo: aparelhos somem sem explodir (special_weapons.md 1.5).
	if ( !pDono && t != TROK_EXP_GRANADA && t != TROK_EXP_IMPACTO )
	{
		UTIL_Remove( this );
		return;
	}

	if ( m_flDetonarEm > 0.0f && flAgora >= m_flDetonarEm )
	{
		Explodir();
		return;
	}

	// Pavio (granada), tempo de vida no ar (foguete) ou de existência (aparelhos).
	if ( m_Info.flPavio > 0.0f && flAgora >= m_flCriado + m_Info.flPavio )
	{
		if ( t == TROK_EXP_GRANADA || t == TROK_EXP_IMPACTO )
			Explodir();
		else
			UTIL_Remove( this );
		return;
	}

	// Claymore e remota explodem quando o dono morre (special_weapons.md 6.3).
	if ( ( t == TROK_EXP_CLAYMORE || t == TROK_EXP_REMOTA ) && pDono && pDono->DeathCount() != m_iMortesDono )
	{
		m_iMortesDono = pDono->DeathCount();
		MarcarDetonacao( 0.05f );
	}

	if ( !IsInWorld() )
	{
		UTIL_Remove( this );
		return;
	}

	// Voo
	if ( !m_bPousou )
	{
		if ( UsaPassoFixo() )
		{
			const float flPasso = trok_explosivo_passo_ca.GetFloat();
			m_flAcumulado += dt;
			int nPassos = 0;
			while ( m_flAcumulado >= flPasso && nPassos < 8 )
			{
				m_flAcumulado -= flPasso;
				++nPassos;
				m_vecPosAnt = m_vecPos;
				Passo( flPasso );
				if ( m_bExplodiu || IsMarkedForDeletion() )
					return;
			}
			if ( nPassos >= 8 )
				m_flAcumulado = 0.0f;
			// Desenho: entre os dois últimos passos (fica no máximo um passo atrás).
			const float f = clamp( m_flAcumulado / flPasso, 0.0f, 1.0f );
			SetAbsOrigin( m_bPousou ? m_vecPos : m_vecPosAnt + ( m_vecPos - m_vecPosAnt ) * f );
		}
		else if ( dt > 0.0f )
		{
			m_vecPosAnt = m_vecPos;
			Passo( dt );
			if ( m_bExplodiu || IsMarkedForDeletion() )
				return;
			SetAbsOrigin( m_vecPos );
		}
		SetAbsVelocity( m_vecVel );

		// Giro visual: os três ângulos crescem juntos (grenades.md C2); parado, congela.
		if ( !m_bParado && t == TROK_EXP_GRANADA && dt > 0.0f )
			SetAbsAngles( GetAbsAngles() + m_angGiro * dt );
	}
	else
	{
		Pousado();
	}
}

// Um passo do CA (grenades.md seção 5).
void CTrokProjetil::Passo( float dt )
{
	const float flEsc = TrokExplosivo_Escala();

	if ( m_bPousou )
		return;
	if ( m_bParado )
	{
		if ( m_bEmpurrar )
		{
			// No passo seguinte ao que parou, recua meia altura x max(0,1; dureza).
			m_bEmpurrar = false;
			m_vecPos -= m_vecDirParada * m_Info.flMeiaAltura * flEsc * MAX( 0.1f, m_flDurezaParada );
		}
		return;
	}

	Vector v = m_vecVel;
	if ( v.LengthSqr() < 0.1f )
		v.Init();

	// Água: viscosidade e gravidade de líquido (grenades.md seção 7).
	const bool bAgua = ( UTIL_PointContents( m_vecPos ) & MASK_WATER ) != 0;
	const float flVel = v.Length();
	if ( bAgua && flVel > 1.0f * flEsc )
	{
		const float flNova = MAX( 0.0f, flVel - 5000.0f * trok_explosivo_agua_viscosidade.GetFloat() * dt * flEsc );
		v *= flNova / flVel;
	}

	float g = 0.0f;
	if ( m_Info.bGravidade )
		g = ( bAgua ? trok_explosivo_agua_gravidade.GetFloat() : trok_explosivo_gravidade.GetFloat() ) * flEsc;

	const Vector vecInicio = v;
	// Δp = v·dt + ¼·g·dt² (o "quarto" do CA), v' = v + g·dt. A velocidade nova vale para o quique.
	Vector vecDelta = v * dt;
	vecDelta.z -= 0.25f * g * dt * dt;
	v.z -= g * dt;

	const Vector vecDe = m_vecPos;
	const Vector vecAte = m_vecPos + vecDelta;
	trace_t tr;
	if ( m_Info.iTipo == TROK_EXP_IMPACTO )
	{
		// Foguete: qualquer coisa sólida ou hitbox, menos quem atirou.
		CTraceFilterSkipTwoEntities filter( this, Dono(), COLLISION_GROUP_NONE );
		UTIL_TraceLine( vecDe, vecAte, MASK_SHOT, &filter, &tr );
	}
	else
	{
		CTrokFiltroVoo filter( this );
		UTIL_TraceLine( vecDe, vecAte, MASK_SOLID, &filter, &tr );
	}

	if ( trok_explosivo_debug.GetBool() )
		NDebugOverlay::Line( vecDe, tr.endpos, 255, 128, 0, true, 3.0f );

	// Céu: some sem explodir.
	if ( tr.fraction < 1.0f && ( tr.surface.flags & SURF_SKY ) )
	{
		UTIL_Remove( this );
		return;
	}

	// Granada atravessa corpos perdendo velocidade.
	if ( m_Info.iTipo == TROK_EXP_GRANADA )
		Corpos( vecDe, tr.endpos, v );

	if ( tr.fraction >= 1.0f && !tr.startsolid )
	{
		m_vecPos = vecAte;
		m_vecVel = v;
		return;
	}

	switch ( m_Info.iTipo )
	{
	case TROK_EXP_IMPACTO:
	{
		// Parede: 1 u antes, pela direção do voo. Corpo: no ponto acertado.
		Vector vecDir = v;
		VectorNormalize( vecDir );
		CBaseEntity *pHit = tr.m_pEnt;
		const bool bCorpo = pHit && pHit->MyCombatCharacterPointer();
		m_vecPos = bCorpo ? tr.endpos : tr.endpos - vecDir * 1.0f * flEsc;
		m_vecVel = v;
		Explodir();
		return;
	}
	case TROK_EXP_GRANADA:
		Quicar( tr, v, vecInicio );
		return;
	default:
		// Aparelhos: grudam no primeiro chão; em parede só perdem a velocidade contra ela.
		if ( tr.plane.normal.z >= trok_explosivo_chao.GetFloat() )
		{
			Pousar( tr );
		}
		else
		{
			const Vector n = tr.plane.normal;
			v -= n * DotProduct( v, n );
			m_vecPos = tr.endpos + n * 0.1f;
			m_vecVel = v;
		}
		return;
	}
}

float CTrokProjetil::Dureza( const trace_t &tr ) const
{
	const surfacedata_t *pSurf = physprops->GetSurfaceData( tr.surface.surfaceProps );
	const char cMat = pSurf ? pSurf->game.material : CHAR_TEX_CONCRETE;
	float h = trok_explosivo_dureza.GetFloat();
	switch ( cMat )
	{
	case CHAR_TEX_GRATE:	h = trok_explosivo_dureza_grade.GetFloat(); break;
	case CHAR_TEX_SAND:		h = trok_explosivo_dureza_areia.GetFloat(); break;
	case CHAR_TEX_SLOSH:	h = trok_explosivo_dureza_agua.GetFloat(); break;
	}
	return clamp( h, 0.0f, 1.0f );
}

// grenades.md seção 6: v'' = h (v' - 2(v'·n)n - 0,1 n); para seco no chão se lento na horizontal.
void CTrokProjetil::Quicar( const trace_t &tr, const Vector &vecVel, const Vector &vecInicio )
{
	const float flEsc = TrokExplosivo_Escala();
	const Vector n = tr.plane.normal;
	const float h = Dureza( tr );
	Vector v = ( vecVel - n * ( 2.0f * DotProduct( vecVel, n ) ) - n * ( 0.1f * flEsc ) ) * h;

	m_vecPos = tr.endpos + n * 0.1f;
	m_angGiro = TrokExplosivo_GiroAleatorio();

	const float flVolume = MIN( 1.0f, v.Length() * h / ( 200.0f * flEsc ) );
	SomQuique( flVolume );

	if ( n.z > trok_explosivo_chao.GetFloat() && v.Length2D() < trok_explosivo_parada.GetFloat() * flEsc )
	{
		v.Init();
		m_bParado = true;
		m_bEmpurrar = true;
		m_vecDirParada = vecInicio;
		VectorNormalize( m_vecDirParada );
		m_flDurezaParada = h;
		CSoundEnt::InsertSound( SOUND_DANGER, m_vecPos, 400, 1.0f, this );
	}
	m_vecVel = v;

	if ( trok_explosivo_debug.GetBool() )
		Msg( "Trok explosivo: quique h=%.2f v=%.0f%s\n", h, v.Length(), m_bParado ? " (parou)" : "" );
}

void CTrokProjetil::SomQuique( float flVolume )
{
	if ( !m_Info.szSomQuique[ 0 ] || flVolume < 0.05f )
		return;
	// Volume menor nos quiques fracos (CA: |v|·h/200 quando < 1).
	CSoundParameters params;
	if ( !GetParametersForSound( m_Info.szSomQuique, params, NULL ) )
		return;
	CPASAttenuationFilter filter( m_vecPos, params.soundlevel );
	EmitSound_t ep( params );
	ep.m_flVolume = params.volume * flVolume;
	ep.m_pOrigin = &m_vecPos;
	EmitSound( filter, entindex(), ep );
}

// Granada cruzando um corpo: perde velocidade (sem espelhar) a cada toque.
void CTrokProjetil::Corpos( const Vector &vecDe, const Vector &vecAte, Vector &vecVel )
{
	if ( ( vecAte - vecDe ).LengthSqr() < 0.01f )
		return;
	const float h = clamp( trok_explosivo_corpo_dureza.GetFloat(), 0.0f, 1.0f );
	const int nToques = MAX( 0, trok_explosivo_corpo_toques.GetInt() );
	CBasePlayer *pDono = Dono();

	Ray_t ray;
	ray.Init( vecDe, vecAte );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer || pPlayer == pDono || !pPlayer->IsAlive() )
			continue;
		trace_t trCorpo;
		enginetrace->ClipRayToEntity( ray, MASK_SHOT, pPlayer, &trCorpo );
		if ( trCorpo.fraction < 1.0f )
		{
			for ( int k = 0; k < nToques; ++k )
				vecVel *= h;
		}
	}
}

void CTrokProjetil::Pousar( const trace_t &tr )
{
	m_bPousou = true;
	m_vecVel.Init();
	m_vecPos = tr.endpos + tr.plane.normal * 0.5f;
	SetAbsOrigin( m_vecPos );
	SetAbsVelocity( vec3_origin );
	// De pé, virado para onde foi jogado.
	QAngle ang( 0, GetAbsAngles().y, 0 );
	SetAbsAngles( ang );
	m_flVidaAtual = m_Info.flVida;
	m_flProximoAjuste = gpGlobals->curtime + 0.1f;

	if ( m_Info.iTipo == TROK_EXP_TORRETA )
	{
		m_flTorretaPronta = gpGlobals->curtime + m_Info.flTorretaPreparo;
		m_iTorretaBalas = m_Info.iTorretaBalas;
		if ( m_Info.szSomAtivo[ 0 ] )
			EmitSound( m_Info.szSomAtivo );
	}
}

// No chão: gruda no chão de baixo a cada 0,1 s, mina vigia, torreta atira.
void CTrokProjetil::Pousado( void )
{
	const float flAgora = gpGlobals->curtime;
	CBasePlayer *pDono = Dono();

	if ( flAgora >= m_flProximoAjuste && m_Info.iTipo != TROK_EXP_CLAYMORE )
	{
		m_flProximoAjuste = flAgora + 0.1f;
		CTrokFiltroVoo filter( this );
		trace_t tr;
		UTIL_TraceLine( m_vecPos + Vector( 0, 0, 2 ), m_vecPos - Vector( 0, 0, 8192 ), MASK_SOLID, &filter, &tr );
		if ( tr.fraction < 1.0f && !tr.startsolid && !( tr.surface.flags & SURF_SKY ) )
		{
			m_vecPos = tr.endpos + Vector( 0, 0, 0.5f );
			SetAbsOrigin( m_vecPos );
		}
	}

	if ( m_Info.iTipo == TROK_EXP_MINA && !m_bAtivada )
	{
		// Inimigo cuja caixa encosta na caixa da mina (special_weapons.md 6.1).
		const Vector vecMin = GetAbsOrigin() + CollisionProp()->OBBMins();
		const Vector vecMax = GetAbsOrigin() + CollisionProp()->OBBMaxs();
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !TrokExplosivo_Inimigo( pDono, pPlayer ) )
				continue;
			const Vector pMin = pPlayer->GetAbsOrigin() + pPlayer->CollisionProp()->OBBMins();
			const Vector pMax = pPlayer->GetAbsOrigin() + pPlayer->CollisionProp()->OBBMaxs();
			if ( IsBoxIntersectingBox( vecMin, vecMax, pMin, pMax ) )
			{
				m_bAtivada = true;
				MarcarDetonacao( m_Info.flMinaAtraso );
				if ( m_Info.szSomAtivo[ 0 ] )
					EmitSound( m_Info.szSomAtivo );
				break;
			}
		}
	}
	else if ( m_Info.iTipo == TROK_EXP_TORRETA )
	{
		Torreta();
	}
}

// Torreta automática (special_weapons.md 6.7). O CA escolhe o alvo no cliente do dono;
// aqui o servidor escolhe o mais perto que o sensor vê, dentro do ângulo de tiro.
void CTrokProjetil::Torreta( void )
{
	const float flAgora = gpGlobals->curtime;
	if ( flAgora < m_flTorretaPronta || flAgora < m_flTorretaTiro )
		return;

	const float flEsc = TrokExplosivo_Escala();
	CBasePlayer *pDono = Dono();
	const Vector vecSensor = GetAbsOrigin() + Vector( 0, 0, m_Info.flTorretaSensor * flEsc );
	const float flAlcance = m_Info.flTorretaAlcance * flEsc;

	CBasePlayer *pAlvo = NULL;
	float flMelhor = FLT_MAX;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !TrokExplosivo_Inimigo( pDono, pPlayer ) )
			continue;
		const Vector vecCentro = pPlayer->WorldSpaceCenter();
		const Vector vecDir = vecCentro - vecSensor;
		const float d = vecDir.Length();
		if ( d > flAlcance || d >= flMelhor )
			continue;
		const float flElev = RAD2DEG( atan2f( vecDir.z, vecDir.Length2D() ) );
		if ( flElev > m_Info.flTorretaAngCima || flElev < -m_Info.flTorretaAngBaixo )
			continue;
		// Centro, e meia altura acima e abaixo.
		const float flMeia = ( pPlayer->CollisionProp()->OBBMaxs().z - pPlayer->CollisionProp()->OBBMins().z ) * 0.5f;
		const Vector pontos[ 3 ] = { vecCentro, vecCentro + Vector( 0, 0, flMeia * 0.9f ), vecCentro - Vector( 0, 0, flMeia * 0.9f ) };
		bool bVe = false;
		for ( int k = 0; k < 3 && !bVe; ++k )
		{
			trace_t tr;
			UTIL_TraceLine( vecSensor, pontos[ k ], MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
			bVe = ( tr.fraction >= 1.0f || tr.m_pEnt == pPlayer );
		}
		if ( !bVe )
			continue;
		pAlvo = pPlayer;
		flMelhor = d;
	}
	if ( !pAlvo )
		return;

	m_flTorretaTiro = flAgora + m_Info.flTorretaIntervalo;

	// Erro de ±erro u em cada eixo no ponto mirado (o mesmo a qualquer distância).
	const float flErro = m_Info.flTorretaErro * flEsc;
	Vector vecMira = pAlvo->WorldSpaceCenter() + Vector( random->RandomFloat( -flErro, flErro ), random->RandomFloat( -flErro, flErro ), random->RandomFloat( -flErro, flErro ) );
	Vector vecDir = vecMira - vecSensor;
	VectorNormalize( vecDir );

	QAngle ang;
	VectorAngles( vecDir, ang );
	SetAbsAngles( QAngle( 0, ang.y, 0 ) );

	// Faixas do Turret-TopGun: 1,0 até 800 u, 0,7 até 900, 0,35 até 1000.
	float flDano = m_Info.flTorretaBalaDano;
	if ( flMelhor > 900.0f * flEsc )
		flDano *= 0.35f;
	else if ( flMelhor > 800.0f * flEsc )
		flDano *= 0.7f;

	FireBulletsInfo_t bala;
	bala.m_iShots = 1;
	bala.m_vecSrc = vecSensor;
	bala.m_vecDirShooting = vecDir;
	bala.m_vecSpread = vec3_origin;
	bala.m_flDistance = 1000.0f * flEsc;
	bala.m_iAmmoType = GetAmmoDef()->Index( "SMG1" );
	bala.m_iTracerFreq = 1;
	bala.m_flDamage = flDano;
	bala.m_iPlayerDamage = (int)flDano;
	bala.m_pAttacker = pDono ? (CBaseEntity *)pDono : (CBaseEntity *)this;
	FireBullets( bala );
	if ( m_Info.szSomTiro[ 0 ] )
		EmitSound( m_Info.szSomTiro );

	if ( --m_iTorretaBalas <= 0 )
		Sumir();
}

void CTrokProjetil::Sumir( void )
{
	if ( m_bExplodiu )
		return;
	m_bExplodiu = true;
	m_takedamage = DAMAGE_NO;
	g_pEffects->Sparks( GetAbsOrigin() + Vector( 0, 0, 8 ) );
	UTIL_Remove( this );
}

void CTrokProjetil::Explodir( void )
{
	if ( m_bExplodiu )
		return;
	m_bExplodiu = true;
	m_takedamage = DAMAGE_NO;

	if ( !m_bPousou )
		SetAbsOrigin( m_vecPos );
	const Vector pos = GetAbsOrigin();

	if ( trok_explosivo_debug.GetBool() )
		Msg( "Trok explosivo: %s explodiu em %.0f %.0f %.0f (%.2f s)\n", GetClassname(), pos.x, pos.y, pos.z, gpGlobals->curtime - m_flCriado );

	if ( m_Info.iTipo == TROK_EXP_GRANADA && m_Info.iEfeito == TROK_EFEITO_LUZ )
		Clarao( pos );
	else if ( m_Info.iTipo == TROK_EXP_GRANADA && m_Info.iEfeito == TROK_EFEITO_FUMACA )
		Fumaca( pos );
	else if ( m_Info.iTipo == TROK_EXP_CLAYMORE )
		ExplosaoClaymore( pos );
	else
	{
		float flDano = m_Info.flDano, flRaio = m_Info.flRaio;
		// M67: os números ficam em trok_granada_dano/raio (balanceamento.cfg).
		if ( flDano <= 0.0f && flRaio <= 0.0f && m_Info.iTipo == TROK_EXP_GRANADA )
		{
			flDano = trok_granada_dano.GetFloat();
			flRaio = trok_granada_raio.GetFloat();
		}
		ExplosaoComum( pos, flDano, flRaio );
	}

	// Some agora; o som e o efeito já foram no ponto.
	SetSolid( SOLID_NONE );
	AddEffects( EF_NODRAW );
	SetAbsVelocity( vec3_origin );
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CTrokProjetil::ExplosaoComum( const Vector &pos, float flDano, float flRaio )
{
	const float R = flRaio * TrokExplosivo_Escala();

	trace_t tr;
	UTIL_TraceLine( pos + Vector( 0, 0, 8 ), pos - Vector( 0, 0, 32 ), MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	const bool bAgua = ( UTIL_PointContents( pos ) & MASK_WATER ) != 0;
	const surfacedata_t *pSurf = tr.fraction < 1.0f ? physprops->GetSurfaceData( tr.surface.surfaceProps ) : NULL;
	const Vector vecNormal = tr.fraction < 1.0f ? tr.plane.normal : Vector( 0, 0, 1 );

	CPASFilter filter( pos );
	te->Explosion( filter, -1.0f, &pos, bAgua ? g_sModelIndexWExplosion : g_sModelIndexFireball,
		clamp( R * 0.03f, 3.0f, 20.0f ), 25, m_Info.szSomExplosao[ 0 ] ? TE_EXPLFLAG_NOSOUND : TE_EXPLFLAG_NONE,
		(int)R, (int)flDano, &vecNormal, pSurf ? (char)pSurf->game.material : 'C' );
	if ( m_Info.szSomExplosao[ 0 ] )
	{
		CPASAttenuationFilter filterSom( pos, m_Info.szSomExplosao );
		EmitSound( filterSom, entindex(), m_Info.szSomExplosao, &pos );
	}
	if ( tr.fraction < 1.0f )
		UTIL_DecalTrace( &tr, "Scorch" );
	UTIL_ScreenShake( pos, 25.0f, 150.0f, 1.0f, R * 1.5f, SHAKE_START );
	CSoundEnt::InsertSound( SOUND_COMBAT, pos, BASEGRENADE_EXPLOSION_VOLUME, 3.0f );

	if ( flDano <= 0.0f || R <= 0.0f )
		return;		// AT4 do CA: explode e não faz nada

	CBaseEntity *pAtacante = Dono() ? (CBaseEntity *)Dono() : (CBaseEntity *)this;
	CTakeDamageInfo info( this, pAtacante, vec3_origin, pos, flDano, DMG_BLAST );
	g_pGameRules->RadiusDamage( info, pos, R, CLASS_NONE, NULL );
}

// Claymore (special_weapons.md 6.3): cone de 160 graus na frente; perto, para todo lado.
void CTrokProjetil::ExplosaoClaymore( const Vector &pos )
{
	const float flEsc = TrokExplosivo_Escala();
	const float Rc = m_Info.flConeRaio * flEsc;
	const float Rp = m_Info.flRaio * flEsc;
	const float flCos = cosf( DEG2RAD( m_Info.flConeAngulo ) );
	Vector vecFrente;
	AngleVectors( QAngle( 0, GetAbsAngles().y, 0 ), &vecFrente );
	const Vector src = pos + Vector( 0, 0, 2.0f * flEsc );

	ExplosaoComum( pos, 0.0f, MAX( Rc, Rp ) );	// só o efeito e o som

	CBaseEntity *pAtacante = Dono() ? (CBaseEntity *)Dono() : (CBaseEntity *)this;
	CBaseEntity *pEnt = NULL;
	for ( CEntitySphereQuery sphere( src, MAX( Rc, Rp ) ); ( pEnt = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( pEnt == this || pEnt->m_takedamage == DAMAGE_NO )
			continue;
		const Vector vecCentro = pEnt->WorldSpaceCenter();
		Vector u = vecCentro - src;
		const float d = u.NormalizeInPlace();

		float flDano = 0.0f;
		if ( d <= Rc && DotProduct( vecFrente, u ) > flCos )
			flDano = m_Info.flConeDano * TrokExplosivo_Queda( d, Rc );
		else if ( d <= Rp )
			flDano = m_Info.flDano * TrokExplosivo_Queda( d, Rp );
		if ( flDano <= 0.0f )
			continue;

		if ( pEnt->MyCombatCharacterPointer() )
		{
			const Vector vecDe = src + u * 1.0f;
			if ( TrokExplosaoBloqueada( vecDe, vecCentro, this ) )
			{
				Vector vecCabeca = vecCentro;
				vecCabeca.z = pEnt->GetAbsOrigin().z + pEnt->CollisionProp()->OBBMaxs().z - 2.0f;
				if ( TrokExplosaoBloqueada( vecDe, vecCabeca, this ) )
					continue;
			}
		}

		CTakeDamageInfo info( this, pAtacante, flDano, DMG_BLAST );
		CalculateExplosiveDamageForce( &info, u, src );
		pEnt->TakeDamage( info );
	}
}

// XM84 (grenades.md seção 10): cega quem vê a explosão; de costas, um quarto do tempo.
void CTrokProjetil::Clarao( const Vector &pos )
{
	const float flEsc = TrokExplosivo_Escala();
	const float R = m_Info.flRaio * flEsc;

	if ( m_Info.szSomExplosao[ 0 ] )
	{
		CPASAttenuationFilter filterSom( pos, m_Info.szSomExplosao );
		EmitSound( filterSom, entindex(), m_Info.szSomExplosao, &pos );
	}
	CPVSFilter filterLuz( pos );
	te->DynamicLight( filterLuz, 0.0f, &pos, 255, 255, 255, 2, 400, 0.1f, 768 );

	static ConVarRef mp_friendlyfire( "mp_friendlyfire" );
	CBasePlayer *pDono = Dono();

	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		CBasePlayer *pVitima = UTIL_PlayerByIndex( i );
		if ( !pVitima || !pVitima->IsAlive() || pVitima->IsObserver() )
			continue;
		// Fogo amigo desligado: o time não fica cego; quem jogou sempre fica.
		if ( pDono && pVitima != pDono && g_pGameRules->IsTeamplay() &&
			g_pGameRules->PlayerRelationship( pDono, pVitima ) == GR_TEAMMATE && !mp_friendlyfire.GetBool() )
			continue;

		const Vector vecCentro = pVitima->WorldSpaceCenter();
		const float d = ( vecCentro - pos ).Length();
		if ( d > R )
			continue;
		if ( TrokExplosaoBloqueada( pos, vecCentro, this ) )
		{
			Vector vecCabeca = vecCentro;
			vecCabeca.z = pVitima->GetAbsOrigin().z + pVitima->CollisionProp()->OBBMaxs().z - 2.0f;
			if ( TrokExplosaoBloqueada( pos, vecCabeca, this ) )
				continue;
		}

		const float D = m_Info.flLuzTempo * TrokExplosivo_Queda( d, R );
		if ( D <= 0.001f )
			continue;

		// De frente ou de costas: só o giro do corpo conta (olhar para cima não muda nada).
		Vector vecFrente;
		AngleVectors( QAngle( 0, pVitima->EyeAngles().y, 0 ), &vecFrente );
		Vector u = vecCentro - pos;
		u.z = 0.0f;
		const bool bDeFrente = DotProduct( vecFrente, u ) < 0.0f;
		const float T = bDeFrente ? D : D * m_Info.flLuzCostas;
		const float flAtraso = bDeFrente ? m_Info.flLuzAtraso : m_Info.flLuzAtrasoCostas;
		if ( T < m_Info.flLuzMinimo )
			continue;

		TrokClarao_Criar( pVitima, gpGlobals->curtime + flAtraso, T );
		if ( trok_explosivo_debug.GetBool() )
			Msg( "Trok clarão: %s %.1f s (%s, %.0f u)\n", pVitima->GetPlayerName(), T, bDeFrente ? "de frente" : "de costas", d );
	}
}

// M18 (grenades.md seção 11): só a nuvem; não bloqueia tiro nem bot (como no CA).
void CTrokProjetil::Fumaca( const Vector &pos )
{
	if ( m_Info.szSomExplosao[ 0 ] )
	{
		CPASAttenuationFilter filterSom( pos, m_Info.szSomExplosao );
		EmitSound( filterSom, entindex(), m_Info.szSomExplosao, &pos );
	}
	ParticleSmokeGrenade *pFumaca = dynamic_cast< ParticleSmokeGrenade * >( CBaseEntity::Create( PARTICLESMOKEGRENADE_ENTITYNAME, pos, vec3_angle ) );
	if ( pFumaca )
	{
		pFumaca->FillVolume();
		pFumaca->SetFadeTime( m_Info.flFumacaSome, m_Info.flFumacaFim );
		pFumaca->SetThink( &CBaseEntity::SUB_Remove );
		pFumaca->SetNextThink( gpGlobals->curtime + m_Info.flFumacaFim + 1.0f );
	}
}

//-----------------------------------------------------------------------------
// Para a arma
//-----------------------------------------------------------------------------
CBaseEntity *TrokExplosivo_Criar( const TrokExplosivoInfo_t &info, CBasePlayer *pDono, const Vector &vecOrigem, const Vector &vecVel, const QAngle &angInicial )
{
	// Um por vez (torreta): o anterior do mesmo dono some.
	if ( info.bUmPorVez && pDono )
	{
		CBaseEntity *pVelho = NULL;
		while ( ( pVelho = gEntList.FindEntityByClassname( pVelho, info.szEntidade ) ) != NULL )
		{
			CTrokProjetil *pProj = dynamic_cast< CTrokProjetil * >( pVelho );
			if ( pProj && pProj->Dono() == pDono )
				UTIL_Remove( pProj );
		}
	}

	CTrokProjetil *pProj = dynamic_cast< CTrokProjetil * >( CBaseEntity::CreateNoSpawn( info.szEntidade, vecOrigem, angInicial, pDono ) );
	if ( !pProj )
	{
		Warning( "Trok: não consegui criar '%s' (campo \"entidade\" de scripts/%s.txt).\n", info.szEntidade, info.szArma );
		return NULL;
	}
	pProj->Configurar( info, pDono, vecVel, angInicial );
	DispatchSpawn( pProj );
	return pProj;
}

int TrokExplosivo_Detonar( CBasePlayer *pDono, const char *pszArma, float flAtraso )
{
	const TrokExplosivoInfo_t &info = TrokExplosivo_Info( pszArma );
	int n = 0;
	CBaseEntity *pEnt = NULL;
	while ( ( pEnt = gEntList.FindEntityByClassname( pEnt, info.szEntidade ) ) != NULL )
	{
		CTrokProjetil *pProj = dynamic_cast< CTrokProjetil * >( pEnt );
		if ( pProj && pProj->Dono() == pDono && !Q_stricmp( pProj->Info().szArma, pszArma ) )
		{
			pProj->MarcarDetonacao( flAtraso );
			++n;
		}
	}
	return n;
}
