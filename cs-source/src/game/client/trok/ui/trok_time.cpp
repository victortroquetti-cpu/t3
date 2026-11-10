//=============================================================================
// Trok: seleção de time (PANEL_TEAM). Só no mata-mata em equipe: Mercenários contra
// Remanescentes, mais "automático" e "assistir" (espectador). No Exército solo e no
// Surto não aparece (o Surto decide o time pela rodada).
//
// Abre sozinha ao entrar no servidor (depois da tela de boas-vindas, trok_motd.cpp) e
// pela tecla de trok_menu_time (padrão M). Manda "jointeam N" ao servidor, que já
// trata o comando (CHL2MP_Player::HandleCommand_JoinTeam). "Automático" é decidido
// aqui: o time com menos gente.
//
// Registrada no CHudViewport::CreatePanelByName (patch em clientmode_hl2mpnormal.cpp).
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "hl2mp_gamerules.h"
#include "IGameUIFuncs.h"
#include "trok/trok_surto.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace TrokUI;

extern IGameUIFuncs *gameuifuncs;

static ConVar trok_ui_time_ao_entrar( "trok_ui_time_ao_entrar", "1", FCVAR_ARCHIVE, "1 = abre a seleção de time ao entrar num servidor de mata-mata em equipe." );

static float s_flAbrirTimeEm = 0.0f;	// pedido pendente (Plat_FloatTime); 0 = nenhum

void TrokTime_PedirAbrir( void )
{
	if ( trok_ui_time_ao_entrar.GetBool() )
		s_flAbrirTimeEm = Plat_FloatTime() + 1.0f; // espera o gamerules e o estado do Surto chegarem
}

class CTrokTimeMenu : public CTrokPainel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CTrokTimeMenu, CTrokPainel );
public:
	CTrokTimeMenu( IViewPort *pViewPort );

	virtual const char *GetName( void ) { return PANEL_TEAM; }
	virtual void SetData( KeyValues *data ) {}
	virtual void Reset( void ) {}
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_MENUCONTROLS; }
	virtual VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnTick( void );

protected:
	virtual void Desenhar( void );

private:
	void Escolher( int iTeam );
	int TimeAutomatico( void );
	int Contar( int iTeam );
	int CartaoTime( int x, int y, int w, int h, int iTeam, const char *pszTecla, const char *pszLinha1, const char *pszLinha2 );
};

