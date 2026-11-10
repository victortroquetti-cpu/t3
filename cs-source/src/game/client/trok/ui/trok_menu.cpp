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
// CONFIGURAÇÕES, CRIAR PARTIDA e SERVIDORES são próprias (mais abaixo; a lista vem de
// trok_servidores.cpp). Os modos de JOGAR e CRIAR PARTIDA vêm de scripts/trok_modos.txt.
// Ainda do GameUI (pendências em design/ui.md): caixas de confirmação. A tela de carregamento
// é trok_loading.cpp.
//=============================================================================

#include "cbase.h"
#include "trok_menu.h"
#include "trok_servidores.h"
#include "tier1/KeyValues.h"
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
static ConVar trok_ui_modo( "trok_ui_modo", "equipe", FCVAR_ARCHIVE, "Menu: modo escolhido (chave de scripts/trok_modos.txt: equipe, solo, surto, corrida)." );
static ConVar trok_ui_mapas( "trok_ui_mapas", "", FCVAR_ARCHIVE, "Menu: último mapa de cada modo (modo:mapa separados por espaço)." );
// SERVIDORES
static ConVar trok_ui_servidores_internet( "trok_ui_servidores_internet", "1", FCVAR_ARCHIVE, "Servidores: 1 = lista da internet, 0 = rede local/VPN." );
static ConVar trok_ui_servidor_ip( "trok_ui_servidor_ip", "", FCVAR_ARCHIVE, "Servidores: último endereço digitado em CONECTAR POR IP." );
static ConVar trok_ui_servidor_senha( "trok_ui_servidor_senha", "", 0, "Servidores: senha para entrar (não fica salva)." );
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
	enum State_t { STATE_ROOT = 0, STATE_PLAY, STATE_OPTIONS, STATE_CREATE, STATE_SERVERS };

	CTrokMenu();
	virtual ~CTrokMenu();

	IGameUI *GetGameUI( void );
	State_t GetState( void ) const { return m_eState; }
	bool IsShown( void ) { return ipanel()->IsFullyVisible( GetVPanel() ); }
	void OnRelayedKey( KeyCode code );
	bool IsCapturingKey( void ) const { return m_eState == STATE_OPTIONS && m_iCapture >= 0; }
	void CaptureButton( ButtonCode_t code );
	// A tecla capturada também chega depois como "digitada": não pode virar ESC/Enter do menu.
	bool IsEditingText( void ) const { return ( m_eState == STATE_CREATE || m_eState == STATE_SERVERS ) && m_iTextField >= 0; }
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
	void CommitTextField( void );		// grava o texto em edição na ConVar do campo
	void RunServers( void );
	void ConnectTo( const char *pszEndereco );

	// Modos (scripts/trok_modos.txt)
	struct Modo_t
	{
		char szChave[ 32 ], szModo[ 32 ], szNome[ 64 ], szCurto[ 32 ], szDescricao[ 160 ];
		char szRegras[ 4 ][ 160 ];
		char szCfg[ 32 ], szMetaCvar[ 48 ], szMetaRotulo[ 48 ], szTempoCvar[ 48 ], szTempoUnidade[ 8 ];
		char szMapas[ 128 ], szMapaPadrao[ 64 ];
		int iMetaOpcoes[ 12 ], nMetaOpcoes;
		int iTempoOpcoes[ 12 ], nTempoOpcoes;
		int iMetaCfg, iTempoCfg;	// lidos do cfg do modo (-1 = não achou)
		bool bTimes, bBots, bEmBreve;
	};
	void BuildModes( void );
	int CurrentMode( void );						// índice em m_Modos do modo escolhido
	void SelectMode( int iModo );					// troca o modo e o mapa (o último deste modo)
	bool MapAllowed( int iMap );					// o mapa serve para o modo escolhido
	void StepMap( int iDir );
	void RememberMap( void );
	void RunModeCards( int x0, int xr, int y, bool bSoBots );
	int RunModeRules( int x, int y, int w );		// regras do modo; devolve a altura usada
	void ModeGoalText( const Modo_t &m, char *pszOut, int iOutSize );
	CUtlVector< Modo_t > m_Modos;
	bool m_bModesBuilt;

	// Servidores
	int m_iServerSel;
	int m_iServerScroll;
	float m_flServerClickTime;

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

	// Campo de texto em edição (-1 = nenhum; 0 = nome e 1 = senha da sala em CRIAR PARTIDA;
	// 2 = endereço e 3 = senha em SERVIDORES)
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
	m_bModesBuilt = false;
	m_iServerSel = -1;
	m_iServerScroll = 0;
	m_flServerClickTime = 0.0f;

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
	if ( m_iTextField >= 0 )
		CommitTextField();
	if ( m_eState == STATE_SERVERS && eState != STATE_SERVERS )
		TrokServidores::Parar();
	m_eState = eState;
	m_iLastHoverId = -1;
	if ( eState == STATE_PLAY || eState == STATE_CREATE )
	{
		BuildModes();
		BuildMapList();
		SelectMode( CurrentMode() ); // mapa do modo escolhido (e um modo que sirva para esta tela)
	}
	if ( eState == STATE_SERVERS )
	{
		m_iServerSel = -1;
		m_iServerScroll = 0;
		TrokServidores::Atualizar( trok_ui_servidores_internet.GetBool() );
	}
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
	else if ( m_eState == STATE_SERVERS )
		m_iServerScroll = MAX( 0, m_iServerScroll - delta * 3 );
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
			const bool bConectar = ( m_iTextField == 2 && ( code == KEY_ENTER || code == KEY_PAD_ENTER ) );
			CommitTextField();
			if ( bConectar && trok_ui_servidor_ip.GetString()[ 0 ] )
				ConnectTo( trok_ui_servidor_ip.GetString() );
		}
		return;
	}
	if ( m_eState == STATE_SERVERS )
	{
		if ( ( code == KEY_ENTER || code == KEY_PAD_ENTER ) && m_iServerSel >= 0 && m_iServerSel < TrokServidores::Quantos() )
		{
			ConnectTo( TrokServidores::Servidor( m_iServerSel ).szEndereco );
			return;
		}
		if ( code == KEY_F5 )
		{
			TrokServidores::Atualizar( trok_ui_servidores_internet.GetBool() );
			return;
		}
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
	case STATE_SERVERS: RunServers(); break;
	default: RunRoot(); break;
	}

	RunBottomBar();
	m_eMode = MODE_PAINT;
}

