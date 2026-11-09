//=============================================================================
// Trok: placar do TAB (concept "Placar", design/ui.md). Substitui o placar do HL2MP
// no CHudViewport::CreatePanelByName (patch). Desenho direto, sem controles VGUI:
// cabeçalho com modo, mapa, placar e tempo; o seu time em azul primeiro, o outro em
// vermelho; sem times, uma lista só. Bots marcados como BOT. Ordem por abates.
// Ideia do placar desenhado à mão: NEOTOKYO Rebuild (neo_scoreboard.cpp).
//=============================================================================

#include "cbase.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "hl2mp_gamerules.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include <wctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static const Color COR_TEXTO( 236, 238, 241, 255 );
static const Color COR_TEXTO2( 163, 169, 177, 255 );
static const Color COR_TEXTO3( 128, 134, 142, 255 );
static const Color COR_ALIADO( 90, 162, 255, 255 );
static const Color COR_INIMIGO( 255, 90, 90, 255 );

class CTrokScoreboard : public Panel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CTrokScoreboard, Panel );
public:
	CTrokScoreboard( IViewPort *pViewPort );

	virtual const char *GetName( void ) { return PANEL_SCOREBOARD; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return false; }
	virtual void ShowPanel( bool bShow );
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_NONE; }
	virtual VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( VPANEL parent ) { BaseClass::SetParent( parent ); }

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * m_flScale + 0.5f ); }
	Color Accent( void ) const;
	void Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 );
	void TextRight( HFont hFont, int xRight, int y, const Color &col, const char *pszUTF8 );
	int TextWide( HFont hFont, const char *pszUTF8 );
	int PaintTeam( int x, int y, int w, int iTeam, const char *pszTitle, const Color &colTeam, int iMaxRows );

	HFont m_hTitle, m_hScore, m_hHeading, m_hBody, m_hSmall;
	float m_flScale;
};

CTrokScoreboard::CTrokScoreboard( IViewPort *pViewPort ) : BaseClass( NULL, PANEL_SCOREBOARD )
{
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	SetVisible( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPaintBackgroundEnabled( false );
	m_hTitle = m_hScore = m_hHeading = m_hBody = m_hSmall = INVALID_FONT;
	m_flScale = 1.0f;
	NOTE_UNUSED( pViewPort );
}

void CTrokScoreboard::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hTitle = pScheme->GetFont( "TrokItem", true );
	m_hScore = pScheme->GetFont( "TrokHudNumber", true );
	m_hHeading = pScheme->GetFont( "TrokHeading", true );
	m_hBody = pScheme->GetFont( "TrokBody", true );
	m_hSmall = pScheme->GetFont( "TrokSmall", true );
	SetPaintBackgroundEnabled( false );
}

void CTrokScoreboard::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;
	SetVisible( bShow );
	if ( bShow )
		MoveToFront();
}

Color CTrokScoreboard::Accent( void ) const
{
	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = 242, g = 181, b = 68;
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
}

