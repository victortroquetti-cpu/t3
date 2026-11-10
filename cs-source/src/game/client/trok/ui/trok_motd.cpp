//=============================================================================
// Trok: tela de boas-vindas do servidor (PANEL_INFO, o "MOTD"), no visual do mod.
// Substitui o CHL2MPTextWindow no CHudViewport::CreatePanelByName (patch em
// clientmode_hl2mpnormal.cpp).
//
// O servidor manda o painel ao entrar (hl2mp_client.cpp, FinishClientPutInServer) com o
// texto do motd.txt (tabela de strings "InfoPanel"). Aqui é só texto: HTML vira texto
// simples e link vira um botão que abre no navegador do overlay da Steam.
// Partida aberta por este PC (contra bots) ou texto vazio: não mostra.
// Ao fechar, abre a seleção de time (trok_time.cpp) quando o modo tem times.
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "networkstringtabledefs.h"
#include "filesystem.h"
#include "steam/steam_api.h"
#include "shareddefs.h"
#include <game/client/iviewport.h>
#include <vgui/ISurface.h>
#include <vgui/IInput.h>
#include "tier1/utlstring.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace TrokUI;

extern INetworkStringTable *g_pStringTableInfoPanel;

static ConVar trok_ui_motd_local( "trok_ui_motd_local", "0", FCVAR_ARCHIVE, "1 = mostra a tela de boas-vindas também na partida aberta por este PC." );

class CTrokMotd : public CTrokPainel, public IViewPortPanel
{
	DECLARE_CLASS_SIMPLE( CTrokMotd, CTrokPainel );
public:
	CTrokMotd( IViewPort *pViewPort );

	virtual const char *GetName( void ) { return PANEL_INFO; }
	virtual void SetData( KeyValues *data );
	virtual void Reset( void ) { m_szTitulo[ 0 ] = 0; m_szMsg[ 0 ] = 0; m_iTipo = TYPE_TEXT; }
	virtual void Update( void ) {}
	virtual bool NeedsUpdate( void ) { return false; }
	virtual bool HasInputElements( void ) { return true; }
	virtual void ShowPanel( bool bShow );
	virtual GameActionSet_t GetPreferredActionSet() { return GAME_ACTION_SET_MENUCONTROLS; }
	virtual VPANEL GetVPanel( void ) { return BaseClass::GetVPanel(); }
	virtual bool IsVisible() { return BaseClass::IsVisible(); }
	virtual void SetParent( VPANEL parent ) { BaseClass::SetParent( parent ); }

	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnMouseWheeled( int delta );

protected:
	virtual void Desenhar( void );

private:
	bool CarregarTexto( void );
	void LimparHTML( const char *pszIn );
	void Quebrar( int iLargura );
	void Fechar( void );

	char m_szTitulo[ 256 ];
	char m_szMsg[ 2048 ];		// texto, nome da entrada da tabela, URL ou arquivo (conforme o tipo)
	int m_iTipo;
	CUtlString m_strTexto;		// texto final, sem HTML
	char m_szLink[ 512 ];		// link para abrir no navegador (vazio = nenhum)
	CUtlVector< CUtlString > m_Linhas;
	int m_iLarguraQuebra;
	int m_iRolagem;
};