static ConVar trok_ui_fundo_animado( "trok_ui_fundo_animado", "1", FCVAR_ARCHIVE, "Menu sem mapa de fundo: 1 = fundo animado (grade, varredura e poeira), 0 = liso." );

// Fundo animado do menu, desenhado por código (sem mapa): grade tática que desliza devagar,
// uma faixa de luz que atravessa a tela, uma linha de varredura e poeira subindo.
// Tudo bem fraco, para não brigar com o texto (direção sóbria de design/lore-e-arte.md).
static void TrokFundoAnimado( int w, int h, float flEscala )
{
	const float t = (float)Plat_FloatTime();
	ISurface *pS = surface();

	// Grade
	const int iCelula = MAX( 8, (int)( 48.0f * flEscala ) );
	const int iDesl = (int)fmodf( t * 4.0f * flEscala, (float)iCelula );
	pS->DrawSetColor( Color( 255, 255, 255, 7 ) );
	for ( int x = -iCelula + iDesl; x < w; x += iCelula )
		pS->DrawFilledRect( x, 0, x + 1, h );
	for ( int y = -iCelula + iDesl / 2; y < h; y += iCelula )
		pS->DrawFilledRect( 0, y, w, y + 1 );

	// Faixa de luz (atravessa em 26 s)
	{
		const float f = fmodf( t / 26.0f, 1.0f );
		const int bw = w / 3;
		const int bx = (int)( -bw + f * ( w + bw * 2 ) ) - bw / 2;
		pS->DrawSetColor( Color( 120, 140, 160, 255 ) );
		pS->DrawFilledRectFade( bx, 0, bx + bw / 2, h, 0, 10, true );
		pS->DrawFilledRectFade( bx + bw / 2, 0, bx + bw, h, 10, 0, true );
	}

	// Linha de varredura (desce em 9 s) com rastro
	{
		const float f = fmodf( t / 9.0f, 1.0f );
		const int y = (int)( f * h );
		const int iRastro = (int)( 90.0f * flEscala );
		pS->DrawSetColor( Color( 255, 255, 255, 255 ) );
		pS->DrawFilledRectFade( 0, y - iRastro, w, y, 0, 6, false );
		pS->DrawSetColor( Color( 255, 255, 255, 16 ) );
		pS->DrawFilledRect( 0, y, w, y + 1 );
	}

	// Poeira: posições fixas por índice, subindo devagar
	for ( int i = 0; i < 48; ++i )
	{
		const unsigned int a = ( i * 2654435761u ) ^ 0x9E3779B9u;
		const float fx = ( ( a & 0xFFFF ) / 65535.0f );
		const float fv = 0.01f + ( ( ( a >> 16 ) & 0xFF ) / 255.0f ) * 0.025f;		// velocidade (tela/s)
		const float fy = 1.0f - fmodf( ( ( a >> 8 ) & 0xFFFF ) / 65535.0f + t * fv, 1.0f );
		const int px = (int)( fx * w + sinf( t * 0.3f + i ) * 12.0f * flEscala );
		const int py = (int)( fy * h );
		const int iTam = ( i % 3 == 0 ) ? 2 : 1;
		pS->DrawSetColor( Color( 210, 220, 230, 18 + ( i % 4 ) * 6 ) );
		pS->DrawFilledRect( px, py, px + iTam, py + iTam );
	}

	// Vinheta em baixo e em cima
	pS->DrawSetColor( Color( 0, 0, 0, 255 ) );
	pS->DrawFilledRectFade( 0, h * 7 / 10, w, h, 0, 140, false );
	pS->DrawFilledRectFade( 0, 0, w, h / 6, 90, 0, false );
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
		// Sem mapa de fundo: fundo escuro com um degradê leve e a animação por código
		// (um background01.bsp próprio substitui isto; ver design/ui.md, "Fundo do menu").
		Rect( 0, 0, w, h, COR_FUNDO );
		surface()->DrawSetColor( Color( 27, 34, 43, 255 ) );
		surface()->DrawFilledRectFade( 0, 0, w, h * 6 / 10, 255, 0, false );
		if ( trok_ui_fundo_animado.GetBool() )
			TrokFundoAnimado( w, h, m_flScale );
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
			SetState( STATE_SERVERS );
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

	Text( m_hBody, iSep + S( 21 ) + TextWide( m_hTitle, "JOGAR" ) + S( 30 ), S( 50 ), COR_TEXTO2, "Contra bots, offline. Escolha o modo, o mapa e os bots." );

	// Cartões de modo (scripts/trok_modos.txt)
	RunModeCards( x0, xr, S( 96 ), true );

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

		// Regras do modo escolhido (os cartões ficam em cima)
		y += RunModeRules( ix, y, iw );

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

		const bool bTimes = m_Modos.Count() ? m_Modos[ CurrentMode() ].bTimes : true;
		Label( ix, y, bTimes ? "BOTS POR TIME" : "BOTS (METADE DO TOTAL)" );
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
			StepMap( -1 );
		if ( BoxButton( mx + mw - S( 18 ) - bs, iy + S( 14 ), bs, bs, m_hButton, "›", false, false ) && m_Maps.Count() )
			StepMap( 1 );
	}
}

//-----------------------------------------------------------------------------
// Modos (scripts/trok_modos.txt): cartões no estilo da sala do Combat Arms, regras, meta
// e tempo lidos do cfg do modo, e o último mapa de cada modo.
//-----------------------------------------------------------------------------

