//=============================================================================
// Trok: menu próprio no lugar do menu do GameUI (tela inicial, jogar contra bots e
// pausa dentro da partida). Visual dos concepts de UI (design/ui.md).
//
// Arquitetura baseada no NEOTOKYO Rebuild (src/game/client/neo/ui/neo_root.cpp e
// OverrideInterface.cpp, Source 1 SDK License):
// - um painel de tela cheia registrado com IGameUI::SetMainMenuOverride; o GameUI
//   mostra e esconde esse painel no lugar do menu dele (inclusive o ESC na partida);
// - o VGUI só entrega teclas a popups: um painel 1x1 invisível (CTrokMenuInput)
//   recebe as teclas e repassa ao menu;
// - CHLClient::HandleUiToggle pergunta TrokMenu_CaptureESC() para o ESC voltar das
//   subtelas em vez de fechar o menu.
// O desenho é imediato: Run() desenha (MODE_PAINT) e trata o clique (MODE_CLICK)
// com o mesmo código, então o que se vê é exatamente onde se clica.
//
// Ainda do GameUI (pendências em design/ui.md): opções, lista de servidores, criar
// servidor e caixas de confirmação. A tela de carregamento é trok_loading.cpp.
//=============================================================================

#include "cbase.h"
#include "trok_menu.h"
#include "ienginevgui.h"
#include "GameUI/IGameUI.h"
#include "filesystem.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "steam/steam_api.h"
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IScheme.h>
#include <vgui/IInput.h>
#include <vgui/IPanel.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_ui_destaque( "trok_ui_destaque", "242 181 68", FCVAR_ARCHIVE, "Cor de destaque do menu (R G B)." );
static ConVar trok_ui_bots_dificuldade( "trok_ui_bots_dificuldade", "1", FCVAR_ARCHIVE, "Menu: última dificuldade escolhida (0 a 3)." );
static ConVar trok_ui_bots_por_time( "trok_ui_bots_por_time", "5", FCVAR_ARCHIVE, "Menu: últimos bots por time." );
static ConVar trok_ui_bots_estilos( "trok_ui_bots_estilos", "31", FCVAR_ARCHIVE, "Menu: estilos marcados (bits: equilibrado, entry, lurker, ancora, suporte)." );
static ConVar trok_ui_mapa( "trok_ui_mapa", "", FCVAR_ARCHIVE, "Menu: último mapa escolhido." );
static ConVar trok_ui_modo( "trok_ui_modo", "0", FCVAR_ARCHIVE, "Menu: modo escolhido (0 = mata-mata em equipe, 1 = Surto)." );
// Sons do menu por caminho (no menu não há mapa carregado, então não dá para usar os nomes do
// game_sounds_trok.txt). TROCAR por sound/trok/ui/... quando houver sons próprios.
static ConVar trok_ui_som_clique( "trok_ui_som_clique", "ui/buttonclick.wav", FCVAR_ARCHIVE, "Som do clique no menu (dentro de sound/). Vazio = sem som." );
static ConVar trok_ui_som_passar( "trok_ui_som_passar", "ui/buttonrollover.wav", FCVAR_ARCHIVE, "Som ao passar o mouse num item do menu. Vazio = sem som." );

static CDllDemandLoader g_TrokGameUIDLL( "GameUI" );

//-----------------------------------------------------------------------------
// Cores do concept (fundo escuro, texto claro, um destaque).
//-----------------------------------------------------------------------------
static const Color COR_FUNDO( 11, 13, 16, 255 );
static const Color COR_TEXTO( 236, 238, 241, 255 );
static const Color COR_TEXTO2( 163, 169, 177, 255 );
static const Color COR_TEXTO3( 128, 134, 142, 255 );
static const Color COR_PAINEL( 20, 23, 28, 210 );
static const Color COR_PAINEL_HOVER( 28, 32, 39, 235 );
static const Color COR_LINHA( 255, 255, 255, 22 );
static const Color COR_LINHA_FORTE( 255, 255, 255, 60 );

// Estilos dos bots: mesmos nomes e pesos padrão de trok_bot_estilos (server/trok/bot/trok_bot_profile.cpp).
static const char *s_pszEstiloNome[] = { "equilibrado", "entry", "lurker", "ancora", "suporte" };
static const char *s_pszEstiloRotulo[] = { "Equilibrado", "Entry", "Lurker", "Âncora", "Suporte" };
static const int s_iEstiloPeso[] = { 30, 20, 15, 20, 15 };
static const int NUM_ESTILOS = ARRAYSIZE( s_pszEstiloNome );

static const char *s_pszNivelRotulo[] = { "FÁCIL", "NORMAL", "DIFÍCIL", "ELITE" };
static const char *s_pszNivelDescricao[] = {
	"Demora para notar e erra bastante. Bom para aprender o mapa.",
	"Reação humana média, mira com erro natural.",
	"Reage rápido, segura ângulos e joga em equipe.",
	"Nível competitivo: disciplina de tiro e jogadas de time.",
};

//-----------------------------------------------------------------------------
// Texto: as strings do código são UTF-8; o VGUI desenha wchar_t.
//-----------------------------------------------------------------------------
static const wchar_t *W( const char *pszUTF8 )
{
	static wchar_t s_buf[ 8 ][ 256 ];
	static int s_i = 0;
	s_i = ( s_i + 1 ) % 8;
	V_UTF8ToUnicode( pszUTF8, s_buf[ s_i ], sizeof( s_buf[ s_i ] ) );
	return s_buf[ s_i ];
}