CTrokTimeMenu::CTrokTimeMenu( IViewPort *pViewPort ) : BaseClass( PANEL_TEAM )
{
	NOTE_UNUSED( pViewPort );
	MakePopup();
	SetVisible( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	ivgui()->AddTickSignal( GetVPanel(), 100 );
}

void CTrokTimeMenu::ShowPanel( bool bShow )
{
	if ( bShow && !ModoComTimes() )
	{
		bShow = false;
		if ( engine->IsInGame() )
			Msg( "Este modo não tem escolha de time.\n" );
	}
	if ( BaseClass::IsVisible() == bShow )
		return;
	if ( bShow )
		s_flAbrirTimeEm = 0.0f;
	SetVisible( bShow );
	SetMouseInputEnabled( bShow );
	SetKeyBoardInputEnabled( bShow );
	if ( bShow )
	{
		MoveToFront();
		RequestFocus();
	}
}

void CTrokTimeMenu::OnTick( void )
{
	if ( !engine->IsInGame() )
	{
		if ( BaseClass::IsVisible() )
			gViewPortInterface->ShowPanel( this, false );
		return;
	}
	// Pedido de abrir ao entrar: espera saber o modo.
	if ( s_flAbrirTimeEm > 0.0f && Plat_FloatTime() >= s_flAbrirTimeEm && C_BasePlayer::GetLocalPlayer() && HL2MPRules() && g_PR )
	{
		s_flAbrirTimeEm = 0.0f;
		if ( ModoComTimes() && !BaseClass::IsVisible() )
			gViewPortInterface->ShowPanel( this, true );
	}
	// O modo mudou (ou era Surto): fecha.
	if ( BaseClass::IsVisible() && !ModoComTimes() )
		gViewPortInterface->ShowPanel( this, false );
}

void CTrokTimeMenu::OnKeyCodePressed( KeyCode code )
{
	const ButtonCode_t teclaMenu = gameuifuncs ? gameuifuncs->GetButtonCodeForBind( "trok_menu_time" ) : BUTTON_CODE_INVALID;
	switch ( code )
	{
	case KEY_1: case KEY_PAD_1: Escolher( TEAM_COMBINE ); return;
	case KEY_2: case KEY_PAD_2: Escolher( TEAM_REBELS ); return;
	case KEY_3: case KEY_PAD_3: Escolher( TimeAutomatico() ); return;
	case KEY_4: case KEY_PAD_4: Escolher( TEAM_SPECTATOR ); return;
	case KEY_ESCAPE:
		gViewPortInterface->ShowPanel( this, false );
		return;
	default:
		break;
	}
	if ( code == teclaMenu )
	{
		gViewPortInterface->ShowPanel( this, false );
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

int CTrokTimeMenu::Contar( int iTeam )
{
	if ( !g_PR )
		return 0;
	int n = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( g_PR->IsConnected( i ) && g_PR->GetTeam( i ) == iTeam )
			++n;
	}
	return n;
}

// Time com menos gente (sem contar você). Empate: fica onde está, ou sorteia.
int CTrokTimeMenu::TimeAutomatico( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const int iMeu = pLocal ? pLocal->GetTeamNumber() : TEAM_UNASSIGNED;
	int nA = Contar( TEAM_COMBINE ) - ( iMeu == TEAM_COMBINE ? 1 : 0 );
	int nB = Contar( TEAM_REBELS ) - ( iMeu == TEAM_REBELS ? 1 : 0 );
	if ( nA < nB )
		return TEAM_COMBINE;
	if ( nB < nA )
		return TEAM_REBELS;
	if ( iMeu == TEAM_COMBINE || iMeu == TEAM_REBELS )
		return iMeu;
	return random->RandomInt( 0, 1 ) ? TEAM_COMBINE : TEAM_REBELS;
}

void CTrokTimeMenu::Escolher( int iTeam )
{
	gViewPortInterface->ShowPanel( this, false );
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal && pLocal->GetTeamNumber() == iTeam )
		return; // já está nele: só fecha
	char szCmd[ 32 ];
	V_snprintf( szCmd, sizeof( szCmd ), "jointeam %d\n", iTeam );
	engine->ClientCmd( szCmd );
}

// Um cartão de time: nome, descrição, quantos e quem está nele. Devolve o time se clicou (0 se não).
int CTrokTimeMenu::CartaoTime( int x, int y, int w, int h, int iTeam, const char *pszTecla, const char *pszLinha1, const char *pszLinha2 )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const bool bMeu = pLocal && pLocal->GetTeamNumber() == iTeam;
	const bool bSobre = Sobre( x, y, w, h );
	const Color acc = Destaque();

	Ret( x, y, w, h, bSobre ? COR_PAINEL_HOVER : COR_PAINEL );
	Contorno( x, y, w, h, bSobre ? COR_LINHA_FORTE : COR_LINHA );
	Ret( x, y, w, S( 3 ), ( bSobre || bMeu ) ? acc : COR_LINHA_FORTE );

	const int ix = x + S( 24 );
	int iy = y + S( 24 );
	// tecla
	{
		const int kw = Largura( m_hPequena, pszTecla ) + S( 12 );
		Contorno( ix, iy, kw, Altura( m_hPequena ) + S( 4 ), bSobre ? acc : COR_LINHA_FORTE );
		Texto( m_hPequena, ix + S( 6 ), iy + S( 2 ), COR_TEXTO, pszTecla );
		if ( bMeu )
			Texto( m_hPequena, ix + kw + S( 10 ), iy + S( 2 ), acc, "SEU TIME AGORA" );
	}
	iy += S( 26 );
	TextoEspacado( m_hTitulo, ix, iy, COR_TEXTO, NomeDoTime( iTeam ), S( 3 ) );
	iy += Altura( m_hTitulo ) + S( 6 );
	Texto( m_hCorpo, ix, iy, COR_TEXTO2, pszLinha1 );
	iy += Altura( m_hCorpo ) + S( 2 );
	Texto( m_hCorpo, ix, iy, COR_TEXTO2, pszLinha2 );
	iy += Altura( m_hCorpo ) + S( 18 );
	Ret( ix, iy, w - S( 48 ), 1, COR_LINHA );
	iy += S( 12 );

	char sz[ 64 ];
	const int n = Contar( iTeam );
	V_snprintf( sz, sizeof( sz ), n == 1 ? "%d JOGADOR" : "%d JOGADORES", n );
	Texto( m_hPequena, ix, iy, COR_TEXTO3, sz );
	iy += Altura( m_hPequena ) + S( 8 );

	// Nomes (até caber no cartão)
	if ( g_PR )
	{
		const int iLinha = Altura( m_hCorpo ) + S( 4 );
		const int iFim = y + h - S( 16 ) - iLinha;
		int iMostrados = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; ++i )
		{
			if ( !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) != iTeam )
				continue;
			if ( iy > iFim )
			{
				V_snprintf( sz, sizeof( sz ), "+ %d", n - iMostrados );
				Texto( m_hCorpo, ix, iy, COR_TEXTO3, sz );
				break;
			}
			const char *pszNome = g_PR->GetPlayerName( i );
			Texto( m_hCorpo, ix, iy, g_PR->IsLocalPlayer( i ) ? acc : COR_TEXTO, pszNome );
			if ( g_PR->IsFakePlayer( i ) )
				Texto( m_hPequena, ix + Largura( m_hCorpo, pszNome ) + S( 8 ), iy + S( 2 ), COR_TEXTO3, "BOT" );
			iy += iLinha;
			++iMostrados;
		}
	}
	return Clicou( x, y, w, h ) ? iTeam : 0;
}

