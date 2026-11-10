//=============================================================================
// Trok: menu de compra (economia estilo CS; design/economia.md).
//
// Abre e fecha com "trok_loja" (tecla padrão B, cfg/config_default.cfg). Só com a economia
// ligada (trok_economia 1) e dentro do tempo de compra. Categorias à esquerda (teclas 1..9),
// itens no meio (clique compra: manda "trok_comprar <id>" ao servidor, que confere dinheiro,
// tempo e zona), detalhes do item à direita (armadura, fôlego, velocidade, capacete).
// Itens, preços e textos vêm de scripts/trok_loja.txt (shared/trok/trok_loja.cpp).
// Painel solto (não passa pelo viewport): criado na primeira vez que o comando roda.
// Visual: base comum dos painéis do Trok (trok_ui_comum.h).
//=============================================================================

#include "cbase.h"
#include "trok_ui_comum.h"
#include "ienginevgui.h"
#include "IGameUIFuncs.h"
#include "trok/trok_loja.h"
#include "trok/trok_placar_extra.h"
#include "trok/trok_surto.h"
#include "trok/trok_zumbis.h"
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <vgui/IInput.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;
using namespace TrokUI;

extern IGameUIFuncs *gameuifuncs;

static void FormatarDinheiro( int iValor, char *pszOut, int iTam )
{
	// $ 16.000
	char szNum[ 32 ];
	V_snprintf( szNum, sizeof( szNum ), "%d", abs( iValor ) );
	char szPontos[ 48 ];
	int n = V_strlen( szNum ), k = 0;
	for ( int i = 0; i < n; ++i )
	{
		szPontos[ k++ ] = szNum[ i ];
		if ( ( n - i - 1 ) % 3 == 0 && i < n - 1 )
			szPontos[ k++ ] = '.';
	}
	szPontos[ k ] = 0;
	V_snprintf( pszOut, iTam, "%s$ %s", iValor < 0 ? "-" : "", szPontos );
}

class CTrokLojaPainel : public CTrokPainel
{
	DECLARE_CLASS_SIMPLE( CTrokLojaPainel, CTrokPainel );
public:
	CTrokLojaPainel();

	void Mostrar( bool bMostrar );
	void IrParaInfectado( void ) { if ( AbaInfectado() >= 0 ) m_iCategoria = AbaInfectado(); }
	virtual void OnKeyCodePressed( KeyCode code );
	virtual void OnTick( void );

protected:
	virtual void Desenhar( void );

private:
	bool PodeComprarAgora( float *pflFalta ) const;
	int Possui( const TrokLojaItem_t &it ) const;	// 0 não, 1 tem, 2 tem igual ou melhor (colete)
	void DesenharDetalhe( int x, int y, int w, const TrokLojaItem_t &it );
	void DesenharInfectado( int x, int y );
	int AbaInfectado( void ) const;		// índice da aba "INFECTADO" (só no Surto; -1 fora)

	int m_iCategoria;
	int m_iItemSobre;
};

static CTrokLojaPainel *s_pLoja = NULL;

CTrokLojaPainel::CTrokLojaPainel() : BaseClass( "TrokLoja" )
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

void CTrokLojaPainel::Mostrar( bool bMostrar )
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

bool CTrokLojaPainel::PodeComprarAgora( float *pflFalta ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( pflFalta )
		*pflFalta = 0.0f;
	if ( !pLocal || !pPlacar || !pPlacar->IsEconomia() || !pLocal->IsAlive() || TrokSurto_IsZumbi( pLocal ) )
		return false;
	const float flFalta = pPlacar->GetCompraAte( pLocal->entindex() ) - gpGlobals->curtime;
	if ( pflFalta )
		*pflFalta = MAX( 0.0f, flFalta );
	return flFalta > 0.0f;
}

void CTrokLojaPainel::OnTick( void )
{
	if ( !IsVisible() )
		return;
	// Fora da partida, morto ou sem tempo de compra: fecha sozinho (no Surto a aba de infectado
	// fica aberta: o tipo de zumbi se escolhe a qualquer hora).
	if ( !engine->IsInGame() || ( !PodeComprarAgora( NULL ) && AbaInfectado() < 0 ) )
		Mostrar( false );
}

int CTrokLojaPainel::AbaInfectado( void ) const
{
	return TrokSurto_Ativo() ? TrokLoja().m_Categorias.Count() : -1;
}