// Lê "nome valor" de um cfg (ignora comentários). Devolve -1 se não achou.
static int TrokLerValorCfg( const char *pszCfg, const char *pszCvar )
{
	if ( !pszCfg[ 0 ] || !pszCvar[ 0 ] )
		return -1;
	char szArq[ 96 ];
	V_snprintf( szArq, sizeof( szArq ), "cfg/%s.cfg", pszCfg );
	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	if ( !g_pFullFileSystem->ReadFile( szArq, "GAME", buf ) )
		return -1;
	int iValor = -1;
	char szLinha[ 512 ];
	while ( buf.IsValid() )
	{
		buf.GetLine( szLinha, sizeof( szLinha ) );
		if ( !szLinha[ 0 ] )
			break;
		char *pCom = V_strstr( szLinha, "//" );
		if ( pCom )
			*pCom = 0;
		char szNome[ 64 ] = "", szValor[ 64 ] = "";
		if ( sscanf( szLinha, " %63s %63s", szNome, szValor ) == 2 && !V_stricmp( szNome, pszCvar ) )
		{
			char *pv = szValor;
			if ( *pv == '"' )
				++pv;
			iValor = atoi( pv ); // a última linha vale, como no console
		}
	}
	return iValor;
}

static int TrokLerLista( const char *psz, int *pOut, int iMax )
{
	int n = 0;
	while ( psz && *psz && n < iMax )
	{
		while ( *psz == ' ' )
			++psz;
		if ( !*psz )
			break;
		pOut[ n++ ] = atoi( psz );
		while ( *psz && *psz != ' ' )
			++psz;
	}
	return n;
}

void CTrokMenu::BuildModes( void )
{
	if ( m_bModesBuilt )
		return;
	m_bModesBuilt = true;
	m_Modos.RemoveAll();

	KeyValues *pKV = new KeyValues( "Modos" );
	if ( pKV->LoadFromFile( g_pFullFileSystem, "scripts/trok_modos.txt", "GAME" ) )
	{
		for ( KeyValues *p = pKV->GetFirstTrueSubKey(); p; p = p->GetNextTrueSubKey() )
		{
			Modo_t m;
			V_memset( &m, 0, sizeof( m ) );
			V_strncpy( m.szChave, p->GetName(), sizeof( m.szChave ) );
			V_strncpy( m.szModo, p->GetString( "modo", p->GetName() ), sizeof( m.szModo ) );
			V_strncpy( m.szNome, p->GetString( "nome", p->GetName() ), sizeof( m.szNome ) );
			V_strncpy( m.szCurto, p->GetString( "curto", m.szNome ), sizeof( m.szCurto ) );
			V_strncpy( m.szDescricao, p->GetString( "descricao" ), sizeof( m.szDescricao ) );
			for ( int r = 0; r < 4; ++r )
			{
				char szChave[ 16 ];
				V_snprintf( szChave, sizeof( szChave ), "regra%d", r + 1 );
				V_strncpy( m.szRegras[ r ], p->GetString( szChave ), sizeof( m.szRegras[ r ] ) );
			}
			V_strncpy( m.szCfg, p->GetString( "cfg", m.szModo ), sizeof( m.szCfg ) );
			V_strncpy( m.szMetaCvar, p->GetString( "meta_cvar" ), sizeof( m.szMetaCvar ) );
			V_strncpy( m.szMetaRotulo, p->GetString( "meta_rotulo", "ABATES" ), sizeof( m.szMetaRotulo ) );
			V_strncpy( m.szTempoCvar, p->GetString( "tempo_cvar" ), sizeof( m.szTempoCvar ) );
			V_strncpy( m.szTempoUnidade, p->GetString( "tempo_unidade", "min" ), sizeof( m.szTempoUnidade ) );
			V_strncpy( m.szMapas, p->GetString( "mapas" ), sizeof( m.szMapas ) );
			V_strncpy( m.szMapaPadrao, p->GetString( "mapa_padrao" ), sizeof( m.szMapaPadrao ) );
			m.nMetaOpcoes = TrokLerLista( p->GetString( "meta_opcoes" ), m.iMetaOpcoes, ARRAYSIZE( m.iMetaOpcoes ) );
			m.nTempoOpcoes = TrokLerLista( p->GetString( "tempo_opcoes" ), m.iTempoOpcoes, ARRAYSIZE( m.iTempoOpcoes ) );
			m.bTimes = p->GetBool( "times" );
			m.bBots = p->GetBool( "bots", true );
			m.bEmBreve = p->GetBool( "em_breve" );
			m.iMetaCfg = TrokLerValorCfg( m.szCfg, m.szMetaCvar );
			m.iTempoCfg = TrokLerValorCfg( m.szCfg, m.szTempoCvar );
			m_Modos.AddToTail( m );
		}
	}
	pKV->deleteThis();

	if ( !m_Modos.Count() )
	{
		// Sem o script: os três modos de sempre, sem textos.
		static const char *s_psz[][ 3 ] = { { "equipe", "MATA-MATA EM EQUIPE", "EQUIPE" }, { "solo", "EXÉRCITO SOLO", "SOLO" }, { "surto", "SURTO", "SURTO" } };
		for ( int i = 0; i < ARRAYSIZE( s_psz ); ++i )
		{
			Modo_t m;
			V_memset( &m, 0, sizeof( m ) );
			V_strncpy( m.szChave, s_psz[ i ][ 0 ], sizeof( m.szChave ) );
			V_strncpy( m.szModo, s_psz[ i ][ 0 ], sizeof( m.szModo ) );
			V_strncpy( m.szCfg, s_psz[ i ][ 0 ], sizeof( m.szCfg ) );
			V_strncpy( m.szNome, s_psz[ i ][ 1 ], sizeof( m.szNome ) );
			V_strncpy( m.szCurto, s_psz[ i ][ 2 ], sizeof( m.szCurto ) );
			m.bTimes = ( i == 0 );
			m.bBots = true;
			m.iMetaCfg = m.iTempoCfg = -1;
			m_Modos.AddToTail( m );
		}
		Warning( "Trok: scripts/trok_modos.txt não encontrado; o menu usa só os modos básicos.\n" );
	}

	// Valor antigo de trok_ui_modo (0, 1, 2) vira a chave.
	const char *pszAntigo = trok_ui_modo.GetString();
	if ( pszAntigo[ 0 ] >= '0' && pszAntigo[ 0 ] <= '9' )
	{
		static const char *s_pszChaves[] = { "equipe", "solo", "surto" };
		trok_ui_modo.SetValue( s_pszChaves[ clamp( atoi( pszAntigo ), 0, 2 ) ] );
	}
}

