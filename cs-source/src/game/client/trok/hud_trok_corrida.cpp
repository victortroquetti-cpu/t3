//=============================================================================
// Trok: HUD da Corrida Armada (trok_modo corrida; regras em shared/trok/trok_corrida.cpp).
//
// - embaixo do placar do topo: a arma atual e o degrau ("ARMA 3/7  SUBMETRALHADORA") e a
//   próxima, com quantos abates faltam e os abates/meta ("PRÓXIMA: ESCOPETA EM 3  ·  7/15");
//   no último degrau, "ÚLTIMA ARMA" (o CA avisa cada jogador do próximo degrau);
// - faixa no meio da tela por alguns segundos quando alguém chega na última arma (o CA avisa
//   todo mundo) e, no fim, quem venceu.
// Só lê o estado em rede (trok_corrida_estado) e a escada de scripts/trok_corrida.txt.
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "trok/trok_corrida.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_corrida_hud( "trok_corrida_hud", "1", FCVAR_ARCHIVE, "Corrida Armada: mostra a arma atual, a próxima e os avisos." );
static ConVar trok_corrida_hud_aviso( "trok_corrida_hud_aviso", "3", FCVAR_ARCHIVE, "Corrida Armada: segundos do aviso de quem chegou na última arma." );

static const Color COR_BRANCO( 236, 238, 241, 255 );
static const Color COR_CINZA( 190, 196, 204, 220 );
static const Color COR_DESTAQUE( 242, 181, 68, 255 );

class CHudTrokCorrida : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokCorrida, Panel );

public:
	CHudTrokCorrida( const char *pElementName );

	virtual bool ShouldDraw( void );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * ScreenHeight() / 720.0f + 0.5f ); }
	void TextoCentro( HFont hFont, int cx, int y, const Color &col, const char *pszUTF8 );
	void Faixa( const char *pszUTF8, int y );

	HFont m_hTexto, m_hPequena, m_hGrande;
};

DECLARE_HUDELEMENT( CHudTrokCorrida );

CHudTrokCorrida::CHudTrokCorrida( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokCorrida" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	m_hTexto = m_hPequena = m_hGrande = INVALID_FONT;
}

void CHudTrokCorrida::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
	m_hTexto = pScheme->GetFont( "TrokHudText", true );
	m_hPequena = pScheme->GetFont( "TrokHudSmall", true );
	m_hGrande = pScheme->GetFont( "TrokHudScore", true );
}

bool CHudTrokCorrida::ShouldDraw( void )
{
	if ( !trok_corrida_hud.GetBool() || engine->IsLevelMainMenuBackground() || !TrokCorrida_Ativo() )
		return false;
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;
	return CHudElement::ShouldDraw();
}

