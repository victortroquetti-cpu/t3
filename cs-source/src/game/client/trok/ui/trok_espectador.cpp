//=============================================================================
// Trok: painel de espectador (PANEL_SPECGUI) no visual do mod, no lugar do
// CHL2MPSpectatorGUI (patch em clientmode_hl2mpnormal.cpp).
//
// Mostra quem você está assistindo (nome, time, vida), em que câmera (1ª ou 3ª pessoa;
// não existe câmera livre, ver trok_espectador_livre em server/player.cpp) e as teclas:
// atirar = próximo, mirar = anterior, pular = trocar câmera (ClientModeShared::
// HandleSpectatorKeyInput). O menu de espectador do Source (agachar) fica desligado.
// Não desenha nada na câmera da morte (a tela de morte do HUD cuida disso).
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "c_playerresource.h"
#include "c_baseplayer.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace TrokUI;

class CTrokEspectador : public CTrokPainel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CTrokEspectador, CTrokPainel );
public:
	CTrokEspectador( IViewPort *pViewPort );

	virtual const char *GetName( void ) { return PANEL_SPECGUI; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return false; }
	virtual void ShowPanel( bool bShow );
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_SPECTATOR; }
	virtual VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( VPANEL parent ) { BaseClass::SetParent( parent ); }

protected:
	virtual void Desenhar( void );

private:
	void TeclaDe( const char *pszBind, const char *pszPadrao, char *pszOut, int iOutSize );
};

