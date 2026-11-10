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
// CONFIGURAÇÕES e CRIAR PARTIDA são próprias (mais abaixo). Ainda do GameUI (pendências em
// design/ui.md): lista de servidores e caixas de confirmação. A tela de carregamento é trok_loading.cpp.
//=============================================================================

#include "cbase.h"
#include "trok_menu.h"
#include "ienginevgui.h"
#include "GameUI/IGameUI.h"
#include "filesystem.h"
#include "tier1/interface.h"
#include "tier1/strtools.h"
#include "steam/steam_api.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/materialsystem_config.h"
#include "inputsystem/iinputsystem.h"
#include "tier1/utlbuffer.h"
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
static ConVar trok_ui_modo( "trok_ui_modo", "0", FCVAR_ARCHIVE, "Menu: modo escolhido (0 = mata-mata em equipe, 1 = exército solo, 2 = Surto)." );
// CRIAR PARTIDA (0 na meta ou no tempo = o valor do cfg do modo)
static ConVar trok_ui_criar_nome( "trok_ui_criar_nome", "Sala Rearmed", FCVAR_ARCHIVE, "Criar partida: nome da sala." );
static ConVar trok_ui_criar_senha( "trok_ui_criar_senha", "", 0, "Criar partida: senha (não fica salva)." );
static ConVar trok_ui_criar_online( "trok_ui_criar_online", "1", FCVAR_ARCHIVE, "Criar partida: 1 = aberta na internet, 0 = só rede local/VPN." );
static ConVar trok_ui_criar_jogadores( "trok_ui_criar_jogadores", "12", FCVAR_ARCHIVE, "Criar partida: máximo de jogadores." );
static ConVar trok_ui_criar_meta( "trok_ui_criar_meta", "0", FCVAR_ARCHIVE, "Criar partida: meta de abates (0 = a do modo)." );
static ConVar trok_ui_criar_tempo( "trok_ui_criar_tempo", "0", FCVAR_ARCHIVE, "Criar partida: tempo em minutos (0 = o do modo)." );
static ConVar trok_ui_criar_bots( "trok_ui_criar_bots", "1", FCVAR_ARCHIVE, "Criar partida: completar as vagas com bots." );
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
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnKeyTyped( wchar_t unichar );
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
	enum State_t { STATE_ROOT = 0, STATE_PLAY, STATE_OPTIONS, STATE_CREATE };

	CTrokMenu();
	virtual ~CTrokMenu();

	IGameUI *GetGameUI( void );
	State_t GetState( void ) const { return m_eState; }
	bool IsShown( void ) { return ipanel()->IsFullyVisible( GetVPanel() ); }
	void OnRelayedKey( KeyCode code );
	bool IsCapturingKey( void ) const { return m_eState == STATE_OPTIONS && m_iCapture >= 0; }
	void CaptureButton( ButtonCode_t code );
	// A tecla capturada também chega depois como "digitada": não pode virar ESC/Enter do menu.
	bool IsEditingText( void ) const { return m_eState == STATE_CREATE && m_iTextField >= 0; }
	void OnRelayedChar( wchar_t ch );
	bool SwallowTyped( KeyCode code ) { const bool b = ( code == m_eSwallowTyped ); m_eSwallowTyped = BUTTON_CODE_INVALID; return b; }

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void PerformLayout( void );
	virtual void OnThink( void );
	virtual void Paint( void );
	virtual void OnCursorMoved( int x, int y );
	virtual void OnMousePressed( MouseCode code );
	virtual void OnMouseWheeled( int delta );
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
	void RunOptions( void );
	void RunCreate( void );
	void RunMapPanel( int mx, int top, int mw, int bottom );
	void RunBottomBar( void );
	bool TextField( int iField, int x, int y, int w, const char *pszValue, bool bHidden );

	// Configurações: uma linha por opção (rótulo à esquerda, controle à direita)
	void OptHeader( int x, int &y, int w, const char *pszText );
	void OptSlider( int x, int &y, int w, const char *pszLabel, const char *pszCvar, float flMin, float flMax, float flStep, const char *pszFormat );
	void OptToggle( int x, int &y, int w, const char *pszLabel, const char *pszCvar );
	bool OptCycle( int x, int &y, int w, const char *pszLabel, const char *pszValue, int &iDir );
	void OptBind( int x, int &y, int w, int iIndex );
	void BuildVideoModes( void );

	// Ações
	void Command( const char *pszGameUICommand );
	void StartBotGame( void );
	void StartCreatedGame( void );
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

	// Configurações
	int m_iOptTab;
	int m_iCapture;			// índice da ação esperando tecla (-1 = nenhuma)
	bool m_bOptDirty;		// mudou algo: grava o config.cfg ao sair da tela
	struct VideoMode_t { int w, h; };
	CUtlVector< VideoMode_t > m_VideoModes;
	int m_iVideoMode;		// resolução escolhida (ainda não aplicada)
	bool m_bVideoWindowed;	// modo escolhido (ainda não aplicado)
	bool m_bVideoPending;
	ButtonCode_t m_eSwallowTyped;

	// Criar partida: campo de texto em edição (-1 = nenhum, 0 = nome, 1 = senha)
	int m_iTextField;
	char m_szText[ 64 ];

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
	if ( m_pMenu->IsCapturingKey() || m_pMenu->SwallowTyped( code ) )
		return; // tratada no OnKeyCodePressed (que também recebe Shift, Ctrl e Alt)
	m_pMenu->OnRelayedKey( code );
}

void CTrokMenuInput::OnKeyTyped( wchar_t unichar )
{
	if ( m_pMenu->IsEditingText() )
		m_pMenu->OnRelayedChar( unichar );
}

