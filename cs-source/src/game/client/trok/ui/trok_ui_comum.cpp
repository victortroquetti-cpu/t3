//=============================================================================
// Trok: base comum dos painéis próprios dentro da partida. Ver trok_ui_comum.h.
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "ienginevgui.h"
#include "inetchannelinfo.h"
#include "hl2mp_gamerules.h"
#include "trok/trok_surto.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

namespace TrokUI
{
	const Color COR_FUNDO( 11, 13, 16, 255 );
	const Color COR_PAINEL( 20, 23, 28, 210 );
	const Color COR_PAINEL_HOVER( 28, 32, 39, 235 );
	const Color COR_TEXTO( 236, 238, 241, 255 );
	const Color COR_TEXTO2( 163, 169, 177, 255 );
	const Color COR_TEXTO3( 128, 134, 142, 255 );
	const Color COR_LINHA( 255, 255, 255, 22 );
	const Color COR_LINHA_FORTE( 255, 255, 255, 60 );
	const Color COR_ALIADO( 90, 162, 255, 255 );
	const Color COR_INIMIGO( 255, 90, 90, 255 );
	const Color COR_ZUMBI( 217, 54, 43, 255 );	// infecção = vermelho (decisão do dono)

	Color Destaque( void )
	{
		static ConVarRef s_destaque( "trok_ui_destaque" );
		int r = 242, g = 181, b = 68;
		if ( s_destaque.IsValid() )
			sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
		return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
	}

