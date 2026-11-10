//=============================================================================
// Trok: HUD principal (concept "HUD em jogo" e "Tela de morte", design/ui.md).
//
// Um painel de tela cheia desenha tudo:
// - vida (número + barra) e fôlego da corrida, embaixo à esquerda;
// - munição (pente grande / reserva, uma marca por bala, nome e modo de tiro), à direita;
// - placar e tempo no topo (cores relativas: seu time azul, o outro vermelho);
// - lista de abates no canto superior direito, com marca de tiro na cabeça;
// - mira em cruz que abre com a dispersão da arma e some mirando pelo cano ou correndo;
// - aviso de abate perto da mira;
// - tela de morte: quem matou, arma, distância, dano trocado, como renascer.
// O HUD do HL2 (vida, colete, munição, energia, mortes, mira) sai por #ifndef TROK
// nos DECLARE_HUDELEMENT (patches/sdk.patch). O marcador de acerto continua em
// hud_trok_hitmarker.cpp.
// Dano por parte do corpo vem do evento "trok_dano" (server/player.cpp, resource/modevents.res).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "weapon_parse.h"
#include "trok/weapon_trok_gun.h"
#include "trok/trok_surto.h"
#include "trok/trok_hordas.h"
#include "trok/hud_trok_v2.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>
#include <wctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hud( "trok_hud", "1", FCVAR_ARCHIVE, "Mostra o HUD do Trok." );
static ConVar trok_hud_mira( "trok_hud_mira", "1", FCVAR_ARCHIVE, "Mira em cruz do HUD (0 = sem mira)." );
static ConVar trok_hud_radar( "trok_hud_radar", "1", FCVAR_ARCHIVE, "Radar no canto superior esquerdo (aliados sempre; inimigos quando vistos ou atirando)." );
static ConVar trok_hud_radar_alcance( "trok_hud_radar_alcance", "1600", FCVAR_ARCHIVE, "Alcance do radar (u) do centro até a borda." );
static ConVar trok_hud_vida_baixa( "trok_hud_vida_baixa", "0.35", FCVAR_ARCHIVE, "Abaixo desta fração da vida a tela avermelha nas bordas (0 = desliga)." );
static ConVar trok_hud_killfeed_tempo( "trok_hud_killfeed_tempo", "6", FCVAR_ARCHIVE, "Segundos que cada abate fica na lista." );

static const Color COR_BRANCO( 236, 238, 241, 255 );
static const Color COR_CINZA( 190, 196, 204, 200 );
static const Color COR_ALIADO( 90, 162, 255, 255 );
static const Color COR_INIMIGO( 255, 90, 90, 255 );
static const Color COR_DESTAQUE_PADRAO( 242, 181, 68, 255 );
static const Color COR_FUNDO( 11, 13, 16, 255 );
static const Color COR_ZUMBI( 217, 54, 43, 255 );	// infecção = vermelho (decisão do dono)

static const float UNIDADES_POR_METRO = 52.49f;
static const int MAX_KILLFEED = 5;

static Color WithAlpha( const Color &c, int a ) { return Color( c.r(), c.g(), c.b(), clamp( a, 0, 255 ) ); }

class CHudTrok : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrok, Panel );
public:
	CHudTrok( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void ) { Reset(); }
	virtual void Reset( void );
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );
	void MsgFunc_Damage( bf_read &msg );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * m_flScale + 0.5f ); }
	Color Accent( void ) const;
	Color RelColor( int iPlayerIndex ) const;
	void Text( HFont hFont, int x, int y, const Color &col, const wchar_t *pwsz );
	void Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 );
	void TextRight( HFont hFont, int xRight, int y, const Color &col, const char *pszUTF8 );
	int TextWide( HFont hFont, const char *pszUTF8 );
	void Bar( int x, int y, int w, int h, float flFrac, const Color &col, bool bFromRight = false );
	void WeaponDisplayName( const char *pszClassOrEventName, char *pszOut, int iOutSize );

	void PaintTop( void );
	void PaintSurtoTop( void );
	void PaintKillfeed( void );
	void PaintHealth( C_BasePlayer *pLocal );
	void PaintAmmo( C_BasePlayer *pLocal );
	void PaintCrosshair( C_BasePlayer *pLocal );
	void PaintKillPopup( void );
	void PaintDeath( C_BasePlayer *pLocal );
	void PaintDamageDir( C_BasePlayer *pLocal );
	void PaintVignette( C_BasePlayer *pLocal );
	void PaintRadar( C_BasePlayer *pLocal );
	void UpdateSpotted( C_BasePlayer *pLocal );
	bool IsAlly( C_BasePlayer *pLocal, int iPlayer ) const;

	HFont m_hNum, m_hScore, m_hText, m_hSmall;
	float m_flScale;

	struct KillEntry_t
	{
		char szAtk[ 64 ];
		char szVic[ 64 ];
		char szWpn[ 48 ];
		int iAtk, iVic;
		bool bHead;
		float flTime;
	};
	CUtlVector< KillEntry_t > m_Kills;

	// Tiro na cabeça que matou (o "trok_dano" chega antes do "player_death").
	int m_iHeadVictim;
	float m_flHeadTime;

	// Aviso de abate
	float m_flPopupTime;
	char m_szPopupName[ 64 ];
	bool m_bPopupHead;

	// Dano trocado nesta vida (índice do outro jogador).
	int m_iDealt[ MAX_PLAYERS + 1 ], m_iDealtHits[ MAX_PLAYERS + 1 ];
	int m_iTaken[ MAX_PLAYERS + 1 ], m_iTakenHits[ MAX_PLAYERS + 1 ], m_iTakenHead[ MAX_PLAYERS + 1 ];
	bool m_bWasAlive;

	// Tela de morte
	bool m_bDeathValid;
	int m_iKiller;
	char m_szKiller[ 64 ];
	char m_szKillerWpn[ 48 ];
	float m_flKillerDist;
	float m_flDeathTime;

	// Direção do dano (mensagem "Damage" do servidor: de onde veio)
	struct DmgMark_t
	{
		Vector vecFrom;
		float flTime;
	};
	DmgMark_t m_Dmg[ 4 ];
	int m_iDmgNext;

	// Radar: até quando cada inimigo aparece
	float m_flSpotted[ MAX_PLAYERS + 1 ];
	float m_flNextSpotCheck;
};

DECLARE_HUDELEMENT( CHudTrok );
DECLARE_HUD_MESSAGE( CHudTrok, Damage );

CHudTrok::CHudTrok( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrok" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	SetHiddenBits( 0 );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	m_hNum = m_hScore = m_hText = m_hSmall = INVALID_FONT;
	m_flScale = 1.0f;
	Reset();
}

void CHudTrok::Init( void )
{
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "trok_dano" );
	HOOK_HUD_MESSAGE( CHudTrok, Damage );
	Reset();
}