class CTrokMenu;

//-----------------------------------------------------------------------------
// Painel 1x1 só para teclado (o VGUI não entrega teclas ao painel do menu).
//-----------------------------------------------------------------------------
class CTrokMenuInput : public Panel
{
	DECLARE_CLASS_SIMPLE( CTrokMenuInput, Panel );
public:
	CTrokMenuInput( CTrokMenu *pMenu );
	virtual void PerformLayout( void )
	{
		SetPos( 0, 0 );
		SetSize( 1, 1 );
	}
	virtual void OnKeyCodeTyped( KeyCode code );
private:
	CTrokMenu *m_pMenu;
};

//-----------------------------------------------------------------------------
// O menu.
//-----------------------------------------------------------------------------
class CTrokMenu : public Panel
{
	DECLARE_CLASS_SIMPLE( CTrokMenu, Panel );
public:
	enum State_t { STATE_ROOT = 0, STATE_PLAY };

	CTrokMenu();
	virtual ~CTrokMenu();

	IGameUI *GetGameUI( void );
	State_t GetState( void ) const { return m_eState; }
	bool IsShown( void ) { return ipanel()->IsFullyVisible( GetVPanel() ); }
	void OnRelayedKey( KeyCode code );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void OnThink( void );
	virtual void Paint( void );
	virtual void OnCursorMoved( int x, int y );
	virtual void OnMousePressed( MouseCode code );
	virtual void OnScreenSizeChanged( int iOldWide, int iOldTall );

private:
	enum Mode_t { MODE_PAINT = 0, MODE_CLICK };

	void SetState( State_t eState );
	bool InGame( void ) const { return engine->IsInGame() && !engine->IsLevelMainMenuBackground(); }
	Color Accent( void ) const;
	int S( float flPx ) const { return (int)( flPx * m_flScale + 0.5f ); }

	// Desenho
	void Text( HFont hFont, int x, int y, const Color &col, const char *psz );
	void TextSpaced( HFont hFont, int x, int y, const Color &col, const char *psz, int iSpacing );
	int TextWide( HFont hFont, const char *psz );
	void Rect( int x, int y, int w, int h, const Color &col );
	void Outline( int x, int y, int w, int h, const Color &col );
	void KeyHint( int &x, int y, const char *pszKey, const char *pszLabel );

	// Widgets (desenham no MODE_PAINT; no MODE_CLICK devolvem true se o clique foi neles)
	bool Hover( int x, int y, int w, int h ) const;
	bool Clicked( int x, int y, int w, int h );
	bool MenuItem( int iId, int x, int &y, const char *pszIndex, const char *pszLabel, const char *pszCaption, bool bBig );
	bool LinkButton( int x, int y, HFont hFont, const char *pszLabel );
	bool BoxButton( int x, int y, int w, int h, HFont hFont, const char *pszLabel, bool bOn, bool bFilled );
	void Label( int x, int y, const char *pszText );

	// Telas
	void Run( Mode_t eMode );
	void RunBackground( void );
	void RunRoot( void );
	void RunPlay( void );
	void RunBottomBar( void );

	// Ações
	void Command( const char *pszGameUICommand );
	void StartBotGame( void );
	void BuildMapList( void );

	CTrokMenuInput *m_pInput;
	IGameUI *m_pGameUI;
	State_t m_eState;
	Mode_t m_eMode;
	bool m_bClickHandled;
	bool m_bWasInGame;
	int m_iMouseX, m_iMouseY;
	int m_iWide, m_iTall;
	float m_flScale;
	int m_iHoverId, m_iLastHoverId;

	HFont m_hLogo, m_hTitle, m_hItem, m_hHeading, m_hButton, m_hBody, m_hSmall;

	// Jogar contra bots
	CUtlVector< CUtlString > m_Maps;
	CUtlVector< bool > m_MapHasNav;
	int m_iMap;
	bool m_bMapsBuilt;
};

static CTrokMenu *g_pTrokMenu = NULL;

void TrokLoading_Create( IGameUI *pGameUI ); // trok_loading.cpp

//-----------------------------------------------------------------------------
CTrokMenuInput::CTrokMenuInput( CTrokMenu *pMenu ) : BaseClass( pMenu, "TrokMenuInput" ), m_pMenu( pMenu )
{
	MakePopup( false );
	SetKeyBoardInputEnabled( true );
	SetMouseInputEnabled( false );
	SetPaintEnabled( false );
	SetPaintBackgroundEnabled( false );
	SetVisible( true );
	PerformLayout();
}

void CTrokMenuInput::OnKeyCodeTyped( KeyCode code )
{
	if ( code <= KEY_NONE )
		return; // teclas multimídia chegam como KEY_NONE (nota do NT;RE)
	m_pMenu->OnRelayedKey( code );
}