void CTrokTimeMenu::Desenhar( void )
{
	const int sw = m_iLargura, sh = m_iAltura;
	const Color acc = Destaque();
	Ret( 0, 0, sw, sh, Color( 11, 13, 16, 200 ) );

	// Cabeçalho
	char szMapa[ 128 ] = "";
	if ( engine->GetLevelName() )
	{
		V_FileBase( engine->GetLevelName(), szMapa, sizeof( szMapa ) );
		V_strupr( szMapa );
	}
	char szTopo[ 192 ];
	V_snprintf( szTopo, sizeof( szTopo ), "MATA-MATA EM EQUIPE  ·  %s", szMapa );
	TextoCentro( m_hPequena, sw / 2, S( 72 ), acc, szTopo );
	{
		const char *pszTitulo = "ESCOLHA SEU LADO";
		const int iEsp = S( 4 );
		const int tw = Largura( m_hTitulo, pszTitulo ) + iEsp * ( (int)V_strlen( pszTitulo ) - 1 );
		TextoEspacado( m_hTitulo, ( sw - tw ) / 2, S( 90 ), COR_TEXTO, pszTitulo, iEsp );
	}

	// Cartões
	const int cw = S( 380 ), ch = S( 340 ), gap = S( 24 );
	const int cx = ( sw - cw * 2 - gap ) / 2, cy = S( 160 );
	int iEscolha = 0;
	int r = CartaoTime( cx, cy, cw, ch, TEAM_COMBINE, "1", "A serviço da corporação, por contrato.", "Equipamento caro e padronizado." );
	if ( r ) iEscolha = r;
	r = CartaoTime( cx + cw + gap, cy, cw, ch, TEAM_REBELS, "2", "Sobreviventes táticos, bem treinados.", "Cada um com o próprio kit." );
	if ( r ) iEscolha = r;

	// Automático e assistir
	const int by = cy + ch + S( 20 ), bh = S( 44 );
	const int bw = ( cw * 2 + gap - S( 12 ) ) / 2;
	if ( Botao( cx, by, bw, bh, m_hCabecalho, "AUTOMÁTICO", true, "3" ) )
		iEscolha = TimeAutomatico();
	if ( Botao( cx + bw + S( 12 ), by, bw, bh, m_hCabecalho, "ASSISTIR", false, "4" ) )
		iEscolha = TEAM_SPECTATOR;

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( pLocal && pLocal->IsAlive() && ( pLocal->GetTeamNumber() == TEAM_COMBINE || pLocal->GetTeamNumber() == TEAM_REBELS ) )
		TextoCentro( m_hPequena, sw / 2, by + bh + S( 14 ), COR_TEXTO3, "Trocar de time com vida conta como uma morte." );

	// Rodapé
	{
		const int h = S( 48 ), y = sh - h;
		Ret( 0, y, sw, h, Color( 11, 13, 16, 178 ) );
		Ret( 0, y, sw, 1, Color( 255, 255, 255, 15 ) );
		const int ty = y + ( h - Altura( m_hPequena ) ) / 2;
		int x = sw - S( 56 ) - S( 330 );
		Dica( x, ty, "1-4", "Escolher" );
		Dica( x, ty, "ESC", "Fechar" );
	}

	if ( iEscolha )
		Escolher( iEscolha );
}

IViewPortPanel *TrokTimeMenu_Create( IViewPort *pViewPort )
{
	return new CTrokTimeMenu( pViewPort );
}

//-----------------------------------------------------------------------------
// Tecla do menu de time (padrão M, cfg/config_default.cfg).
//-----------------------------------------------------------------------------
CON_COMMAND_F( trok_menu_time, "Abre ou fecha a seleção de time (só no mata-mata em equipe).", FCVAR_CLIENTCMD_CAN_EXECUTE )
{
	if ( !gViewPortInterface || !engine->IsInGame() )
		return;
	IViewPortPanel *pPanel = gViewPortInterface->FindPanelByName( PANEL_TEAM );
	if ( !pPanel )
		return;
	if ( !ModoComTimes() )
	{
		Msg( "Este modo não tem escolha de time.\n" );
		return;
	}
	gViewPortInterface->ShowPanel( pPanel, !pPanel->IsVisible() );
}