void CHudTrok::Reset( void )
{
	m_Kills.RemoveAll();
	m_iHeadVictim = 0;
	m_flHeadTime = -100.0f;
	m_flPopupTime = -100.0f;
	m_szPopupName[ 0 ] = 0;
	m_bPopupHead = false;
	V_memset( m_iDealt, 0, sizeof( m_iDealt ) );
	V_memset( m_iDealtHits, 0, sizeof( m_iDealtHits ) );
	V_memset( m_iTaken, 0, sizeof( m_iTaken ) );
	V_memset( m_iTakenHits, 0, sizeof( m_iTakenHits ) );
	V_memset( m_iTakenHead, 0, sizeof( m_iTakenHead ) );
	m_bWasAlive = false;
	m_bDeathValid = false;
	m_iKiller = 0;
	m_szKiller[ 0 ] = 0;
	m_szKillerWpn[ 0 ] = 0;
	m_flKillerDist = -1.0f;
	m_flDeathTime = 0.0f;
	for ( int i = 0; i < ARRAYSIZE( m_Dmg ); ++i )
		m_Dmg[ i ].flTime = -100.0f;
	m_iDmgNext = 0;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
		m_flSpotted[ i ] = 0.0f;
	m_flNextSpotCheck = 0.0f;
}

void CHudTrok::MsgFunc_Damage( bf_read &msg )
{
	msg.ReadByte();		// colete
	msg.ReadByte();		// dano
	msg.ReadLong();		// tipos de dano
	Vector vecFrom;
	vecFrom.x = msg.ReadFloat();
	vecFrom.y = msg.ReadFloat();
	vecFrom.z = msg.ReadFloat();
	if ( vecFrom == vec3_origin )
		return; // queda e dano sem origem: só a vinheta
	m_Dmg[ m_iDmgNext ].vecFrom = vecFrom;
	m_Dmg[ m_iDmgNext ].flTime = gpGlobals->curtime;
	m_iDmgNext = ( m_iDmgNext + 1 ) % ARRAYSIZE( m_Dmg );
}

void CHudTrok::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hNum = pScheme->GetFont( "TrokHudNumber", true );
	m_hScore = pScheme->GetFont( "TrokHudScore", true );
	m_hText = pScheme->GetFont( "TrokHudText", true );
	m_hSmall = pScheme->GetFont( "TrokHudSmall", true );
	SetPaintBackgroundEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
}

bool CHudTrok::ShouldDraw( void )
{
	if ( !trok_hud.GetBool() || engine->IsLevelMainMenuBackground() )
		return false;
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;
	return CHudElement::ShouldDraw();
}

void CHudTrok::OnThink( void )
{
	if ( GetWide() != ScreenWidth() || GetTall() != ScreenHeight() )
		SetSize( ScreenWidth(), ScreenHeight() );
	m_flScale = ScreenHeight() / 720.0f;

	// Nasceu de novo: zera o dano trocado e a tela de morte.
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const bool bAlive = pLocal && pLocal->IsAlive();
	if ( bAlive && !m_bWasAlive )
	{
		V_memset( m_iDealt, 0, sizeof( m_iDealt ) );
		V_memset( m_iDealtHits, 0, sizeof( m_iDealtHits ) );
		V_memset( m_iTaken, 0, sizeof( m_iTaken ) );
		V_memset( m_iTakenHits, 0, sizeof( m_iTakenHits ) );
		V_memset( m_iTakenHead, 0, sizeof( m_iTakenHead ) );
		m_bDeathValid = false;
	}
	m_bWasAlive = bAlive;

	// Tira da lista os abates velhos.
	const float flLife = trok_hud_killfeed_tempo.GetFloat();
	for ( int i = m_Kills.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime - m_Kills[ i ].flTime > flLife || gpGlobals->curtime < m_Kills[ i ].flTime )
			m_Kills.Remove( i );
	}
}

Color CHudTrok::Accent( void ) const
{
	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = COR_DESTAQUE_PADRAO.r(), g = COR_DESTAQUE_PADRAO.g(), b = COR_DESTAQUE_PADRAO.b();
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
}

// Cor relativa: o seu time (ou você, sem times) azul; o resto vermelho.
Color CHudTrok::RelColor( int iPlayerIndex ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !g_PR || iPlayerIndex <= 0 )
		return COR_BRANCO;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
		return g_PR->GetTeam( iPlayerIndex ) == pLocal->GetTeamNumber() ? COR_ALIADO : COR_INIMIGO;
	return iPlayerIndex == pLocal->entindex() ? COR_ALIADO : COR_INIMIGO;
}

//-----------------------------------------------------------------------------
// Desenho básico
//-----------------------------------------------------------------------------
void CHudTrok::Text( HFont hFont, int x, int y, const Color &col, const wchar_t *pwsz )
{
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( pwsz, V_wcslen( pwsz ) );
}