//-----------------------------------------------------------------------------
CTrokMenu::CTrokMenu() : BaseClass( NULL, "TrokMenu" )
{
	g_pTrokMenu = this;
	m_pGameUI = NULL;
	m_eState = STATE_ROOT;
	m_eMode = MODE_PAINT;
	m_bClickHandled = false;
	m_bWasInGame = false;
	m_iMouseX = m_iMouseY = -1;
	m_iWide = m_iTall = 1;
	m_flScale = 1.0f;
	m_iHoverId = m_iLastHoverId = -1;
	m_hLogo = m_hTitle = m_hItem = m_hHeading = m_hButton = m_hBody = m_hSmall = INVALID_FONT;
	m_iMap = 0;
	m_bMapsBuilt = false;

	SetProportional( false );
	SetPaintBackgroundEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetMouseInputEnabled( true );
	SetVisible( true );

	HScheme hScheme = scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" );
	SetScheme( hScheme );

	m_pInput = new CTrokMenuInput( this );
	ivgui()->AddTickSignal( GetVPanel(), 0 );
}

CTrokMenu::~CTrokMenu()
{
	if ( g_pTrokMenu == this )
		g_pTrokMenu = NULL;
}

IGameUI *CTrokMenu::GetGameUI( void )
{
	if ( !m_pGameUI )
	{
		CreateInterfaceFn factory = g_TrokGameUIDLL.GetFactory();
		if ( factory )
			m_pGameUI = (IGameUI *)factory( GAMEUI_INTERFACE_VERSION, NULL );
	}
	return m_pGameUI;
}

void CTrokMenu::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	m_hLogo = pScheme->GetFont( "TrokLogo", true );
	m_hTitle = pScheme->GetFont( "TrokTitle", true );
	m_hItem = pScheme->GetFont( "TrokItem", true );
	m_hHeading = pScheme->GetFont( "TrokHeading", true );
	m_hButton = pScheme->GetFont( "TrokButton", true );
	m_hBody = pScheme->GetFont( "TrokBody", true );
	m_hSmall = pScheme->GetFont( "TrokSmall", true );
	SetPaintBackgroundEnabled( false );
	InvalidateLayout();
}

void CTrokMenu::PerformLayout( void )
{
	BaseClass::PerformLayout();
	surface()->GetScreenSize( m_iWide, m_iTall );
	SetPos( 0, 0 );
	SetSize( m_iWide, m_iTall );
	// Concept em 1280x720: tudo escala pela altura da tela.
	m_flScale = m_iTall / 720.0f;
}

void CTrokMenu::OnScreenSizeChanged( int iOldWide, int iOldTall )
{
	BaseClass::OnScreenSizeChanged( iOldWide, iOldTall );
	InvalidateLayout( true, true );
}

void CTrokMenu::OnThink( void )
{
	BaseClass::OnThink();

	// Entrou ou saiu de uma partida: volta para a tela inicial.
	const bool bInGame = InGame();
	if ( bInGame != m_bWasInGame )
	{
		m_bWasInGame = bInGame;
		SetState( STATE_ROOT );
	}

	// Teclado no painel de repasse quando ninguém tem o foco (ex.: depois de fechar as opções).
	if ( IsShown() && input()->GetFocus() == 0 )
		m_pInput->RequestFocus();

	int w, h;
	surface()->GetScreenSize( w, h );
	if ( w != m_iWide || h != m_iTall )
		InvalidateLayout();
}

void CTrokMenu::SetState( State_t eState )
{
	m_eState = eState;
	m_iLastHoverId = -1;
	if ( eState == STATE_PLAY )
		BuildMapList();
	m_pInput->RequestFocus();
}

Color CTrokMenu::Accent( void ) const
{
	int r = 242, g = 181, b = 68;
	sscanf( trok_ui_destaque.GetString(), "%d %d %d", &r, &g, &b );
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
}

//-----------------------------------------------------------------------------
// Entrada
//-----------------------------------------------------------------------------
void CTrokMenu::OnCursorMoved( int x, int y )
{
	m_iMouseX = x;
	m_iMouseY = y;
}

void CTrokMenu::OnMousePressed( MouseCode code )
{
	if ( code != MOUSE_LEFT )
		return;
	m_bClickHandled = false;
	Run( MODE_CLICK );
	if ( m_bClickHandled && trok_ui_som_clique.GetString()[ 0 ] )
		surface()->PlaySound( trok_ui_som_clique.GetString() );
}

void CTrokMenu::OnRelayedKey( KeyCode code )
{
	if ( code == KEY_ESCAPE && m_eState != STATE_ROOT )
	{
		SetState( STATE_ROOT );
		return;
	}
	if ( ( code == KEY_ENTER || code == KEY_PAD_ENTER ) && m_eState == STATE_PLAY )
	{
		StartBotGame();
		return;
	}
	if ( code == KEY_BACKQUOTE )
		engine->ClientCmd_Unrestricted( "toggleconsole" );
}

bool CTrokMenu::Hover( int x, int y, int w, int h ) const
{
	return m_iMouseX >= x && m_iMouseX < x + w && m_iMouseY >= y && m_iMouseY < y + h;
}

bool CTrokMenu::Clicked( int x, int y, int w, int h )
{
	if ( m_eMode != MODE_CLICK || m_bClickHandled || !Hover( x, y, w, h ) )
		return false;
	m_bClickHandled = true;
	return true;
}

//-----------------------------------------------------------------------------
// Desenho básico
//-----------------------------------------------------------------------------
void CTrokMenu::Text( HFont hFont, int x, int y, const Color &col, const char *psz )
{
	if ( m_eMode != MODE_PAINT )
		return;
	const wchar_t *pwsz = W( psz );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x, y );
	surface()->DrawPrintText( pwsz, V_wcslen( pwsz ) );
}

