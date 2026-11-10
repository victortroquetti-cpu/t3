//=============================================================================
// Trok: menu da LOJA do modo Hordas (design/modo-hordas.md, "Loja").
//
// Não abre com tecla: o jogador anda até uma func_trok_loja e aperta E (o servidor muda um
// contador em rede, trok_hordas_estado, e o HUD do modo chama TrokHordasLoja_Pensar, que abre
// este painel). Fecha sozinho se o jogador se afastar da loja, cair ou morrer; ESC ou E fecham.
// Categorias à esquerda (1..9), itens no meio (clique compra: "trok_hordas_comprar <id>"; o
// servidor confere distância, pontos e limites), detalhe à direita. Catálogo em
// scripts/trok_hordas_loja.txt (shared/trok/trok_hordas.cpp). Mesma base visual do menu de
// compra da economia (ui/trok_loja_menu.cpp, CTrokPainel), mas com os pontos do modo.
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "ienginevgui.h"
#include "ammodef.h"
#include "trok/trok_hordas.h"
#include "trok/trok_surto.h"
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace TrokUI;


class CTrokHordasLojaPainel : public CTrokPainel
{
	DECLARE_CLASS_SIMPLE( CTrokHordasLojaPainel, CTrokPainel );
public:
	CTrokHordasLojaPainel();

	void Mostrar( bool bMostrar );
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnTick( void );

protected:
	virtual void Desenhar( void );

private:
	// Estado do item para o jogador local: preço a pagar agora, texto da direita e se dá para comprar.
	void Situacao( const TrokHordasItem_t &it, int *piPreco, char *pszDir, int iTam, bool *pbPode ) const;

	int m_iCategoria;
	int m_iItemSobre;
};

static CTrokHordasLojaPainel *s_pLojaHordas = NULL;
static int s_iUltimoPedido = -1;