	Color CorDoTime( int iTeam )
	{
		if ( iTeam != TEAM_COMBINE && iTeam != TEAM_REBELS )
			return COR_TEXTO2;
		if ( TrokSurto_Ativo() )
			return ( iTeam == TEAM_TROK_ZUMBIS ) ? COR_ZUMBI : COR_ALIADO;
		if ( !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
			return COR_TEXTO2;
		C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
		int iMeu = pLocal ? pLocal->GetTeamNumber() : TEAM_UNASSIGNED;
		if ( iMeu != TEAM_COMBINE && iMeu != TEAM_REBELS )
			return ( iTeam == TEAM_REBELS ) ? COR_ALIADO : COR_INIMIGO; // espectador: cor fixa por time
		return ( iTeam == iMeu ) ? COR_ALIADO : COR_INIMIGO;
	}

	const char *NomeDoTime( int iTeam )
	{
		if ( TrokSurto_Ativo() )
		{
			if ( iTeam == TEAM_TROK_HUMANOS ) return "HUMANOS";
			if ( iTeam == TEAM_TROK_ZUMBIS ) return "INFECTADOS";
		}
		switch ( iTeam )
		{
		case TEAM_COMBINE: return "MERCENÁRIOS";
		case TEAM_REBELS: return "REMANESCENTES";
		case TEAM_SPECTATOR: return "ESPECTADORES";
		default: return "";
		}
	}

	bool ModoComTimes( void )
	{
		if ( !HL2MPRules() || !HL2MPRules()->IsTeamplay() )
			return false;
		return TrokSurto_Estado() == NULL;
	}

	bool ServidorLocal( void )
	{
		INetChannelInfo *pInfo = engine->GetNetChannelInfo();
		return pInfo && pInfo->IsLoopback();
	}

	HScheme Esquema( void )
	{
		static HScheme s_hScheme = 0;
		if ( !s_hScheme )
			s_hScheme = scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" );
		return s_hScheme;
	}

	HFont Fonte( const char *pszNome )
	{
		IScheme *pScheme = scheme()->GetIScheme( Esquema() );
		return pScheme ? pScheme->GetFont( pszNome, true ) : INVALID_FONT;
	}

	const wchar_t *W( const char *pszUTF8 )
	{
		static wchar_t s_buf[ 8 ][ 512 ];
		static int s_i = 0;
		s_i = ( s_i + 1 ) % 8;
		V_UTF8ToUnicode( pszUTF8, s_buf[ s_i ], sizeof( s_buf[ s_i ] ) );
		return s_buf[ s_i ];
	}
}

using namespace TrokUI;

// Fonte das mensagens do centro e das dicas (patches em vgui_centerstringpanel.cpp e hud_hintdisplay.cpp).
HFont TrokUI_FonteCentro( void )
{
	return Fonte( "TrokCentro" );
}

//-----------------------------------------------------------------------------
// ESC: fecha o painel do Trok que estiver aberto (time, boas-vindas).
//-----------------------------------------------------------------------------
bool TrokUI_CaptureESC( void )
{
	if ( !gViewPortInterface || !engine->IsInGame() || enginevgui->IsGameUIVisible() )
		return false;
	static const char *s_pszPaineis[] = { PANEL_TEAM, PANEL_INFO };
	for ( int i = 0; i < ARRAYSIZE( s_pszPaineis ); ++i )
	{
		IViewPortPanel *pPanel = gViewPortInterface->FindPanelByName( s_pszPaineis[ i ] );
		if ( pPanel && pPanel->IsVisible() )
		{
			gViewPortInterface->ShowPanel( pPanel, false );
			return true;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
// CTrokPainel
//-----------------------------------------------------------------------------
CTrokPainel::CTrokPainel( const char *pszNome ) : BaseClass( NULL, pszNome )
{
	m_hLogo = m_hTitulo = m_hItem = m_hCabecalho = m_hBotao = m_hCorpo = m_hPequena = m_hNumero = INVALID_FONT;
	m_iLargura = m_iAltura = 1;
	m_flEscala = 1.0f;
	m_bModoClique = false;
	m_bCliqueTratado = false;
	m_iMouseX = m_iMouseY = -1;
	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetScheme( Esquema() );
}

void CTrokPainel::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hLogo = pScheme->GetFont( "TrokLogo", true );
	m_hTitulo = pScheme->GetFont( "TrokTitle", true );
	m_hItem = pScheme->GetFont( "TrokItem", true );
	m_hCabecalho = pScheme->GetFont( "TrokHeading", true );
	m_hBotao = pScheme->GetFont( "TrokButton", true );
	m_hCorpo = pScheme->GetFont( "TrokBody", true );
	m_hPequena = pScheme->GetFont( "TrokSmall", true );
	m_hNumero = pScheme->GetFont( "TrokHudNumber", true );
	SetPaintBackgroundEnabled( false );
}

void CTrokPainel::Paint( void )
{
	int sw, sh;
	surface()->GetScreenSize( sw, sh );
	if ( GetWide() != sw || GetTall() != sh )
		SetBounds( 0, 0, sw, sh );
	m_iLargura = sw;
	m_iAltura = sh;
	m_flEscala = sh / 720.0f; // concept em 1280x720
	m_bModoClique = false;
	Desenhar();
}

void CTrokPainel::OnCursorMoved( int x, int y )
{
	m_iMouseX = x;
	m_iMouseY = y;
}

void CTrokPainel::OnMousePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;
	input()->GetCursorPos( m_iMouseX, m_iMouseY );
	ScreenToLocal( m_iMouseX, m_iMouseY );
	m_bModoClique = true;
	m_bCliqueTratado = false;
	Desenhar();
	m_bModoClique = false;
	if ( m_bCliqueTratado )
	{
		static ConVarRef s_som( "trok_ui_som_clique" );
		if ( s_som.IsValid() && s_som.GetString()[ 0 ] )
			surface()->PlaySound( s_som.GetString() );
	}
}

void CTrokPainel::Texto( HFont hFont, int x, int y, const Color &col, const char *psz )
{
	if ( m_bModoClique || !psz || !psz[ 0 ] )
		return;
	const wchar_t *pwsz = W( psz );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( pwsz, V_wcslen( pwsz ) );
}

void CTrokPainel::TextoDir( HFont hFont, int xDir, int y, const Color &col, const char *psz )
{
	Texto( hFont, xDir - Largura( hFont, psz ), y, col, psz );
}

void CTrokPainel::TextoCentro( HFont hFont, int xCentro, int y, const Color &col, const char *psz )
{
	Texto( hFont, xCentro - Largura( hFont, psz ) / 2, y, col, psz );
}

// Letra a letra (DrawPrintText por letra: o DrawUnicodeChar tem defeito, nota do NT;RE).
void CTrokPainel::TextoEspacado( HFont hFont, int x, int y, const Color &col, const char *psz, int iEspaco )
{
	if ( m_bModoClique || !psz )
		return;
	const wchar_t *pwsz = W( psz );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	for ( const wchar_t *p = pwsz; *p; ++p )
	{
		wchar_t ch[ 2 ] = { *p, 0 };
		int cw, ct;
		surface()->GetTextSize( hFont, ch, cw, ct );
		surface()->DrawSetTextPos( x, y );
		surface()->DrawPrintText( ch, 1 );
		x += cw + iEspaco;
	}
}

int CTrokPainel::Largura( HFont hFont, const char *psz )
{
	if ( !psz || !psz[ 0 ] )
		return 0;
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, W( psz ), w, t );
	return w;
}

int CTrokPainel::Altura( HFont hFont )
{
	return surface()->GetFontTall( hFont );
}

void CTrokPainel::Ret( int x, int y, int w, int h, const Color &col )
{
	if ( m_bModoClique )
		return;
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( x, y, x + w, y + h );
}

void CTrokPainel::Contorno( int x, int y, int w, int h, const Color &col )
{
	if ( m_bModoClique )
		return;
	surface()->DrawSetColor( col );
	surface()->DrawOutlinedRect( x, y, x + w, y + h );
}

bool CTrokPainel::Sobre( int x, int y, int w, int h ) const
{
	return m_iMouseX >= x && m_iMouseX < x + w && m_iMouseY >= y && m_iMouseY < y + h;
}

bool CTrokPainel::Clicou( int x, int y, int w, int h )
{
	if ( !m_bModoClique || m_bCliqueTratado || !Sobre( x, y, w, h ) )
		return false;
	m_bCliqueTratado = true;
	return true;
}

bool CTrokPainel::Botao( int x, int y, int w, int h, HFont hFont, const char *pszRotulo, bool bCheio, const char *pszTecla )
{
	const bool bSobre = Sobre( x, y, w, h );
	const Color acc = Destaque();
	if ( bCheio )
	{
		Color col = acc;
		if ( bSobre )
			col = Color( MIN( 255, acc.r() + 13 ), MIN( 255, acc.g() + 13 ), MIN( 255, acc.b() + 13 ), 255 );
		Ret( x, y, w, h, col );
	}
	else
	{
		Ret( x, y, w, h, bSobre ? COR_PAINEL_HOVER : COR_PAINEL );
		Contorno( x, y, w, h, bSobre ? COR_LINHA_FORTE : COR_LINHA );
	}
	const Color colTexto = bCheio ? COR_FUNDO : ( bSobre ? COR_TEXTO : COR_TEXTO2 );
	int tx = x + ( w - Largura( hFont, pszRotulo ) ) / 2;
	if ( pszTecla && pszTecla[ 0 ] )
	{
		Texto( m_hPequena, x + S( 12 ), y + ( h - Altura( m_hPequena ) ) / 2, bCheio ? COR_FUNDO : COR_TEXTO3, pszTecla );
		tx = MAX( tx, x + S( 12 ) + Largura( m_hPequena, pszTecla ) + S( 10 ) );
	}
	Texto( hFont, tx, y + ( h - Altura( hFont ) ) / 2, colTexto, pszRotulo );
	return Clicou( x, y, w, h );
}

void CTrokPainel::Dica( int &x, int y, const char *pszTecla, const char *pszRotulo )
{
	const int kw = Largura( m_hPequena, pszTecla ) + S( 12 );
	const int kh = Altura( m_hPequena ) + S( 4 );
	Contorno( x, y - S( 2 ), kw, kh, COR_LINHA_FORTE );
	Texto( m_hPequena, x + S( 6 ), y, COR_TEXTO, pszTecla );
	x += kw + S( 8 );
	Texto( m_hPequena, x, y, COR_TEXTO3, pszRotulo );
	x += Largura( m_hPequena, pszRotulo ) + S( 24 );
}