void CHudTrok::Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 )
{
	wchar_t wsz[ 256 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	Text( hFont, x, y, col, wsz );
}

int CHudTrok::TextWide( HFont hFont, const char *pszUTF8 )
{
	wchar_t wsz[ 256 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	return w;
}

void CHudTrok::TextRight( HFont hFont, int xRight, int y, const Color &col, const char *pszUTF8 )
{
	Text( hFont, xRight - TextWide( hFont, pszUTF8 ), y, col, pszUTF8 );
}

void CHudTrok::Bar( int x, int y, int w, int h, float flFrac, const Color &col, bool bFromRight )
{
	flFrac = clamp( flFrac, 0.0f, 1.0f );
	surface()->DrawSetColor( Color( 255, 255, 255, 50 ) );
	surface()->DrawFilledRect( x, y, x + w, y + h );
	const int fw = (int)( w * flFrac );
	surface()->DrawSetColor( col );
	if ( bFromRight )
		surface()->DrawFilledRect( x + w - fw, y, x + w, y + h );
	else
		surface()->DrawFilledRect( x, y, x + fw, y + h );
}

// "trok_fuzil" / "weapon_trok_fuzil" -> nome do script ("Fuzil"), em caixa alta. Arma branca = FACA.
void CHudTrok::WeaponDisplayName( const char *pszName, char *pszOut, int iOutSize )
{
	char szClass[ 64 ];
	if ( V_strnicmp( pszName, "weapon_", 7 ) == 0 )
		V_strncpy( szClass, pszName, sizeof( szClass ) );
	else
		V_snprintf( szClass, sizeof( szClass ), "weapon_%s", pszName );

	if ( V_stricmp( szClass, "weapon_crowbar" ) == 0 || V_stricmp( szClass, "weapon_stunstick" ) == 0 )
	{
		V_strncpy( pszOut, "FACA", iOutSize );
		return;
	}

	const char *pszPrint = pszName;
	WEAPON_FILE_INFO_HANDLE h = LookupWeaponInfoSlot( szClass );
	FileWeaponInfo_t *pInfo = GetFileWeaponInfoFromHandle( h );
	if ( pInfo && pInfo->szPrintName[ 0 ] && pInfo->szPrintName[ 0 ] != '#' )
		pszPrint = pInfo->szPrintName;

	// Caixa alta (também com acento) passando por wchar_t.
	wchar_t wsz[ 64 ];
	V_UTF8ToUnicode( pszPrint, wsz, sizeof( wsz ) );
	for ( wchar_t *p = wsz; *p; ++p )
		*p = (wchar_t)towupper( *p );
	V_UnicodeToUTF8( wsz, pszOut, iOutSize );
}

//-----------------------------------------------------------------------------
// Eventos
//-----------------------------------------------------------------------------
void CHudTrok::FireGameEvent( IGameEvent *event )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const int iLocal = pLocal ? pLocal->entindex() : 0;
	const char *pszName = event->GetName();

	if ( !V_strcmp( pszName, "trok_dano" ) )
	{
		const int iVic = engine->GetPlayerForUserID( event->GetInt( "vitima" ) );
		const int iAtk = engine->GetPlayerForUserID( event->GetInt( "atacante" ) );
		const int iDano = event->GetInt( "dano" );
		const bool bHead = event->GetInt( "parte" ) == HITGROUP_HEAD;

		// Quem acerta alguém aparece no radar dos inimigos por um instante.
		if ( iAtk > 0 && iAtk <= MAX_PLAYERS )
			m_flSpotted[ iAtk ] = MAX( m_flSpotted[ iAtk ], gpGlobals->curtime + 2.0f );

		if ( event->GetInt( "vida" ) <= 0 && bHead )
		{
			m_iHeadVictim = iVic;
			m_flHeadTime = gpGlobals->curtime;
		}
		if ( iLocal && iAtk == iLocal && iVic > 0 && iVic <= MAX_PLAYERS && iVic != iLocal )
		{
			m_iDealt[ iVic ] += iDano;
			m_iDealtHits[ iVic ]++;
		}
		if ( iLocal && iVic == iLocal && iAtk > 0 && iAtk <= MAX_PLAYERS )
		{
			m_iTaken[ iAtk ] += iDano;
			m_iTakenHits[ iAtk ]++;
			if ( bHead )
				m_iTakenHead[ iAtk ]++;
		}
		return;
	}

	if ( !V_strcmp( pszName, "player_death" ) )
	{
		const int iVic = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		const int iAtk = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		const char *pszWpn = event->GetString( "weapon" );
		const bool bHead = ( iVic == m_iHeadVictim ) && ( gpGlobals->curtime - m_flHeadTime < 0.5f );

		KillEntry_t k;
		V_memset( &k, 0, sizeof( k ) );
		k.iAtk = iAtk;
		k.iVic = iVic;
		k.bHead = bHead;
		k.flTime = gpGlobals->curtime;
		if ( g_PR && iAtk > 0 && iAtk != iVic )
			V_strncpy( k.szAtk, g_PR->GetPlayerName( iAtk ), sizeof( k.szAtk ) );
		if ( g_PR && iVic > 0 )
			V_strncpy( k.szVic, g_PR->GetPlayerName( iVic ), sizeof( k.szVic ) );
		WeaponDisplayName( pszWpn, k.szWpn, sizeof( k.szWpn ) );
		m_Kills.AddToTail( k );
		while ( m_Kills.Count() > MAX_KILLFEED )
			m_Kills.Remove( 0 );

		// Você abateu alguém.
		if ( iLocal && iAtk == iLocal && iVic != iLocal )
		{
			m_flPopupTime = gpGlobals->curtime;
			V_strncpy( m_szPopupName, k.szVic, sizeof( m_szPopupName ) );
			m_bPopupHead = bHead;
		}

		// Você morreu.
		if ( iLocal && iVic == iLocal )
		{
			m_bDeathValid = true;
			m_flDeathTime = gpGlobals->curtime;
			m_iKiller = ( iAtk > 0 && iAtk != iLocal ) ? iAtk : 0;
			V_strncpy( m_szKiller, k.szAtk, sizeof( m_szKiller ) );
			V_strncpy( m_szKillerWpn, k.szWpn, sizeof( m_szKillerWpn ) );
			m_flKillerDist = -1.0f;
			C_BasePlayer *pKiller = m_iKiller ? UTIL_PlayerByIndex( m_iKiller ) : NULL;
			if ( pKiller && pLocal && !pKiller->IsDormant() )
				m_flKillerDist = ( pKiller->GetAbsOrigin() - pLocal->GetAbsOrigin() ).Length() / UNIDADES_POR_METRO;
		}
	}
}

//-----------------------------------------------------------------------------
// Desenho
//-----------------------------------------------------------------------------
void CHudTrok::Paint( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;

	const bool bSpectator = pLocal->GetTeamNumber() == TEAM_SPECTATOR;
	const bool bDead = !pLocal->IsAlive() && !bSpectator;

	// HUD novo (hud_trok_v2.cpp, trok_hud_estilo 2): ele desenha a partida; aqui só a tela de morte.
	if ( TrokHudV2_Ativo() )
	{
		if ( bDead && m_bDeathValid )
			PaintDeath( pLocal );
		return;
	}

	if ( bDead && m_bDeathValid )
	{
		PaintDeath( pLocal );
		PaintTop();
		PaintKillfeed();
		return;
	}

	PaintTop();
	PaintKillfeed();
	if ( bSpectator || !pLocal->IsAlive() )
		return;
	PaintVignette( pLocal );
	PaintRadar( pLocal );
	PaintDamageDir( pLocal );
	PaintHealth( pLocal );
	PaintAmmo( pLocal );
	PaintCrosshair( pLocal );
	PaintKillPopup();

	// Proteção de nascimento (equipe e solo)
	C_HL2MP_Player *pHL2MP = dynamic_cast< C_HL2MP_Player * >( pLocal );
	if ( pHL2MP && pHL2MP->m_flTrokProtegidoAte > gpGlobals->curtime )
	{
		char szProt[ 32 ];
		V_snprintf( szProt, sizeof( szProt ), "PROTEGIDO %d", (int)ceilf( pHL2MP->m_flTrokProtegidoAte - gpGlobals->curtime ) );
		Text( m_hSmall, ScreenWidth() / 2 - TextWide( m_hSmall, szProt ) / 2, ScreenHeight() / 2 + S( 70 ), Accent(), szProt );
	}
}

void CHudTrok::PaintTop( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !g_PR )
		return;
	if ( TrokSurto_Ativo() )
	{
		PaintSurtoTop();
		return;
	}
	if ( TrokHordas_Ativo() )
		return;	// Hordas: rodada e zumbis no hud_trok_hordas.cpp

	int iMine = 0, iTheirs = 0;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
	{
		int iMyTeam = pLocal->GetTeamNumber();
		if ( iMyTeam != TEAM_COMBINE && iMyTeam != TEAM_REBELS )
			iMyTeam = TEAM_COMBINE;
		const int iOther = ( iMyTeam == TEAM_COMBINE ) ? TEAM_REBELS : TEAM_COMBINE;
		C_Team *pMine = GetGlobalTeam( iMyTeam );
		C_Team *pOther = GetGlobalTeam( iOther );
		iMine = pMine ? pMine->Get_Score() : 0;
		iTheirs = pOther ? pOther->Get_Score() : 0;
	}
	else
	{
		// Sem times (exército solo): você contra o melhor dos outros, e sua posição.
		iMine = g_PR->GetPlayerScore( pLocal->entindex() );
		int iPos = 1, iTotal = 1;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			if ( i == pLocal->entindex() || !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) == TEAM_SPECTATOR )
				continue;
			iTheirs = MAX( iTheirs, g_PR->GetPlayerScore( i ) );
			++iTotal;
			if ( g_PR->GetPlayerScore( i ) > iMine )
				++iPos;
		}
		char szPos[ 32 ];
		V_snprintf( szPos, sizeof( szPos ), "%dº DE %d", iPos, iTotal );
		Text( m_hText, ScreenWidth() / 2 - TextWide( m_hText, szPos ) / 2, S( 22 ) + S( 60 ), iPos == 1 ? Accent() : COR_CINZA, szPos );
	}

	static ConVarRef s_fraglimit( "mp_fraglimit" );
	const int iLimit = s_fraglimit.IsValid() ? s_fraglimit.GetInt() : 0;
	const float flRef = iLimit > 0 ? (float)iLimit : (float)MAX( 1, MAX( iMine, iTheirs ) );

	const int cx = ScreenWidth() / 2;
	const int y = S( 22 );
	const int bw = S( 120 ), bh = S( 3 );

	// Tempo no meio
	char szTime[ 16 ] = "";
	const float flLeft = HL2MPRules() ? HL2MPRules()->GetMapRemainingTime() : 0.0f;
	if ( flLeft > 0.0f )
	{
		const int iSec = (int)flLeft;
		V_snprintf( szTime, sizeof( szTime ), "%02d:%02d", iSec / 60, iSec % 60 );
	}
	const int tw = szTime[ 0 ] ? TextWide( m_hText, szTime ) + S( 24 ) : S( 12 );
	if ( szTime[ 0 ] )
	{
		surface()->DrawSetColor( Color( 11, 13, 16, 160 ) );
		surface()->DrawFilledRect( cx - tw / 2, y + S( 2 ), cx + tw / 2, y + S( 30 ) );
		Text( m_hText, cx - TextWide( m_hText, szTime ) / 2, y + S( 6 ), COR_BRANCO, szTime );
	}

	char szNum[ 16 ];
	V_snprintf( szNum, sizeof( szNum ), "%d", iMine );
	TextRight( m_hScore, cx - tw / 2 - S( 14 ), y, COR_ALIADO, szNum );
	Bar( cx - tw / 2 - S( 14 ) - bw, y + S( 34 ), bw, bh, iMine / flRef, COR_ALIADO, true );

	V_snprintf( szNum, sizeof( szNum ), "%d", iTheirs );
	Text( m_hScore, cx + tw / 2 + S( 14 ), y, COR_INIMIGO, szNum );
	Bar( cx + tw / 2 + S( 14 ), y + S( 34 ), bw, bh, iTheirs / flRef, COR_INIMIGO );

	if ( iLimit > 0 )
	{
		char szLim[ 32 ];
		V_snprintf( szLim, sizeof( szLim ), "%d PARA VENCER", iLimit );
		Text( m_hSmall, cx - TextWide( m_hSmall, szLim ) / 2, y + S( 44 ), COR_CINZA, szLim );
	}
}