void CTrokLojaPainel::OnKeyCodePressed( KeyCode code )
{
	const ButtonCode_t teclaLoja = gameuifuncs ? gameuifuncs->GetButtonCodeForBind( "trok_loja" ) : BUTTON_CODE_INVALID;
	if ( code == KEY_ESCAPE || code == teclaLoja )
	{
		Mostrar( false );
		return;
	}
	const CTrokLojaDados &loja = TrokLoja();
	int iNum = -1;
	if ( code >= KEY_1 && code <= KEY_9 )
		iNum = code - KEY_1;
	else if ( code >= KEY_PAD_1 && code <= KEY_PAD_9 )
		iNum = code - KEY_PAD_1;
	if ( iNum >= 0 && ( iNum < loja.m_Categorias.Count() || iNum == AbaInfectado() ) )
	{
		m_iCategoria = iNum;
		return;
	}
	BaseClass::OnKeyCodePressed( code );
}

int CTrokLojaPainel::Possui( const TrokLojaItem_t &it ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !pLocal )
		return 0;
	const int i = pLocal->entindex();
	switch ( it.iTipo )
	{
	case LOJA_ARMA:
		return ( it.iMax == 0 && pLocal->Weapon_OwnsThisType( it.szEntidade ) ) ? 1 : 0;
	case LOJA_COLETE:
		return ( pPlacar && pPlacar->GetColete( i ) >= it.iNivel ) ? 2 : 0;
	case LOJA_CAPACETE:
		return ( pPlacar && pPlacar->GetCapacete( i ) > 0 ) ? 1 : 0;
	default:
		return 0;
	}
}

