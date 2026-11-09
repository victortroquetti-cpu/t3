//=============================================================================
// Trok: tela de carregamento (concept "Carregando", design/ui.md).
//
// Receita do NEOTOKYO Rebuild (src/game/client/neo/ui/neo_loading.cpp, Source 1 SDK
// License): o painel é entregue ao GameUI com IGameUI::SetLoadingBackgroundDialog e
// recebe "Activate"/"deactivate". O diálogo de carregamento do GameUI continua sendo
// quem recebe o progresso e as mensagens do motor; ele é tirado da tela e este painel
// lê a barra ("Progress") e o texto ("InfoLabel") dele e desenha no visual do Trok.
// Carregando x desconectado: o NT;RE compara o título do diálogo (precisa de um
// acessor que o nosso Frame não tem); aqui a barra de progresso visível = carregando.
//=============================================================================

#include "cbase.h"
#include "ienginevgui.h"
#include "GameUI/IGameUI.h"
#include "tier1/KeyValues.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui/IPanel.h>
#include <vgui/IVGui.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/ProgressBar.h>
#include <vgui_controls/Label.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static const char *s_pszDicas[] = {
	"Segure pular contra uma caixa ou borda para escalar.",
	"Caminhando com C seus passos não fazem barulho, e os bots não ouvem.",
	"Correndo, a arma fica abaixada. Pare um instante antes de atirar.",
	"Mirar pelo cano fecha a dispersão e diminui o recuo.",
	"Com a faca na mão você corre mais rápido.",
	"Agachado, os tiros espalham menos.",
	"Tiros seguidos abrem a mira. De longe, rajadas curtas acertam mais.",
	"Os bots notam quem se mexe na luz bem antes de quem espera parado na sombra.",
};

class CTrokLoading : public Panel
{
	DECLARE_CLASS_SIMPLE( CTrokLoading, Panel );
public:
	CTrokLoading();

	virtual void OnMessage( const KeyValues *params, VPANEL fromPanel );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	void FetchGameUIPanels( void );
	int S( float px ) const { return (int)( px * m_flScale + 0.5f ); }
	void Text( HFont hFont, int x, int y, const Color &col, const wchar_t *pwsz );
	void Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 );
	int TextWide( HFont hFont, const wchar_t *pwsz );
	Color Accent( void ) const;

	VPANEL m_hProgress, m_hInfo;
	ProgressBar *m_pProgress;
	Label *m_pInfo;
	HFont m_hLogo, m_hMap, m_hBody, m_hSmall, m_hHeading;
	float m_flScale;
	int m_iDica;
};

static CTrokLoading *g_pTrokLoading = NULL;

CTrokLoading::CTrokLoading() : BaseClass( NULL, "TrokLoading" )
{
	SetParent( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
	SetVisible( false );
	MakePopup( false );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	m_hProgress = m_hInfo = 0;
	m_pProgress = NULL;
	m_pInfo = NULL;
	m_hLogo = m_hMap = m_hBody = m_hSmall = m_hHeading = INVALID_FONT;
	m_flScale = 1.0f;
	m_iDica = 0;
	InvalidateLayout( false, true );
}

void CTrokLoading::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hLogo = pScheme->GetFont( "TrokLogo", true );
	m_hMap = pScheme->GetFont( "TrokLoadingMap", true );
	m_hHeading = pScheme->GetFont( "TrokHeading", true );
	m_hBody = pScheme->GetFont( "TrokBody", true );
	m_hSmall = pScheme->GetFont( "TrokSmall", true );
	SetPaintBackgroundEnabled( false );
	int w, h;
	surface()->GetScreenSize( w, h );
	SetBounds( 0, 0, w, h );
}

void CTrokLoading::OnMessage( const KeyValues *params, VPANEL fromPanel )
{
	BaseClass::OnMessage( params, fromPanel );
	if ( !V_strcmp( params->GetName(), "Activate" ) )
	{
		FetchGameUIPanels();
		m_iDica = RandomInt( 0, ARRAYSIZE( s_pszDicas ) - 1 );
	}
}