// Modo Surto: fase, relógio, quantos humanos e zumbis; avisos de alarme e abrigos; vencedor.
void CHudTrok::PaintSurtoTop( void )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado )
		return;

	const int cx = ScreenWidth() / 2;
	const int y = S( 22 );
	const float flLeft = MAX( 0.0f, pEstado->GetFimFase() - gpGlobals->curtime );
	char szTime[ 16 ];
	V_snprintf( szTime, sizeof( szTime ), "%d:%02d", (int)flLeft / 60, (int)flLeft % 60 );

	// Relógio no meio
	const int tw = TextWide( m_hScore, szTime ) + S( 28 );
	surface()->DrawSetColor( Color( 11, 13, 16, 170 ) );
	surface()->DrawFilledRect( cx - tw / 2, y, cx + tw / 2, y + S( 36 ) );
	const bool bAlarme = pEstado->IsAlarme() && pEstado->GetFase() == SURTO_JOGO;
	const bool bPisca = bAlarme && ( (int)( gpGlobals->curtime * 2.0f ) % 2 ) == 0;
	Text( m_hScore, cx - TextWide( m_hScore, szTime ) / 2, y + S( 4 ), bPisca ? Accent() : COR_BRANCO, szTime );

	// Contagem dos lados
	char szNum[ 32 ];
	V_snprintf( szNum, sizeof( szNum ), "%d HUMANOS", pEstado->GetHumanosVivos() );
	TextRight( m_hText, cx - tw / 2 - S( 14 ), y + S( 10 ), COR_ALIADO, szNum );
	V_snprintf( szNum, sizeof( szNum ), "%d ZUMBIS", pEstado->GetZumbisVivos() );
	Text( m_hText, cx + tw / 2 + S( 14 ), y + S( 10 ), COR_ZUMBI, szNum );

	// Linha de baixo: o que está acontecendo
	const char *pszLinha = NULL;
	Color colLinha = COR_CINZA;
	if ( pEstado->GetFase() == SURTO_PREPARO )
	{
		pszLinha = "PREPARE-SE · OS INFECTADOS SERÃO SORTEADOS";
		colLinha = Accent();
	}
	else if ( pEstado->GetFase() == SURTO_JOGO )
	{
		if ( pEstado->IsAbrigos() )
		{
			pszLinha = "ABRIGOS ABERTOS · FECHE AS GRADES";
			colLinha = Accent();
		}
		else if ( bAlarme )
		{
			pszLinha = "ALARME · OS ABRIGOS VÃO ABRIR";
			colLinha = Accent();
		}
		else
		{
			pszLinha = "SOBREVIVA ATÉ O FIM DO TEMPO";
		}
	}
	if ( pszLinha )
		Text( m_hSmall, cx - TextWide( m_hSmall, pszLinha ) / 2, y + S( 44 ), colLinha, pszLinha );

	// Fim da rodada: vencedor no meio da tela
	if ( pEstado->GetFase() == SURTO_FIM )
	{
		const bool bHumanos = pEstado->GetVencedor() == SURTO_HUMANOS;
		const char *pszTitulo = bHumanos ? "HUMANOS SOBREVIVERAM" : "OS INFECTADOS VENCERAM";
		const int ty = ScreenHeight() / 2 - S( 60 );
		surface()->DrawSetColor( Color( 11, 13, 16, 170 ) );
		surface()->DrawFilledRect( 0, ty - S( 16 ), ScreenWidth(), ty + S( 70 ) );
		Text( m_hNum, cx - TextWide( m_hNum, pszTitulo ) / 2, ty, bHumanos ? COR_ALIADO : COR_ZUMBI, pszTitulo );
		// Destaque da rodada
		if ( pEstado->GetDestaque() > 0 && g_PR && g_PR->IsConnected( pEstado->GetDestaque() ) )
		{
			char szDest[ 160 ];
			const int v = pEstado->GetDestaqueValor();
			if ( pEstado->GetDestaqueTipo() == SURTO_DESTAQUE_INFECCOES )
				V_snprintf( szDest, sizeof( szDest ), "DESTAQUE: %s · %d %s", g_PR->GetPlayerName( pEstado->GetDestaque() ), v, v == 1 ? "infecção" : "infecções" );
			else
				V_snprintf( szDest, sizeof( szDest ), "DESTAQUE: %s · %d %s", g_PR->GetPlayerName( pEstado->GetDestaque() ), v, v == 1 ? "zumbi abatido" : "zumbis abatidos" );
			Text( m_hText, cx - TextWide( m_hText, szDest ) / 2, ty - S( 34 ), Accent(), szDest );
		}

		char szProx[ 48 ];
		V_snprintf( szProx, sizeof( szProx ), "Nova rodada em %d", (int)ceilf( flLeft ) );
		Text( m_hSmall, cx - TextWide( m_hSmall, szProx ) / 2, ty + surface()->GetFontTall( m_hNum ) + S( 6 ), COR_CINZA, szProx );
	}
}