CTrokHordasLojaPainel::CTrokHordasLojaPainel() : BaseClass( "TrokHordasLoja" )
{
	SetParent( enginevgui->GetPanel( PANEL_CLIENTDLL ) );
	MakePopup();
	SetVisible( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	ivgui()->AddTickSignal( GetVPanel(), 150 );
	m_iCategoria = 0;
	m_iItemSobre = -1;
}

void CTrokHordasLojaPainel::Mostrar( bool bMostrar )
{
	if ( IsVisible() == bMostrar )
		return;
	SetVisible( bMostrar );
	SetMouseInputEnabled( bMostrar );
	SetKeyBoardInputEnabled( bMostrar );
	if ( bMostrar )
	{
		MoveToFront();
		RequestFocus();
	}
}

static bool PodeUsarLoja( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	return engine->IsInGame() && pLocal && pLocal->IsAlive() && pLocal->GetTeamNumber() == TEAM_TROK_HUMANOS &&
		!TrokHordas_Caido( pLocal ) && TrokHordas_LojaPerto( pLocal ) != NULL;
}

void CTrokHordasLojaPainel::OnTick( void )
{
	if ( IsVisible() && !PodeUsarLoja() )
		Mostrar( false );
}

void CTrokHordasLojaPainel::OnKeyCodePressed( KeyCode code )
{
	if ( code == KEY_ESCAPE || code == KEY_E )
	{
		Mostrar( false );
		return;
	}
	int iNum = -1;
	if ( code >= KEY_1 && code <= KEY_9 )
		iNum = code - KEY_1;
	else if ( code >= KEY_PAD_1 && code <= KEY_PAD_9 )
		iNum = code - KEY_PAD_1;
	if ( iNum >= 0 && iNum < TrokHordasLoja().categorias.Count() )
	{
		m_iCategoria = iNum;
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

void CTrokHordasLojaPainel::Situacao( const TrokHordasItem_t &it, int *piPreco, char *pszDir, int iTam, bool *pbPode ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	*piPreco = it.iPreco;
	*pbPode = true;
	if ( !pLocal || !pEstado )
	{
		*pbPode = false;
		V_strncpy( pszDir, "", iTam );
		return;
	}
	if ( it.bEnergia && !pEstado->IsEnergia() )
	{
		*pbPode = false;
		V_strncpy( pszDir, "SEM ENERGIA", iTam );
		return;
	}
	switch ( it.iTipo )
	{
	case HITEM_ARMA:
	case HITEM_DEFESA:
	{
		C_BaseCombatWeapon *pTem = pLocal->Weapon_OwnsThisType( it.szEntidade );
		if ( pTem )
		{
			const int iTipo = pTem->GetPrimaryAmmoType();
			if ( !pTem->UsesClipsForAmmo1() && iTipo >= 0 && pLocal->GetAmmoCount( iTipo ) >= GetAmmoDef()->MaxCarry( iTipo ) )
			{
				*pbPode = false;
				V_strncpy( pszDir, "CHEIO", iTam );
				return;
			}
			*piPreco = ( it.iTipo == HITEM_ARMA && TrokHordas_NivelArma( pTem ) > 0 ) ? TrokUpgrade().iMunicaoPreco : it.iPrecoRepor;
			V_snprintf( pszDir, iTam, "REPOR  %d", *piPreco );
			break;
		}
		V_snprintf( pszDir, iTam, "%d", *piPreco );
		break;
	}
	case HITEM_COLETE:		// a armadura não vem em rede para o cliente: o servidor avisa se está cheio
	case HITEM_KIT:
		if ( pEstado->TemKit( pLocal->entindex() ) )
		{
			*pbPode = false;
			V_strncpy( pszDir, "EQUIPADO", iTam );
			return;
		}
		V_snprintf( pszDir, iTam, "%d", *piPreco );
		break;
	default:
		V_snprintf( pszDir, iTam, "%d", *piPreco );
		break;
	}
	if ( pEstado->GetPontos( pLocal->entindex() ) < *piPreco )
		*pbPode = false;
}

void CTrokHordasLojaPainel::Desenhar( void )
{
	const TrokHordasLoja_t &loja = TrokHordasLoja();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	const int iPontos = ( pLocal && pEstado ) ? pEstado->GetPontos( pLocal->entindex() ) : 0;
	const Color acc = Destaque();

	Ret( 0, 0, m_iLargura, m_iAltura, Color( COR_FUNDO.r(), COR_FUNDO.g(), COR_FUNDO.b(), 200 ) );

	const int x0 = S( 64 ), y0 = S( 56 );
	const int wTotal = m_iLargura - 2 * x0;

	Texto( m_hTitulo, x0, y0, COR_TEXTO, "LOJA" );
	char sz[ 96 ];
	V_snprintf( sz, sizeof( sz ), "%d PONTOS", iPontos );
	TextoDir( m_hTitulo, x0 + wTotal, y0, acc, sz );
	const int ySub = y0 + Altura( m_hTitulo ) + S( 2 );
	if ( pLocal && pEstado )
	{
		const int iDef = pEstado->GetNivelDefesa( pLocal->entindex() );
		V_snprintf( sz, sizeof( sz ), "DEFESAS %s", iDef > 0 ? TrokUpgrade().defesa[ iDef ].szNome : "SEM MELHORIA" );
		Texto( m_hPequena, x0, ySub, COR_TEXTO3, sz );
	}

	const int yCorpo = ySub + Altura( m_hPequena ) + S( 18 );
	Ret( x0, yCorpo - S( 9 ), wTotal, 1, COR_LINHA );

	if ( loja.categorias.Count() == 0 )
	{
		Texto( m_hCorpo, x0, yCorpo, COR_TEXTO2, "Loja vazia: falta scripts/trok_hordas_loja.txt." );
		return;
	}

	// Categorias
	const int wCat = S( 220 ), hCat = S( 40 );
	m_iCategoria = clamp( m_iCategoria, 0, loja.categorias.Count() - 1 );
	for ( int c = 0; c < loja.categorias.Count(); ++c )
	{
		char szTecla[ 8 ];
		V_snprintf( szTecla, sizeof( szTecla ), "%d", c + 1 );
		if ( Botao( x0, yCorpo + c * ( hCat + S( 6 ) ), wCat, hCat, m_hBotao, loja.categorias[ c ].szNome, c == m_iCategoria, szTecla ) )
			m_iCategoria = c;
	}

	// Itens
	const char *pszCat = loja.categorias[ m_iCategoria ].szId;
	const int xItens = x0 + wCat + S( 24 );
	const int wItens = S( 440 ), hItem = S( 46 );
	int y = yCorpo;
	int iSobre = -1;
	FOR_EACH_VEC( loja.itens, k )
	{
		const TrokHordasItem_t &it = loja.itens[ k ];
		if ( V_stricmp( it.szCategoria, pszCat ) )
			continue;
		int iPreco = 0;
		bool bPode = true;
		char szDir[ 48 ];
		Situacao( it, &iPreco, szDir, sizeof( szDir ), &bPode );
		const bool bSobre = Sobre( xItens, y, wItens, hItem );
		if ( bSobre )
			iSobre = k;
		Ret( xItens, y, wItens, hItem, bSobre ? COR_PAINEL_HOVER : COR_PAINEL );
		Contorno( xItens, y, wItens, hItem, bSobre ? COR_LINHA_FORTE : COR_LINHA );
		Texto( m_hItem, xItens + S( 14 ), y + ( hItem - Altura( m_hItem ) ) / 2, bPode ? COR_TEXTO : COR_TEXTO3, it.szNome );
		TextoDir( m_hItem, xItens + wItens - S( 14 ), y + ( hItem - Altura( m_hItem ) ) / 2, bPode ? COR_TEXTO2 : COR_INIMIGO, szDir );
		if ( Clicou( xItens, y, wItens, hItem ) && bPode )
		{
			char szCmd[ 64 ];
			V_snprintf( szCmd, sizeof( szCmd ), "trok_hordas_comprar %s\n", it.szId );
			engine->ClientCmd( szCmd );
		}
		y += hItem + S( 6 );
	}
	if ( Pintando() )
		m_iItemSobre = iSobre;

	// Detalhe
	if ( Pintando() && loja.itens.IsValidIndex( m_iItemSobre ) )
	{
		const TrokHordasItem_t &it = loja.itens[ m_iItemSobre ];
		const int xDet = xItens + wItens + S( 24 );
		Texto( m_hItem, xDet, yCorpo, COR_TEXTO, it.szNome );
		Texto( m_hPequena, xDet, yCorpo + Altura( m_hItem ) + S( 6 ), COR_TEXTO2, it.szDescricao );
		if ( it.iTipo == HITEM_ARMA || it.iTipo == HITEM_DEFESA )
		{
			V_snprintf( sz, sizeof( sz ), "COMPRAR %d  ·  REPOR %d", it.iPreco, it.iPrecoRepor );
			Texto( m_hPequena, xDet, yCorpo + Altura( m_hItem ) + Altura( m_hPequena ) + S( 12 ), COR_TEXTO3, sz );
		}
	}

	int xDica = x0;
	const int yDica = m_iAltura - S( 48 );
	Dica( xDica, yDica, "1-9", "CATEGORIA" );
	Dica( xDica, yDica, "CLIQUE", "COMPRAR" );
	Dica( xDica, yDica, "E / ESC", "FECHAR" );
	if ( Botao( x0 + wTotal - S( 140 ), m_iAltura - S( 60 ), S( 140 ), S( 36 ), m_hBotao, "FECHAR", false ) )
		Mostrar( false );
}

//-----------------------------------------------------------------------------
// Chamado a cada quadro pelo HUD do modo (hud_trok_hordas.cpp): o E na loja abre o menu.
//-----------------------------------------------------------------------------
void TrokHordasLoja_Pensar( void )
{
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pEstado || !pLocal )
	{
		s_iUltimoPedido = -1;
		return;
	}
	const int iPedido = pEstado->GetLojaPedidos( pLocal->entindex() );
	if ( s_iUltimoPedido < 0 )
	{
		s_iUltimoPedido = iPedido;		// primeira leitura: não abre
		return;
	}
	if ( iPedido == s_iUltimoPedido )
		return;
	s_iUltimoPedido = iPedido;
	if ( !PodeUsarLoja() )
		return;
	if ( !s_pLojaHordas )
		s_pLojaHordas = new CTrokHordasLojaPainel();
	s_pLojaHordas->Mostrar( true );
}

CON_COMMAND( trok_hordas_loja_lista, "Hordas: lista no console os itens da loja (id para trok_hordas_comprar, preço)." )
{
	const TrokHordasLoja_t &loja = TrokHordasLoja();
	FOR_EACH_VEC( loja.itens, k )
		Msg( "  %-22s %6d (repor %d)  %s\n", loja.itens[ k ].szId, loja.itens[ k ].iPreco, loja.itens[ k ].iPrecoRepor, loja.itens[ k ].szNome );
	Msg( "Comprar: perto de uma loja, E; ou trok_hordas_comprar <id>.\n" );
}
