//=============================================================================
// Trok: dinheiro no HUD (economia estilo CS; design/economia.md).
//
// Só aparece com a economia ligada (trok_economia 1, lido da entidade trok_placar_extra):
// dinheiro em cima da vida, embaixo à esquerda; "+$300" quando ganha e "-$2.500" quando
// gasta; no tempo de compra, quantos segundos faltam e a tecla da loja; colete e capacete.
// Painel de tela cheia, sem HudLayout.res (como hud_trok_hitmarker.cpp).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "trok/trok_placar_extra.h"
#include "trok/trok_surto.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hud_dinheiro( "trok_hud_dinheiro", "1", FCVAR_ARCHIVE, "Mostra o dinheiro no HUD quando a economia está ligada." );

class CHudTrokEconomia : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokEconomia, Panel );
public:
	CHudTrokEconomia( const char *pElementName );
	virtual void Init( void ) { Reset(); }
	virtual void VidInit( void ) { Reset(); }
	virtual void Reset( void ) { m_iUltimo = -1; m_flDeltaTempo = 0.0f; m_iDelta = 0; }
	virtual bool ShouldDraw( void );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * ScreenHeight() / 720.0f + 0.5f ); }
	void Texto( HFont h, int x, int y, const Color &c, const char *psz );
	int Largura( HFont h, const char *psz );

	HFont m_hNum, m_hPequena;
	int m_iUltimo;
	int m_iDelta;
	float m_flDeltaTempo;
};

DECLARE_HUDELEMENT( CHudTrokEconomia );

CHudTrokEconomia::CHudTrokEconomia( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokEconomia" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	m_hNum = m_hPequena = INVALID_FONT;
	Reset();
}

void CHudTrokEconomia::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hNum = pScheme->GetFont( "TrokHudScore", true );
	m_hPequena = pScheme->GetFont( "TrokHudSmall", true );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
}

bool CHudTrokEconomia::ShouldDraw( void )
{
	if ( !trok_hud_dinheiro.GetBool() || engine->IsLevelMainMenuBackground() )
		return false;
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pPlacar || !pPlacar->IsEconomia() || !pLocal || pLocal->GetTeamNumber() == TEAM_SPECTATOR )
		return false;
	return CHudElement::ShouldDraw();
}

void CHudTrokEconomia::Texto( HFont h, int x, int y, const Color &c, const char *psz )
{
	wchar_t wsz[ 128 ];
	V_UTF8ToUnicode( psz, wsz, sizeof( wsz ) );
	surface()->DrawSetTextFont( h );
	surface()->DrawSetTextColor( c );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
}

int CHudTrokEconomia::Largura( HFont h, const char *psz )
{
	wchar_t wsz[ 128 ];
	V_UTF8ToUnicode( psz, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( h, wsz, w, t );
	return w;
}

void CHudTrokEconomia::Paint( void )
{
	if ( GetWide() != ScreenWidth() || GetTall() != ScreenHeight() )
		SetSize( ScreenWidth(), ScreenHeight() );
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pPlacar || !pLocal )
		return;
	const int i = pLocal->entindex();
	const int iDin = pPlacar->GetDinheiro( i );

	// Ganhou ou gastou: mostra a diferença por 2 s.
	if ( m_iUltimo >= 0 && iDin != m_iUltimo )
	{
		m_iDelta = iDin - m_iUltimo;
		m_flDeltaTempo = gpGlobals->curtime;
	}
	m_iUltimo = iDin;

	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = 242, g = 181, b = 68;
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	const Color acc( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
	const Color cinza( 190, 196, 204, 200 );

	// Acima do bloco da vida (hud_trok.cpp desenha a vida em x 32, rente ao rodapé).
	const int x = S( 32 );
	const int y = ScreenHeight() - S( 150 );
	char sz[ 64 ];
	V_snprintf( sz, sizeof( sz ), "$ %d", iDin );
	Texto( m_hNum, x, y, acc, sz );
	int xd = x + Largura( m_hNum, sz ) + S( 10 );

	const float flIdade = gpGlobals->curtime - m_flDeltaTempo;
	if ( m_iDelta != 0 && flIdade >= 0.0f && flIdade < 2.0f )
	{
		const int a = flIdade > 1.5f ? (int)( 255 * ( 2.0f - flIdade ) / 0.5f ) : 255;
		V_snprintf( sz, sizeof( sz ), "%s$%d", m_iDelta > 0 ? "+" : "-", abs( m_iDelta ) );
		Texto( m_hPequena, xd, y + S( 6 ), m_iDelta > 0 ? Color( 120, 210, 120, a ) : Color( 255, 90, 90, a ), sz );
	}

	// Tempo de compra e equipamento, embaixo do dinheiro.
	const int y2 = y + surface()->GetFontTall( m_hNum );
	int x2 = x;
	const float flFalta = pPlacar->GetCompraAte( i ) - gpGlobals->curtime;
	if ( flFalta > 0.0f && pLocal->IsAlive() && !TrokSurto_IsZumbi( pLocal ) )
	{
		V_snprintf( sz, sizeof( sz ), "COMPRA %d s  [B]", (int)ceilf( flFalta ) );
		Texto( m_hPequena, x2, y2, cinza, sz );
		x2 += Largura( m_hPequena, sz ) + S( 12 );
	}
	const int iCol = pPlacar->GetColete( i ), iCap = pPlacar->GetCapacete( i );
	if ( iCol > 0 )
	{
		V_snprintf( sz, sizeof( sz ), "COLETE %d", iCol );
		Texto( m_hPequena, x2, y2, cinza, sz );
		x2 += Largura( m_hPequena, sz ) + S( 12 );
	}
	if ( iCap > 0 )
		Texto( m_hPequena, x2, y2, cinza, "CAPACETE" );
}
