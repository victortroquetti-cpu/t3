//=============================================================================
// Trok: aviso "E — PEGAR <ARMA>" quando o jogador olha para uma arma no chão a até
// trok_pegar_distancia (como no CA). A conta da mira é a mesma do servidor
// (shared/trok/trok_inventario.cpp). Painel de tela cheia próprio, sem HudLayout.
// Também guarda, a cada quadro, a última arma usada em cada classe (teclas 1-6).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_basecombatweapon.h"
#include "trok/trok_inventario.h"
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hud_pegar( "trok_hud_pegar", "1", FCVAR_ARCHIVE, "Mostra o aviso para pegar arma do chão (E)." );

class CHudTrokPegar : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokPegar, Panel );
public:
	CHudTrokPegar( const char *pElementName );
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual bool ShouldDraw( void );
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	HFont m_hFonte;
	wchar_t m_wszTexto[ 96 ];
};

DECLARE_HUDELEMENT( CHudTrokPegar );

CHudTrokPegar::CHudTrokPegar( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokPegar" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	SetHiddenBits( 0 );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	m_hFonte = INVALID_FONT;
	m_wszTexto[ 0 ] = 0;
}

void CHudTrokPegar::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hFonte = pScheme->GetFont( "TrokHudText", true );
	if ( m_hFonte == INVALID_FONT )
		m_hFonte = pScheme->GetFont( "Default", true );
	SetPaintBackgroundEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
}

bool CHudTrokPegar::ShouldDraw( void )
{
	return C_BasePlayer::GetLocalPlayer() && !engine->IsLevelMainMenuBackground() && CHudElement::ShouldDraw();
}

void CHudTrokPegar::OnThink( void )
{
	TrokInventario_ClienteFrame();

	m_wszTexto[ 0 ] = 0;
	if ( !trok_hud_pegar.GetBool() )
		return;
	C_BaseCombatWeapon *pArma = TrokInventario_ArmaMirada( C_BasePlayer::GetLocalPlayer() );
	if ( !pArma )
		return;

	char szNome[ 64 ];
	V_strncpy( szNome, pArma->GetPrintName(), sizeof( szNome ) );
	if ( szNome[ 0 ] == '#' )
		V_strncpy( szNome, pArma->GetClassname(), sizeof( szNome ) );
	V_strupr( szNome );
	char szTexto[ 96 ];
	V_snprintf( szTexto, sizeof( szTexto ), "E  —  PEGAR %s", szNome );
	g_pVGuiLocalize->ConvertANSIToUnicode( szTexto, m_wszTexto, sizeof( m_wszTexto ) );
}

void CHudTrokPegar::Paint( void )
{
	if ( !m_wszTexto[ 0 ] || m_hFonte == INVALID_FONT )
		return;
	int w, h;
	surface()->GetTextSize( m_hFonte, m_wszTexto, w, h );
	const int x = ( GetWide() - w ) / 2;
	const int y = (int)( GetTall() * 0.62f );
	surface()->DrawSetTextFont( m_hFonte );
	// sombra + texto
	surface()->DrawSetTextColor( Color( 0, 0, 0, 200 ) );
	surface()->DrawSetTextPos( x + 2, y + 2 );
	surface()->DrawPrintText( m_wszTexto, wcslen( m_wszTexto ) );
	surface()->DrawSetTextColor( Color( 235, 235, 225, 255 ) );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( m_wszTexto, wcslen( m_wszTexto ) );
}