int CTrokMenu::CurrentMode( void )
{
	const char *pszChave = trok_ui_modo.GetString();
	FOR_EACH_VEC( m_Modos, i )
	{
		if ( !m_Modos[ i ].bEmBreve && !V_stricmp( m_Modos[ i ].szChave, pszChave ) )
			return i;
	}
	FOR_EACH_VEC( m_Modos, i )
	{
		if ( !m_Modos[ i ].bEmBreve )
			return i;
	}
	return 0;
}

bool CTrokMenu::MapAllowed( int iMap )
{
	if ( !m_Modos.Count() || iMap < 0 || iMap >= m_Maps.Count() )
		return true;
	const char *pszPrefixos = m_Modos[ CurrentMode() ].szMapas;
	if ( !pszPrefixos[ 0 ] )
		return true;
	const char *pszMapa = m_Maps[ iMap ].Get();
	const char *p = pszPrefixos;
	while ( *p )
	{
		while ( *p == ' ' )
			++p;
		const char *pFim = p;
		while ( *pFim && *pFim != ' ' )
			++pFim;
		const int iLen = (int)( pFim - p );
		if ( iLen > 0 && !V_strnicmp( pszMapa, p, iLen ) )
			return true;
		p = pFim;
	}
	return false;
}

void CTrokMenu::StepMap( int iDir )
{
	const int n = m_Maps.Count();
	for ( int k = 1; k <= n; ++k )
	{
		const int i = ( ( m_iMap + iDir * k ) % n + n ) % n;
		if ( MapAllowed( i ) )
		{
			m_iMap = i;
			break;
		}
	}
	RememberMap();
}

// Guarda o mapa atual como o último deste modo (trok_ui_mapas "modo:mapa ...").
void CTrokMenu::RememberMap( void )
{
	if ( !m_Maps.Count() || !m_Modos.Count() )
		return;
	const char *pszChave = m_Modos[ CurrentMode() ].szChave;
	char szNovo[ 512 ] = "";
	char szAtual[ 512 ];
	V_strncpy( szAtual, trok_ui_mapas.GetString(), sizeof( szAtual ) );
	for ( char *pTok = strtok( szAtual, " " ); pTok; pTok = strtok( NULL, " " ) )
	{
		const char *pDois = strchr( pTok, ':' );
		if ( pDois && (int)( pDois - pTok ) == V_strlen( pszChave ) && !V_strnicmp( pTok, pszChave, V_strlen( pszChave ) ) )
			continue;
		if ( szNovo[ 0 ] )
			V_strncat( szNovo, " ", sizeof( szNovo ) );
		V_strncat( szNovo, pTok, sizeof( szNovo ) );
	}
	char szPar[ 128 ];
	V_snprintf( szPar, sizeof( szPar ), "%s%s:%s", szNovo[ 0 ] ? " " : "", pszChave, m_Maps[ m_iMap ].Get() );
	V_strncat( szNovo, szPar, sizeof( szNovo ) );
	trok_ui_mapas.SetValue( szNovo );
}

void CTrokMenu::SelectMode( int iModo )
{
	if ( !m_Modos.IsValidIndex( iModo ) || m_Modos[ iModo ].bEmBreve )
		return;
	// JOGAR só mostra modos com bots.
	if ( m_eState == STATE_PLAY && !m_Modos[ iModo ].bBots )
	{
		FOR_EACH_VEC( m_Modos, i )
		{
			if ( m_Modos[ i ].bBots && !m_Modos[ i ].bEmBreve )
			{
				iModo = i;
				break;
			}
		}
	}
	trok_ui_modo.SetValue( m_Modos[ iModo ].szChave );
	if ( !m_Maps.Count() )
		return;

	// Mapa: o último usado neste modo, senão o padrão do modo, senão o atual (se servir).
	const char *pszChave = m_Modos[ iModo ].szChave;
	char szLembrado[ 64 ] = "";
	{
		char szLista[ 512 ];
		V_strncpy( szLista, trok_ui_mapas.GetString(), sizeof( szLista ) );
		for ( char *pTok = strtok( szLista, " " ); pTok; pTok = strtok( NULL, " " ) )
		{
			const char *pDois = strchr( pTok, ':' );
			if ( pDois && (int)( pDois - pTok ) == V_strlen( pszChave ) && !V_strnicmp( pTok, pszChave, V_strlen( pszChave ) ) )
				V_strncpy( szLembrado, pDois + 1, sizeof( szLembrado ) );
		}
	}
	const char *pszQuero[] = { szLembrado, m_Modos[ iModo ].szMapaPadrao };
	for ( int q = 0; q < 2; ++q )
	{
		if ( !pszQuero[ q ][ 0 ] )
			continue;
		FOR_EACH_VEC( m_Maps, i )
		{
			if ( !V_stricmp( m_Maps[ i ], pszQuero[ q ] ) && MapAllowed( i ) )
			{
				m_iMap = i;
				return;
			}
		}
	}
	if ( !MapAllowed( m_iMap ) )
		StepMap( 1 );
}