void CHudTrok::PaintKillfeed( void )
{
	const float flLife = trok_hud_killfeed_tempo.GetFloat();
	const int xr = ScreenWidth() - S( 32 );
	int y = S( 28 );
	const int iRowH = S( 26 );
	for ( int i = 0; i < m_Kills.Count(); ++i )
	{
		const KillEntry_t &k = m_Kills[ i ];
		const float flAge = gpGlobals->curtime - k.flTime;
		const int iAlpha = flAge > flLife - 1.0f ? (int)( 255 * ( flLife - flAge ) ) : 255;

		// Mede a linha: [atacante] ARMA [CABEÇA] vítima
		const int gap = S( 8 );
		const int wAtk = k.szAtk[ 0 ] ? TextWide( m_hText, k.szAtk ) + gap : 0;
		const int wWpn = TextWide( m_hSmall, k.szWpn ) + gap;
		const int wHead = k.bHead ? TextWide( m_hSmall, "CABEÇA" ) + S( 8 ) + gap : 0;
		const int wVic = TextWide( m_hText, k.szVic );
		const int total = S( 10 ) + wAtk + wWpn + wHead + wVic + S( 10 );
		int x = xr - total;

		surface()->DrawSetColor( Color( 11, 13, 16, iAlpha * 140 / 255 ) );
		surface()->DrawFilledRect( x, y, xr, y + iRowH - S( 4 ) );
		x += S( 10 );
		const int ty = y + ( iRowH - S( 4 ) - surface()->GetFontTall( m_hText ) ) / 2;
		const int sy = y + ( iRowH - S( 4 ) - surface()->GetFontTall( m_hSmall ) ) / 2;
		if ( k.szAtk[ 0 ] )
		{
			Text( m_hText, x, ty, WithAlpha( RelColor( k.iAtk ), iAlpha ), k.szAtk );
			x += wAtk;
		}
		Text( m_hSmall, x, sy, WithAlpha( COR_CINZA, iAlpha * 200 / 255 ), k.szWpn );
		x += wWpn;
		if ( k.bHead )
		{
			const int hw = wHead - gap;
			surface()->DrawSetColor( WithAlpha( Accent(), iAlpha ) );
			surface()->DrawFilledRect( x, sy - S( 1 ), x + hw, sy + surface()->GetFontTall( m_hSmall ) + S( 1 ) );
			Text( m_hSmall, x + S( 4 ), sy, WithAlpha( COR_FUNDO, iAlpha ), "CABEÇA" );
			x += wHead;
		}
		Text( m_hText, x, ty, WithAlpha( RelColor( k.iVic ), iAlpha ), k.szVic );
		y += iRowH;
	}
}

void CHudTrok::PaintHealth( C_BasePlayer *pLocal )
{
	const int x = S( 32 );
	const int yBottom = ScreenHeight() - S( 32 );
	const int iHealth = MAX( 0, pLocal->GetHealth() );
	const bool bLow = iHealth <= 30 && !TrokSurto_IsZumbi( pLocal );
	const Color col = bLow ? COR_INIMIGO : COR_BRANCO;

	char szHealth[ 16 ];
	V_snprintf( szHealth, sizeof( szHealth ), "%d", iHealth );
	const int numTall = surface()->GetFontTall( m_hNum );
	const int yNum = yBottom - S( 22 ) - numTall;
	Text( m_hNum, x, yNum, col, szHealth );
	const bool bZumbi = TrokSurto_IsZumbi( pLocal );
	Text( m_hSmall, x + TextWide( m_hNum, szHealth ) + S( 10 ), yNum + numTall - surface()->GetFontTall( m_hSmall ) - S( 6 ),
		bZumbi ? COR_ZUMBI : COR_CINZA, bZumbi ? "ZUMBI" : "VIDA" );

	const int iMaxHealth = MAX( 1, pLocal->GetMaxHealth() );
	Bar( x, yBottom - S( 16 ), S( 240 ), S( 4 ), iHealth / (float)MAX( iMaxHealth, iHealth ), bZumbi ? COR_ZUMBI : col );

	// Fôlego da corrida (energia do traje do HL2): só aparece quando não está cheio.
	C_BaseHLPlayer *pHL = dynamic_cast< C_BaseHLPlayer * >( pLocal );
	if ( pHL )
	{
		const float flPower = pHL->m_HL2Local.m_flSuitPower;
		if ( flPower < 99.0f )
		{
			const int ly = yBottom - S( 6 );
			Text( m_hSmall, x, ly - surface()->GetFontTall( m_hSmall ) / 2, COR_CINZA, "FÔLEGO" );
			Bar( x + S( 58 ), ly - S( 1 ), S( 182 ), S( 2 ), flPower / 100.0f, Accent() );
		}
	}
}