// Letra a letra com espaçamento (títulos em caixa alta). DrawPrintText por letra:
// o DrawUnicodeChar tem defeito (nota do NT;RE).
void CTrokMenu::TextSpaced( HFont hFont, int x, int y, const Color &col, const char *psz, int iSpacing )
{
	if ( m_eMode != MODE_PAINT )
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
		x += cw + iSpacing;
	}
}

int CTrokMenu::TextWide( HFont hFont, const char *psz )
{
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, W( psz ), w, t );
	return w;
}

void CTrokMenu::Rect( int x, int y, int w, int h, const Color &col )
{
	if ( m_eMode != MODE_PAINT )
		return;
	surface()->DrawSetColor( col );
	surface()->DrawFilledRect( x, y, x + w, y + h );
}

void CTrokMenu::Outline( int x, int y, int w, int h, const Color &col )
{
	if ( m_eMode != MODE_PAINT )
		return;
	surface()->DrawSetColor( col );
	surface()->DrawOutlinedRect( x, y, x + w, y + h );
}

void CTrokMenu::Label( int x, int y, const char *pszText )
{
	Text( m_hSmall, x, y, COR_TEXTO3, pszText );
}

void CTrokMenu::KeyHint( int &x, int y, const char *pszKey, const char *pszLabel )
{
	const int kw = TextWide( m_hSmall, pszKey ) + S( 12 );
	const int kh = surface()->GetFontTall( m_hSmall ) + S( 4 );
	Outline( x, y - S( 2 ), kw, kh, COR_LINHA_FORTE );
	Text( m_hSmall, x + S( 6 ), y, COR_TEXTO, pszKey );
	x += kw + S( 8 );
	Text( m_hSmall, x, y, COR_TEXTO3, pszLabel );
	x += TextWide( m_hSmall, pszLabel ) + S( 24 );
}

//-----------------------------------------------------------------------------
// Widgets
//-----------------------------------------------------------------------------

// Item do menu da tela inicial: índice pequeno + rótulo grande; desliza ao passar o mouse.
bool CTrokMenu::MenuItem( int iId, int x, int &y, const char *pszIndex, const char *pszLabel, const char *pszCaption, bool bBig )
{
	HFont hFont = bBig ? m_hTitle : m_hItem;
	const int iFontTall = surface()->GetFontTall( hFont );
	const int iCapTall = pszCaption ? surface()->GetFontTall( m_hBody ) + S( 2 ) : 0;
	const int iRowTall = iFontTall + iCapTall + S( 12 );
	const int iRowWide = S( 36 ) + MAX( TextWide( hFont, pszLabel ) + S( 8 ) * (int)V_strlen( pszLabel ), S( 300 ) );

	const bool bHover = Hover( x, y, iRowWide, iRowTall );
	if ( bHover )
		m_iHoverId = iId;
	const bool bClicked = Clicked( x, y, iRowWide, iRowTall );

	const int iShift = bHover ? S( 8 ) : 0;
	const Color colLabel = ( bHover || bBig ) ? COR_TEXTO : COR_TEXTO2;
	Text( m_hSmall, x + iShift, y + iFontTall - surface()->GetFontTall( m_hSmall ) - S( 6 ), bBig ? Accent() : colLabel, pszIndex );
	TextSpaced( hFont, x + S( 36 ) + iShift, y, colLabel, pszLabel, S( bBig ? 3 : 2 ) );
	if ( pszCaption )
		Text( m_hBody, x + S( 36 ) + iShift, y + iFontTall + S( 2 ), COR_TEXTO2, pszCaption );

	y += iRowTall;
	return bClicked;
}

bool CTrokMenu::LinkButton( int x, int y, HFont hFont, const char *pszLabel )
{
	const int w = TextWide( hFont, pszLabel );
	const int h = surface()->GetFontTall( hFont );
	const bool bHover = Hover( x - S( 4 ), y - S( 4 ), w + S( 8 ), h + S( 8 ) );
	Text( hFont, x, y, bHover ? COR_TEXTO : COR_TEXTO2, pszLabel );
	return Clicked( x - S( 4 ), y - S( 4 ), w + S( 8 ), h + S( 8 ) );
}

bool CTrokMenu::BoxButton( int x, int y, int w, int h, HFont hFont, const char *pszLabel, bool bOn, bool bFilled )
{
	const bool bHover = Hover( x, y, w, h );
	const Color acc = Accent();
	if ( bFilled )
	{
		Color col = acc;
		if ( bHover )
			col = Color( MIN( 255, acc.r() + 13 ), MIN( 255, acc.g() + 13 ), MIN( 255, acc.b() + 13 ), 255 );
		Rect( x, y, w, h, col );
	}
	else if ( bOn )
	{
		Outline( x, y, w, h, acc );
	}
	else
	{
		Outline( x, y, w, h, bHover ? COR_LINHA_FORTE : COR_LINHA );
	}
	const int tw = TextWide( hFont, pszLabel );
	const int th = surface()->GetFontTall( hFont );
	const Color colText = bFilled ? COR_FUNDO : ( ( bOn || bHover ) ? COR_TEXTO : COR_TEXTO2 );
	Text( hFont, x + ( w - tw ) / 2, y + ( h - th ) / 2, colText, pszLabel );
	return Clicked( x, y, w, h );
}