// BaseGameUIPanel > LoadingDialog > Progress / InfoLabel (nomes do GameUI).
void CTrokLoading::FetchGameUIPanels( void )
{
	m_hProgress = m_hInfo = 0;
	m_pProgress = NULL;
	m_pInfo = NULL;

	VPANEL root = enginevgui->GetPanel( PANEL_GAMEUIDLL );
	VPANEL base = 0;
	for ( int i = 0; i < ipanel()->GetChildCount( root ); ++i )
	{
		VPANEL c = ipanel()->GetChild( root, i );
		if ( !V_strcmp( ipanel()->GetName( c ), "BaseGameUIPanel" ) )
		{
			base = c;
			break;
		}
	}
	if ( !base )
		return;

	VPANEL dialog = 0;
	for ( int i = 0; i < ipanel()->GetChildCount( base ); ++i )
	{
		VPANEL c = ipanel()->GetChild( base, i );
		if ( !V_strcmp( ipanel()->GetName( c ), "LoadingDialog" ) )
		{
			dialog = c;
			break;
		}
	}
	if ( !dialog )
		return;

	for ( int i = 0; i < ipanel()->GetChildCount( dialog ); ++i )
	{
		VPANEL c = ipanel()->GetChild( dialog, i );
		const char *pszName = ipanel()->GetName( c );
		Panel *pPanel = ipanel()->GetPanel( c, "GAMEUI" );
		if ( !pPanel )
			continue;
		if ( !V_strcmp( pszName, "Progress" ) )
		{
			m_hProgress = c;
			m_pProgress = static_cast< ProgressBar * >( pPanel );
		}
		else if ( !V_strcmp( pszName, "InfoLabel" ) )
		{
			m_hInfo = c;
			m_pInfo = static_cast< Label * >( pPanel );
		}
	}

	// Tira o diálogo do GameUI da tela; ele continua recebendo o progresso.
	ipanel()->SetParent( dialog, 0 );
}

Color CTrokLoading::Accent( void ) const
{
	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = 242, g = 181, b = 68;
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
}

void CTrokLoading::Text( HFont hFont, int x, int y, const Color &col, const wchar_t *pwsz )
{
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( pwsz, V_wcslen( pwsz ) );
}

void CTrokLoading::Text( HFont hFont, int x, int y, const Color &col, const char *pszUTF8 )
{
	wchar_t wsz[ 256 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	Text( hFont, x, y, col, wsz );
}

int CTrokLoading::TextWide( HFont hFont, const wchar_t *pwsz )
{
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, pwsz, w, t );
	return w;
}

