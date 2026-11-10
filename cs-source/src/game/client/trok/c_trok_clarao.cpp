//=============================================================================
// Trok: clarão da granada de luz (XM84 do CA; Chimera grenades.md seção 10).
//
// O servidor cria uma entidade trok_clarao mandada só para a vítima, com a hora em que
// a tela fica branca e o tempo T. Aqui a tela fica branca por cima do HUD e some como
// w(t) = cos(pi t / T): some toda na metade de T (de frente e perto: T = 25 s, some em
// 12,5 s; de costas: T = 6,25 s, some em ~3,1 s). Sem zumbido (o CA desta versão não tem).
// Só cliente; não precisa de HudLayout.res (o painel ocupa a tela inteira).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "c_baseplayer.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_clarao_cor( "trok_clarao_cor", "255", FCVAR_ARCHIVE, "Brilho (0-255) do branco do clarão." );

class C_TrokClarao : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_TrokClarao, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_TrokClarao() { m_flInicio = 0.0f; m_flDuracao = 0.0f; s_Lista.AddToTail( this ); }
	virtual ~C_TrokClarao() { s_Lista.FindAndRemove( this ); }

	virtual bool ShouldDraw( void ) { return false; }

	float m_flInicio;
	float m_flDuracao;

	static CUtlVector< C_TrokClarao * > s_Lista;
};

CUtlVector< C_TrokClarao * > C_TrokClarao::s_Lista;

IMPLEMENT_CLIENTCLASS_DT( C_TrokClarao, DT_TrokClarao, CTrokClarao )
	RecvPropFloat( RECVINFO( m_flInicio ) ),
	RecvPropFloat( RECVINFO( m_flDuracao ) ),
END_RECV_TABLE()

// Brancura agora (0..1): o clarão mais novo vale (um novo substitui o anterior).
static float TrokClarao_Branco( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !pLocal->IsAlive() )
		return 0.0f;

	const C_TrokClarao *pMaisNovo = NULL;
	for ( int i = 0; i < C_TrokClarao::s_Lista.Count(); ++i )
	{
		const C_TrokClarao *p = C_TrokClarao::s_Lista[ i ];
		if ( p->m_flDuracao > 0.0f && gpGlobals->curtime >= p->m_flInicio && ( !pMaisNovo || p->m_flInicio > pMaisNovo->m_flInicio ) )
			pMaisNovo = p;
	}
	if ( !pMaisNovo )
		return 0.0f;

	const float t = gpGlobals->curtime - pMaisNovo->m_flInicio;
	const float T = pMaisNovo->m_flDuracao;
	if ( t >= T * 0.5f )
		return 0.0f;
	return clamp( cosf( M_PI_F * t / T ), 0.0f, 1.0f );
}

class CHudTrokClarao : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokClarao, Panel );

public:
	CHudTrokClarao( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokClarao" )
	{
		SetParent( g_pClientMode->GetViewport() );
	}

	virtual bool ShouldDraw( void ) { return TrokClarao_Branco() > 0.0f; }

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme )
	{
		BaseClass::ApplySchemeSettings( pScheme );
		SetPaintBackgroundEnabled( false );
		SetMouseInputEnabled( false );
		SetKeyBoardInputEnabled( false );
		int w, h;
		surface()->GetScreenSize( w, h );
		SetBounds( 0, 0, w, h );
		SetZPos( 1000 );	// por cima do resto do HUD, como no CA
	}

	virtual void Paint( void )
	{
		const float w = TrokClarao_Branco();
		if ( w <= 0.0f )
			return;
		const int c = clamp( trok_clarao_cor.GetInt(), 0, 255 );
		int sw, sh;
		GetSize( sw, sh );
		surface()->DrawSetColor( c, c, c, (int)( 255.0f * w ) );
		surface()->DrawFilledRect( 0, 0, sw, sh );
	}
};

DECLARE_HUDELEMENT( CHudTrokClarao );