//-----------------------------------------------------------------------------
// Telas
//-----------------------------------------------------------------------------
void CTrokMenu::Paint( void )
{
	m_iHoverId = -1;
	Run( MODE_PAINT );
	if ( m_iHoverId != -1 && m_iHoverId != m_iLastHoverId && trok_ui_som_passar.GetString()[ 0 ] )
		surface()->PlaySound( trok_ui_som_passar.GetString() );
	m_iLastHoverId = m_iHoverId;
}

void CTrokMenu::Run( Mode_t eMode )
{
	m_eMode = eMode;
	if ( eMode == MODE_PAINT )
		RunBackground();

	switch ( m_eState )
	{
	case STATE_PLAY: RunPlay(); break;
	default: RunRoot(); break;
	}

	RunBottomBar();
	m_eMode = MODE_PAINT;
}

void CTrokMenu::RunBackground( void )
{
	const int w = m_iWide, h = m_iTall;
	if ( InGame() )
	{
		// Pausa: escurece a partida e reforça o lado do menu.
		Rect( 0, 0, w, h, Color( 11, 13, 16, 150 ) );
	}
	else if ( !engine->IsLevelMainMenuBackground() )
	{
		// Sem mapa de fundo: fundo liso com um degradê leve (o mapa ao vivo entra quando existir).
		Rect( 0, 0, w, h, COR_FUNDO );
		surface()->DrawSetColor( Color( 27, 34, 43, 255 ) );
		surface()->DrawFilledRectFade( 0, 0, w, h * 6 / 10, 255, 0, false );
	}
	// Escurece à esquerda, onde fica o menu (por cima do mapa de fundo ou da partida).
	surface()->DrawSetColor( COR_FUNDO );
	surface()->DrawFilledRectFade( 0, 0, w * 68 / 100, h, 242, 0, true );
}

void CTrokMenu::RunRoot( void )
{
	const Color acc = Accent();
	const int x0 = S( 64 );

	// Marca
	Rect( x0, S( 48 ), S( 10 ), S( 34 ), acc );
	TextSpaced( m_hLogo, x0 + S( 24 ), S( 46 ), COR_TEXTO, "TROK", S( 9 ) );

	// Jogador (canto superior direito)
	{
		const char *pszName = "Jogador";
		if ( steamapicontext && steamapicontext->SteamFriends() )
			pszName = steamapicontext->SteamFriends()->GetPersonaName();
		const int iBox = S( 40 );
		const int iNameW = MAX( TextWide( m_hBody, pszName ), S( 120 ) );
		const int iCardW = S( 10 ) + iBox + S( 14 ) + iNameW + S( 14 );
		const int cx = m_iWide - S( 56 ) - iCardW;
		const int cy = S( 44 );
		Rect( cx, cy, iCardW, S( 60 ), COR_PAINEL );
		Outline( cx, cy, iCardW, S( 60 ), COR_LINHA );
		Rect( cx + S( 10 ), cy + S( 10 ), iBox, iBox, Color( 35, 42, 51, 255 ) );
		char szInitial[ 8 ];
		V_strncpy( szInitial, pszName, 2 );
		V_strupr( szInitial );
		const int iw = TextWide( m_hHeading, szInitial );
		Text( m_hHeading, cx + S( 10 ) + ( iBox - iw ) / 2, cy + S( 10 ) + ( iBox - surface()->GetFontTall( m_hHeading ) ) / 2, acc, szInitial );
		Text( m_hBody, cx + S( 10 ) + iBox + S( 14 ), cy + S( 14 ), COR_TEXTO, pszName );
		Text( m_hSmall, cx + S( 10 ) + iBox + S( 14 ), cy + S( 34 ), COR_TEXTO3, InGame() ? "EM PARTIDA" : "ONLINE" );
	}

	// Menu
	int y = S( 196 );
	if ( InGame() )
	{
		if ( MenuItem( 1, x0, y, "01", "CONTINUAR", "Voltar para a partida", true ) )
			Command( "ResumeGame" );
		y += S( 6 );
		if ( MenuItem( 2, x0, y, "02", "CONFIGURAÇÕES", NULL, false ) )
			Command( "OpenOptionsDialog" );
		if ( MenuItem( 3, x0, y, "03", "DESCONECTAR", NULL, false ) )
			Command( "Disconnect" );
		if ( MenuItem( 4, x0, y, "04", "SAIR", NULL, false ) )
			engine->ClientCmd_Unrestricted( "quit\n" );
	}
	else
	{
		if ( MenuItem( 1, x0, y, "01", "JOGAR", "Contra bots, offline", true ) )
			SetState( STATE_PLAY );
		y += S( 6 );
		if ( MenuItem( 2, x0, y, "02", "SERVIDORES", NULL, false ) )
			Command( "OpenServerBrowser" );
		if ( MenuItem( 3, x0, y, "03", "CRIAR PARTIDA", NULL, false ) )
			Command( "OpenCreateMultiplayerGameDialog" );
		if ( MenuItem( 4, x0, y, "04", "CONFIGURAÇÕES", NULL, false ) )
			Command( "OpenOptionsDialog" );
		if ( MenuItem( 5, x0, y, "05", "SAIR", NULL, false ) )
			engine->ClientCmd_Unrestricted( "quit\n" );

		// Novidade (canto inferior direito)
		const int nw = S( 320 ), nh = S( 120 );
		const int nx = m_iWide - S( 56 ) - nw, ny = m_iTall - S( 84 ) - nh;
		Rect( nx, ny, nw, nh, COR_PAINEL );
		Outline( nx, ny, nw, nh, COR_LINHA );
		Text( m_hSmall, nx + S( 16 ), ny + S( 16 ), acc, "NOVIDADE" );
		TextSpaced( m_hHeading, nx + S( 16 ), ny + S( 34 ), COR_TEXTO, "BOTS COM ESTILO PRÓPRIO", S( 1 ) );
		Text( m_hBody, nx + S( 16 ), ny + S( 66 ), COR_TEXTO2, "Entry, lurker, âncora e suporte." );
		Text( m_hBody, nx + S( 16 ), ny + S( 86 ), COR_TEXTO2, "O time divide as entradas." );
	}
}