void CHudTrok::PaintAmmo( C_BasePlayer *pLocal )
{
	C_BaseCombatWeapon *pWeapon = pLocal->GetActiveWeapon();
	if ( !pWeapon )
		return;

	const int xr = ScreenWidth() - S( 32 );
	const int yBottom = ScreenHeight() - S( 32 );

	char szName[ 48 ];
	WeaponDisplayName( pWeapon->GetClassname(), szName, sizeof( szName ) );
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pWeapon );
	const char *pszMode = pGun ? ( pGun->IsAutomatic() ? "AUTO" : "SEMI" ) : "";

	// Linha de baixo: NOME   MODO
	const int ly = yBottom - surface()->GetFontTall( m_hText );
	int x = xr;
	if ( pszMode[ 0 ] )
	{
		x -= TextWide( m_hText, pszMode );
		Text( m_hText, x, ly, COR_CINZA, pszMode );
		x -= S( 10 );
	}
	TextRight( m_hText, x, ly, COR_BRANCO, szName );

	if ( !pWeapon->UsesClipsForAmmo1() )
		return;

	const int iClip = MAX( 0, pWeapon->Clip1() );
	const int iMax = MAX( 1, pWeapon->GetMaxClip1() );
	const int iReserve = pLocal->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );

	// Uma marca por bala (pentes até 40).
	int yPips = ly - S( 16 );
	if ( iMax <= 40 )
	{
		const int pw = S( 3 ), ph = S( 10 ), pg = MAX( 1, S( 2 ) );
		int px = xr - iMax * ( pw + pg ) + pg;
		for ( int i = 0; i < iMax; ++i )
		{
			surface()->DrawSetColor( i < iClip ? COR_BRANCO : Color( 255, 255, 255, 56 ) );
			surface()->DrawFilledRect( px, yPips, px + pw, yPips + ph );
			px += pw + pg;
		}
	}
	else
	{
		yPips += S( 10 );
	}

	// Pente grande / reserva
	char szClip[ 16 ], szRes[ 16 ];
	V_snprintf( szClip, sizeof( szClip ), "%d", iClip );
	V_snprintf( szRes, sizeof( szRes ), "/ %d", iReserve );
	const int numTall = surface()->GetFontTall( m_hNum );
	const int yNum = yPips - S( 6 ) - numTall;
	const int wRes = TextWide( m_hScore, szRes );
	Text( m_hScore, xr - wRes, yNum + numTall - surface()->GetFontTall( m_hScore ) - S( 3 ), COR_CINZA, szRes );
	const bool bLow = iClip <= iMax / 4;
	TextRight( m_hNum, xr - wRes - S( 8 ), yNum, bLow ? COR_INIMIGO : COR_BRANCO, szClip );
}

extern ConVar trok_mira_calibrar; // weapon_trok_gun.cpp

void CHudTrok::PaintCrosshair( C_BasePlayer *pLocal )
{
	const int cx = ScreenWidth() / 2, cy = ScreenHeight() / 2;

	// Calibração da mira pelo cano: linhas finas cruzando o centro da tela inteira.
	if ( trok_mira_calibrar.GetBool() )
	{
		surface()->DrawSetColor( Color( 255, 40, 40, 200 ) );
		surface()->DrawFilledRect( cx, 0, cx + 1, ScreenHeight() );
		surface()->DrawFilledRect( 0, cy, ScreenWidth(), cy + 1 );
		return;
	}

	if ( !trok_hud_mira.GetBool() )
		return;

	C_BaseCombatWeapon *pWeapon = pLocal->GetActiveWeapon();
	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pWeapon );

	if ( !pGun )
	{
		// Faca e outras: um ponto discreto.
		surface()->DrawSetColor( Color( 236, 238, 241, 200 ) );
		surface()->DrawFilledRect( cx - 1, cy - 1, cx + 1, cy + 1 );
		return;
	}

	// Some mirando pelo cano (a alça da arma é a mira) e correndo (arma abaixada).
	const float flVis = ( 1.0f - pGun->GetADSBlend() ) * ( 1.0f - pGun->GetSprintBlend() );
	if ( flVis < 0.05f )
		return;

	// Distância do centro = meia abertura do cone de dispersão projetada na tela.
	const float flFov = MAX( 10, pLocal->GetFOV() );
	const float flTanHalfV = tanf( DEG2RAD( flFov * 0.5f ) ) * 0.75f; // FOV do Source é horizontal em 4:3
	const float flHalfSpread = DEG2RAD( 0.5f * pGun->CurrentSpreadDegrees() );
	int gap = (int)( ( ScreenHeight() * 0.5f ) * tanf( flHalfSpread ) / flTanHalfV );
	gap = clamp( gap, S( 4 ), S( 90 ) );
	const int len = S( 9 );
	const int th = MAX( 2, S( 2 ) );

	surface()->DrawSetColor( Color( 236, 238, 241, (int)( 220 * flVis ) ) );
	surface()->DrawFilledRect( cx - th / 2, cy - gap - len, cx - th / 2 + th, cy - gap );
	surface()->DrawFilledRect( cx - th / 2, cy + gap, cx - th / 2 + th, cy + gap + len );
	surface()->DrawFilledRect( cx - gap - len, cy - th / 2, cx - gap, cy - th / 2 + th );
	surface()->DrawFilledRect( cx + gap, cy - th / 2, cx + gap + len, cy - th / 2 + th );
}

void CHudTrok::PaintKillPopup( void )
{
	const float flAge = gpGlobals->curtime - m_flPopupTime;
	if ( flAge < 0.0f || flAge > 1.6f )
		return;
	const int iAlpha = flAge > 1.2f ? (int)( 255 * ( 1.6f - flAge ) / 0.4f ) : 255;
	const int x = ScreenWidth() / 2 + S( 36 );
	const int y = ScreenHeight() / 2 + S( 18 );
	const bool bInfectou = TrokSurto_IsZumbi( C_BasePlayer::GetLocalPlayer() );
	Text( m_hText, x, y, WithAlpha( bInfectou ? COR_ZUMBI : Accent(), iAlpha ), bInfectou ? "INFECTOU" : ( m_bPopupHead ? "ABATE · CABEÇA" : "ABATE" ) );
	Text( m_hSmall, x, y + surface()->GetFontTall( m_hText ), WithAlpha( COR_CINZA, iAlpha ), m_szPopupName );
}

bool CHudTrok::IsAlly( C_BasePlayer *pLocal, int iPlayer ) const
{
	if ( !g_PR || iPlayer <= 0 )
		return false;
	if ( !( HL2MPRules() && HL2MPRules()->IsTeamplay() ) )
		return false;
	return g_PR->GetTeam( iPlayer ) == pLocal->GetTeamNumber();
}