// "80 ABATES DO TIME  ·  15 MIN" com os números do cfg do modo.
void CTrokMenu::ModeGoalText( const Modo_t &m, char *pszOut, int iOutSize )
{
	char szMeta[ 64 ] = "", szTempo[ 64 ] = "";
	if ( m.szMetaCvar[ 0 ] )
	{
		if ( m.iMetaCfg > 0 )
			V_snprintf( szMeta, sizeof( szMeta ), "META %d %s", m.iMetaCfg, m.szMetaRotulo );
		else
			V_snprintf( szMeta, sizeof( szMeta ), "META: SEM LIMITE" );
	}
	if ( m.szTempoCvar[ 0 ] && m.iTempoCfg > 0 )
	{
		if ( !V_stricmp( m.szTempoUnidade, "s" ) )
			V_snprintf( szTempo, sizeof( szTempo ), "RODADA %d:%02d", m.iTempoCfg / 60, m.iTempoCfg % 60 );
		else
			V_snprintf( szTempo, sizeof( szTempo ), "TEMPO %d MIN", m.iTempoCfg );
	}
	V_snprintf( pszOut, iOutSize, "%s%s%s", szMeta, ( szMeta[ 0 ] && szTempo[ 0 ] ) ? "  ·  " : "", szTempo );
}

// Faixa de cartões: um por modo; os "em breve" ficam apagados e não clicam.
void CTrokMenu::RunModeCards( int x0, int xr, int y, bool bSoBots )
{
	const Color acc = Accent();
	int nCartoes = 0;
	FOR_EACH_VEC( m_Modos, i )
	{
		if ( !bSoBots || m_Modos[ i ].bBots || m_Modos[ i ].bEmBreve )
			++nCartoes;
	}
	if ( !nCartoes )
		return;
	const int iAtual = CurrentMode();
	const int iGap = S( 8 );
	const int cw = MIN( S( 220 ), ( xr - x0 - iGap * ( nCartoes - 1 ) ) / nCartoes );
	const int ch = S( 104 );
	int cx = x0;
	int iNum = 0;
	FOR_EACH_VEC( m_Modos, i )
	{
		const Modo_t &m = m_Modos[ i ];
		if ( bSoBots && !m.bBots && !m.bEmBreve )
			continue;
		++iNum;
		const bool bAtual = ( i == iAtual );
		const bool bHover = !m.bEmBreve && Hover( cx, y, cw, ch );
		Rect( cx, y, cw, ch, bAtual ? COR_PAINEL_HOVER : ( bHover ? COR_PAINEL_HOVER : COR_PAINEL ) );
		Outline( cx, y, cw, ch, bAtual ? acc : ( bHover ? COR_LINHA_FORTE : COR_LINHA ) );
		if ( bAtual )
			Rect( cx, y, cw, S( 3 ), acc );

		char szNum[ 8 ];
		V_snprintf( szNum, sizeof( szNum ), "%02d", iNum );
		Text( m_hSmall, cx + S( 14 ), y + S( 12 ), m.bEmBreve ? COR_TEXTO3 : ( bAtual ? acc : COR_TEXTO3 ), szNum );
		if ( m.bEmBreve )
			Text( m_hSmall, cx + cw - S( 14 ) - TextWide( m_hSmall, "EM BREVE" ), y + S( 12 ), COR_TEXTO3, "EM BREVE" );
		else if ( m.bTimes )
			Text( m_hSmall, cx + cw - S( 14 ) - TextWide( m_hSmall, "TIMES" ), y + S( 12 ), COR_TEXTO3, "TIMES" );
		TextSpaced( m_hHeading, cx + S( 14 ), y + S( 32 ), m.bEmBreve ? COR_TEXTO3 : COR_TEXTO, m.szCurto, S( 1 ) );
		// Nome completo em letra pequena quando difere do curto
		if ( V_stricmp( m.szNome, m.szCurto ) )
			Text( m_hSmall, cx + S( 14 ), y + S( 62 ), m.bEmBreve ? Color( 128, 134, 142, 150 ) : COR_TEXTO2, m.szNome );
		if ( !m.bEmBreve && Clicked( cx, y, cw, ch ) )
			SelectMode( i );
		if ( bHover )
			m_iHoverId = 200 + i;
		cx += cw + iGap;
	}

	// Linha do modo escolhido: descrição à esquerda, meta e tempo à direita.
	if ( m_Modos.IsValidIndex( iAtual ) )
	{
		const Modo_t &m = m_Modos[ iAtual ];
		const int ty = y + ch + S( 14 );
		Text( m_hBody, x0, ty, COR_TEXTO2, m.szDescricao );
		char szMeta[ 160 ];
		ModeGoalText( m, szMeta, sizeof( szMeta ) );
		if ( szMeta[ 0 ] )
			Text( m_hSmall, xr - TextWide( m_hSmall, szMeta ), ty + S( 2 ), acc, szMeta );
	}
}

// Bloco de regras do modo escolhido no painel da direita.
int CTrokMenu::RunModeRules( int x, int y, int w )
{
	if ( !m_Modos.Count() )
		return 0;
	const Modo_t &m = m_Modos[ CurrentMode() ];
	int yy = y;
	Label( x, yy, "REGRAS" );
	Text( m_hSmall, x + w - TextWide( m_hSmall, m.szNome ), yy, COR_TEXTO3, m.szNome );
	yy += S( 20 );
	const int iLinha = surface()->GetFontTall( m_hBody ) + S( 3 );
	int nRegras = 0;
	for ( int r = 0; r < 4 && nRegras < 3; ++r )
	{
		if ( !m.szRegras[ r ][ 0 ] )
			continue;
		Rect( x, yy + iLinha / 2 - S( 2 ), S( 4 ), S( 4 ), Accent() );
		Text( m_hBody, x + S( 14 ), yy, COR_TEXTO2, m.szRegras[ r ] );
		yy += iLinha;
		++nRegras;
	}
	if ( !nRegras )
	{
		Text( m_hBody, x, yy, COR_TEXTO2, m.szDescricao );
		yy += iLinha;
	}
	return ( yy - y ) + S( 14 );
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
		KeyHint( x, ty, "ENTER", m_iTextField == 2 ? "Conectar" : "Confirmar" );
	else if ( m_eState == STATE_SERVERS )
	{
		x -= S( 90 );
		KeyHint( x, ty, "F5", "Atualizar" );
		KeyHint( x, ty, "ENTER", "Entrar" );
	}
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
		OptToggle( ix, y, iw, "Segurar o botão para mirar (desligado: aperta para entrar e sair)", "trok_mira_segurar" );
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
			CommitTextField();
		m_iTextField = iField;
		V_strncpy( m_szText, pszValue, sizeof( m_szText ) );
		return true;
	}
	return false;
}