void CTrokMenu::RunPlay( void )
{
	const Color acc = Accent();
	const int x0 = S( 64 );
	const int xr = m_iWide - S( 56 );

	// Cabeçalho
	if ( LinkButton( x0, S( 52 ), m_hSmall, "‹ VOLTAR" ) )
	{
		SetState( STATE_ROOT );
		return;
	}
	const int iSep = x0 + TextWide( m_hSmall, "‹ VOLTAR" ) + S( 20 );
	Rect( iSep, S( 46 ), 1, S( 22 ), COR_LINHA_FORTE );
	TextSpaced( m_hTitle, iSep + S( 21 ), S( 36 ), COR_TEXTO, "JOGAR", S( 3 ) );

	// Cartões de modo
	{
		const char *pszNome[] = { "CONTRA BOTS", "SERVIDORES", "CRIAR PARTIDA" };
		const char *pszDesc[] = { "Você e seu time contra a IA, offline.", "Lista de servidores com filtros.", "Seu servidor, suas regras." };
		const int iGap = S( 16 );
		const int cw = ( xr - x0 - iGap * 2 ) / 3;
		const int ch = S( 108 );
		for ( int i = 0; i < 3; ++i )
		{
			const int cx = x0 + i * ( cw + iGap ), cy = S( 112 );
			const bool bHover = Hover( cx, cy, cw, ch );
			Rect( cx, cy, cw, ch, bHover ? COR_PAINEL_HOVER : COR_PAINEL );
			Outline( cx, cy, cw, ch, bHover ? COR_LINHA_FORTE : COR_LINHA );
			if ( i == 0 )
				Rect( cx, cy, cw, S( 3 ), acc );
			TextSpaced( m_hHeading, cx + S( 18 ), cy + S( 20 ), COR_TEXTO, pszNome[ i ], S( 1 ) );
			Text( m_hBody, cx + S( 18 ), cy + S( 56 ), COR_TEXTO2, pszDesc[ i ] );
			if ( Clicked( cx, cy, cw, ch ) )
			{
				if ( i == 1 )
					Command( "OpenServerBrowser" );
				else if ( i == 2 )
					Command( "OpenCreateMultiplayerGameDialog" );
			}
		}
	}

	const int top = S( 252 ), bottom = m_iTall - S( 72 );

	// Mapa
	{
		const int mw = S( 420 ), mx = x0;
		Rect( mx, top, mw, bottom - top, COR_PAINEL );
		Outline( mx, top, mw, bottom - top, COR_LINHA );
		const int iInfoTall = S( 96 );
		Rect( mx + 1, top + 1, mw - 2, bottom - top - iInfoTall, Color( 23, 30, 38, 255 ) );
		Label( mx + S( 14 ), top + S( 12 ), "[foto do mapa]" );

		const int iy = bottom - iInfoTall + S( 16 );
		Label( mx + S( 18 ), iy, "MAPA" );
		const char *pszMap = m_Maps.Count() ? m_Maps[ m_iMap ].Get() : "nenhum mapa encontrado";
		char szUpper[ 128 ];
		V_strncpy( szUpper, pszMap, sizeof( szUpper ) );
		V_strupr( szUpper );
		TextSpaced( m_hItem, mx + S( 18 ), iy + S( 16 ), COR_TEXTO, szUpper, S( 1 ) );
		if ( m_Maps.Count() && !m_MapHasNav[ m_iMap ] )
			Text( m_hSmall, mx + S( 18 ), iy + S( 56 ), COR_TEXTO3, "Sem nav mesh: os bots geram na primeira vez (demora)." );

		const int bs = S( 34 );
		if ( BoxButton( mx + mw - S( 18 ) - bs * 2 - S( 6 ), iy + S( 14 ), bs, bs, m_hButton, "‹", false, false ) && m_Maps.Count() )
			m_iMap = ( m_iMap + m_Maps.Count() - 1 ) % m_Maps.Count();
		if ( BoxButton( mx + mw - S( 18 ) - bs, iy + S( 14 ), bs, bs, m_hButton, "›", false, false ) && m_Maps.Count() )
			m_iMap = ( m_iMap + 1 ) % m_Maps.Count();
	}

	// Configuração da partida
	{
		const int px = x0 + S( 420 ) + S( 16 ), pw = xr - px;
		Rect( px, top, pw, bottom - top, COR_PAINEL );
		Outline( px, top, pw, bottom - top, COR_LINHA );
		const int ix = px + S( 24 ), iw = pw - S( 48 );
		int y = top + S( 22 );

		Label( ix, y, "MODO" );
		{
			const char *pszModos[] = { "MATA-MATA EM EQUIPE", "SURTO" };
			const char *pszDesc[] = { "Os bots completam os times.", "Infecção: sobreviva até o fim do tempo. Os bots viram zumbis com você." };
			const int iModo = clamp( trok_ui_modo.GetInt(), 0, 1 );
			const int mw = ( iw - S( 6 ) ) / 2;
			for ( int i = 0; i < 2; ++i )
			{
				if ( BoxButton( ix + i * ( mw + S( 6 ) ), y + S( 18 ), mw, S( 36 ), m_hButton, pszModos[ i ], i == iModo, i == iModo ) )
					trok_ui_modo.SetValue( i );
			}
			Text( m_hSmall, ix, y + S( 60 ), COR_TEXTO2, pszDesc[ iModo ] );
		}
		y += S( 84 );

		Label( ix, y, "DIFICULDADE DOS BOTS" );
		const int iNivel = clamp( trok_ui_bots_dificuldade.GetInt(), 0, 3 );
		const int iSegGap = S( 6 );
		const int sw = ( iw - iSegGap * 3 ) / 4;
		for ( int i = 0; i < 4; ++i )
		{
			if ( BoxButton( ix + i * ( sw + iSegGap ), y + S( 18 ), sw, S( 40 ), m_hButton, s_pszNivelRotulo[ i ], i == iNivel, i == iNivel ) )
				trok_ui_bots_dificuldade.SetValue( i );
		}
		Text( m_hBody, ix, y + S( 66 ), COR_TEXTO2, s_pszNivelDescricao[ iNivel ] );
		y += S( 92 );

		Label( ix, y, trok_ui_modo.GetInt() == 1 ? "BOTS (METADE DO TOTAL)" : "BOTS POR TIME" );
		const int bs = S( 34 );
		const int iCount = clamp( trok_ui_bots_por_time.GetInt(), 1, 16 );
		if ( BoxButton( ix, y + S( 18 ), bs, bs, m_hButton, "−", false, false ) )
			trok_ui_bots_por_time.SetValue( MAX( 1, iCount - 1 ) );
		char szCount[ 8 ];
		V_snprintf( szCount, sizeof( szCount ), "%d", iCount );
		Text( m_hItem, ix + bs + S( 12 ) + ( S( 28 ) - TextWide( m_hItem, szCount ) ) / 2, y + S( 18 ) + ( bs - surface()->GetFontTall( m_hItem ) ) / 2, COR_TEXTO, szCount );
		if ( BoxButton( ix + bs + S( 52 ), y + S( 18 ), bs, bs, m_hButton, "+", false, false ) )
			trok_ui_bots_por_time.SetValue( MIN( 16, iCount + 1 ) );

		const int sx = ix + S( 170 );
		Label( sx, y, "ESTILOS DE JOGO" );
		int iMask = trok_ui_bots_estilos.GetInt();
		int cx = sx;
		for ( int i = 0; i < NUM_ESTILOS; ++i )
		{
			const int chw = TextWide( m_hBody, s_pszEstiloRotulo[ i ] ) + S( 28 );
			const bool bOn = ( iMask & ( 1 << i ) ) != 0;
			if ( BoxButton( cx, y + S( 18 ), chw, bs, m_hBody, s_pszEstiloRotulo[ i ], bOn, false ) )
			{
				iMask ^= ( 1 << i );
				if ( ( iMask & ( ( 1 << NUM_ESTILOS ) - 1 ) ) == 0 )
					iMask = 1; // pelo menos um estilo
				trok_ui_bots_estilos.SetValue( iMask );
			}
			cx += chw + S( 6 );
		}

		// Iniciar
		const int gw = S( 180 ), gh = S( 52 );
		const int gy = bottom - S( 24 ) - gh;
		Text( m_hSmall, ix, gy + ( gh - surface()->GetFontTall( m_hSmall ) ) / 2, COR_TEXTO3, "O time divide as entradas: cada bot vigia a que ninguém está olhando." );
		if ( BoxButton( px + pw - S( 24 ) - gw, gy, gw, gh, m_hHeading, "INICIAR", true, true ) )
			StartBotGame();
	}
}