void CTrokLojaPainel::Desenhar( void )
{
	const CTrokLojaDados &loja = TrokLoja();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	const int iLocal = pLocal ? pLocal->entindex() : 0;
	const int iDinheiro = pPlacar ? pPlacar->GetDinheiro( iLocal ) : 0;
	const Color acc = Destaque();

	// Fundo escurecido (o jogo continua atrás).
	Ret( 0, 0, m_iLargura, m_iAltura, Color( COR_FUNDO.r(), COR_FUNDO.g(), COR_FUNDO.b(), 200 ) );

	const int x0 = S( 64 ), y0 = S( 56 );
	const int wTotal = m_iLargura - 2 * x0;

	// Cabeçalho: LOJA, dinheiro e tempo de compra. No Surto são duas moedas, lado a lado.
	Texto( m_hTitulo, x0, y0, COR_TEXTO, "LOJA" );
	char szDin[ 32 ];
	FormatarDinheiro( iDinheiro, szDin, sizeof( szDin ) );
	TextoDir( m_hTitulo, x0 + wTotal, y0, acc, szDin );
	if ( AbaInfectado() >= 0 )
	{
		const int wDin = Largura( m_hTitulo, szDin );
		TextoDir( m_hPequena, x0 + wTotal - wDin - S( 10 ), y0 + Altura( m_hTitulo ) - Altura( m_hPequena ) - S( 4 ), COR_TEXTO3, "HUMANO" );
		char szInf[ 48 ];
		V_snprintf( szInf, sizeof( szInf ), "INFECTADO  %d", TrokZumbis_Dinheiro( pLocal ) );
		const int xInf = x0 + wTotal - wDin - S( 10 ) - Largura( m_hPequena, "HUMANO" ) - S( 28 );
		TextoDir( m_hItem, xInf, y0 + Altura( m_hTitulo ) - Altura( m_hItem ) - S( 2 ), COR_ZUMBI, szInf );
	}
	float flFalta = 0.0f;
	const bool bPode = PodeComprarAgora( &flFalta );
	char szTempo[ 64 ];
	if ( bPode )
		V_snprintf( szTempo, sizeof( szTempo ), "TEMPO DE COMPRA  %d s", (int)ceilf( flFalta ) );
	else
		V_strncpy( szTempo, "COMPRA FECHADA", sizeof( szTempo ) );
	const int yTempo = y0 + Altura( m_hTitulo ) + S( 2 );
	TextoDir( m_hPequena, x0 + wTotal, yTempo, bPode ? COR_TEXTO2 : COR_INIMIGO, szTempo );

	// Equipamento atual
	if ( pPlacar )
	{
		char szEq[ 96 ];
		const int iCol = pPlacar->GetColete( iLocal ), iCap = pPlacar->GetCapacete( iLocal );
		char szCap[ 32 ] = "SEM CAPACETE";
		if ( iCap >= 255 )
			V_strncpy( szCap, "CAPACETE", sizeof( szCap ) );
		else if ( iCap > 0 )
			V_snprintf( szCap, sizeof( szCap ), "CAPACETE (%d)", iCap );
		if ( iCol > 0 )
			V_snprintf( szEq, sizeof( szEq ), "COLETE NÍVEL %d  ·  %s", iCol, szCap );
		else
			V_snprintf( szEq, sizeof( szEq ), "SEM COLETE  ·  %s", szCap );
		Texto( m_hPequena, x0, yTempo, COR_TEXTO3, szEq );
	}

	const int yCorpo = yTempo + Altura( m_hPequena ) + S( 18 );
	Ret( x0, yCorpo - S( 9 ), wTotal, 1, COR_LINHA );

	// Categorias
	const int wCat = S( 220 ), hCat = S( 40 );
	const int iAbaInf = AbaInfectado();
	const int nAbas = loja.m_Categorias.Count() + ( iAbaInf >= 0 ? 1 : 0 );
	m_iCategoria = clamp( m_iCategoria, 0, MAX( 0, nAbas - 1 ) );
	for ( int c = 0; c < nAbas; ++c )
	{
		char szTecla[ 8 ];
		V_snprintf( szTecla, sizeof( szTecla ), "%d", c + 1 );
		const int y = yCorpo + c * ( hCat + S( 6 ) );
		const char *pszNome = ( c == iAbaInf ) ? "INFECTADO" : loja.m_Categorias[ c ].szNome;
		if ( Botao( x0, y, wCat, hCat, m_hBotao, pszNome, c == m_iCategoria, szTecla ) )
			m_iCategoria = c;
	}
	if ( m_iCategoria == iAbaInf )
	{
		DesenharInfectado( x0 + wCat + S( 24 ), yCorpo );
		int xDica = x0;
		Dica( xDica, m_iAltura - S( 48 ), "B", "FECHAR" );
		return;
	}
	if ( loja.m_Categorias.Count() == 0 )
	{
		Texto( m_hCorpo, x0, yCorpo, COR_TEXTO2, "Loja vazia: falta scripts/trok_loja.txt." );
		return;
	}

	// Itens da categoria
	const char *pszCat = loja.m_Categorias[ m_iCategoria ].szId;
	const int xItens = x0 + wCat + S( 24 );
	const int wItens = S( 440 ), hItem = S( 46 );
	int y = yCorpo;
	int iSobre = -1;
	FOR_EACH_VEC( loja.m_Itens, k )
	{
		const TrokLojaItem_t &it = loja.m_Itens[ k ];
		if ( V_stricmp( it.szCategoria, pszCat ) )
			continue;
		const int iTem = Possui( it );
		const bool bCaro = iDinheiro < it.iPreco;
		const bool bSobre = Sobre( xItens, y, wItens, hItem );
		if ( bSobre )
			iSobre = k;

		Ret( xItens, y, wItens, hItem, bSobre ? COR_PAINEL_HOVER : COR_PAINEL );
		Contorno( xItens, y, wItens, hItem, bSobre ? COR_LINHA_FORTE : COR_LINHA );
		if ( iTem )
			Ret( xItens, y, S( 3 ), hItem, acc );

		const Color corNome = ( bCaro || iTem ) ? COR_TEXTO3 : COR_TEXTO;
		Texto( m_hItem, xItens + S( 14 ), y + ( hItem - Altura( m_hItem ) ) / 2, corNome, it.szNome );

		char szPreco[ 32 ];
		FormatarDinheiro( it.iPreco, szPreco, sizeof( szPreco ) );
		const char *pszDir = iTem == 2 ? "EQUIPADO" : ( iTem ? "NA MÃO" : szPreco );
		TextoDir( m_hItem, xItens + wItens - S( 14 ), y + ( hItem - Altura( m_hItem ) ) / 2,
			iTem ? acc : ( bCaro ? COR_INIMIGO : COR_TEXTO2 ), pszDir );

		if ( Clicou( xItens, y, wItens, hItem ) && bPode && !bCaro && !iTem )
		{
			char szCmd[ 64 ];
			V_snprintf( szCmd, sizeof( szCmd ), "trok_comprar %s\n", it.szId );
			engine->ClientCmd( szCmd );
		}
		y += hItem + S( 6 );
	}
	if ( Pintando() )
		m_iItemSobre = iSobre;

	// Detalhe do item sob o mouse
	if ( Pintando() && loja.m_Itens.IsValidIndex( m_iItemSobre ) )
		DesenharDetalhe( xItens + wItens + S( 24 ), yCorpo, x0 + wTotal - ( xItens + wItens + S( 24 ) ), loja.m_Itens[ m_iItemSobre ] );

	// Rodapé
	int xDica = x0;
	const int yDica = m_iAltura - S( 48 );
	Dica( xDica, yDica, "1-9", "CATEGORIA" );
	Dica( xDica, yDica, "CLIQUE", "COMPRAR" );
	Dica( xDica, yDica, "B", "FECHAR" );
	if ( Botao( x0 + wTotal - S( 140 ), m_iAltura - S( 60 ), S( 140 ), S( 36 ), m_hBotao, "FECHAR", false ) )
		Mostrar( false );
}