void CTrokMenu::CommitTextField( void )
{
	switch ( m_iTextField )
	{
	case 0: trok_ui_criar_nome.SetValue( m_szText ); break;
	case 1: trok_ui_criar_senha.SetValue( m_szText ); break;
	case 2: trok_ui_servidor_ip.SetValue( m_szText ); break;
	case 3: trok_ui_servidor_senha.SetValue( m_szText ); break;
	default: break;
	}
	m_iTextField = -1;
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
	Text( m_hBody, iSep + S( 21 ) + TextWide( m_hTitle, "CRIAR PARTIDA" ) + S( 40 ), S( 50 ), COR_TEXTO2, "Seu servidor, suas regras. Os amigos entram pelo seu IP (LEIA-ME: JOGAR COM AMIGOS)." );

	// Cartões de modo (scripts/trok_modos.txt)
	RunModeCards( x0, xr, S( 96 ), false );

	const int top = S( 252 ), bottom = m_iTall - S( 72 );
	RunMapPanel( x0, top, S( 380 ), bottom );

	const int px = x0 + S( 380 ) + S( 16 ), pw = xr - px;
	Rect( px, top, pw, bottom - top, COR_PAINEL );
	Outline( px, top, pw, bottom - top, COR_LINHA );
	const int ix = px + S( 24 ), iw = pw - S( 48 );
	const int iHalf = ( iw - S( 24 ) ) / 2;
	int y = top + S( 18 );
	const Modo_t *pModo = m_Modos.Count() ? &m_Modos[ CurrentMode() ] : NULL;

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

	// Meta e tempo: as escolhas de cada modo vêm de scripts/trok_modos.txt (0 = a do cfg do modo).
	{
		int yl = y, yr = y;
		if ( pModo && pModo->nMetaOpcoes > 0 )
		{
			int iMeta = 0;
			for ( int i = 0; i < pModo->nMetaOpcoes; ++i ) if ( trok_ui_criar_meta.GetInt() == pModo->iMetaOpcoes[ i ] ) iMeta = i;
			char szMeta[ 48 ];
			if ( pModo->iMetaOpcoes[ iMeta ] > 0 )
				V_snprintf( szMeta, sizeof( szMeta ), "%d", pModo->iMetaOpcoes[ iMeta ] );
			else if ( pModo->iMetaCfg > 0 )
				V_snprintf( szMeta, sizeof( szMeta ), "%d (PADRÃO)", pModo->iMetaCfg );
			else
				V_strncpy( szMeta, "DO MODO", sizeof( szMeta ) );
			char szRotulo[ 64 ];
			V_snprintf( szRotulo, sizeof( szRotulo ), "META: %s", pModo->szMetaRotulo );
			Label( ix, y, szRotulo );
			yl += S( 18 );
			if ( OptCycle( ix, yl, iHalf, "", szMeta, iDir ) )
				trok_ui_criar_meta.SetValue( pModo->iMetaOpcoes[ ( iMeta + iDir + pModo->nMetaOpcoes ) % pModo->nMetaOpcoes ] );
		}
		if ( pModo && pModo->nTempoOpcoes > 0 )
		{
			int iTempo = 0;
			for ( int i = 0; i < pModo->nTempoOpcoes; ++i ) if ( trok_ui_criar_tempo.GetInt() == pModo->iTempoOpcoes[ i ] ) iTempo = i;
			char szTempo[ 48 ];
			if ( pModo->iTempoOpcoes[ iTempo ] > 0 )
				V_snprintf( szTempo, sizeof( szTempo ), "%d MIN", pModo->iTempoOpcoes[ iTempo ] );
			else if ( pModo->iTempoCfg > 0 )
				V_snprintf( szTempo, sizeof( szTempo ), "%d MIN (PADRÃO)", pModo->iTempoCfg );
			else
				V_strncpy( szTempo, "DO MODO", sizeof( szTempo ) );
			Label( ix + iHalf + S( 24 ), y, "TEMPO DA PARTIDA" );
			yr += S( 18 );
			if ( OptCycle( ix + iHalf + S( 24 ), yr, iHalf, "", szTempo, iDir ) )
				trok_ui_criar_tempo.SetValue( pModo->iTempoOpcoes[ ( iTempo + iDir + pModo->nTempoOpcoes ) % pModo->nTempoOpcoes ] );
		}
		if ( yl == y && yr == y )
		{
			char szCfg[ 128 ];
			V_snprintf( szCfg, sizeof( szCfg ), "Neste modo, a meta e os tempos vêm do cfg/%s.cfg.", pModo ? pModo->szCfg : "" );
			Text( m_hSmall, ix, y + S( 6 ), COR_TEXTO3, szCfg );
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
		CommitTextField();
	const char *pszMap = m_Maps[ m_iMap ].Get();
	trok_ui_mapa.SetValue( pszMap );
	RememberMap();
	BuildModes();
	const Modo_t *pModo = m_Modos.Count() ? &m_Modos[ CurrentMode() ] : NULL;
	const char *pszModo = pModo ? pModo->szModo : "equipe";

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
	// Meta e tempo só se o modo oferece a escolha (e o valor escolhido é uma das opções dele).
	if ( pModo && pModo->szMetaCvar[ 0 ] && trok_ui_criar_meta.GetInt() > 0 )
	{
		for ( int i = 0; i < pModo->nMetaOpcoes; ++i )
		{
			if ( pModo->iMetaOpcoes[ i ] == trok_ui_criar_meta.GetInt() )
			{
				buf.Printf( "%s %d\n", pModo->szMetaCvar, trok_ui_criar_meta.GetInt() );
				break;
			}
		}
	}
	if ( pModo && pModo->szTempoCvar[ 0 ] && trok_ui_criar_tempo.GetInt() > 0 )
	{
		const bool bSegundos = !V_stricmp( pModo->szTempoUnidade, "s" );
		for ( int i = 0; i < pModo->nTempoOpcoes; ++i )
		{
			if ( pModo->iTempoOpcoes[ i ] == trok_ui_criar_tempo.GetInt() )
			{
				buf.Printf( "%s %d\n", pModo->szTempoCvar, trok_ui_criar_tempo.GetInt() * ( bSegundos ? 60 : 1 ) );
				break;
			}
		}
	}
	g_pFullFileSystem->WriteFile( "cfg/partida_menu.cfg", "MOD", buf );

	// Partida para outras pessoas: o online.cfg (sem trapaças) passa a valer em cada mapa.
	char szCmd[ 1024 ];
	V_snprintf( szCmd, sizeof( szCmd ),
		"progress_enable; disconnect; exec online; trok_modo %s; hl2mp_bot_difficulty %d; "
		"hl2mp_bot_quota %d; hl2mp_bot_quota_mode fill; maxplayers %d; map %s\n",
		pszModo,
		clamp( trok_ui_bots_dificuldade.GetInt(), 0, 3 ),
		trok_ui_criar_bots.GetBool() ? MAX( 1, iJog - 2 ) : 0, iJog, pszMap ); // "fill": sobram 2 vagas; cada amigo que entra tira um bot
	engine->ClientCmd_Unrestricted( szCmd );
}

//-----------------------------------------------------------------------------
// Servidores (substitui o navegador de servidores do GameUI; lista em trok_servidores.cpp)
//-----------------------------------------------------------------------------
void CTrokMenu::ConnectTo( const char *pszEndereco )
{
	char szEnd[ 64 ], szSenha[ 64 ];
	TrokLimparTexto( pszEndereco, szEnd, sizeof( szEnd ) );
	TrokLimparTexto( trok_ui_servidor_senha.GetString(), szSenha, sizeof( szSenha ) );
	for ( char *p = szEnd; *p; ++p )
	{
		if ( *p == ' ' )
		{
			*p = 0;
			break;
		}
	}
	if ( !szEnd[ 0 ] )
		return;
	TrokServidores::Parar();
	// Partida contra bots ou sala criada antes não deve valer num servidor de fora.
	char szCmd[ 256 ];
	V_snprintf( szCmd, sizeof( szCmd ), "progress_enable; password \"%s\"; connect %s\n", szSenha, szEnd );
	engine->ClientCmd_Unrestricted( szCmd );
}

void CTrokMenu::RunServers( void )
{
	const Color acc = Accent();
	const int x0 = S( 64 );
	const int xr = m_iWide - S( 56 );

	if ( LinkButton( x0, S( 52 ), m_hSmall, "‹ VOLTAR" ) )
	{
		SetState( STATE_ROOT );
		return;
	}
	const int iSep = x0 + TextWide( m_hSmall, "‹ VOLTAR" ) + S( 20 );
	Rect( iSep, S( 46 ), 1, S( 22 ), COR_LINHA_FORTE );
	TextSpaced( m_hTitle, iSep + S( 21 ), S( 36 ), COR_TEXTO, "SERVIDORES", S( 3 ) );

	// Abas e atualizar
	const int ty = S( 100 );
	const bool bInternet = trok_ui_servidores_internet.GetBool();
	if ( BoxButton( x0, ty, S( 150 ), S( 34 ), m_hButton, "INTERNET", bInternet, bInternet ) && !bInternet )
	{
		trok_ui_servidores_internet.SetValue( 1 );
		m_iServerSel = -1;
		m_iServerScroll = 0;
		TrokServidores::Atualizar( true );
	}
	if ( BoxButton( x0 + S( 156 ), ty, S( 190 ), S( 34 ), m_hButton, "REDE LOCAL / VPN", !bInternet, !bInternet ) && bInternet )
	{
		trok_ui_servidores_internet.SetValue( 0 );
		m_iServerSel = -1;
		m_iServerScroll = 0;
		TrokServidores::Atualizar( false );
	}
	if ( BoxButton( xr - S( 140 ), ty, S( 140 ), S( 34 ), m_hButton, TrokServidores::Atualizando() ? "PROCURANDO..." : "ATUALIZAR", false, false ) )
	{
		m_iServerSel = -1;
		TrokServidores::Atualizar( bInternet );
	}
	{
		char szStatus[ 160 ];
		const int n = TrokServidores::Quantos();
		if ( TrokServidores::Erro()[ 0 ] && !n )
			V_strncpy( szStatus, TrokServidores::Erro(), sizeof( szStatus ) );
		else if ( TrokServidores::Atualizando() )
			V_snprintf( szStatus, sizeof( szStatus ), "Procurando servidores... %d encontrado%s", n, n == 1 ? "" : "s" );
		else
			V_snprintf( szStatus, sizeof( szStatus ), n == 1 ? "1 servidor" : "%d servidores", n );
		Text( m_hSmall, x0 + S( 362 ), ty + ( S( 34 ) - surface()->GetFontTall( m_hSmall ) ) / 2, COR_TEXTO3, szStatus );
	}

	// Lista
	const int lx = x0, lw = xr - x0;
	const int ly = S( 148 ), lb = m_iTall - S( 168 );
	Rect( lx, ly, lw, lb - ly, COR_PAINEL );
	Outline( lx, ly, lw, lb - ly, COR_LINHA );
	const int xNome = lx + S( 18 );
	const int xPing = lx + lw - S( 18 );
	const int xJog = xPing - S( 90 );
	const int xMapa = xJog - S( 120 ) - S( 220 );
	const int hy = ly + S( 12 );
	Label( xNome, hy, "NOME" );
	Label( xMapa, hy, "MAPA" );
	Text( m_hSmall, xJog - TextWide( m_hSmall, "JOGADORES" ), hy, COR_TEXTO3, "JOGADORES" );
	Text( m_hSmall, xPing - TextWide( m_hSmall, "PING" ), hy, COR_TEXTO3, "PING" );
	const int iRow = S( 32 );
	const int ry0 = ly + S( 34 );
	Rect( lx + S( 6 ), ry0 - 1, lw - S( 12 ), 1, COR_LINHA );
	const int nCabem = MAX( 1, ( lb - ry0 - S( 6 ) ) / iRow );
	const int n = TrokServidores::Quantos();
	m_iServerScroll = clamp( m_iServerScroll, 0, MAX( 0, n - nCabem ) );
	const int bodyTall = surface()->GetFontTall( m_hBody );
	for ( int r = 0; r < nCabem && m_iServerScroll + r < n; ++r )
	{
		const int i = m_iServerScroll + r;
		const TrokServidor_t &sv = TrokServidores::Servidor( i );
		const int ry = ry0 + r * iRow;
		const bool bSel = ( i == m_iServerSel );
		const bool bHover = Hover( lx + 1, ry, lw - 2, iRow );
		if ( bSel )
			Rect( lx + 1, ry, lw - 2, iRow, Color( acc.r(), acc.g(), acc.b(), 34 ) );
		else if ( bHover )
			Rect( lx + 1, ry, lw - 2, iRow, Color( 255, 255, 255, 10 ) );
		const int tyRow = ry + ( iRow - bodyTall ) / 2;
		const bool bCheio = sv.iMaximo > 0 && sv.iJogadores >= sv.iMaximo;
		int nx = xNome;
		if ( sv.bSenha )
		{
			Text( m_hSmall, nx, tyRow + S( 2 ), acc, "SENHA" );
			nx += TextWide( m_hSmall, "SENHA" ) + S( 10 );
		}
		Text( m_hBody, nx, tyRow, bCheio ? COR_TEXTO3 : COR_TEXTO, sv.szNome );
		Text( m_hBody, xMapa, tyRow, COR_TEXTO2, sv.szMapa );
		char sz[ 48 ];
		if ( sv.iBots > 0 )
			V_snprintf( sz, sizeof( sz ), "%d/%d +%d bots", sv.iJogadores, sv.iMaximo, sv.iBots );
		else
			V_snprintf( sz, sizeof( sz ), "%d/%d", sv.iJogadores, sv.iMaximo );
		Text( m_hBody, xJog - TextWide( m_hBody, sz ), tyRow, bCheio ? Color( 255, 90, 90, 255 ) : COR_TEXTO2, sz );
		V_snprintf( sz, sizeof( sz ), "%d", sv.iPing );
		Text( m_hBody, xPing - TextWide( m_hBody, sz ), tyRow, sv.iPing > 150 ? COR_TEXTO3 : COR_TEXTO2, sz );
		if ( Clicked( lx + 1, ry, lw - 2, iRow ) )
		{
			const bool bDuplo = ( i == m_iServerSel ) && ( Plat_FloatTime() - m_flServerClickTime < 0.4 );
			m_iServerSel = i;
			m_flServerClickTime = (float)Plat_FloatTime();
			if ( bDuplo )
			{
				ConnectTo( sv.szEndereco );
				return;
			}
		}
	}
	if ( !n )
	{
		const char *pszVazio = bInternet ? "Nenhum servidor do Rearmed encontrado na internet." :
			"Nenhuma sala na rede. Numa VPN (Radmin, ZeroTier, Hamachi) a sala só aparece se a VPN repassar a busca; senão, use CONECTAR POR IP.";
		Text( m_hBody, xNome, ry0 + S( 14 ), COR_TEXTO3, TrokServidores::Atualizando() ? "Procurando..." : pszVazio );
	}
	if ( n > nCabem )
	{
		const int bx = lx + lw - S( 6 ), bh = lb - ry0 - S( 6 );
		Rect( bx, ry0, S( 3 ), bh, COR_LINHA );
		const int th = MAX( S( 20 ), bh * nCabem / n );
		Rect( bx, ry0 + ( bh - th ) * m_iServerScroll / MAX( 1, n - nCabem ), S( 3 ), th, COR_LINHA_FORTE );
	}

	// Conectar por IP e entrar
	const int by = lb + S( 18 );
	Label( x0, by, "CONECTAR POR IP (IP:PORTA)" );
	TextField( 2, x0, by + S( 18 ), S( 300 ), trok_ui_servidor_ip.GetString(), false );
	if ( BoxButton( x0 + S( 308 ), by + S( 18 ), S( 120 ), S( 34 ), m_hButton, "CONECTAR", false, false ) )
	{
		if ( m_iTextField >= 0 )
			CommitTextField();
		if ( trok_ui_servidor_ip.GetString()[ 0 ] )
		{
			ConnectTo( trok_ui_servidor_ip.GetString() );
			return;
		}
	}
	Label( x0 + S( 452 ), by, "SENHA DO SERVIDOR" );
	TextField( 3, x0 + S( 452 ), by + S( 18 ), S( 220 ), trok_ui_servidor_senha.GetString(), true );

	const int gw = S( 180 ), gh = S( 52 );
	const bool bTemSel = ( m_iServerSel >= 0 && m_iServerSel < n );
	if ( BoxButton( xr - gw, by + S( 6 ), gw, gh, m_hHeading, "ENTRAR", bTemSel, bTemSel ) && bTemSel )
	{
		if ( m_iTextField >= 0 )
			CommitTextField();
		ConnectTo( TrokServidores::Servidor( m_iServerSel ).szEndereco );
	}
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
	RememberMap();
	BuildModes();
	const char *pszModo = m_Modos.Count() ? m_Modos[ CurrentMode() ].szModo : "equipe";

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
		pszModo, iNivel, szEstilos, iPorTime * 2, MIN( 32, iPorTime * 2 + 2 ), pszMap );
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