void CTrokMenu::RunBottomBar( void )
{
	const int h = S( 48 );
	const int y = m_iTall - h;
	Rect( 0, y, m_iWide, h, Color( 11, 13, 16, 178 ) );
	Rect( 0, y, m_iWide, 1, Color( 255, 255, 255, 15 ) );
	const int ty = y + ( h - surface()->GetFontTall( m_hSmall ) ) / 2;
	Text( m_hSmall, S( 64 ), ty, COR_TEXTO3, "Protótipo · Source SDK 2013" );

	int x = m_iWide - S( 56 ) - S( 300 );
	if ( m_eState == STATE_PLAY )
		KeyHint( x, ty, "ENTER", "Iniciar" );
	else
		KeyHint( x, ty, "CLIQUE", "Selecionar" );
	KeyHint( x, ty, "ESC", m_eState == STATE_ROOT && InGame() ? "Continuar" : "Voltar" );
}

//-----------------------------------------------------------------------------
// Ações
//-----------------------------------------------------------------------------
void CTrokMenu::Command( const char *pszGameUICommand )
{
	IGameUI *pGameUI = GetGameUI();
	if ( pGameUI )
		pGameUI->SendMainMenuCommand( pszGameUICommand );
}

void CTrokMenu::BuildMapList( void )
{
	if ( m_bMapsBuilt )
		return;
	m_bMapsBuilt = true;

	CUtlVector< CUtlString > withNav, withoutNav;
	FileFindHandle_t hFind;
	for ( const char *pszFile = g_pFullFileSystem->FindFirstEx( "maps/*.bsp", "GAME", &hFind ); pszFile; pszFile = g_pFullFileSystem->FindNext( hFind ) )
	{
		if ( g_pFullFileSystem->FindIsDirectory( hFind ) )
			continue;
		char szName[ 128 ];
		V_StripExtension( pszFile, szName, sizeof( szName ) );
		if ( V_strnicmp( szName, "background", 10 ) == 0 || V_stristr( szName, "test" ) )
			continue;
		bool bDup = false;
		FOR_EACH_VEC( withNav, i ) bDup |= ( V_stricmp( withNav[ i ], szName ) == 0 );
		FOR_EACH_VEC( withoutNav, i ) bDup |= ( V_stricmp( withoutNav[ i ], szName ) == 0 );
		if ( bDup )
			continue;
		char szNav[ 160 ];
		V_snprintf( szNav, sizeof( szNav ), "maps/%s.nav", szName );
		if ( g_pFullFileSystem->FileExists( szNav, "GAME" ) )
			withNav.AddToTail( szName );
		else
			withoutNav.AddToTail( szName );
	}
	g_pFullFileSystem->FindClose( hFind );

	// Mapas com nav mesh primeiro (os bots precisam dela).
	m_Maps.RemoveAll();
	m_MapHasNav.RemoveAll();
	FOR_EACH_VEC( withNav, i ) { m_Maps.AddToTail( withNav[ i ] ); m_MapHasNav.AddToTail( true ); }
	FOR_EACH_VEC( withoutNav, i ) { m_Maps.AddToTail( withoutNav[ i ] ); m_MapHasNav.AddToTail( false ); }

	m_iMap = 0;
	FOR_EACH_VEC( m_Maps, i )
	{
		if ( V_stricmp( m_Maps[ i ], trok_ui_mapa.GetString() ) == 0 )
			m_iMap = i;
	}
}

