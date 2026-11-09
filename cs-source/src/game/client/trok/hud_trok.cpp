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
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "weapon_parse.h"
#include "trok/weapon_trok_gun.h"
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
static ConVar trok_hud_killfeed_tempo( "trok_hud_killfeed_tempo", "6", FCVAR_ARCHIVE, "Segundos que cada abate fica na lista." );

static const Color COR_BRANCO( 236, 238, 241, 255 );
static const Color COR_CINZA( 190, 196, 204, 200 );
static const Color COR_ALIADO( 90, 162, 255, 255 );
static const Color COR_INIMIGO( 255, 90, 90, 255 );
static const Color COR_DESTAQUE_PADRAO( 242, 181, 68, 255 );
static const Color COR_FUNDO( 11, 13, 16, 255 );

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
	void PaintKillfeed( void );
	void PaintHealth( C_BasePlayer *pLocal );
	void PaintAmmo( C_BasePlayer *pLocal );
	void PaintCrosshair( C_BasePlayer *pLocal );
	void PaintKillPopup( void );
	void PaintDeath( C_BasePlayer *pLocal );

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
};

DECLARE_HUDELEMENT( CHudTrok );

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
	PaintHealth( pLocal );
	PaintAmmo( pLocal );
	PaintCrosshair( pLocal );
	PaintKillPopup();
}

void CHudTrok::PaintTop( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !g_PR )
		return;

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
		// Sem times: você contra o melhor dos outros.
		iMine = g_PR->GetPlayerScore( pLocal->entindex() );
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			if ( i != pLocal->entindex() && g_PR->IsConnected( i ) )
				iTheirs = MAX( iTheirs, g_PR->GetPlayerScore( i ) );
		}
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
	const bool bLow = iHealth <= 30;
	const Color col = bLow ? COR_INIMIGO : COR_BRANCO;

	char szHealth[ 16 ];
	V_snprintf( szHealth, sizeof( szHealth ), "%d", iHealth );
	const int numTall = surface()->GetFontTall( m_hNum );
	const int yNum = yBottom - S( 22 ) - numTall;
	Text( m_hNum, x, yNum, col, szHealth );
	Text( m_hSmall, x + TextWide( m_hNum, szHealth ) + S( 10 ), yNum + numTall - surface()->GetFontTall( m_hSmall ) - S( 6 ), COR_CINZA, "VIDA" );

	Bar( x, yBottom - S( 16 ), S( 240 ), S( 4 ), iHealth / (float)MAX( 100, iHealth ), col );

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

void CHudTrok::PaintCrosshair( C_BasePlayer *pLocal )
{
	if ( !trok_hud_mira.GetBool() )
		return;

	const int cx = ScreenWidth() / 2, cy = ScreenHeight() / 2;
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
	Text( m_hText, x, y, WithAlpha( Accent(), iAlpha ), m_bPopupHead ? "ABATE · CABEÇA" : "ABATE" );
	Text( m_hSmall, x, y + surface()->GetFontTall( m_hText ), WithAlpha( COR_CINZA, iAlpha ), m_szPopupName );
}

void CHudTrok::PaintDeath( C_BasePlayer *pLocal )
{
	const int w = ScreenWidth(), h = ScreenHeight();
	surface()->DrawSetColor( Color( 11, 13, 16, 120 ) );
	surface()->DrawFilledRect( 0, 0, w, h );

	const int x = S( 64 );
	int y = S( 64 );
	if ( m_iKiller )
	{
		Text( m_hSmall, x, y, COR_CINZA, "ABATIDO POR" );
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

	// Renascer (HL2MP: qualquer botão depois da animação de morte).
	if ( gpGlobals->curtime - m_flDeathTime > 1.0f )
	{
		const char *pszHint = "CLIQUE PARA RENASCER";
		Text( m_hText, w / 2 - TextWide( m_hText, pszHint ) / 2, h - S( 96 ), COR_BRANCO, pszHint );
	}
	NOTE_UNUSED( pLocal );
}