void CTrokScoreboard::Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 )
{
	wchar_t wsz[ 256 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
}

int CTrokScoreboard::TextWide( HFont hFont, const char *pszUTF8 )
{
	wchar_t wsz[ 256 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	return w;
}

void CTrokScoreboard::TextRight( HFont hFont, int xRight, int y, const Color &col, const char *pszUTF8 )
{
	Text( hFont, xRight - TextWide( hFont, pszUTF8 ), y, col, pszUTF8 );
}

// Desenha um time (iTeam < 0 = todos os conectados, sem times). Devolve a altura usada.
int CTrokScoreboard::PaintTeam( int x, int y, int w, int iTeam, const char *pszTitle, const Color &colTeam, int iMaxRows )
{
	// Junta e ordena por abates (depois menos mortes).
	int idx[ MAX_PLAYERS ];
	int n = 0;
	for ( int i = 1; i <= gpGlobals->maxClients && n < MAX_PLAYERS; ++i )
	{
		if ( !g_PR->IsConnected( i ) )
			continue;
		const int t = g_PR->GetTeam( i );
		if ( iTeam >= 0 ? ( t != iTeam ) : ( t == TEAM_SPECTATOR || t == TEAM_UNASSIGNED ) )
			continue;
		idx[ n++ ] = i;
	}
	for ( int a = 1; a < n; ++a )
	{
		int v = idx[ a ], b = a - 1;
		while ( b >= 0 && ( g_PR->GetPlayerScore( idx[ b ] ) < g_PR->GetPlayerScore( v ) ||
			( g_PR->GetPlayerScore( idx[ b ] ) == g_PR->GetPlayerScore( v ) && g_PR->GetDeaths( idx[ b ] ) > g_PR->GetDeaths( v ) ) ) )
		{
			idx[ b + 1 ] = idx[ b ];
			--b;
		}
		idx[ b + 1 ] = v;
	}
	n = MIN( n, iMaxRows );

	const int iRow = S( 34 );
	const int iHead = S( 40 );
	const int h = S( 3 ) + iHead + n * iRow + S( 8 );

	surface()->DrawSetColor( Color( 20, 23, 28, 210 ) );
	surface()->DrawFilledRect( x, y, x + w, y + h );
	surface()->DrawSetColor( colTeam );
	surface()->DrawFilledRect( x, y, x + w, y + S( 3 ) );

	// Colunas (da direita para a esquerda)
	const int xPing = x + w - S( 18 );
	const int xMortes = xPing - S( 90 );
	const int xAbates = xMortes - S( 90 );
	const int hy = y + S( 16 );
	Text( m_hHeading, x + S( 18 ), hy - S( 4 ), colTeam, pszTitle );
	TextRight( m_hSmall, xAbates, hy, COR_TEXTO3, "ABATES" );
	TextRight( m_hSmall, xMortes, hy, COR_TEXTO3, "MORTES" );
	TextRight( m_hSmall, xPing, hy, COR_TEXTO3, "PING" );

	int ry = y + S( 3 ) + iHead;
	const int bodyTall = surface()->GetFontTall( m_hBody );
	for ( int r = 0; r < n; ++r )
	{
		const int i = idx[ r ];
		if ( g_PR->IsLocalPlayer( i ) )
		{
			const Color acc = Accent();
			surface()->DrawSetColor( Color( acc.r(), acc.g(), acc.b(), 26 ) );
			surface()->DrawFilledRect( x + 1, ry, x + w - 1, ry + iRow );
		}
		surface()->DrawSetColor( Color( 255, 255, 255, 13 ) );
		surface()->DrawFilledRect( x + S( 6 ), ry, x + w - S( 6 ), ry + 1 );

		const int ty = ry + ( iRow - bodyTall ) / 2;
		const bool bAlive = g_PR->IsAlive( i );
		const Color colName = bAlive ? COR_TEXTO : COR_TEXTO3;
		const char *pszName = g_PR->GetPlayerName( i );
		Text( m_hBody, x + S( 18 ), ty, colName, pszName );
		int nx = x + S( 18 ) + TextWide( m_hBody, pszName ) + S( 10 );
		if ( g_PR->IsFakePlayer( i ) )
		{
			Text( m_hSmall, nx, ty + S( 2 ), COR_TEXTO3, "BOT" );
			nx += TextWide( m_hSmall, "BOT" ) + S( 8 );
		}
		if ( !bAlive )
			Text( m_hSmall, nx, ty + S( 2 ), COR_TEXTO3, "morto" );

		char sz[ 16 ];
		V_snprintf( sz, sizeof( sz ), "%d", g_PR->GetPlayerScore( i ) );
		TextRight( m_hBody, xAbates, ty, COR_TEXTO, sz );
		V_snprintf( sz, sizeof( sz ), "%d", g_PR->GetDeaths( i ) );
		TextRight( m_hBody, xMortes, ty, COR_TEXTO2, sz );
		if ( g_PR->IsFakePlayer( i ) )
			V_strncpy( sz, "—", sizeof( sz ) );
		else
			V_snprintf( sz, sizeof( sz ), "%d", g_PR->GetPing( i ) );
		TextRight( m_hBody, xPing, ty, COR_TEXTO3, sz );
		ry += iRow;
	}
	return h;
}

void CTrokScoreboard::Paint( void )
{
	if ( !g_PR )
		return;

	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	if ( GetWide() != sw || GetTall() != sh )
		SetBounds( 0, 0, sw, sh );
	m_flScale = sh / 720.0f;

	surface()->DrawSetColor( Color( 11, 13, 16, 200 ) );
	surface()->DrawFilledRect( 0, 0, sw, sh );

	// Coluna central (não espicha em telas largas)
	const int w = MIN( sw - S( 120 ), S( 1040 ) );
	const int x = ( sw - w ) / 2;
	int y = S( 44 );

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const bool bTeams = HL2MPRules() && HL2MPRules()->IsTeamplay();

	// Cabeçalho
	const Color acc = Accent();
	Text( m_hSmall, x, y, acc, bTeams ? "MATA-MATA EM EQUIPE" : "MATA-MATA" );
	char szMap[ 128 ] = "";
	if ( engine->GetLevelName() )
	{
		V_FileBase( engine->GetLevelName(), szMap, sizeof( szMap ) );
		V_strupr( szMap );
	}
	Text( m_hTitle, x, y + S( 18 ), COR_TEXTO, szMap );

	char szTime[ 16 ] = "";
	const float flLeft = HL2MPRules() ? HL2MPRules()->GetMapRemainingTime() : 0.0f;
	if ( flLeft > 0.0f )
		V_snprintf( szTime, sizeof( szTime ), "%02d:%02d", (int)flLeft / 60, (int)flLeft % 60 );

	int iMyTeam = pLocal ? pLocal->GetTeamNumber() : TEAM_COMBINE;
	if ( iMyTeam != TEAM_COMBINE && iMyTeam != TEAM_REBELS )
		iMyTeam = TEAM_COMBINE;
	const int iOther = ( iMyTeam == TEAM_COMBINE ) ? TEAM_REBELS : TEAM_COMBINE;

	int xr = x + w;
	if ( bTeams )
	{
		C_Team *pMine = GetGlobalTeam( iMyTeam );
		C_Team *pOther = GetGlobalTeam( iOther );
		char a[ 16 ], b[ 16 ];
		V_snprintf( a, sizeof( a ), "%d", pMine ? pMine->Get_Score() : 0 );
		V_snprintf( b, sizeof( b ), "%d", pOther ? pOther->Get_Score() : 0 );
		const int sy = y + S( 6 );
		xr -= TextWide( m_hScore, b );
		Text( m_hScore, xr, sy, COR_INIMIGO, b );
		if ( szTime[ 0 ] )
		{
			xr -= S( 18 ) + TextWide( m_hBody, szTime );
			Text( m_hBody, xr, sy + S( 12 ), COR_TEXTO2, szTime );
		}
		xr -= S( 18 ) + TextWide( m_hScore, a );
		Text( m_hScore, xr, sy, COR_ALIADO, a );
	}
	else if ( szTime[ 0 ] )
	{
		TextRight( m_hBody, xr, y + S( 18 ), COR_TEXTO2, szTime );
	}

	y += S( 88 );

	if ( bTeams )
	{
		const int iRows = MAX( 1, ( sh - y - S( 120 ) ) / 2 / S( 34 ) );
		y += PaintTeam( x, y, w, iMyTeam, "SEU TIME", COR_ALIADO, iRows ) + S( 16 );
		y += PaintTeam( x, y, w, iOther, "INIMIGOS", COR_INIMIGO, iRows ) + S( 16 );
	}
	else
	{
		const int iRows = MAX( 1, ( sh - y - S( 80 ) ) / S( 34 ) );
		y += PaintTeam( x, y, w, -1, "JOGADORES", COR_TEXTO, iRows ) + S( 16 );
	}

	// Espectadores numa linha
	char szSpec[ 512 ] = "";
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( g_PR->IsConnected( i ) && g_PR->GetTeam( i ) == TEAM_SPECTATOR )
		{
			if ( szSpec[ 0 ] )
				V_strncat( szSpec, ", ", sizeof( szSpec ) );
			V_strncat( szSpec, g_PR->GetPlayerName( i ), sizeof( szSpec ) );
		}
	}
	if ( szSpec[ 0 ] && y < sh - S( 40 ) )
	{
		char szLine[ 600 ];
		V_snprintf( szLine, sizeof( szLine ), "Assistindo: %s", szSpec );
		Text( m_hSmall, x, y, COR_TEXTO3, szLine );
	}
}

IViewPortPanel *TrokScoreboard_Create( IViewPort *pViewPort )
{
	return new CTrokScoreboard( pViewPort );
}