void CTrokLojaPainel::DesenharDetalhe( int x, int y, int w, const TrokLojaItem_t &it )
{
	if ( w < S( 120 ) )
		return;
	Ret( x, y, w, S( 220 ), COR_PAINEL );
	Contorno( x, y, w, S( 220 ), COR_LINHA );
	int ty = y + S( 14 );
	Texto( m_hItem, x + S( 14 ), ty, COR_TEXTO, it.szNome );
	ty += Altura( m_hItem ) + S( 6 );
	if ( it.szDescricao[ 0 ] )
	{
		Texto( m_hCorpo, x + S( 14 ), ty, COR_TEXTO2, it.szDescricao );
		ty += Altura( m_hCorpo ) + S( 10 );
	}
	char sz[ 96 ];
	if ( it.iTipo == LOJA_COLETE )
	{
		V_snprintf( sz, sizeof( sz ), "NÍVEL %d  ·  ARMADURA %d", it.iNivel, it.iArmadura );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
		ty += Altura( m_hPequena ) + S( 4 );
		V_snprintf( sz, sizeof( sz ), "FÔLEGO %+d%%  ·  VELOCIDADE %+d%%", (int)floorf( ( it.flFolego - 1.0f ) * 100.0f + 0.5f ), (int)floorf( ( it.flVelocidade - 1.0f ) * 100.0f + 0.5f ) );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
	}
	else if ( it.iTipo == LOJA_CAPACETE )
	{
		V_snprintf( sz, sizeof( sz ), "TIRA %d%% DO EXTRA DA CABEÇA", (int)( it.flProtecao * 100.0f + 0.5f ) );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
		ty += Altura( m_hPequena ) + S( 4 );
		if ( it.iGolpes > 0 )
			V_snprintf( sz, sizeof( sz ), "AGUENTA %d TIROS NA CABEÇA", it.iGolpes );
		else
			V_strncpy( sz, "NÃO QUEBRA", sizeof( sz ) );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
	}
	else if ( it.iTipo == LOJA_MEDICO )
	{
		V_snprintf( sz, sizeof( sz ), "+%d DE VIDA", it.iVida );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
	}
	else if ( it.iTipo == LOJA_ARMA && it.iMax > 0 )
	{
		V_snprintf( sz, sizeof( sz ), "CARREGA ATÉ %d", it.iMax );
		Texto( m_hPequena, x + S( 14 ), ty, COR_TEXTO2, sz );
	}
}