void CTrokLoading::Paint( void )
{
	int w, h;
	surface()->GetScreenSize( w, h );
	if ( GetWide() != w || GetTall() != h )
		SetBounds( 0, 0, w, h );
	m_flScale = h / 720.0f;

	// Carregando o mapa de fundo do menu: não cobre nada.
	if ( engine->IsLevelMainMenuBackground() )
		return;

	if ( !m_pProgress || !m_pInfo )
		FetchGameUIPanels();

	const bool bLoading = m_hProgress && ipanel()->IsVisible( m_hProgress );
	const Color acc = Accent();
	const Color colText( 236, 238, 241, 255 ), colText2( 163, 169, 177, 255 ), colText3( 128, 134, 142, 255 );

	// Fundo: escuro, mais claro no alto (a arte do mapa entra aqui quando existir).
	surface()->DrawSetColor( Color( 11, 13, 16, 255 ) );
	surface()->DrawFilledRect( 0, 0, w, h );
	surface()->DrawSetColor( Color( 27, 34, 43, 255 ) );
	surface()->DrawFilledRectFade( 0, 0, w, h * 7 / 10, 255, 0, false );

	// Marca
	surface()->DrawSetColor( acc );
	surface()->DrawFilledRect( S( 64 ), S( 48 ), S( 64 ) + S( 8 ), S( 48 ) + S( 26 ) );
	Text( m_hLogo, S( 84 ), S( 44 ), colText, "REARMED" );

	wchar_t wszInfo[ 512 ] = L"";
	if ( m_pInfo )
		m_pInfo->GetText( wszInfo, sizeof( wszInfo ) );

	if ( !bLoading )
	{
		// Desconectado / erro do servidor: a mensagem do motor no nosso visual.
		const int x = S( 64 );
		int y = h / 2 - S( 60 );
		Text( m_hSmall, x, y, acc, "CONEXÃO ENCERRADA" );
		y += S( 22 );
		Text( m_hHeading, x, y, colText, wszInfo[ 0 ] ? wszInfo : L"Desconectado." );
		y += S( 48 );
		Text( m_hSmall, x, y, colText3, "ESC para voltar" );
		return;
	}

	// Nome do mapa (o motor já sabe qual é durante o carregamento).
	char szMap[ 128 ] = "";
	const char *pszLevel = engine->GetLevelName();
	if ( pszLevel && pszLevel[ 0 ] )
	{
		V_FileBase( pszLevel, szMap, sizeof( szMap ) );
		V_strupr( szMap );
	}

	const int x = S( 64 );
	const int iMapTall = surface()->GetFontTall( m_hMap );
	const int yMap = h - S( 128 ) - iMapTall;
	Text( m_hSmall, x, yMap - S( 22 ), acc, szMap[ 0 ] ? "PRÓXIMA PARTIDA" : "CARREGANDO" );
	Text( m_hMap, x, yMap, colText, szMap[ 0 ] ? szMap : "REARMED" );

	// Dica
	{
		const int bw = S( 360 ), bh = S( 92 );
		const int bx = w - S( 56 ) - bw, by = h - S( 128 ) - bh;
		surface()->DrawSetColor( Color( 20, 23, 28, 200 ) );
		surface()->DrawFilledRect( bx, by, bx + bw, by + bh );
		surface()->DrawSetColor( Color( 255, 255, 255, 20 ) );
		surface()->DrawOutlinedRect( bx, by, bx + bw, by + bh );
		Text( m_hSmall, bx + S( 20 ), by + S( 16 ), colText3, "DICA" );

		// Quebra simples em duas linhas pela largura.
		wchar_t wszDica[ 256 ];
		V_UTF8ToUnicode( s_pszDicas[ m_iDica ], wszDica, sizeof( wszDica ) );
		const int maxW = bw - S( 40 );
		int iBreak = -1;
		const int iLen = V_wcslen( wszDica );
		for ( int i = 0; i < iLen; ++i )
		{
			if ( wszDica[ i ] != L' ' )
				continue;
			wchar_t c = wszDica[ i ];
			wszDica[ i ] = 0;
			const bool bFits = TextWide( m_hBody, wszDica ) <= maxW;
			wszDica[ i ] = c;
			if ( bFits )
				iBreak = i;
			else
				break;
		}
		if ( TextWide( m_hBody, wszDica ) <= maxW || iBreak < 0 )
		{
			Text( m_hBody, bx + S( 20 ), by + S( 38 ), colText, wszDica );
		}
		else
		{
			wszDica[ iBreak ] = 0;
			Text( m_hBody, bx + S( 20 ), by + S( 36 ), colText, wszDica );
			Text( m_hBody, bx + S( 20 ), by + S( 58 ), colText, wszDica + iBreak + 1 );
		}
	}

	// Progresso
	const float flProgress = m_pProgress ? clamp( m_pProgress->GetProgress(), 0.0f, 1.0f ) : 0.0f;
	const int px = S( 64 ), pw = w - S( 64 ) - S( 56 );
	const int py = h - S( 56 );
	Text( m_hSmall, px, py - S( 22 ), colText2, wszInfo );
	char szPct[ 16 ];
	V_snprintf( szPct, sizeof( szPct ), "%d%%", (int)( flProgress * 100.0f + 0.5f ) );
	wchar_t wszPct[ 16 ];
	V_UTF8ToUnicode( szPct, wszPct, sizeof( wszPct ) );
	Text( m_hSmall, px + pw - TextWide( m_hSmall, wszPct ), py - S( 22 ), colText2, wszPct );
	surface()->DrawSetColor( Color( 255, 255, 255, 30 ) );
	surface()->DrawFilledRect( px, py, px + pw, py + MAX( 2, S( 2 ) ) );
	surface()->DrawSetColor( acc );
	surface()->DrawFilledRect( px, py, px + (int)( pw * flProgress ), py + MAX( 2, S( 2 ) ) );
}

void TrokLoading_Create( IGameUI *pGameUI )
{
	if ( g_pTrokLoading || !pGameUI )
		return;
	g_pTrokLoading = new CTrokLoading();
	pGameUI->SetLoadingBackgroundDialog( g_pTrokLoading->GetVPanel() );
}