CTrokEspectador::CTrokEspectador( IViewPort *pViewPort ) : BaseClass( PANEL_SPECGUI )
{
	NOTE_UNUSED( pViewPort );
	SetVisible( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CTrokEspectador::ShowPanel( bool bShow )
{
	if ( BaseClass::IsVisible() == bShow )
		return;
	SetVisible( bShow );
}

// Nome da tecla ligada ao comando, em caixa alta (ex.: "MOUSE1"); se não houver, o padrão.
void CTrokEspectador::TeclaDe( const char *pszBind, const char *pszPadrao, char *pszOut, int iOutSize )
{
	const char *pszTecla = engine->Key_LookupBinding( pszBind );
	V_strncpy( pszOut, ( pszTecla && pszTecla[ 0 ] ) ? pszTecla : pszPadrao, iOutSize );
	V_strupr( pszOut );
	if ( !V_stricmp( pszOut, "SPACE" ) )
		V_strncpy( pszOut, "ESPAÇO", iOutSize );
}

void CTrokEspectador::Desenhar( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !engine->IsInGame() )
		return;
	const int iModo = pLocal->GetObserverMode();
	if ( iModo == OBS_MODE_NONE || iModo == OBS_MODE_DEATHCAM || iModo == OBS_MODE_FREEZECAM )
		return;

	const int sw = m_iLargura, sh = m_iAltura;
	const Color acc = Destaque();
	const bool bEspectador = pLocal->GetTeamNumber() == TEAM_SPECTATOR || pLocal->GetTeamNumber() == TEAM_UNASSIGNED;

	// Faixa de cima: quem está só assistindo vê que pode entrar.
	if ( bEspectador )
	{
		TextoCentro( m_hPequena, sw / 2, S( 28 ), acc, "VOCÊ ESTÁ ASSISTINDO" );
		if ( ModoComTimes() )
		{
			char szM[ 32 ];
			TeclaDe( "trok_menu_time", "M", szM, sizeof( szM ) );
			char sz[ 96 ];
			V_snprintf( sz, sizeof( sz ), "%s  escolher time", szM );
			TextoCentro( m_hCorpo, sw / 2, S( 46 ), COR_TEXTO2, sz );
		}
	}

	// Cartão de baixo: quem você assiste.
	C_BasePlayer *pAlvo = ToBasePlayer( pLocal->GetObserverTarget() );
	if ( pAlvo == pLocal )
		pAlvo = NULL;
	const int cw = S( 420 ), ch = S( 76 );
	const int cx = ( sw - cw ) / 2, cy = sh - S( 64 ) - ch;
	Ret( cx, cy, cw, ch, COR_PAINEL );
	Contorno( cx, cy, cw, ch, COR_LINHA );

	if ( pAlvo && g_PR )
	{
		const int idx = pAlvo->entindex();
		const int iTime = g_PR->GetTeam( idx );
		const Color colTime = CorDoTime( iTime );
		Ret( cx, cy, S( 3 ), ch, colTime );

		Texto( m_hPequena, cx + S( 20 ), cy + S( 12 ), COR_TEXTO3, "ASSISTINDO" );
		const char *pszTime = NomeDoTime( iTime );
		if ( pszTime[ 0 ] )
			Texto( m_hPequena, cx + S( 20 ) + Largura( m_hPequena, "ASSISTINDO" ) + S( 10 ), cy + S( 12 ), colTime, pszTime );
		Texto( m_hItem, cx + S( 20 ), cy + S( 28 ), COR_TEXTO, g_PR->GetPlayerName( idx ) );

		// Vida (barra + número) à direita
		const bool bVivo = g_PR->IsAlive( idx );
		int iVida = pAlvo->IsDormant() ? g_PR->GetHealth( idx ) : pAlvo->GetHealth();
		iVida = bVivo ? MAX( 0, iVida ) : 0;
		const int iMax = MAX( 100, iVida );
		const int bw = S( 120 ), bx = cx + cw - S( 20 ) - bw, by = cy + S( 48 );
		char sz[ 32 ];
		V_snprintf( sz, sizeof( sz ), bVivo ? "%d" : "MORTO", iVida );
		TextoDir( m_hCabecalho, cx + cw - S( 20 ), cy + S( 18 ), bVivo ? COR_TEXTO : COR_TEXTO3, sz );
		Texto( m_hPequena, bx, cy + S( 22 ), COR_TEXTO3, "VIDA" );
		Ret( bx, by, bw, S( 4 ), COR_LINHA );
		if ( bVivo )
		{
			const Color colVida = ( iVida * 4 <= iMax ) ? COR_INIMIGO : COR_TEXTO;
			Ret( bx, by, bw * iVida / iMax, S( 4 ), colVida );
		}
	}
	else
	{
		Ret( cx, cy, S( 3 ), ch, COR_LINHA_FORTE );
		Texto( m_hPequena, cx + S( 20 ), cy + S( 12 ), COR_TEXTO3, "ASSISTINDO" );
		Texto( m_hItem, cx + S( 20 ), cy + S( 28 ), COR_TEXTO2, "Ninguém para assistir" );
	}

	// Teclas (as mesmas que o ClientModeShared trata no modo espectador)
	{
		char szProx[ 32 ], szAnt[ 32 ], szCam[ 32 ];
		TeclaDe( "+attack", "MOUSE1", szProx, sizeof( szProx ) );
		TeclaDe( "+attack2", "MOUSE2", szAnt, sizeof( szAnt ) );
		TeclaDe( "+jump", "SPACE", szCam, sizeof( szCam ) );
		const bool bPrimeira = ( iModo == OBS_MODE_IN_EYE );
		const char *pszCam = bPrimeira ? "Ver em 3ª pessoa" : "Ver em 1ª pessoa";
		const int ty = sh - S( 44 );
		const int iTotal = Largura( m_hPequena, szProx ) + Largura( m_hPequena, "Próximo" ) +
			Largura( m_hPequena, szAnt ) + Largura( m_hPequena, "Anterior" ) +
			Largura( m_hPequena, szCam ) + Largura( m_hPequena, pszCam ) + S( ( 12 + 8 + 24 ) * 3 - 24 );
		int x = ( sw - iTotal ) / 2;
		Dica( x, ty, szProx, "Próximo" );
		Dica( x, ty, szAnt, "Anterior" );
		Dica( x, ty, szCam, pszCam );
	}
}

IViewPortPanel *TrokEspectador_Create( IViewPort *pViewPort )
{
	return new CTrokEspectador( pViewPort );
}