// Aba do Surto: tipos de zumbi pagos com o DINHEIRO DE INFECTADO (outra moeda, de
// trok_zumbis.cpp). Clicar só escolhe o tipo (trok_zumbi_classe); paga-se ao virar zumbi.
void CTrokLojaPainel::DesenharInfectado( int x, int y )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const int iSaldo = TrokZumbis_Dinheiro( pLocal );
	const int iEscolha = TrokZumbis_EscolhaDe( pLocal );
	const Color acc = Destaque();
	const int w = S( 440 ), h = S( 46 );

	char sz[ 128 ];
	V_snprintf( sz, sizeof( sz ), "DINHEIRO DE INFECTADO: %d   (moeda separada do dinheiro de humano)", iSaldo );
	Texto( m_hPequena, x, y, COR_ZUMBI, sz );
	Texto( m_hPequena, x, y + Altura( m_hPequena ) + S( 2 ), COR_TEXTO3, "Escolha o tipo agora; paga ao virar zumbi (sem saldo, vira o normal)." );
	y += 2 * Altura( m_hPequena ) + S( 12 );

	for ( int i = 0; i < TrokZumbis_Num(); ++i )
	{
		const TrokZumbiClasse_t &c = TrokZumbis_Classe( i );
		if ( !c.bJogavel )
			continue;
		const bool bSobre = Sobre( x, y, w, h );
		const bool bEscolhido = ( i == iEscolha );
		const bool bCaro = c.iPreco > iSaldo;
		Ret( x, y, w, h, bSobre ? COR_PAINEL_HOVER : COR_PAINEL );
		Contorno( x, y, w, h, bSobre ? COR_LINHA_FORTE : COR_LINHA );
		if ( bEscolhido )
			Ret( x, y, S( 3 ), h, COR_ZUMBI );
		Texto( m_hItem, x + S( 14 ), y + ( h - Altura( m_hItem ) ) / 2, bCaro ? COR_TEXTO3 : COR_TEXTO, c.szTitulo );
		if ( bEscolhido )
			V_strncpy( sz, "ESCOLHIDO", sizeof( sz ) );
		else if ( c.iPreco > 0 )
			V_snprintf( sz, sizeof( sz ), "%d INFECTADO", c.iPreco );
		else
			V_strncpy( sz, "GRÁTIS", sizeof( sz ) );
		TextoDir( m_hItem, x + w - S( 14 ), y + ( h - Altura( m_hItem ) ) / 2,
			bEscolhido ? acc : ( bCaro ? COR_INIMIGO : COR_ZUMBI ), sz );
		if ( Clicou( x, y, w, h ) && !bEscolhido )
		{
			V_snprintf( sz, sizeof( sz ), "trok_zumbi_classe %s\n", c.szNome );
			engine->ClientCmd( sz );
		}
		y += h + S( 6 );
	}
}

//-----------------------------------------------------------------------------
// Comandos
//-----------------------------------------------------------------------------
CON_COMMAND( trok_loja, "Abre ou fecha o menu de compra (economia: trok_economia 1)." )
{
	CTrokPlacarExtra *pPlacar = TrokPlacarExtra();
	if ( !engine->IsInGame() )
		return;
	if ( !pPlacar || !pPlacar->IsEconomia() )
	{
		Msg( "A economia está desligada neste servidor (trok_economia 0).\n" );
		return;
	}
	if ( !s_pLoja )
		s_pLoja = new CTrokLojaPainel();
	if ( s_pLoja->IsVisible() )
	{
		s_pLoja->Mostrar( false );
		return;
	}
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const bool bPodeHumano = pLocal && pLocal->IsAlive() && !TrokSurto_IsZumbi( pLocal ) &&
		pPlacar->GetCompraAte( pLocal->entindex() ) > gpGlobals->curtime;
	if ( !bPodeHumano && !TrokSurto_Ativo() )
	{
		Msg( "Agora você não pode comprar (morto, zumbi ou o tempo de compra acabou).\n" );
		return;
	}
	if ( !bPodeHumano )
		s_pLoja->IrParaInfectado();	// Surto, sem poder comprar de humano: abre na aba do infectado
	s_pLoja->Mostrar( true );
}

CON_COMMAND( trok_loja_lista, "Lista no console os itens da loja (id para trok_comprar, preço e categoria)." )
{
	const CTrokLojaDados &loja = TrokLoja();
	FOR_EACH_VEC( loja.m_Categorias, c )
	{
		Msg( "== %s ==\n", loja.m_Categorias[ c ].szNome );
		FOR_EACH_VEC( loja.m_Itens, k )
		{
			const TrokLojaItem_t &it = loja.m_Itens[ k ];
			if ( !V_stricmp( it.szCategoria, loja.m_Categorias[ c ].szId ) )
				Msg( "  %-22s $%-6d %s\n", it.szId, it.iPreco, it.szNome );
		}
	}
	Msg( "Comprar: trok_comprar <id> (ou tecla B para o menu).\n" );
}

CON_COMMAND( trok_loja_recarregar, "Relê scripts/trok_loja.txt no cliente (o servidor relê a cada mapa)." )
{
	TrokLoja().Carregar();
}