void CTrokMenuInput::OnKeyCodePressed( KeyCode code )
{
	if ( code > KEY_NONE && m_pMenu->IsCapturingKey() )
	{
		m_pMenu->CaptureButton( code );
		return;
	}
	BaseClass::OnKeyCodePressed( code );
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
	m_iOptTab = 0;
	m_iCapture = -1;
	m_bOptDirty = false;
	m_iVideoMode = 0;
	m_bVideoWindowed = false;
	m_bVideoPending = false;
	m_eSwallowTyped = BUTTON_CODE_INVALID;
	m_iTextField = -1;
	m_szText[ 0 ] = 0;

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
	if ( m_eState == STATE_OPTIONS && eState != STATE_OPTIONS )
	{
		m_iCapture = -1;
		m_bVideoPending = false;
		if ( m_bOptDirty )
			engine->ClientCmd_Unrestricted( "host_writeconfig\n" ); // grava as escolhas no config.cfg
		m_bOptDirty = false;
	}
	if ( eState == STATE_OPTIONS && m_eState != STATE_OPTIONS )
		BuildVideoModes();
	if ( m_eState == STATE_CREATE && m_iTextField >= 0 )
	{
		( m_iTextField == 0 ? trok_ui_criar_nome : trok_ui_criar_senha ).SetValue( m_szText );
		m_iTextField = -1;
	}
	m_eState = eState;
	m_iLastHoverId = -1;
	if ( eState == STATE_PLAY || eState == STATE_CREATE )
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
	if ( IsCapturingKey() )
	{
		CaptureButton( code ); // MouseCode é um ButtonCode_t
		return;
	}
	if ( code != MOUSE_LEFT )
		return;
	m_bClickHandled = false;
	Run( MODE_CLICK );
	if ( m_bClickHandled && trok_ui_som_clique.GetString()[ 0 ] )
		surface()->PlaySound( trok_ui_som_clique.GetString() );
}

void CTrokMenu::OnMouseWheeled( int delta )
{
	if ( IsCapturingKey() )
		CaptureButton( delta > 0 ? MOUSE_WHEEL_UP : MOUSE_WHEEL_DOWN );
}