// Arco vermelho em volta da mira apontando para de onde veio o dano.
void CHudTrok::PaintDamageDir( C_BasePlayer *pLocal )
{
	const int cx = ScreenWidth() / 2, cy = ScreenHeight() / 2;
	const float R = (float)S( 118 );
	QAngle angView = pLocal->EyeAngles();
	angView.x = angView.z = 0.0f;
	Vector fwd, right;
	AngleVectors( angView, &fwd, &right, NULL );

	for ( int k = 0; k < ARRAYSIZE( m_Dmg ); ++k )
	{
		const float flAge = gpGlobals->curtime - m_Dmg[ k ].flTime;
		if ( flAge < 0.0f || flAge > 1.3f )
			continue;
		const int iAlpha = flAge < 0.8f ? 230 : (int)( 230 * ( 1.3f - flAge ) / 0.5f );
		Vector rel = m_Dmg[ k ].vecFrom - pLocal->EyePosition();
		rel.z = 0.0f;
		const float a = atan2f( DotProduct( rel, right ), DotProduct( rel, fwd ) ); // 0 = frente, + = direita
		surface()->DrawSetColor( Color( 255, 72, 60, iAlpha ) );
		const float span = DEG2RAD( 20.0f );
		const int iSeg = 10;
		for ( int t = 0; t < 4; ++t )	// espessura
		{
			const float r = R + t;
			for ( int sgm = 0; sgm < iSeg; ++sgm )
			{
				const float a0 = a - span + 2.0f * span * sgm / iSeg;
				const float a1 = a - span + 2.0f * span * ( sgm + 1 ) / iSeg;
				surface()->DrawLine( cx + (int)( sinf( a0 ) * r ), cy - (int)( cosf( a0 ) * r ),
									 cx + (int)( sinf( a1 ) * r ), cy - (int)( cosf( a1 ) * r ) );
			}
		}
	}
}

// Pouca vida: bordas da tela avermelhando, pulsando de leve.
void CHudTrok::PaintVignette( C_BasePlayer *pLocal )
{
	const float flLimite = trok_hud_vida_baixa.GetFloat();
	if ( flLimite <= 0.0f || TrokSurto_IsZumbi( pLocal ) )
		return;
	const float flFrac = pLocal->GetHealth() / (float)MAX( 1, pLocal->GetMaxHealth() );
	if ( flFrac >= flLimite )
		return;
	const float k = 1.0f - flFrac / flLimite;
	const int a = (int)( k * ( 95.0f + 25.0f * sinf( gpGlobals->curtime * 4.0f ) ) );
	const int w = ScreenWidth(), h = ScreenHeight();
	surface()->DrawSetColor( Color( 170, 20, 20, 255 ) );
	surface()->DrawFilledRectFade( 0, 0, w, h / 4, a, 0, false );
	surface()->DrawFilledRectFade( 0, h * 3 / 4, w, h, 0, a, false );
	surface()->DrawFilledRectFade( 0, 0, w / 5, h, a, 0, true );
	surface()->DrawFilledRectFade( w * 4 / 5, 0, w, h, 0, a, true );
}

// Inimigo visível (linha de visão livre) aparece no radar por um segundo.
void CHudTrok::UpdateSpotted( C_BasePlayer *pLocal )
{
	if ( gpGlobals->curtime < m_flNextSpotCheck )
		return;
	m_flNextSpotCheck = gpGlobals->curtime + 0.2f;
	const Vector eye = pLocal->EyePosition();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !p->IsAlive() || IsAlly( pLocal, i ) )
			continue;
		trace_t tr;
		UTIL_TraceLine( eye, p->EyePosition(), MASK_VISIBLE, pLocal, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction > 0.98f || tr.m_pEnt == p )
			m_flSpotted[ i ] = MAX( m_flSpotted[ i ], gpGlobals->curtime + 1.0f );
	}
}

// Radar que gira com a visão: aliados sempre, inimigos só quando vistos ou atirando.
void CHudTrok::PaintRadar( C_BasePlayer *pLocal )
{
	if ( !trok_hud_radar.GetBool() || !g_PR )
		return;
	UpdateSpotted( pLocal );

	const int size = S( 176 );
	const int x0 = S( 32 ), y0 = S( 28 );
	const int cx = x0 + size / 2, cy = y0 + size / 2;
	const float flRange = MAX( 200.0f, trok_hud_radar_alcance.GetFloat() );

	surface()->DrawSetColor( Color( 11, 13, 16, 150 ) );
	surface()->DrawFilledRect( x0, y0, x0 + size, y0 + size );
	surface()->DrawSetColor( Color( 255, 255, 255, 36 ) );
	surface()->DrawOutlinedRect( x0, y0, x0 + size, y0 + size );
	surface()->DrawSetColor( Color( 255, 255, 255, 14 ) );
	surface()->DrawLine( cx, y0 + 4, cx, y0 + size - 4 );
	surface()->DrawLine( x0 + 4, cy, x0 + size - 4, cy );

	QAngle angView = pLocal->EyeAngles();
	angView.x = angView.z = 0.0f;
	Vector fwd, right;
	AngleVectors( angView, &fwd, &right, NULL );

	const int dot = MAX( 3, S( 4 ) );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !p->IsAlive() )
			continue;
		const bool bAlly = IsAlly( pLocal, i );
		if ( !bAlly && gpGlobals->curtime > m_flSpotted[ i ] )
			continue;
		Vector rel = p->GetAbsOrigin() - pLocal->GetAbsOrigin();
		const float rx = DotProduct( rel, right ) / flRange;
		const float ry = DotProduct( rel, fwd ) / flRange;
		if ( fabsf( rx ) > 1.0f || fabsf( ry ) > 1.0f )
			continue;
		const int px = cx + (int)( rx * ( size / 2 - dot ) );
		const int py = cy - (int)( ry * ( size / 2 - dot ) );
		Color col = bAlly ? COR_ALIADO : ( TrokSurto_IsZumbi( p ) ? COR_ZUMBI : COR_INIMIGO );
		surface()->DrawSetColor( col );
		surface()->DrawFilledRect( px - dot / 2, py - dot / 2, px + dot / 2 + 1, py + dot / 2 + 1 );
	}

	// Você: seta para a frente
	const int t = S( 7 );
	surface()->DrawSetColor( COR_BRANCO );
	surface()->DrawLine( cx, cy - t, cx - t * 2 / 3, cy + t * 2 / 3 );
	surface()->DrawLine( cx, cy - t, cx + t * 2 / 3, cy + t * 2 / 3 );
	surface()->DrawLine( cx - t * 2 / 3, cy + t * 2 / 3, cx + t * 2 / 3, cy + t * 2 / 3 );
}