CTrokMotd::CTrokMotd( IViewPort *pViewPort ) : BaseClass( PANEL_INFO )
{
	NOTE_UNUSED( pViewPort );
	Reset();
	m_szLink[ 0 ] = 0;
	m_iLarguraQuebra = -1;
	m_iRolagem = 0;
	MakePopup();
	SetVisible( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
}

void CTrokMotd::SetData( KeyValues *data )
{
	V_strncpy( m_szTitulo, data->GetString( "title", "" ), sizeof( m_szTitulo ) );
	V_strncpy( m_szMsg, data->GetString( "msg", "" ), sizeof( m_szMsg ) );
	m_iTipo = data->GetInt( "type", TYPE_TEXT );
}

// Converte HTML em texto: tira as tags, quebra linha em <br>, <p>, <div>, <li>, <h*>.
void CTrokMotd::LimparHTML( const char *pszIn )
{
	CUtlString str;
	const char *p = pszIn;
	bool bScript = false;
	while ( *p )
	{
		if ( *p == '<' )
		{
			const char *pFim = strchr( p, '>' );
			if ( !pFim )
				break;
			char szTag[ 16 ] = "";
			int n = 0;
			const char *q = p + 1;
			if ( *q == '/' )
				++q;
			while ( q < pFim && n < (int)sizeof( szTag ) - 1 && ( isalnum( (unsigned char)*q ) ) )
				szTag[ n++ ] = (char)tolower( (unsigned char)*q++ );
			szTag[ n ] = 0;
			const bool bFecha = ( p[ 1 ] == '/' );
			if ( !V_strcmp( szTag, "script" ) || !V_strcmp( szTag, "style" ) || !V_strcmp( szTag, "head" ) )
				bScript = !bFecha;
			else if ( !V_strcmp( szTag, "br" ) || !V_strcmp( szTag, "p" ) || !V_strcmp( szTag, "div" ) || !V_strcmp( szTag, "li" ) ||
				( szTag[ 0 ] == 'h' && szTag[ 1 ] >= '1' && szTag[ 1 ] <= '6' ) || !V_strcmp( szTag, "tr" ) )
				str += "\n";
			p = pFim + 1;
			continue;
		}
		if ( bScript )
		{
			++p;
			continue;
		}
		if ( *p == '&' )
		{
			static const struct { const char *pszEnt; const char *pszTxt; } s_Ent[] = {
				{ "&amp;", "&" }, { "&lt;", "<" }, { "&gt;", ">" }, { "&nbsp;", " " }, { "&quot;", "\"" }, { "&#39;", "'" },
			};
			bool bAchou = false;
			for ( int i = 0; i < ARRAYSIZE( s_Ent ); ++i )
			{
				const int iLen = V_strlen( s_Ent[ i ].pszEnt );
				if ( !V_strnicmp( p, s_Ent[ i ].pszEnt, iLen ) )
				{
					str += s_Ent[ i ].pszTxt;
					p += iLen;
					bAchou = true;
					break;
				}
			}
			if ( bAchou )
				continue;
		}
		if ( *p == '\r' || *p == '\t' )
		{
			str += " ";
			++p;
			continue;
		}
		char sz[ 2 ] = { *p, 0 };
		str += sz;
		++p;
	}
	m_strTexto = str;
}

// Lê o conteúdo conforme o tipo. Devolve false se não há o que mostrar.
bool CTrokMotd::CarregarTexto( void )
{
	m_strTexto.Clear();
	m_szLink[ 0 ] = 0;
	m_iLarguraQuebra = -1;
	m_iRolagem = 0;

	CUtlString strDados;
	switch ( m_iTipo )
	{
	case TYPE_INDEX:
		if ( g_pStringTableInfoPanel )
		{
			// Prefere a versão só texto (motdfile_text), se o servidor tiver.
			const char *pszEntradas[] = { "motd_text", m_szMsg };
			for ( int e = 0; e < 2 && strDados.IsEmpty(); ++e )
			{
				const int idx = g_pStringTableInfoPanel->FindStringIndex( pszEntradas[ e ] );
				if ( idx == ::INVALID_STRING_INDEX )
					continue;
				int iLen = 0;
				const char *pData = (const char *)g_pStringTableInfoPanel->GetStringUserData( idx, &iLen );
				if ( pData && iLen > 0 && pData[ 0 ] )
				{
					CUtlString s;
					s.SetDirect( pData, iLen );
					strDados = s;
				}
			}
		}
		break;
	case TYPE_URL:
		strDados = m_szMsg;
		break;
	case TYPE_FILE:
		{
			CUtlBuffer buf;
			if ( g_pFullFileSystem->ReadFile( m_szMsg, "GAME", buf ) )
			{
				CUtlString s;
				s.SetDirect( (const char *)buf.Base(), buf.TellPut() );
				strDados = s;
			}
		}
		break;
	default:
		strDados = m_szMsg;
		break;
	}

	const char *pszDados = strDados.Get();
	while ( *pszDados && ( *pszDados == ' ' || *pszDados == '\n' || *pszDados == '\r' || *pszDados == '\t' ) )
		++pszDados;
	if ( !*pszDados )
		return false;

	if ( !V_strncmp( pszDados, "http://", 7 ) || !V_strncmp( pszDados, "https://", 8 ) )
	{
		V_strncpy( m_szLink, pszDados, sizeof( m_szLink ) );
		for ( char *p = m_szLink; *p; ++p )
		{
			if ( *p == '\n' || *p == '\r' || *p == ' ' )
			{
				*p = 0;
				break;
			}
		}
		m_strTexto = "Este servidor tem uma página de boas-vindas na internet.";
		return true;
	}
	if ( pszDados[ 0 ] == '<' )
		LimparHTML( pszDados );
	else
	{
		m_strTexto = pszDados;
		m_strTexto = m_strTexto.Replace( '\r', ' ' );
		m_strTexto = m_strTexto.Replace( '\t', ' ' );
	}
	return !m_strTexto.IsEmpty();
}

// Quebra o texto em linhas que cabem na largura.
void CTrokMotd::Quebrar( int iLargura )
{
	m_Linhas.RemoveAll();
	m_iLarguraQuebra = iLargura;
	const char *p = m_strTexto.Get();
	int iVaziasSeguidas = 0;
	while ( *p )
	{
		const char *pFimPar = strchr( p, '\n' );
		const int iLenPar = pFimPar ? (int)( pFimPar - p ) : V_strlen( p );
		CUtlString strPar;
		strPar.SetDirect( p, iLenPar );
		p += iLenPar + ( pFimPar ? 1 : 0 );

		// linha vazia: no máximo uma seguida
		strPar.Trim();
		if ( strPar.IsEmpty() )
		{
			if ( ++iVaziasSeguidas <= 1 && m_Linhas.Count() )
				m_Linhas.AddToTail( CUtlString() );
			continue;
		}
		iVaziasSeguidas = 0;

		// palavra a palavra
		CUtlString strLinha;
		const char *w = strPar.Get();
		while ( *w )
		{
			while ( *w == ' ' )
				++w;
			if ( !*w )
				break;
			const char *we = w;
			while ( *we && *we != ' ' )
				++we;
			CUtlString strPalavra;
			strPalavra.SetDirect( w, (int)( we - w ) );
			w = we;

			CUtlString strTeste = strLinha.IsEmpty() ? strPalavra : ( strLinha + " " + strPalavra );
			if ( !strLinha.IsEmpty() && Largura( m_hCorpo, strTeste.Get() ) > iLargura )
			{
				m_Linhas.AddToTail( strLinha );
				strLinha = strPalavra;
			}
			else
				strLinha = strTeste;
		}
		if ( !strLinha.IsEmpty() )
			m_Linhas.AddToTail( strLinha );
	}
}

void CTrokMotd::ShowPanel( bool bShow )
{
	if ( bShow )
	{
		const bool bLocal = ServidorLocal() && !trok_ui_motd_local.GetBool();
		if ( bLocal || !CarregarTexto() )
		{
			// Nada a mostrar: vai direto para a escolha de time.
			if ( BaseClass::IsVisible() )
				SetVisible( false );
			TrokTime_PedirAbrir();
			return;
		}
	}
	if ( BaseClass::IsVisible() == bShow )
		return;
	SetVisible( bShow );
	SetMouseInputEnabled( bShow );
	SetKeyBoardInputEnabled( bShow );
	if ( bShow )
	{
		MoveToFront();
		RequestFocus();
	}
	else if ( engine->IsInGame() )
	{
		TrokTime_PedirAbrir();
	}
}

void CTrokMotd::Fechar( void )
{
	gViewPortInterface->ShowPanel( this, false );
}

void CTrokMotd::OnKeyCodePressed( KeyCode code )
{
	switch ( code )
	{
	case KEY_ENTER: case KEY_PAD_ENTER: case KEY_SPACE: case KEY_ESCAPE:
		Fechar();
		return;
	case KEY_UP: m_iRolagem = MAX( 0, m_iRolagem - 1 ); return;
	case KEY_DOWN: ++m_iRolagem; return;
	case KEY_PAGEUP: m_iRolagem = MAX( 0, m_iRolagem - 8 ); return;
	case KEY_PAGEDOWN: m_iRolagem += 8; return;
	default:
		BaseClass::OnKeyCodePressed( code );
	}
}

void CTrokMotd::OnMouseWheeled( int delta )
{
	m_iRolagem = MAX( 0, m_iRolagem - delta * 3 );
}

void CTrokMotd::Desenhar( void )
{
	const int sw = m_iLargura, sh = m_iAltura;
	const Color acc = Destaque();
	Ret( 0, 0, sw, sh, Color( 11, 13, 16, 210 ) );

	const int pw = MIN( sw - S( 120 ), S( 760 ) );
	const int ph = sh - S( 200 );
	const int px = ( sw - pw ) / 2, py = S( 70 );
	Ret( px, py, pw, ph, COR_PAINEL );
	Contorno( px, py, pw, ph, COR_LINHA );
	Ret( px, py, pw, S( 3 ), acc );

	const int ix = px + S( 32 ), iw = pw - S( 64 );
	Texto( m_hPequena, ix, py + S( 26 ), acc, "BEM-VINDO AO SERVIDOR" );
	TextoEspacado( m_hItem, ix, py + S( 44 ), COR_TEXTO, m_szTitulo[ 0 ] ? m_szTitulo : "REARMED", S( 1 ) );
	const int iTopo = py + S( 44 ) + Altura( m_hItem ) + S( 16 );
	Ret( ix, iTopo, iw, 1, COR_LINHA );

	// Texto com rolagem
	if ( Pintando() && m_iLarguraQuebra != iw )
		Quebrar( iw );
	const int iLinha = Altura( m_hCorpo ) + S( 4 );
	const int iBaseBotoes = py + ph - S( 24 ) - S( 44 );
	const int iAreaTopo = iTopo + S( 16 );
	const int iCabem = MAX( 1, ( iBaseBotoes - S( 16 ) - iAreaTopo ) / iLinha );
	m_iRolagem = clamp( m_iRolagem, 0, MAX( 0, m_Linhas.Count() - iCabem ) );
	int y = iAreaTopo;
	for ( int i = m_iRolagem; i < m_Linhas.Count() && i < m_iRolagem + iCabem; ++i )
	{
		Texto( m_hCorpo, ix, y, COR_TEXTO2, m_Linhas[ i ].Get() );
		y += iLinha;
	}
	if ( m_Linhas.Count() > iCabem )
	{
		// barra de rolagem fina
		const int bx = px + pw - S( 14 ), bh = iBaseBotoes - S( 16 ) - iAreaTopo;
		Ret( bx, iAreaTopo, S( 3 ), bh, COR_LINHA );
		const int th = MAX( S( 20 ), bh * iCabem / m_Linhas.Count() );
		const int ty = iAreaTopo + ( bh - th ) * m_iRolagem / MAX( 1, m_Linhas.Count() - iCabem );
		Ret( bx, ty, S( 3 ), th, COR_LINHA_FORTE );
	}

	// Botões
	const int bw = S( 200 ), bh = S( 44 );
	if ( Botao( px + pw - S( 32 ) - bw, iBaseBotoes, bw, bh, m_hCabecalho, "CONTINUAR", true ) )
	{
		Fechar();
		return;
	}
	if ( m_szLink[ 0 ] )
	{
		Texto( m_hPequena, ix, iBaseBotoes - S( 20 ), COR_TEXTO3, m_szLink );
		if ( Botao( px + pw - S( 32 ) - bw * 2 - S( 12 ), iBaseBotoes, bw, bh, m_hCabecalho, "ABRIR PÁGINA", false ) )
		{
			if ( steamapicontext && steamapicontext->SteamFriends() )
				steamapicontext->SteamFriends()->ActivateGameOverlayToWebPage( m_szLink );
		}
	}

	// Rodapé
	{
		const int h = S( 48 ), fy = sh - h;
		Ret( 0, fy, sw, h, Color( 11, 13, 16, 178 ) );
		Ret( 0, fy, sw, 1, Color( 255, 255, 255, 15 ) );
		const int ty = fy + ( h - Altura( m_hPequena ) ) / 2;
		int x = sw - S( 56 ) - S( 300 );
		Dica( x, ty, "ENTER", "Continuar" );
		if ( m_Linhas.Count() > iCabem )
			Dica( x, ty, "RODA", "Rolar" );
	}
}

IViewPortPanel *TrokMotd_Create( IViewPort *pViewPort )
{
	return new CTrokMotd( pViewPort );
}