void CTrokMenu::OnRelayedKey( KeyCode code )
{
	if ( code == KEY_ESCAPE && IsCapturingKey() )
	{
		m_iCapture = -1;
		return;
	}
	if ( IsEditingText() )
	{
		const int iLen = V_strlen( m_szText );
		if ( code == KEY_BACKSPACE && iLen > 0 )
		{
			// apaga um caractere UTF-8 inteiro
			int i = iLen - 1;
			while ( i > 0 && ( m_szText[ i ] & 0xC0 ) == 0x80 )
				--i;
			m_szText[ i ] = 0;
		}
		else if ( code == KEY_ENTER || code == KEY_PAD_ENTER || code == KEY_ESCAPE || code == KEY_TAB )
		{
			( m_iTextField == 0 ? trok_ui_criar_nome : trok_ui_criar_senha ).SetValue( m_szText );
			m_iTextField = -1;
		}
		return;
	}
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
	if ( ( code == KEY_ENTER || code == KEY_PAD_ENTER ) && m_eState == STATE_CREATE )
	{
		StartCreatedGame();
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
	case STATE_OPTIONS: RunOptions(); break;
	case STATE_CREATE: RunCreate(); break;
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
	TextSpaced( m_hLogo, x0 + S( 24 ), S( 46 ), COR_TEXTO, "REARMED", S( 9 ) );

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
			SetState( STATE_OPTIONS );
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
			SetState( STATE_CREATE );
		if ( MenuItem( 4, x0, y, "04", "CONFIGURAÇÕES", NULL, false ) )
			SetState( STATE_OPTIONS );
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
				{
					SetState( STATE_CREATE );
					return;
				}
			}
		}
	}

	const int top = S( 252 ), bottom = m_iTall - S( 72 );

	// Mapa
	RunMapPanel( x0, top, S( 420 ), bottom );

	// Configuração da partida
	{
		const int px = x0 + S( 420 ) + S( 16 ), pw = xr - px;
		Rect( px, top, pw, bottom - top, COR_PAINEL );
		Outline( px, top, pw, bottom - top, COR_LINHA );
		const int ix = px + S( 24 ), iw = pw - S( 48 );
		int y = top + S( 22 );

		Label( ix, y, "MODO" );
		{
			const char *pszModos[] = { "EQUIPE", "EXÉRCITO SOLO", "SURTO" };
			const char *pszDesc[] = { "Mata-mata em equipe: o time que chegar primeiro na meta de abates vence.",
									  "Todos contra todos: quem chegar primeiro na meta de abates vence.",
									  "Infecção: sobreviva até o fim do tempo. Os bots viram zumbis com você." };
			const int iModo = clamp( trok_ui_modo.GetInt(), 0, 2 );
			const int mw = ( iw - S( 12 ) ) / 3;
			for ( int i = 0; i < 3; ++i )
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

		Label( ix, y, trok_ui_modo.GetInt() == 0 ? "BOTS POR TIME" : "BOTS (METADE DO TOTAL)" );
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

void CTrokMenu::RunMapPanel( int mx, int top, int mw, int bottom )
{
	{
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
	if ( IsEditingText() )
		KeyHint( x, ty, "ENTER", "Confirmar" );
	else if ( m_eState == STATE_PLAY || m_eState == STATE_CREATE )
		KeyHint( x, ty, "ENTER", "Iniciar" );
	else if ( IsCapturingKey() )
		KeyHint( x, ty, "TECLA", "Definir" );
	else
		KeyHint( x, ty, "CLIQUE", "Selecionar" );
	KeyHint( x, ty, "ESC", IsCapturingKey() ? "Cancelar" : ( m_eState == STATE_ROOT && InGame() ? "Continuar" : "Voltar" ) );
}

//-----------------------------------------------------------------------------
// Configurações (substitui a janela de opções do motor)
//-----------------------------------------------------------------------------

// Ações que dá para trocar de tecla. Comandos iguais aos do cfg/config_default.cfg.
struct TrokBind_t { const char *pszLabel; const char *pszCmd; };
static const TrokBind_t s_Binds[] = {
	{ "Andar para frente", "+forward" },	{ "Andar para trás", "+back" },
	{ "Andar para a esquerda", "+moveleft" }, { "Andar para a direita", "+moveright" },
	{ "Pular / escalar", "+jump" },			{ "Agachar", "+duck" },
	{ "Correr", "+speed" },					{ "Andar sem barulho", "+walk" },
	{ "Atirar", "+attack" },				{ "Mirar pelo cano", "+attack2" },
	{ "Recarregar", "+reload" },			{ "Usar / abrir grade", "+use" },
	{ "Arma primária", "slot1" },			{ "Pistola", "slot2" },
	{ "Faca", "slot3" },					{ "Última arma", "lastinv" },
	{ "Placar", "+showscores" },			{ "Chat geral", "messagemode" },
	{ "Chat do time", "messagemode2" },		{ "Tirar print", "jpeg" },
};
static const int NUM_BINDS = ARRAYSIZE( s_Binds );

static const char *s_pszAbas[] = { "JOGO", "VÍDEO", "ÁUDIO", "CONTROLES" };

// Nome da tecla para mostrar: "MOUSE1" vira "MOUSE 1", "SPACE" vira "ESPAÇO" etc.
static void TrokKeyLabel( const char *pszKey, char *pszOut, int iOutSize )
{
	if ( !pszKey || !pszKey[ 0 ] )
	{
		V_strncpy( pszOut, "—", iOutSize );
		return;
	}
	static const char *s_Nomes[][ 2 ] = {
		{ "SPACE", "ESPAÇO" }, { "MOUSE1", "MOUSE 1" }, { "MOUSE2", "MOUSE 2" }, { "MOUSE3", "MOUSE 3" },
		{ "MOUSE4", "MOUSE 4" }, { "MOUSE5", "MOUSE 5" }, { "MWHEELUP", "RODA ↑" }, { "MWHEELDOWN", "RODA ↓" },
		{ "LSHIFT", "SHIFT" }, { "RSHIFT", "SHIFT DIR." }, { "LCTRL", "CTRL" }, { "RCTRL", "CTRL DIR." },
		{ "LALT", "ALT" }, { "RALT", "ALT DIR." }, { "ENTER", "ENTER" }, { "BACKSPACE", "BACKSPACE" },
		{ "UPARROW", "↑" }, { "DOWNARROW", "↓" }, { "LEFTARROW", "←" }, { "RIGHTARROW", "→" },
	};
	for ( int i = 0; i < ARRAYSIZE( s_Nomes ); ++i )
	{
		if ( V_stricmp( pszKey, s_Nomes[ i ][ 0 ] ) == 0 )
		{
			V_strncpy( pszOut, s_Nomes[ i ][ 1 ], iOutSize );
			return;
		}
	}
	V_strncpy( pszOut, pszKey, iOutSize );
	V_strupr( pszOut );
}

void CTrokMenu::CaptureButton( ButtonCode_t code )
{
	if ( m_iCapture < 0 || m_iCapture >= NUM_BINDS )
		return;
	if ( IsKeyCode( code ) )
		m_eSwallowTyped = code;
	const int iAction = m_iCapture;
	m_iCapture = -1;

	// ESC cancela; o console (') não pode ser tomado.
	if ( code == KEY_ESCAPE || code == KEY_BACKQUOTE || code == BUTTON_CODE_INVALID || code == BUTTON_CODE_NONE )
		return;
	const char *pszKey = inputsystem ? inputsystem->ButtonCodeToString( code ) : NULL;
	if ( !pszKey || !pszKey[ 0 ] )
		return;

	// Tira a ação das teclas antigas (até 4) e liga na nova. Se a tecla nova era de outra
	// ação, o bind novo substitui.
	const char *pszCmd = s_Binds[ iAction ].pszCmd;
	for ( int i = 0; i < 4; ++i )
	{
		const char *pszOld = engine->Key_LookupBinding( pszCmd );
		if ( !pszOld || !pszOld[ 0 ] || V_stricmp( pszOld, pszKey ) == 0 )
			break;
		char szUnbind[ 128 ];
		V_snprintf( szUnbind, sizeof( szUnbind ), "unbind \"%s\"", pszOld );
		engine->ExecuteClientCmd( szUnbind ); // na hora, para a próxima busca já ver a mudança
	}
	char szBind[ 160 ];
	V_snprintf( szBind, sizeof( szBind ), "bind \"%s\" \"%s\"", pszKey, pszCmd );
	engine->ExecuteClientCmd( szBind );
	m_bOptDirty = true;
}

void CTrokMenu::BuildVideoModes( void )
{
	m_VideoModes.RemoveAll();
	m_iVideoMode = 0;
	if ( !materials )
		return;
	const MaterialSystem_Config_t &config = materials->GetCurrentConfigForVideoCard();
	m_bVideoWindowed = config.Windowed();
	m_bVideoPending = false;

	// Resoluções da placa, sem repetir (a mesma em várias taxas de atualização), da menor para a maior.
	const int iCount = materials->GetModeCount( materials->GetCurrentAdapter() );
	for ( int i = 0; i < iCount; ++i )
	{
		MaterialVideoMode_t mode;
		materials->GetModeInfo( materials->GetCurrentAdapter(), i, mode );
		if ( mode.m_Width < 1024 || mode.m_Height < 600 )
			continue;
		bool bDup = false;
		FOR_EACH_VEC( m_VideoModes, j )
			bDup |= ( m_VideoModes[ j ].w == mode.m_Width && m_VideoModes[ j ].h == mode.m_Height );
		if ( bDup )
			continue;
		VideoMode_t vm = { mode.m_Width, mode.m_Height };
		int iPos = 0;
		while ( iPos < m_VideoModes.Count() && ( m_VideoModes[ iPos ].w * 10000 + m_VideoModes[ iPos ].h ) < ( vm.w * 10000 + vm.h ) )
			++iPos;
		m_VideoModes.InsertBefore( iPos, vm );
	}
	FOR_EACH_VEC( m_VideoModes, i )
	{
		if ( m_VideoModes[ i ].w == config.m_VideoMode.m_Width && m_VideoModes[ i ].h == config.m_VideoMode.m_Height )
			m_iVideoMode = i;
	}
}

void CTrokMenu::OptHeader( int x, int &y, int w, const char *pszText )
{
	y += S( 6 );
	Text( m_hSmall, x, y, Accent(), pszText );
	y += surface()->GetFontTall( m_hSmall ) + S( 6 );
	Rect( x, y, w, 1, COR_LINHA );
	y += S( 8 );
}

// Barra com − e +: clicar na barra põe o valor no ponto clicado.
void CTrokMenu::OptSlider( int x, int &y, int w, const char *pszLabel, const char *pszCvar, float flMin, float flMax, float flStep, const char *pszFormat )
{
	ConVarRef var( pszCvar, true );
	if ( !var.IsValid() )
		return;
	const int rh = S( 34 );
	const int cx = x + w / 2, cw = w / 2;
	Text( m_hBody, x, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO2, pszLabel );

	float flValue = clamp( var.GetFloat(), flMin, flMax );
	const int bs = S( 26 );
	const int tx = cx + bs + S( 10 ), tw = cw - bs * 2 - S( 20 ) - S( 56 );
	const int ty = y + rh / 2;

	float flNew = flValue;
	if ( BoxButton( cx, y + ( rh - bs ) / 2, bs, bs, m_hBody, "−", false, false ) )
		flNew = flValue - flStep;
	if ( BoxButton( tx + tw + S( 10 ), y + ( rh - bs ) / 2, bs, bs, m_hBody, "+", false, false ) )
		flNew = flValue + flStep;

	const float flFrac = ( flMax > flMin ) ? ( flValue - flMin ) / ( flMax - flMin ) : 0.0f;
	const bool bHover = Hover( tx, y, tw, rh );
	Rect( tx, ty - S( 2 ), tw, S( 4 ), bHover ? COR_LINHA_FORTE : COR_LINHA );
	Rect( tx, ty - S( 2 ), (int)( tw * flFrac ), S( 4 ), Accent() );
	Rect( tx + (int)( tw * flFrac ) - S( 3 ), ty - S( 8 ), S( 6 ), S( 16 ), COR_TEXTO );
	if ( Clicked( tx, y, tw, rh ) )
	{
		const float t = clamp( (float)( m_iMouseX - tx ) / (float)MAX( tw, 1 ), 0.0f, 1.0f );
		flNew = flMin + t * ( flMax - flMin );
		flNew = flMin + flStep * floorf( ( flNew - flMin ) / flStep + 0.5f );
	}

	char szValue[ 32 ];
	V_snprintf( szValue, sizeof( szValue ), pszFormat, flValue );
	Text( m_hBody, cx + cw - TextWide( m_hBody, szValue ), y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO, szValue );

	flNew = clamp( flNew, flMin, flMax );
	if ( flNew != flValue )
	{
		var.SetValue( flNew );
		m_bOptDirty = true;
	}
	y += rh + S( 4 );
}

void CTrokMenu::OptToggle( int x, int &y, int w, const char *pszLabel, const char *pszCvar )
{
	ConVarRef var( pszCvar, true );
	if ( !var.IsValid() )
		return;
	const int rh = S( 34 );
	Text( m_hBody, x, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO2, pszLabel );
	const bool bOn = var.GetBool();
	const int bw = S( 120 ), bh = S( 28 );
	if ( BoxButton( x + w - bw, y + ( rh - bh ) / 2, bw, bh, m_hButton, bOn ? "LIGADO" : "DESLIGADO", bOn, false ) )
	{
		var.SetValue( bOn ? 0 : 1 );
		m_bOptDirty = true;
	}
	y += rh + S( 4 );
}

// Valor com ‹ e ›; iDir volta -1 ou +1 quando clicado.
bool CTrokMenu::OptCycle( int x, int &y, int w, const char *pszLabel, const char *pszValue, int &iDir )
{
	const int rh = S( 34 );
	Text( m_hBody, x, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO2, pszLabel );
	const int bs = S( 26 ), vw = S( 150 );
	const int bx = x + w - bs * 2 - vw - S( 8 );
	iDir = 0;
	if ( BoxButton( bx, y + ( rh - bs ) / 2, bs, bs, m_hBody, "‹", false, false ) )
		iDir = -1;
	Text( m_hBody, bx + bs + S( 4 ) + ( vw - TextWide( m_hBody, pszValue ) ) / 2, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO, pszValue );
	if ( BoxButton( bx + bs + vw + S( 8 ), y + ( rh - bs ) / 2, bs, bs, m_hBody, "›", false, false ) )
		iDir = 1;
	y += rh + S( 4 );
	return iDir != 0;
}

void CTrokMenu::OptBind( int x, int &y, int w, int iIndex )
{
	const int rh = S( 32 );
	Text( m_hBody, x, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO2, s_Binds[ iIndex ].pszLabel );
	char szKey[ 64 ];
	if ( m_iCapture == iIndex )
		V_strncpy( szKey, "APERTE UMA TECLA", sizeof( szKey ) );
	else
		TrokKeyLabel( engine->Key_LookupBinding( s_Binds[ iIndex ].pszCmd ), szKey, sizeof( szKey ) );
	const int bw = S( 150 ), bh = S( 26 );
	if ( BoxButton( x + w - bw, y + ( rh - bh ) / 2, bw, bh, m_hBody, szKey, m_iCapture == iIndex, false ) )
		m_iCapture = iIndex;
	y += rh + S( 2 );
}

void CTrokMenu::RunOptions( void )
{
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
	TextSpaced( m_hTitle, iSep + S( 21 ), S( 36 ), COR_TEXTO, "CONFIGURAÇÕES", S( 3 ) );

	// Abas
	{
		int tx = x0;
		const int th = S( 36 );
		for ( int i = 0; i < ARRAYSIZE( s_pszAbas ); ++i )
		{
			const int tw = TextWide( m_hButton, s_pszAbas[ i ] ) + S( 40 );
			if ( BoxButton( tx, S( 108 ), tw, th, m_hButton, s_pszAbas[ i ], i == m_iOptTab, i == m_iOptTab ) )
			{
				m_iOptTab = i;
				m_iCapture = -1;
			}
			tx += tw + S( 6 );
		}
	}

	const int top = S( 160 ), bottom = m_iTall - S( 72 );
	const int pw = MIN( xr - x0, S( 900 ) );
	Rect( x0, top, pw, bottom - top, COR_PAINEL );
	Outline( x0, top, pw, bottom - top, COR_LINHA );
	const int ix = x0 + S( 28 ), iw = pw - S( 56 );
	int y = top + S( 14 );

	switch ( m_iOptTab )
	{
	case 0: // Jogo
	{
		OptHeader( ix, y, iw, "MOUSE" );
		OptSlider( ix, y, iw, "Sensibilidade", "sensitivity", 0.2f, 10.0f, 0.1f, "%.1f" );
		OptSlider( ix, y, iw, "Sensibilidade mirando (relativa)", "zoom_sensitivity_ratio", 0.2f, 2.0f, 0.05f, "%.2f" );
		{
			ConVarRef pitch( "m_pitch", true );
			if ( pitch.IsValid() )
			{
				const int rh = S( 34 );
				Text( m_hBody, ix, y + ( rh - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO2, "Inverter mouse" );
				const bool bInv = pitch.GetFloat() < 0.0f;
				const int bw = S( 120 ), bh = S( 28 );
				if ( BoxButton( ix + iw - bw, y + ( rh - bh ) / 2, bw, bh, m_hButton, bInv ? "LIGADO" : "DESLIGADO", bInv, false ) )
				{
					pitch.SetValue( -pitch.GetFloat() );
					m_bOptDirty = true;
				}
				y += rh + S( 4 );
			}
		}
		OptToggle( ix, y, iw, "Entrada direta do mouse (raw input)", "m_rawinput" );
		OptHeader( ix, y, iw, "TELA" );
		OptSlider( ix, y, iw, "Campo de visão", "fov_desired", 75.0f, 90.0f, 1.0f, "%.0f°" );
		OptToggle( ix, y, iw, "Radar", "trok_hud_radar" );
		OptToggle( ix, y, iw, "Balanço da arma ao andar", "trok_arma_balanco" );
		OptToggle( ix, y, iw, "Mostrar FPS", "cl_showfps" );
		break;
	}
	case 1: // Vídeo
	{
		OptHeader( ix, y, iw, "TELA" );
		char szRes[ 32 ] = "—";
		if ( m_VideoModes.Count() )
			V_snprintf( szRes, sizeof( szRes ), "%d × %d", m_VideoModes[ m_iVideoMode ].w, m_VideoModes[ m_iVideoMode ].h );
		int iDir;
		if ( OptCycle( ix, y, iw, "Resolução", szRes, iDir ) && m_VideoModes.Count() )
		{
			m_iVideoMode = ( m_iVideoMode + iDir + m_VideoModes.Count() ) % m_VideoModes.Count();
			m_bVideoPending = true;
		}
		if ( OptCycle( ix, y, iw, "Modo", m_bVideoWindowed ? "JANELA" : "TELA CHEIA", iDir ) )
		{
			m_bVideoWindowed = !m_bVideoWindowed;
			m_bVideoPending = true;
		}
		{
			const int bw = S( 160 ), bh = S( 32 );
			if ( m_bVideoPending )
				Text( m_hSmall, ix, y + ( bh - surface()->GetFontTall( m_hSmall ) ) / 2, COR_TEXTO3, "Resolução e modo só mudam ao aplicar." );
			if ( BoxButton( ix + iw - bw, y, bw, bh, m_hButton, "APLICAR", false, m_bVideoPending ) && m_bVideoPending && m_VideoModes.Count() )
			{
				char szCmd[ 96 ];
				V_snprintf( szCmd, sizeof( szCmd ), "mat_setvideomode %d %d %d\n", m_VideoModes[ m_iVideoMode ].w, m_VideoModes[ m_iVideoMode ].h, m_bVideoWindowed ? 1 : 0 );
				engine->ClientCmd_Unrestricted( szCmd );
				m_bVideoPending = false;
				m_bOptDirty = true;
			}
			y += bh + S( 8 );
		}
		OptToggle( ix, y, iw, "Sincronização vertical (V-Sync)", "mat_vsync" );
		{
			ConVarRef fps( "fps_max", true );
			if ( fps.IsValid() )
			{
				static const int s_Fps[] = { 60, 120, 144, 165, 240, 300, 0 };
				int iCur = ARRAYSIZE( s_Fps ) - 1;
				for ( int i = 0; i < ARRAYSIZE( s_Fps ); ++i )
					if ( fps.GetInt() == s_Fps[ i ] ) iCur = i;
				char szFps[ 32 ];
				if ( s_Fps[ iCur ] )
					V_snprintf( szFps, sizeof( szFps ), "%d", s_Fps[ iCur ] );
				else
					V_strncpy( szFps, "SEM LIMITE", sizeof( szFps ) );
				if ( OptCycle( ix, y, iw, "Limite de FPS", szFps, iDir ) )
				{
					fps.SetValue( s_Fps[ ( iCur + iDir + ARRAYSIZE( s_Fps ) ) % ARRAYSIZE( s_Fps ) ] );
					m_bOptDirty = true;
				}
			}
		}
		OptSlider( ix, y, iw, "Brilho", "mat_monitorgamma", 1.6f, 2.6f, 0.05f, "%.2f" );
		OptHeader( ix, y, iw, "QUALIDADE" );
		{
			ConVarRef picmip( "mat_picmip", true );
			if ( picmip.IsValid() )
			{
				static const char *s_Tex[] = { "MUITO ALTA", "ALTA", "MÉDIA", "BAIXA" }; // mat_picmip -1, 0, 1, 2
				const int iCur = clamp( picmip.GetInt() + 1, 0, 3 );
				if ( OptCycle( ix, y, iw, "Texturas", s_Tex[ iCur ], iDir ) )
				{
					picmip.SetValue( clamp( iCur - iDir, 0, 3 ) - 1 ); // › melhora, ‹ piora
					m_bOptDirty = true;
				}
			}
		}
		y += S( 6 );
		if ( LinkButton( ix, y, m_hSmall, "MAIS OPÇÕES DE VÍDEO (JANELA DO MOTOR) ›" ) )
			Command( "OpenOptionsDialog" );
		break;
	}
	case 2: // Áudio
	{
		OptHeader( ix, y, iw, "VOLUME" );
		OptSlider( ix, y, iw, "Volume geral", "volume", 0.0f, 1.0f, 0.05f, "%.2f" );
		OptSlider( ix, y, iw, "Música", "snd_musicvolume", 0.0f, 1.0f, 0.05f, "%.2f" );
		OptHeader( ix, y, iw, "VOZ" );
		OptToggle( ix, y, iw, "Chat de voz", "voice_enable" );
		OptSlider( ix, y, iw, "Volume da voz dos outros", "voice_scale", 0.0f, 1.0f, 0.05f, "%.2f" );
		break;
	}
	default: // Controles
	{
		const int colw = ( iw - S( 40 ) ) / 2;
		const int iHalf = ( NUM_BINDS + 1 ) / 2;
		int yl = y, yr = y;
		for ( int i = 0; i < NUM_BINDS; ++i )
		{
			if ( i < iHalf )
				OptBind( ix, yl, colw, i );
			else
				OptBind( ix + colw + S( 40 ), yr, colw, i );
		}
		y = MAX( yl, yr ) + S( 10 );
		Text( m_hSmall, ix, y, COR_TEXTO3, "Clique numa ação e aperte a tecla ou o botão do mouse. ESC cancela." );
		const int bw = S( 200 ), bh = S( 30 );
		if ( BoxButton( ix + iw - bw, y - S( 8 ), bw, bh, m_hButton, "RESTAURAR PADRÃO", false, false ) )
		{
			engine->ClientCmd_Unrestricted( "exec config_default\n" );
			m_iCapture = -1;
			m_bOptDirty = true;
		}
		break;
	}
	}
}

//-----------------------------------------------------------------------------
// Criar partida (substitui a janela do motor)
//-----------------------------------------------------------------------------

// Tira o que quebraria o comando do console (aspas, ponto e vírgula, quebras de linha).
static void TrokLimparTexto( const char *pszIn, char *pszOut, int iOutSize )
{
	int o = 0;
	for ( const char *p = pszIn; *p && o < iOutSize - 1; ++p )
	{
		if ( *p == '"' || *p == ';' || *p == '\n' || *p == '\r' || *p == '\\' )
			continue;
		pszOut[ o++ ] = *p;
	}
	pszOut[ o ] = 0;
}

void CTrokMenu::OnRelayedChar( wchar_t ch )
{
	if ( !IsEditingText() || ch < 32 || ch == 127 )
		return;
	char szChar[ 8 ];
	const wchar_t wsz[ 2 ] = { ch, 0 };
	V_UnicodeToUTF8( wsz, szChar, sizeof( szChar ) );
	if ( szChar[ 0 ] == '"' || szChar[ 0 ] == ';' || szChar[ 0 ] == '\\' )
		return;
	if ( V_strlen( m_szText ) + V_strlen( szChar ) < (int)sizeof( m_szText ) - 1 && V_strlen( m_szText ) < 40 )
		V_strncat( m_szText, szChar, sizeof( m_szText ) );
}

// Caixa de texto: clicar começa a editar; o texto da edição vem de OnRelayedChar.
bool CTrokMenu::TextField( int iField, int x, int y, int w, const char *pszValue, bool bHidden )
{
	const int h = S( 34 );
	const bool bEditing = ( m_iTextField == iField );
	const bool bHover = Hover( x, y, w, h );
	Rect( x, y, w, h, Color( 11, 13, 16, 200 ) );
	Outline( x, y, w, h, bEditing ? Accent() : ( bHover ? COR_LINHA_FORTE : COR_LINHA ) );
	char szShow[ 80 ];
	const char *pszText = bEditing ? m_szText : pszValue;
	if ( bHidden && !bEditing )
	{
		const int n = MIN( V_strlen( pszText ), 30 );
		for ( int i = 0; i < n; ++i ) szShow[ i ] = '*';
		szShow[ n ] = 0;
	}
	else
		V_strncpy( szShow, pszText, sizeof( szShow ) );
	if ( bEditing && ( (int)( Plat_FloatTime() * 2.0 ) & 1 ) )
		V_strncat( szShow, "|", sizeof( szShow ) );
	if ( !szShow[ 0 ] && !bEditing )
		Text( m_hBody, x + S( 10 ), y + ( h - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO3, bHidden ? "sem senha" : "" );
	else
		Text( m_hBody, x + S( 10 ), y + ( h - surface()->GetFontTall( m_hBody ) ) / 2, COR_TEXTO, szShow );
	if ( Clicked( x, y, w, h ) && !bEditing )
	{
		if ( m_iTextField >= 0 )
			( m_iTextField == 0 ? trok_ui_criar_nome : trok_ui_criar_senha ).SetValue( m_szText );
		m_iTextField = iField;
		V_strncpy( m_szText, pszValue, sizeof( m_szText ) );
		return true;
	}
	return false;
}

void CTrokMenu::RunCreate( void )
{
	const int x0 = S( 64 );
	const int xr = m_iWide - S( 56 );

	if ( LinkButton( x0, S( 52 ), m_hSmall, "‹ VOLTAR" ) )
	{
		SetState( STATE_ROOT );
		return;
	}
	const int iSep = x0 + TextWide( m_hSmall, "‹ VOLTAR" ) + S( 20 );
	Rect( iSep, S( 46 ), 1, S( 22 ), COR_LINHA_FORTE );
	TextSpaced( m_hTitle, iSep + S( 21 ), S( 36 ), COR_TEXTO, "CRIAR PARTIDA", S( 3 ) );
	Text( m_hBody, x0, S( 96 ), COR_TEXTO2, "Seu servidor, suas regras. Os amigos entram pelo seu IP (veja o LEIA-ME: JOGAR COM AMIGOS)." );

	const int top = S( 128 ), bottom = m_iTall - S( 72 );
	RunMapPanel( x0, top, S( 380 ), bottom );

	const int px = x0 + S( 380 ) + S( 16 ), pw = xr - px;
	Rect( px, top, pw, bottom - top, COR_PAINEL );
	Outline( px, top, pw, bottom - top, COR_LINHA );
	const int ix = px + S( 24 ), iw = pw - S( 48 );
	const int iHalf = ( iw - S( 24 ) ) / 2;
	int y = top + S( 18 );

	// Modo
	Label( ix, y, "MODO" );
	{
		const char *pszModos[] = { "EQUIPE", "EXÉRCITO SOLO", "SURTO" };
		const int iModo = clamp( trok_ui_modo.GetInt(), 0, 2 );
		const int mw = ( iw - S( 12 ) ) / 3;
		for ( int i = 0; i < 3; ++i )
		{
			if ( BoxButton( ix + i * ( mw + S( 6 ) ), y + S( 18 ), mw, S( 34 ), m_hButton, pszModos[ i ], i == iModo, i == iModo ) )
				trok_ui_modo.SetValue( i );
		}
	}
	y += S( 66 );

	// Nome e senha
	Label( ix, y, "NOME DA SALA" );
	Label( ix + iHalf + S( 24 ), y, "SENHA (OPCIONAL)" );
	TextField( 0, ix, y + S( 18 ), iHalf, trok_ui_criar_nome.GetString(), false );
	TextField( 1, ix + iHalf + S( 24 ), y + S( 18 ), iHalf, trok_ui_criar_senha.GetString(), true );
	y += S( 66 );

	// Online e jogadores
	int iDir = 0;
	{
		const int iCol = ix + iHalf + S( 24 );
		Label( ix, y, "QUEM PODE ENTRAR" );
		const bool bOnline = trok_ui_criar_online.GetBool();
		const int bw = ( iHalf - S( 6 ) ) / 2;
		if ( BoxButton( ix, y + S( 18 ), bw, S( 34 ), m_hButton, "INTERNET", bOnline, bOnline ) )
			trok_ui_criar_online.SetValue( 1 );
		if ( BoxButton( ix + bw + S( 6 ), y + S( 18 ), bw, S( 34 ), m_hButton, "REDE LOCAL / VPN", !bOnline, !bOnline ) )
			trok_ui_criar_online.SetValue( 0 );

		Label( iCol, y, "MÁXIMO DE JOGADORES" );
		int yy = y + S( 18 ) - S( 0 );
		char szN[ 16 ];
		const int iJog = clamp( trok_ui_criar_jogadores.GetInt(), 2, 32 );
		V_snprintf( szN, sizeof( szN ), "%d", iJog );
		if ( OptCycle( iCol, yy, iHalf, "", szN, iDir ) )
			trok_ui_criar_jogadores.SetValue( clamp( iJog + iDir * 2, 2, 32 ) );
	}
	y += S( 66 );

	// Meta e tempo
	{
		static const int s_Metas[] = { 0, 30, 50, 80, 100, 150 };
		static const int s_Tempos[] = { 0, 5, 10, 15, 20, 30 };
		const bool bSurto = trok_ui_modo.GetInt() == 2;
		int iMeta = 0, iTempo = 0;
		for ( int i = 0; i < ARRAYSIZE( s_Metas ); ++i ) if ( trok_ui_criar_meta.GetInt() == s_Metas[ i ] ) iMeta = i;
		for ( int i = 0; i < ARRAYSIZE( s_Tempos ); ++i ) if ( trok_ui_criar_tempo.GetInt() == s_Tempos[ i ] ) iTempo = i;
		char szMeta[ 32 ], szTempo[ 32 ];
		if ( s_Metas[ iMeta ] ) V_snprintf( szMeta, sizeof( szMeta ), "%d ABATES", s_Metas[ iMeta ] ); else V_strncpy( szMeta, "DO MODO", sizeof( szMeta ) );
		if ( s_Tempos[ iTempo ] ) V_snprintf( szTempo, sizeof( szTempo ), "%d MIN", s_Tempos[ iTempo ] ); else V_strncpy( szTempo, "DO MODO", sizeof( szTempo ) );

		int yl = y, yr = y;
		if ( !bSurto )
		{
			Label( ix, y, "META DE ABATES" );
			yl += S( 18 );
			if ( OptCycle( ix, yl, iHalf, "", szMeta, iDir ) )
				trok_ui_criar_meta.SetValue( s_Metas[ ( iMeta + iDir + ARRAYSIZE( s_Metas ) ) % ARRAYSIZE( s_Metas ) ] );
			Label( ix + iHalf + S( 24 ), y, "TEMPO DA PARTIDA" );
			yr += S( 18 );
			if ( OptCycle( ix + iHalf + S( 24 ), yr, iHalf, "", szTempo, iDir ) )
				trok_ui_criar_tempo.SetValue( s_Tempos[ ( iTempo + iDir + ARRAYSIZE( s_Tempos ) ) % ARRAYSIZE( s_Tempos ) ] );
		}
		else
		{
			Text( m_hSmall, ix, y + S( 6 ), COR_TEXTO3, "No Surto, os tempos da rodada vêm do cfg/surto.cfg." );
			yl += S( 30 );
		}
		y = MAX( yl, yr ) + S( 10 );
	}

	// Bots
	{
		Label( ix, y, "BOTS" );
		const bool bBots = trok_ui_criar_bots.GetBool();
		const int bw = S( 150 );
		if ( BoxButton( ix, y + S( 18 ), bw, S( 34 ), m_hButton, bBots ? "COMPLETAR VAGAS" : "SEM BOTS", bBots, false ) )
			trok_ui_criar_bots.SetValue( bBots ? 0 : 1 );
		if ( bBots )
		{
			const int iNivel = clamp( trok_ui_bots_dificuldade.GetInt(), 0, 3 );
			const int sx = ix + bw + S( 18 );
			const int sw = ( iw - bw - S( 18 ) - S( 18 ) ) / 4;
			for ( int i = 0; i < 4; ++i )
			{
				if ( BoxButton( sx + i * ( sw + S( 6 ) ), y + S( 18 ), sw, S( 34 ), m_hButton, s_pszNivelRotulo[ i ], i == iNivel, i == iNivel ) )
					trok_ui_bots_dificuldade.SetValue( i );
			}
		}
	}

	// Criar
	const int gw = S( 180 ), gh = S( 52 );
	const int gy = bottom - S( 24 ) - gh;
	Text( m_hSmall, ix, gy + ( gh - surface()->GetFontTall( m_hSmall ) ) / 2, COR_TEXTO3,
		trok_ui_criar_online.GetBool() ? "Internet: libere a porta 27015 no roteador ou use uma VPN grátis." : "Só quem estiver na mesma rede ou VPN vê a sala." );
	if ( BoxButton( px + pw - S( 24 ) - gw, gy, gw, gh, m_hHeading, "CRIAR", true, true ) )
		StartCreatedGame();
}

void CTrokMenu::StartCreatedGame( void )
{
	if ( !m_Maps.Count() )
		return;
	if ( m_iTextField >= 0 )
	{
		( m_iTextField == 0 ? trok_ui_criar_nome : trok_ui_criar_senha ).SetValue( m_szText );
		m_iTextField = -1;
	}
	const char *pszMap = m_Maps[ m_iMap ].Get();
	trok_ui_mapa.SetValue( pszMap );

	char szNome[ 64 ], szSenha[ 64 ];
	TrokLimparTexto( trok_ui_criar_nome.GetString(), szNome, sizeof( szNome ) );
	TrokLimparTexto( trok_ui_criar_senha.GetString(), szSenha, sizeof( szSenha ) );
	if ( !szNome[ 0 ] )
		V_strncpy( szNome, "Sala Rearmed", sizeof( szNome ) );
	const bool bOnline = trok_ui_criar_online.GetBool();
	const int iJog = clamp( trok_ui_criar_jogadores.GetInt(), 2, 32 );

	// As escolhas desta tela vão para cfg/partida_menu.cfg, que o servidor executa depois do cfg do
	// modo (trok_gamerules.cpp); assim a meta e o tempo daqui não são sobrescritos.
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buf.Printf( "// Escrito pelo menu CRIAR PARTIDA. Apagado ao jogar pelo JOGAR.\n" );
	buf.Printf( "hostname \"%s\"\n", szNome );
	buf.Printf( "sv_password \"%s\"\n", szSenha );
	buf.Printf( "sv_lan %d\n", bOnline ? 0 : 1 );
	if ( trok_ui_criar_meta.GetInt() > 0 && trok_ui_modo.GetInt() != 2 )
		buf.Printf( "mp_fraglimit %d\n", trok_ui_criar_meta.GetInt() );
	if ( trok_ui_criar_tempo.GetInt() > 0 && trok_ui_modo.GetInt() != 2 )
		buf.Printf( "mp_timelimit %d\n", trok_ui_criar_tempo.GetInt() );
	g_pFullFileSystem->WriteFile( "cfg/partida_menu.cfg", "MOD", buf );

	// Partida para outras pessoas: o online.cfg (sem trapaças) passa a valer em cada mapa.
	char szCmd[ 1024 ];
	V_snprintf( szCmd, sizeof( szCmd ),
		"progress_enable; disconnect; exec online; trok_modo %s; hl2mp_bot_difficulty %d; "
		"hl2mp_bot_quota %d; hl2mp_bot_quota_mode fill; maxplayers %d; map %s\n",
		( trok_ui_modo.GetInt() == 2 ) ? "surto" : ( trok_ui_modo.GetInt() == 1 ? "solo" : "equipe" ),
		clamp( trok_ui_bots_dificuldade.GetInt(), 0, 3 ),
		trok_ui_criar_bots.GetBool() ? MAX( 1, iJog - 2 ) : 0, iJog, pszMap ); // "fill": sobram 2 vagas; cada amigo que entra tira um bot
	engine->ClientCmd_Unrestricted( szCmd );
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

	// Partida contra bots: sem as regras de uma sala criada antes.
	if ( g_pFullFileSystem->FileExists( "cfg/partida_menu.cfg", "MOD" ) )
		g_pFullFileSystem->RemoveFile( "cfg/partida_menu.cfg", "MOD" );

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
		"progress_enable; lservercfgfile listenserver.cfg; trok_modo %s; hl2mp_bot_difficulty %d; trok_bot_estilo \"\"; trok_bot_estilos \"%s\"; "
		"hl2mp_bot_quota %d; hl2mp_bot_quota_mode fill; maxplayers %d; map %s\n",
		( trok_ui_modo.GetInt() == 2 ) ? "surto" : ( trok_ui_modo.GetInt() == 1 ? "solo" : "equipe" ), iNivel, szEstilos, iPorTime * 2, MIN( 32, iPorTime * 2 + 2 ), pszMap );
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