void CHudTrok::PaintDeath( C_BasePlayer *pLocal )
{
	const int w = ScreenWidth(), h = ScreenHeight();
	surface()->DrawSetColor( Color( 11, 13, 16, 120 ) );
	surface()->DrawFilledRect( 0, 0, w, h );

	const int x = S( 64 );
	int y = S( 64 );
	C_BasePlayer *pKillerEnt = m_iKiller ? UTIL_PlayerByIndex( m_iKiller ) : NULL;
	const bool bInfectado = TrokSurto_Ativo() && pLocal->GetTeamNumber() == TEAM_TROK_HUMANOS && pKillerEnt && TrokSurto_IsZumbi( pKillerEnt );
	if ( m_iKiller )
	{
		Text( m_hSmall, x, y, bInfectado ? COR_ZUMBI : COR_CINZA, bInfectado ? "INFECTADO POR" : "ABATIDO POR" );
		y += S( 18 );
		wchar_t wsz[ 64 ];
		V_UTF8ToUnicode( m_szKiller, wsz, sizeof( wsz ) );
		for ( wchar_t *p = wsz; *p; ++p )
			*p = (wchar_t)towupper( *p );
		Text( m_hNum, x, y, COR_INIMIGO, wsz );
		y += surface()->GetFontTall( m_hNum ) + S( 4 );
		if ( g_PR && g_PR->IsFakePlayer( m_iKiller ) )
		{
			Text( m_hSmall, x, y, COR_CINZA, "Bot" );
			y += surface()->GetFontTall( m_hSmall );
		}
	}
	else
	{
		Text( m_hNum, x, y, COR_BRANCO, "VOCÊ MORREU" );
		y += surface()->GetFontTall( m_hNum ) + S( 4 );
	}

	// Cartão: arma, distância, vida dele; dano trocado.
	if ( m_iKiller )
	{
		y += S( 20 );
		const int cw = S( 380 );
		const int ch = S( 160 );
		surface()->DrawSetColor( Color( 20, 23, 28, 218 ) );
		surface()->DrawFilledRect( x, y, x + cw, y + ch );
		surface()->DrawSetColor( Color( 255, 255, 255, 20 ) );
		surface()->DrawOutlinedRect( x, y, x + cw, y + ch );

		const int colW = ( cw - S( 40 ) ) / 3;
		int cx = x + S( 20 );
		const int ly = y + S( 14 );
		const int vy = ly + surface()->GetFontTall( m_hSmall ) + S( 2 );

		Text( m_hSmall, cx, ly, COR_CINZA, "ARMA" );
		Text( m_hText, cx, vy, COR_BRANCO, m_szKillerWpn );
		cx += colW;

		char szVal[ 32 ];
		Text( m_hSmall, cx, ly, COR_CINZA, "DISTÂNCIA" );
		if ( m_flKillerDist >= 0.0f )
			V_snprintf( szVal, sizeof( szVal ), "%d m", (int)( m_flKillerDist + 0.5f ) );
		else
			V_strncpy( szVal, "—", sizeof( szVal ) );
		Text( m_hText, cx, vy, COR_BRANCO, szVal );
		cx += colW;

		Text( m_hSmall, cx, ly, COR_CINZA, "VIDA DELE" );
		C_BasePlayer *pKiller = UTIL_PlayerByIndex( m_iKiller );
		if ( pKiller && pKiller->IsAlive() && !pKiller->IsDormant() )
			V_snprintf( szVal, sizeof( szVal ), "%d", pKiller->GetHealth() );
		else
			V_strncpy( szVal, "—", sizeof( szVal ) );
		Text( m_hText, cx, vy, COR_BRANCO, szVal );

		// Dano
		int dy = y + S( 66 );
		surface()->DrawSetColor( Color( 255, 255, 255, 15 ) );
		surface()->DrawFilledRect( x + 1, dy - S( 10 ), x + cw - 1, dy - S( 9 ) );

		char szLine[ 96 ];
		const int iDealt = m_iDealt[ m_iKiller ], iDealtHits = m_iDealtHits[ m_iKiller ];
		Text( m_hSmall, x + S( 20 ), dy, COR_CINZA, "Você causou" );
		V_snprintf( szLine, sizeof( szLine ), "%d · %d %s", iDealt, iDealtHits, iDealtHits == 1 ? "acerto" : "acertos" );
		TextRight( m_hSmall, x + cw - S( 20 ), dy, COR_BRANCO, szLine );
		Bar( x + S( 20 ), dy + S( 18 ), cw - S( 40 ), S( 4 ), iDealt / 100.0f, COR_ALIADO );

		dy += S( 40 );
		const int iTaken = m_iTaken[ m_iKiller ], iTakenHits = m_iTakenHits[ m_iKiller ], iHead = m_iTakenHead[ m_iKiller ];
		Text( m_hSmall, x + S( 20 ), dy, COR_CINZA, "Você recebeu" );
		if ( iHead > 0 )
			V_snprintf( szLine, sizeof( szLine ), "%d · %d %s, %d na cabeça", iTaken, iTakenHits, iTakenHits == 1 ? "acerto" : "acertos", iHead );
		else
			V_snprintf( szLine, sizeof( szLine ), "%d · %d %s", iTaken, iTakenHits, iTakenHits == 1 ? "acerto" : "acertos" );
		TextRight( m_hSmall, x + cw - S( 20 ), dy, COR_BRANCO, szLine );
		Bar( x + S( 20 ), dy + S( 18 ), cw - S( 40 ), S( 4 ), iTaken / 100.0f, COR_INIMIGO );
	}

	// Renascer (HL2MP: qualquer botão depois da animação de morte). No Surto o servidor decide.
	if ( gpGlobals->curtime - m_flDeathTime > 1.0f )
	{
		char szRenascer[ 48 ];
		const float flFalta = m_flDeathTime + trok_renascer_tempo.GetFloat() - gpGlobals->curtime;
		V_snprintf( szRenascer, sizeof( szRenascer ), "RENASCENDO EM %d", MAX( 0, (int)ceilf( flFalta ) ) );
		const char *pszHint = szRenascer;
		if ( TrokSurto_Ativo() )
			pszHint = ( pLocal->GetTeamNumber() == TEAM_TROK_HUMANOS ) ? "VOCÊ VAI LEVANTAR COMO ZUMBI" : "ESPERE A PRÓXIMA RODADA";
		else if ( TrokHordas_Ativo() )
			pszHint = "VOCÊ VOLTA NA PRÓXIMA RODADA";
		Text( m_hText, w / 2 - TextWide( m_hText, pszHint ) / 2, h - S( 96 ), bInfectado ? COR_ZUMBI : COR_BRANCO, pszHint );
	}
}