void CHudTrokCorrida::TextoCentro( HFont hFont, int cx, int y, const Color &col, const char *pszUTF8 )
{
	if ( hFont == INVALID_FONT || !pszUTF8[ 0 ] )
		return;
	wchar_t wsz[ 160 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( cx - w / 2, y );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
}

void CHudTrokCorrida::Faixa( const char *pszUTF8, int y )
{
	if ( m_hGrande == INVALID_FONT )
		return;
	wchar_t wsz[ 160 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( m_hGrande, wsz, w, t );
	const int cx = ScreenWidth() / 2;
	surface()->DrawSetColor( Color( 11, 13, 16, 170 ) );
	surface()->DrawFilledRect( cx - w / 2 - S( 18 ), y - S( 6 ), cx + w / 2 + S( 18 ), y + t + S( 6 ) );
	surface()->DrawSetColor( COR_DESTAQUE );
	surface()->DrawFilledRect( cx - w / 2 - S( 18 ), y - S( 6 ), cx + w / 2 + S( 18 ), y - S( 4 ) );
	TextoCentro( m_hGrande, cx, y, COR_BRANCO, pszUTF8 );
}

void CHudTrokCorrida::Paint( void )
{
	CTrokCorridaEstado *pEstado = TrokCorrida_Estado();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pEstado || !pLocal || !g_PR )
		return;

	const int cx = ScreenWidth() / 2;
	char sz[ 160 ];

	// Fim: quem venceu.
	if ( pEstado->IsFim() )
	{
		const int v = pEstado->GetVencedor();
		if ( v > 0 && g_PR->IsConnected( v ) )
		{
			if ( v == pLocal->entindex() )
				V_strncpy( sz, "VOCÊ VENCEU A CORRIDA", sizeof( sz ) );
			else
				V_snprintf( sz, sizeof( sz ), "%s VENCEU A CORRIDA", g_PR->GetPlayerName( v ) );
		}
		else
		{
			V_strncpy( sz, "FIM DA CORRIDA", sizeof( sz ) );
		}
		Faixa( sz, S( 170 ) );
		return;
	}

	// Alguém chegou na última arma (para todos).
	const int u = pEstado->GetUltimoJogador();
	if ( u > 0 && gpGlobals->curtime - pEstado->GetUltimoHora() < trok_corrida_hud_aviso.GetFloat() &&
		 gpGlobals->curtime >= pEstado->GetUltimoHora() && g_PR->IsConnected( u ) )
	{
		if ( u == pLocal->entindex() )
			V_strncpy( sz, "VOCÊ ESTÁ NA ÚLTIMA ARMA", sizeof( sz ) );
		else
			V_snprintf( sz, sizeof( sz ), "%s ESTÁ NA ÚLTIMA ARMA", g_PR->GetPlayerName( u ) );
		Faixa( sz, S( 170 ) );
	}

	if ( !pLocal->IsAlive() || pLocal->GetTeamNumber() == TEAM_SPECTATOR )
		return;

	const TrokCorridaEscada_t *pEscada = TrokCorrida_Escada( pEstado->GetMeta() );
	if ( !pEscada || pEscada->nDegraus <= 0 )
		return;
	const int i = pLocal->entindex();
	const int iDegrau = clamp( pEstado->GetDegrau( i ), 0, pEscada->nDegraus - 1 );
	const int iAbates = pEstado->GetAbates( i );
	const bool bUltima = ( iDegrau == pEscada->nDegraus - 1 );

	// Arma atual: a que ele recebeu (pode ser a própria, pelo sorteio do CA); senão o nome do degrau.
	char szArma[ 64 ];
	V_strncpy( szArma, pEscada->degraus[ iDegrau ].szNome, sizeof( szArma ) );
	C_BaseCombatWeapon *pArma = pLocal->GetWeapon( 0 );
	if ( pArma && pArma->GetPrintName() && pArma->GetPrintName()[ 0 ] && pArma->GetPrintName()[ 0 ] != '#' )
	{
		V_strncpy( szArma, pArma->GetPrintName(), sizeof( szArma ) );
		V_strupr( szArma );
	}

	int y = S( 22 ) + S( 92 );
	V_snprintf( sz, sizeof( sz ), "ARMA %d/%d   %s", iDegrau + 1, pEscada->nDegraus, szArma );
	TextoCentro( m_hTexto, cx, y, bUltima ? COR_DESTAQUE : COR_BRANCO, sz );
	y += m_hTexto != INVALID_FONT ? surface()->GetFontTall( m_hTexto ) : S( 20 );

	if ( bUltima )
	{
		V_snprintf( sz, sizeof( sz ), "ÚLTIMA ARMA   ·   %d/%d", iAbates, pEstado->GetMeta() );
	}
	else
	{
		const TrokCorridaDegrau_t &prox = pEscada->degraus[ iDegrau + 1 ];
		const int iFaltam = MAX( 1, prox.iAbates - iAbates );
		V_snprintf( sz, sizeof( sz ), "PRÓXIMA: %s EM %d   ·   %d/%d", prox.szNome, iFaltam, iAbates, pEstado->GetMeta() );
	}
	TextoCentro( m_hPequena, cx, y, COR_CINZA, sz );
}
