//=============================================================================
// Trok: marcador de acerto (hitmarker) e de abate.
//
// Ouve o evento "player_hurt" que o servidor já manda a cada dano. Quando
// quem atirou é o jogador local, desenha quatro traços curtos em volta do
// centro da tela por um instante e toca um som. Se o tiro matou (vida 0),
// o marcador fica maior, vermelho e dura mais.
// Só cliente; não precisa de HudLayout.res (o painel ocupa a tela inteira).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "trok/hud_trok_v2.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hitmarker( "trok_hitmarker", "1", FCVAR_ARCHIVE, "Mostra o marcador de acerto." );
static ConVar trok_hitmarker_tempo( "trok_hitmarker_tempo", "0.18", FCVAR_ARCHIVE, "Duração (s) do marcador de acerto." );
static ConVar trok_hitmarker_tamanho( "trok_hitmarker_tamanho", "7", FCVAR_ARCHIVE, "Comprimento (px a 1080p) de cada traço." );
static ConVar trok_hitmarker_vao( "trok_hitmarker_vao", "5", FCVAR_ARCHIVE, "Distância (px a 1080p) entre o centro e o começo do traço." );
static ConVar trok_hitmarker_som( "trok_hitmarker_som", "buttons/lightswitch2.wav", FCVAR_ARCHIVE, "Som do acerto (caminho dentro de sound/). Vazio = sem som." );
static ConVar trok_hitmarker_som_abate( "trok_hitmarker_som_abate", "buttons/button15.wav", FCVAR_ARCHIVE, "Som do abate. Vazio = sem som." );

class CHudTrokHitmarker : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokHitmarker, Panel );

public:
	CHudTrokHitmarker( const char *pElementName );

	virtual void Init( void );
	virtual void Reset( void );
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	float m_flShowUntil;
	float m_flDuration;
	bool m_bKill;
};

DECLARE_HUDELEMENT( CHudTrokHitmarker );

CHudTrokHitmarker::CHudTrokHitmarker( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokHitmarker" )
{
	SetParent( g_pClientMode->GetViewport() );
	m_flShowUntil = 0.0f;
	m_flDuration = 0.0f;
	m_bKill = false;
}

void CHudTrokHitmarker::Init( void )
{
	ListenForGameEvent( "player_hurt" );
	Reset();
}

void CHudTrokHitmarker::Reset( void )
{
	m_flShowUntil = 0.0f;
}

void CHudTrokHitmarker::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );

	int w, h;
	surface()->GetScreenSize( w, h );
	SetBounds( 0, 0, w, h );
}

bool CHudTrokHitmarker::ShouldDraw( void )
{
	// Com o HUD novo o desenho é dele (hud_trok_v2.cpp); aqui fica só o som.
	return trok_hitmarker.GetBool() && !TrokHudV2_Ativo() && gpGlobals->curtime < m_flShowUntil && CHudElement::ShouldDraw();
}

void CHudTrokHitmarker::FireGameEvent( IGameEvent *event )
{
	if ( Q_strcmp( event->GetName(), "player_hurt" ) )
		return;

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;

	int iAttacker = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
	int iVictim = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
	if ( iAttacker != pLocal->entindex() || iVictim == pLocal->entindex() || iVictim <= 0 )
		return;

	m_bKill = event->GetInt( "health" ) <= 0;
	m_flDuration = trok_hitmarker_tempo.GetFloat() * ( m_bKill ? 2.2f : 1.0f );
	m_flShowUntil = gpGlobals->curtime + m_flDuration;

	const char *pszSound = m_bKill ? trok_hitmarker_som_abate.GetString() : trok_hitmarker_som.GetString();
	if ( pszSound && pszSound[ 0 ] )
		surface()->PlaySound( pszSound );
}

void CHudTrokHitmarker::Paint( void )
{
	float flLeft = m_flShowUntil - gpGlobals->curtime;
	if ( flLeft <= 0.0f || m_flDuration <= 0.0f )
		return;

	// Some no último terço do tempo.
	float flAlpha = clamp( flLeft / ( m_flDuration * 0.35f ), 0.0f, 1.0f );

	int w, h;
	GetSize( w, h );
	int cx = w / 2, cy = h / 2;

	float flScale = h / 1080.0f;
	int iGap = (int)( trok_hitmarker_vao.GetFloat() * flScale + 0.5f );
	int iLen = (int)( trok_hitmarker_tamanho.GetFloat() * flScale * ( m_bKill ? 1.5f : 1.0f ) + 0.5f );
	int iThick = MAX( 1, (int)( 2.0f * flScale + 0.5f ) );

	Color color = m_bKill ? Color( 235, 60, 50, (int)( 255 * flAlpha ) ) : Color( 245, 245, 245, (int)( 235 * flAlpha ) );

	// Quatro traços diagonais (um "X" aberto no meio). Cada traço é feito de
	// quadradinhos para ter espessura em qualquer resolução.
	static const int s_dirs[ 4 ][ 2 ] = { { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
	surface()->DrawSetColor( Color( 0, 0, 0, (int)( 120 * flAlpha ) ) );
	for ( int d = 0; d < 4; ++d )
	{
		for ( int i = iGap; i < iGap + iLen; ++i )
		{
			int x = cx + s_dirs[ d ][ 0 ] * i, y = cy + s_dirs[ d ][ 1 ] * i;
			surface()->DrawFilledRect( x - iThick / 2 - 1, y - iThick / 2 - 1, x + iThick / 2 + 2, y + iThick / 2 + 2 );
		}
	}
	surface()->DrawSetColor( color );
	for ( int d = 0; d < 4; ++d )
	{
		for ( int i = iGap; i < iGap + iLen; ++i )
		{
			int x = cx + s_dirs[ d ][ 0 ] * i, y = cy + s_dirs[ d ][ 1 ] * i;
			surface()->DrawFilledRect( x - iThick / 2, y - iThick / 2, x + iThick / 2 + 1, y + iThick / 2 + 1 );
		}
	}
}