void CTrokMenu::StartBotGame( void )
{
	if ( !m_Maps.Count() )
		return;

	const char *pszMap = m_Maps[ m_iMap ].Get();
	trok_ui_mapa.SetValue( pszMap );

	const int iNivel = clamp( trok_ui_bots_dificuldade.GetInt(), 0, 3 );
	const int iPorTime = clamp( trok_ui_bots_por_time.GetInt(), 1, 16 );
	const int iMask = trok_ui_bots_estilos.GetInt();

	char szEstilos[ 256 ] = "";
	for ( int i = 0; i < NUM_ESTILOS; ++i )
	{
		if ( iMask & ( 1 << i ) )
		{
			char szPart[ 32 ];
			V_snprintf( szPart, sizeof( szPart ), "%s%s %d", szEstilos[ 0 ] ? " " : "", s_pszEstiloNome[ i ], s_iEstiloPeso[ i ] );
			V_strncat( szEstilos, szPart, sizeof( szEstilos ) );
		}
	}

	// "fill": a cota conta o jogador, então os dois times ficam com iPorTime cada.
	char szCmd[ 1024 ];
	V_snprintf( szCmd, sizeof( szCmd ),
		"progress_enable; mp_teamplay 1; trok_modo %s; hl2mp_bot_difficulty %d; trok_bot_estilo \"\"; trok_bot_estilos \"%s\"; "
		"hl2mp_bot_quota %d; hl2mp_bot_quota_mode fill; maxplayers %d; map %s\n",
		trok_ui_modo.GetInt() == 1 ? "surto" : "tdm", iNivel, szEstilos, iPorTime * 2, MIN( 32, iPorTime * 2 + 2 ), pszMap );
	engine->ClientCmd_Unrestricted( szCmd );
}

//-----------------------------------------------------------------------------
// Interface com o resto do cliente
//-----------------------------------------------------------------------------
void TrokMenu_Create()
{
	if ( g_pTrokMenu )
		return;
	if ( CommandLine()->CheckParm( "-tools" ) )
		return; // o modo de ferramentas tem a própria interface (igual ao NT;RE)

	CTrokMenu *pMenu = new CTrokMenu();
	IGameUI *pGameUI = pMenu->GetGameUI();
	if ( pGameUI )
	{
		pGameUI->SetMainMenuOverride( pMenu->GetVPanel() );
		TrokLoading_Create( pGameUI );
	}
	else
		Warning( "Trok: GameUI não encontrado; o menu próprio não foi registrado.\n" );
}

void TrokMenu_Destroy()
{
	if ( g_pTrokMenu )
	{
		g_pTrokMenu->SetParent( (VPANEL)0 );
		delete g_pTrokMenu;
		g_pTrokMenu = NULL;
	}
}

bool TrokMenu_CaptureESC()
{
	return g_pTrokMenu && g_pTrokMenu->IsShown() && g_pTrokMenu->GetState() != CTrokMenu::STATE_ROOT;
}
