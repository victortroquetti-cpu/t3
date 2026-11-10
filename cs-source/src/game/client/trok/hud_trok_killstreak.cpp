//=============================================================================
// Trok: anúncio da série de abates como no Combat Arms (cliente).
//
// Regras: design/chimera/specs/combat_arms/kill_streaks.md, Parte A. O servidor manda o
// evento "trok_serie" em todo abate de inimigo (shared/trok/trok_killstreak.cpp). Aqui:
// - cada abate vira UM evento, pela prioridade do CA: primeiro abate > vingança > passo da
//   série (multi-abate 2..7+) > na cabeça (só abate sem série) > abate simples (só o seu);
// - faixa no alto da tela (só para quem matou): entra 5x maior e transparente, solta um "eco"
//   que cresce enquanto treme, segura e sai crescendo e sumindo (1,4 s no CA). Uma faixa por
//   vez: pedido de nível menor que o da faixa na tela é descartado, igual ou maior troca na
//   hora, sem fila;
// - no máximo uma voz por abate (a do evento; senão a da cabeça se o abate foi na cabeça),
//   tocada como som de interface; vozes de abates seguidos se sobrepõem;
// - linha do lado esquerdo ("NOME  ABATE DUPLO") para todos, em primeiro abate e passos.
// Textos, sons, níveis, cores e tempos: scripts/trok_anunciador.txt (trok_serie_recarregar
// relê). O desenho é texto (fonte Barlow Condensed em vários tamanhos), sem arte do CA.
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "hl2mp_gamerules.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "trok/trok_killstreak.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_serie_hud( "trok_serie_hud", "1", FCVAR_ARCHIVE, "Mostra a faixa e a lista da série de abates." );

#define TROK_ANUNCIADOR_ARQUIVO "scripts/trok_anunciador.txt"

static const int SERIE_ESCALAS = 16;
static const float s_flEscalas[ SERIE_ESCALAS ] = { 0.4f, 0.5f, 0.6f, 0.75f, 0.9f, 1.0f, 1.15f, 1.3f, 1.5f, 1.75f, 2.0f, 2.4f, 2.9f, 3.5f, 4.2f, 5.0f };
static const int SERIE_FONTE_MAX = 250;		// o cache de fontes do VGUI não passa muito disso

enum TrokSerieEvento_t
{
	EV_NENHUM = 0,
	EV_PRIMEIRO,
	EV_VINGANCA,
	EV_PASSO,
	EV_CABECA,
	EV_ABATE,
	EV_SORTE,
};

struct TrokAnuncio_t
{
	char szTexto[ 48 ];			// texto da faixa
	char szTextoVarios[ 48 ];	// abate simples com mais de 1 no total
	char szSom[ 128 ];			// voz (caminho em sound/); vazio = sem voz
	char szLista[ 48 ];			// rótulo da linha da esquerda; vazio = sem linha
	char szTopo[ 32 ];			// marca sobre a faixa (a "caveira" do CA, aqui texto)
	Color corFaixa;
	Color corLista;
	int iNivel;
};

static Color LerCor( KeyValues *kv, const char *pszKey, const Color &padrao )
{
	const char *psz = kv ? kv->GetString( pszKey, "" ) : "";
	if ( !psz[ 0 ] )
		return padrao;
	int r = padrao.r(), g = padrao.g(), b = padrao.b(), a = 255;
	const int n = sscanf( psz, "%d %d %d %d", &r, &g, &b, &a );
	if ( n < 4 )
		a = 255;
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), clamp( a, 0, 255 ) );
}

class CHudTrokSerie : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokSerie, Panel );

public:
	CHudTrokSerie( const char *pElementName );

	virtual void Init( void );
	virtual void VidInit( void ) { Reset(); }
	virtual void LevelInit( void ) { Reset(); }
	virtual void Reset( void );
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );

	void CarregarPacote( void );
	void Abate( int iAtk, int iMulti, int iAbates, bool bCabeca, bool bPrimeiro, bool bVinganca, bool bSorte );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * ScreenHeight() / 720.0f + 0.5f ); }
	void CriarFontes( void );
	HFont FonteFaixa( float flEscala ) const;
	HFont FonteLista( float flEscala ) const;
	void TextoCentro( HFont hFont, int cx, int cy, const Color &col, const char *pszUTF8 );
	void PedirFaixa( const TrokAnuncio_t &a, int iNumero, const char *pszTopo );
	void Voz( const char *pszSom );
	void NovaLinha( int iJogador, const TrokAnuncio_t &a );
	Color CorJogador( int iJogador ) const;
	void PintarFaixa( void );
	void PintarCopia( float flEscala, int iAlpha, int dx, int dy, bool bEco );
	void PintarLista( void );

	// Pacote (scripts/trok_anunciador.txt)
	TrokAnuncio_t m_Passos[ 8 ];		// 2..7 (7 = 7 ou mais)
	TrokAnuncio_t m_Primeiro, m_Vinganca, m_Cabeca, m_Abate, m_Sorte;
	float m_flTotal, m_flInicio, m_flEntrada, m_flEco, m_flSaidaInicio, m_flSaida;
	float m_flEscalaEntrada, m_flEscalaEco, m_flEscalaSaida;
	int m_iTremor;
	float m_flCentroY, m_flAlturaTexto;
	float m_flListaX, m_flListaY, m_flListaTempo, m_flListaFase1, m_flListaFase2, m_flListaFase3, m_flListaEscala;
	int m_iListaMax;
	Color m_corAliado, m_corInimigo;

	// Fontes em vários tamanhos (a faixa cresce e encolhe)
	HFont m_hFaixa[ SERIE_ESCALAS ];
	HFont m_hLista[ 3 ];
	HFont m_hListaNome;
	int m_iFontesAltura;

	// Faixa na tela (uma só)
	bool m_bFaixa;
	float m_flFaixaInicio;
	int m_iFaixaNivel;
	char m_szFaixaTexto[ 64 ];
	char m_szFaixaTopo[ 32 ];
	Color m_corFaixa;

	// Linhas da esquerda
	struct Linha_t
	{
		char szNome[ 64 ];
		char szRotulo[ 48 ];
		Color corNome, corRotulo;
		float flInicio;
	};
	CUtlVector< Linha_t > m_Linhas;
};

DECLARE_HUDELEMENT( CHudTrokSerie );

CHudTrokSerie::CHudTrokSerie( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokSerie" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	for ( int i = 0; i < SERIE_ESCALAS; ++i )
		m_hFaixa[ i ] = INVALID_FONT;
	for ( int i = 0; i < 3; ++i )
		m_hLista[ i ] = INVALID_FONT;
	m_hListaNome = INVALID_FONT;
	m_iFontesAltura = 0;
	m_bFaixa = false;
	CarregarPacote();
}

void CHudTrokSerie::Init( void )
{
	ListenForGameEvent( "trok_serie" );
	Reset();
}

void CHudTrokSerie::Reset( void )
{
	// Entrar no mapa (ou reconectar) apaga tudo (CA A4).
	m_bFaixa = false;
	m_Linhas.RemoveAll();
}

bool CHudTrokSerie::ShouldDraw( void )
{
	if ( !trok_serie_hud.GetBool() || engine->IsLevelMainMenuBackground() )
		return false;
	if ( !m_bFaixa && m_Linhas.Count() == 0 )
		return false;
	return CHudElement::ShouldDraw();
}

//-----------------------------------------------------------------------------
// Pacote do anunciador
//-----------------------------------------------------------------------------
static void LerAnuncio( KeyValues *pRoot, const char *pszNome, TrokAnuncio_t &a, int iNivelPadrao, const char *pszTextoPadrao )
{
	V_memset( &a, 0, sizeof( a ) );
	KeyValues *kv = pRoot ? pRoot->FindKey( pszNome ) : NULL;
	V_strncpy( a.szTexto, kv ? kv->GetString( "texto", pszTextoPadrao ) : pszTextoPadrao, sizeof( a.szTexto ) );
	V_strncpy( a.szTextoVarios, kv ? kv->GetString( "texto_varios", a.szTexto ) : a.szTexto, sizeof( a.szTextoVarios ) );
	V_strncpy( a.szSom, kv ? kv->GetString( "som", "" ) : "", sizeof( a.szSom ) );
	V_strncpy( a.szLista, kv ? kv->GetString( "lista", "" ) : "", sizeof( a.szLista ) );
	V_strncpy( a.szTopo, kv ? kv->GetString( "topo", "" ) : "", sizeof( a.szTopo ) );
	a.corFaixa = LerCor( kv, "cor", Color( 236, 238, 241, 255 ) );
	a.corLista = LerCor( kv, "cor_lista", Color( 236, 238, 241, 255 ) );
	a.iNivel = kv ? kv->GetInt( "nivel", iNivelPadrao ) : iNivelPadrao;
}

void CHudTrokSerie::CarregarPacote( void )
{
	KeyValues *pRoot = new KeyValues( "Anunciador" );
	if ( !pRoot->LoadFromFile( g_pFullFileSystem, TROK_ANUNCIADOR_ARQUIVO, "MOD" ) )
		Warning( "Trok: não achei %s; anunciador com os padrões.\n", TROK_ANUNCIADOR_ARQUIVO );

	static const char *s_pszPassos[ 8 ] = { "", "", "ABATE DUPLO", "ABATE MÚLTIPLO", "ULTRA ABATE", "FANTÁSTICO", "INACREDITÁVEL", "INACREDITÁVEL+" };
	KeyValues *pPassos = pRoot->FindKey( "passos" );
	for ( int n = 0; n < 8; ++n )
	{
		char szKey[ 8 ];
		V_snprintf( szKey, sizeof( szKey ), "%d", n );
		LerAnuncio( pPassos, szKey, m_Passos[ n ], SERIE_NIVEL_ESPECIAL, s_pszPassos[ n ] );
	}
	LerAnuncio( pRoot, "primeiro", m_Primeiro, SERIE_NIVEL_EVENTO, "PRIMEIRO ABATE" );
	LerAnuncio( pRoot, "vinganca", m_Vinganca, SERIE_NIVEL_EVENTO, "VINGANÇA" );
	LerAnuncio( pRoot, "cabeca", m_Cabeca, SERIE_NIVEL_ESPECIAL, "NA CABEÇA" );
	LerAnuncio( pRoot, "abate", m_Abate, SERIE_NIVEL_ABATE, "ABATE" );
	LerAnuncio( pRoot, "sorte", m_Sorte, SERIE_NIVEL_SORTE, "TIRO DE SORTE" );

	// Tempos da faixa (CA B4) e da lista (CA B6).
	KeyValues *f = pRoot->FindKey( "faixa" );
	m_flTotal = f ? f->GetFloat( "total", 1.4f ) : 1.4f;
	m_flInicio = f ? f->GetFloat( "inicio", 0.0f ) : 0.0f;
	m_flEntrada = MAX( 0.001f, f ? f->GetFloat( "entrada", 0.1f ) : 0.1f );
	m_flEco = MAX( 0.001f, f ? f->GetFloat( "eco", 0.15f ) : 0.15f );
	m_flSaidaInicio = f ? f->GetFloat( "saida_inicio", 1.2f ) : 1.2f;
	m_flSaida = MAX( 0.001f, f ? f->GetFloat( "saida", 0.2f ) : 0.2f );
	m_flEscalaEntrada = MAX( 1.001f, f ? f->GetFloat( "escala_entrada", 5.0f ) : 5.0f );
	m_flEscalaEco = MAX( 1.001f, f ? f->GetFloat( "escala_eco", 3.0f ) : 3.0f );
	m_flEscalaSaida = MAX( 1.001f, f ? f->GetFloat( "escala_saida", 5.0f ) : 5.0f );
	m_iTremor = f ? f->GetInt( "tremor", 5 ) : 5;
	m_flCentroY = f ? f->GetFloat( "centro_y", 0.266f ) : 0.266f;
	m_flAlturaTexto = f ? f->GetFloat( "altura_texto", 0.075f ) : 0.075f;

	KeyValues *l = pRoot->FindKey( "lista" );
	m_flListaX = l ? l->GetFloat( "x", 32.0f ) : 32.0f;
	m_flListaY = l ? l->GetFloat( "y", 222.0f ) : 222.0f;
	m_flListaTempo = l ? l->GetFloat( "tempo", 3.75f ) : 3.75f;
	m_flListaFase1 = MAX( 0.001f, l ? l->GetFloat( "fase1", 1.0f ) : 1.0f );
	m_flListaFase2 = MAX( m_flListaFase1 + 0.001f, l ? l->GetFloat( "fase2", 1.375f ) : 1.375f );
	m_flListaFase3 = l ? l->GetFloat( "fase3", 2.375f ) : 2.375f;
	m_flListaEscala = l ? l->GetFloat( "escala", 1.1f ) : 1.1f;
	m_iListaMax = MAX( 1, l ? l->GetInt( "max", 5 ) : 5 );
	m_corAliado = LerCor( l, "cor_aliado", Color( 90, 162, 255, 255 ) );
	m_corInimigo = LerCor( l, "cor_inimigo", Color( 255, 90, 90, 255 ) );

	pRoot->deleteThis();
	m_iFontesAltura = 0;	// recria as fontes (a altura do texto pode ter mudado)
}

//-----------------------------------------------------------------------------
// Fontes
//-----------------------------------------------------------------------------
void CHudTrokSerie::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
	m_iFontesAltura = 0;
}

void CHudTrokSerie::CriarFontes( void )
{
	if ( m_iFontesAltura == ScreenHeight() )
		return;
	m_iFontesAltura = ScreenHeight();

	const float flBase = MAX( 8.0f, m_flAlturaTexto * ScreenHeight() );
	for ( int i = 0; i < SERIE_ESCALAS; ++i )
	{
		if ( m_hFaixa[ i ] == INVALID_FONT )
			m_hFaixa[ i ] = surface()->CreateFont();
		const int iTall = MIN( SERIE_FONTE_MAX, (int)( flBase * s_flEscalas[ i ] + 0.5f ) );
		surface()->SetFontGlyphSet( m_hFaixa[ i ], "Barlow Condensed", iTall, 700, 0, 0, ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW );
	}
	static const float s_flLista[ 3 ] = { 1.0f, 1.05f, 1.1f };
	for ( int i = 0; i < 3; ++i )
	{
		if ( m_hLista[ i ] == INVALID_FONT )
			m_hLista[ i ] = surface()->CreateFont();
		surface()->SetFontGlyphSet( m_hLista[ i ], "Barlow Condensed", (int)( S( 17 ) * s_flLista[ i ] + 0.5f ), 700, 0, 0, ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW );
	}
	if ( m_hListaNome == INVALID_FONT )
		m_hListaNome = surface()->CreateFont();
	surface()->SetFontGlyphSet( m_hListaNome, "Barlow Condensed SemiBold", S( 16 ), 400, 0, 0, ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW );
}

HFont CHudTrokSerie::FonteFaixa( float flEscala ) const
{
	int iMelhor = 0;
	float flDist = FLT_MAX;
	for ( int i = 0; i < SERIE_ESCALAS; ++i )
	{
		const float d = fabsf( s_flEscalas[ i ] - flEscala );
		if ( d < flDist )
		{
			flDist = d;
			iMelhor = i;
		}
	}
	return m_hFaixa[ iMelhor ];
}

HFont CHudTrokSerie::FonteLista( float flEscala ) const
{
	if ( flEscala >= 1.075f )
		return m_hLista[ 2 ];
	if ( flEscala >= 1.025f )
		return m_hLista[ 1 ];
	return m_hLista[ 0 ];
}

void CHudTrokSerie::TextoCentro( HFont hFont, int cx, int cy, const Color &col, const char *pszUTF8 )
{
	if ( hFont == INVALID_FONT || !pszUTF8[ 0 ] )
		return;
	wchar_t wsz[ 128 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( cx - w / 2, cy - t / 2 );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
}

//-----------------------------------------------------------------------------
// Eventos
//-----------------------------------------------------------------------------
void CHudTrokSerie::FireGameEvent( IGameEvent *event )
{
	if ( V_strcmp( event->GetName(), "trok_serie" ) )
		return;
	const int iAtk = engine->GetPlayerForUserID( event->GetInt( "atacante" ) );
	Abate( iAtk, event->GetInt( "multi" ), event->GetInt( "abates" ), event->GetBool( "cabeca" ),
		event->GetBool( "primeiro" ), event->GetBool( "vinganca" ), event->GetBool( "sorte" ) );
}

// Um abate, em todos os clientes (kill_streaks.md A2, A3).
void CHudTrokSerie::Abate( int iAtk, int iMulti, int iAbates, bool bCabeca, bool bPrimeiro, bool bVinganca, bool bSorte )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const bool bLocal = pLocal && iAtk > 0 && iAtk == pLocal->entindex();

	// Um evento por abate, nesta ordem.
	int iEvento = EV_NENHUM;
	const TrokAnuncio_t *pA = NULL;
	bool bCabecaAinda = bCabeca;	// a marca de cabeça vale para o topo e para a voz reserva
	if ( bPrimeiro )
	{
		iEvento = EV_PRIMEIRO;
		pA = &m_Primeiro;
	}
	else if ( bVinganca )
	{
		iEvento = EV_VINGANCA;
		pA = &m_Vinganca;
	}
	else if ( iMulti == 1 )
	{
		if ( bCabeca )
		{
			iEvento = EV_CABECA;
			pA = &m_Cabeca;
			bCabecaAinda = false;	// usada pelo próprio evento: sem marca no topo nem voz reserva
		}
		else if ( bLocal )
		{
			iEvento = EV_ABATE;
			pA = &m_Abate;
		}
	}
	else if ( iMulti >= 2 )
	{
		iEvento = EV_PASSO;
		pA = &m_Passos[ MIN( iMulti, 7 ) ];
	}

	// Linha da esquerda: para todos, em primeiro abate e passos (o resto não tem rótulo).
	if ( pA && ( iEvento == EV_PRIMEIRO || iEvento == EV_PASSO ) && pA->szLista[ 0 ] )
		NovaLinha( iAtk, *pA );

	if ( !bLocal )
		return;

	if ( pA )
	{
		// Abate simples: "ABATE" com o total até 1, "N ABATES" acima; 0 = sem número.
		int iNumero = 0;
		if ( iEvento == EV_ABATE )
			iNumero = iAbates > 0 ? ( iAbates % 1000 ) : -1;
		const char *pszTopo = ( iEvento != EV_ABATE && bCabecaAinda ) ? m_Cabeca.szTopo : "";
		if ( iEvento == EV_ABATE && iAbates > 1 )
		{
			TrokAnuncio_t varios = *pA;
			V_strncpy( varios.szTexto, pA->szTextoVarios, sizeof( varios.szTexto ) );
			PedirFaixa( varios, iNumero, pszTopo );
		}
		else
		{
			PedirFaixa( *pA, iNumero, pszTopo );
		}

		// Uma voz: a do evento; senão a da cabeça, se o abate foi na cabeça.
		if ( pA->szSom[ 0 ] )
			Voz( pA->szSom );
		else if ( bCabecaAinda && m_Cabeca.szSom[ 0 ] )
			Voz( m_Cabeca.szSom );
	}

	// Tiro de sorte: faixa própria por cima (nível maior), sem voz.
	if ( bSorte )
		PedirFaixa( m_Sorte, -1, "" );
}

void CHudTrokSerie::PedirFaixa( const TrokAnuncio_t &a, int iNumero, const char *pszTopo )
{
	// Nível menor que o da faixa na tela: descartado (sem fila). Igual ou maior: troca na hora.
	if ( m_bFaixa && gpGlobals->curtime - m_flFaixaInicio < m_flTotal && m_iFaixaNivel > a.iNivel )
		return;
	m_bFaixa = true;
	m_flFaixaInicio = gpGlobals->curtime;
	m_iFaixaNivel = a.iNivel;
	if ( iNumero >= 0 )
		V_snprintf( m_szFaixaTexto, sizeof( m_szFaixaTexto ), "%d %s", iNumero, a.szTexto );
	else
		V_strncpy( m_szFaixaTexto, a.szTexto, sizeof( m_szFaixaTexto ) );
	V_strncpy( m_szFaixaTopo, pszTopo ? pszTopo : "", sizeof( m_szFaixaTopo ) );
	m_corFaixa = a.corFaixa;
}

void CHudTrokSerie::Voz( const char *pszSom )
{
	// Som de interface: sem posição, nunca para nem espera a voz anterior.
	if ( trok_serie_voz.GetBool() && pszSom && pszSom[ 0 ] )
		surface()->PlaySound( pszSom );
}

Color CHudTrokSerie::CorJogador( int iJogador ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !g_PR || iJogador <= 0 )
		return m_corInimigo;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
		return g_PR->GetTeam( iJogador ) == pLocal->GetTeamNumber() ? m_corAliado : m_corInimigo;
	return iJogador == pLocal->entindex() ? m_corAliado : m_corInimigo;
}

void CHudTrokSerie::NovaLinha( int iJogador, const TrokAnuncio_t &a )
{
	// Até N linhas: cheia, a mais velha sai e a nova entra no fim.
	while ( m_Linhas.Count() >= m_iListaMax )
		m_Linhas.Remove( 0 );
	Linha_t &l = m_Linhas[ m_Linhas.AddToTail() ];
	V_strncpy( l.szNome, ( g_PR && iJogador > 0 ) ? g_PR->GetPlayerName( iJogador ) : "", sizeof( l.szNome ) );
	V_strncpy( l.szRotulo, a.szLista, sizeof( l.szRotulo ) );
	l.corNome = CorJogador( iJogador );
	l.corRotulo = a.corLista;
	l.flInicio = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Desenho
//-----------------------------------------------------------------------------
void CHudTrokSerie::Paint( void )
{
	if ( GetWide() != ScreenWidth() || GetTall() != ScreenHeight() )
		SetSize( ScreenWidth(), ScreenHeight() );
	CriarFontes();
	PintarFaixa();
	PintarLista();
}

static float Quarta( float x ) { return x * x * x * x; }

// Alfa que acompanha a escala: 255 na escala 1, 0 na escala máxima (CA A5).
static int AlfaDaEscala( float s, float flMax )
{
	return clamp( (int)( 255.0f - ( s - 1.0f ) * 255.0f / ( flMax - 1.0f ) ), 0, 255 );
}

void CHudTrokSerie::PintarFaixa( void )
{
	if ( !m_bFaixa )
		return;
	const float t = gpGlobals->curtime - m_flFaixaInicio;
	if ( t >= m_flTotal || t < 0.0f )
	{
		m_bFaixa = false;
		return;
	}

	// Cópia principal: entrada (5x -> 1x), segura, saída (1x -> 5x).
	float s = 1.0f;
	int a = 255;
	if ( t < m_flInicio )
	{
		a = 0;
	}
	else if ( t >= m_flSaidaInicio && t < m_flSaidaInicio + m_flSaida )
	{
		const float tau = t - m_flSaidaInicio;
		s = m_flEscalaSaida - ( m_flEscalaSaida - 1.0f ) * Quarta( ( m_flSaida - tau ) / m_flSaida );
		a = AlfaDaEscala( s, m_flEscalaSaida );
	}
	else if ( t >= m_flSaidaInicio + m_flSaida )
	{
		a = 0;
	}
	else if ( t < m_flInicio + m_flEntrada )
	{
		const float tau = t - m_flInicio;
		s = m_flEscalaEntrada - ( m_flEscalaEntrada - 1.0f ) * Quarta( tau / m_flEntrada );
		a = AlfaDaEscala( s, m_flEscalaEntrada );
	}

	// Eco: só logo depois da entrada, cresce de 1x a 3x sumindo; a principal treme nesse tempo.
	const float tauEco = t - ( m_flInicio + m_flEntrada );
	int dx = 0, dy = 0;
	if ( tauEco >= 0.0f && tauEco < m_flEco )
	{
		dx = RandomInt( -m_iTremor, m_iTremor );
		dy = RandomInt( -m_iTremor, m_iTremor );
	}

	if ( a > 0 )
		PintarCopia( s, a, dx, dy, false );
	if ( tauEco >= 0.0f && tauEco < m_flEco )
	{
		const float se = m_flEscalaEco - ( m_flEscalaEco - 1.0f ) * Quarta( ( m_flEco - tauEco ) / m_flEco );
		const int ae = AlfaDaEscala( se, m_flEscalaEco );
		if ( ae > 0 )
			PintarCopia( se, ae, 0, 0, true );
	}
}

void CHudTrokSerie::PintarCopia( float flEscala, int iAlpha, int dx, int dy, bool bEco )
{
	const int cx = ScreenWidth() / 2 + dx;
	const int cy = (int)( m_flCentroY * ScreenHeight() ) + dy;
	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = 242, g = 181, b = 68;
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	const Color corDestaque( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), iAlpha );

	const Color cor = bEco ? corDestaque : Color( m_corFaixa.r(), m_corFaixa.g(), m_corFaixa.b(), iAlpha * m_corFaixa.a() / 255 );
	TextoCentro( FonteFaixa( flEscala ), cx, cy, cor, m_szFaixaTexto );

	// Marca no topo (a caveira do CA vira texto), na mesma escala e alfa.
	if ( m_szFaixaTopo[ 0 ] )
	{
		const int iSobe = (int)( m_flAlturaTexto * ScreenHeight() * 0.75f * flEscala );
		TextoCentro( FonteFaixa( flEscala * 0.4f ), cx, cy - iSobe, corDestaque, m_szFaixaTopo );
	}
}

void CHudTrokSerie::PintarLista( void )
{
	const float flNow = gpGlobals->curtime;
	// A linha mais velha sai quando passa do tempo.
	while ( m_Linhas.Count() > 0 && ( flNow - m_Linhas[ 0 ].flInicio > m_flListaTempo || flNow < m_Linhas[ 0 ].flInicio ) )
		m_Linhas.Remove( 0 );

	const int x0 = S( m_flListaX );
	int y = S( m_flListaY );
	const int iAltura = S( 26 );
	const int iLargura = S( 250 );
	for ( int k = 0; k < m_Linhas.Count(); ++k )
	{
		const Linha_t &l = m_Linhas[ k ];
		const float t = flNow - l.flInicio;

		// Barra e rótulo: crescem até 1,1x aparecendo, voltam a 1x, depois somem (CA A7).
		float s = 1.0f;
		int a = 255;
		if ( t < m_flListaFase1 )
		{
			const float f = t / m_flListaFase1;
			s = 1.0f + f * f * ( m_flListaEscala - 1.0f );
			a = MIN( (int)( f * 255.0f ), 255 );
		}
		else if ( t < m_flListaFase2 )
		{
			const float f = ( t - m_flListaFase1 ) / ( m_flListaFase2 - m_flListaFase1 );
			s = m_flListaEscala - f * f * ( m_flListaEscala - 1.0f );
		}
		else if ( t >= m_flListaFase3 )
		{
			a = clamp( (int)( 255.0f * ( 1.0f - ( t - m_flListaFase3 ) / MAX( 0.001f, m_flListaTempo - m_flListaFase3 ) ) ), 0, 255 );
		}

		// Barra escalada em volta do próprio centro.
		const int bw = (int)( iLargura * s ), bh = (int)( iAltura * s );
		const int bcx = x0 + iLargura / 2, bcy = y + iAltura / 2;
		surface()->DrawSetColor( Color( 11, 13, 16, 150 * a / 255 ) );
		surface()->DrawFilledRect( bcx - bw / 2, bcy - bh / 2, bcx + bw / 2, bcy + bh / 2 );
		surface()->DrawSetColor( Color( l.corRotulo.r(), l.corRotulo.g(), l.corRotulo.b(), a ) );
		surface()->DrawFilledRect( bcx - bw / 2, bcy - bh / 2, bcx - bw / 2 + MAX( 2, S( 3 ) ), bcy + bh / 2 );

		// Nome (sem escala nem fade) e rótulo 5 px depois.
		wchar_t wsz[ 64 ];
		int w = 0, th = 0;
		V_UTF8ToUnicode( l.szNome, wsz, sizeof( wsz ) );
		surface()->GetTextSize( m_hListaNome, wsz, w, th );
		const int xNome = x0 + S( 10 );
		surface()->DrawSetTextFont( m_hListaNome );
		surface()->DrawSetTextColor( l.corNome );
		surface()->DrawSetTextPos( xNome, bcy - th / 2 );
		surface()->DrawPrintText( wsz, V_wcslen( wsz ) );

		HFont hRotulo = FonteLista( s );
		wchar_t wszR[ 64 ];
		int wr = 0, tr = 0;
		V_UTF8ToUnicode( l.szRotulo, wszR, sizeof( wszR ) );
		surface()->GetTextSize( hRotulo, wszR, wr, tr );
		surface()->DrawSetTextFont( hRotulo );
		surface()->DrawSetTextColor( Color( l.corRotulo.r(), l.corRotulo.g(), l.corRotulo.b(), a ) );
		surface()->DrawSetTextPos( xNome + w + S( 5 ), bcy - tr / 2 );
		surface()->DrawPrintText( wszR, V_wcslen( wszR ) );

		y += iAltura + S( 4 );
	}
}

//-----------------------------------------------------------------------------
// Comandos
//-----------------------------------------------------------------------------
CON_COMMAND( trok_serie_recarregar, "Relê scripts/trok_anunciador.txt (textos, sons, níveis e tempos da série de abates)." )
{
	CHudTrokSerie *pHud = GET_HUDELEMENT( CHudTrokSerie );
	if ( pHud )
		pHud->CarregarPacote();
}

// Teste sem abater ninguém: trok_serie_teste <multi> [cabeca] [primeiro] [vinganca] [sorte] [abates N]
CON_COMMAND( trok_serie_teste, "Simula um abate seu: trok_serie_teste <multi> [cabeca] [primeiro] [vinganca] [sorte] [abates N]." )
{
	CHudTrokSerie *pHud = GET_HUDELEMENT( CHudTrokSerie );
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pHud || !pLocal )
		return;
	const int iMulti = args.ArgC() > 1 ? atoi( args[ 1 ] ) : 1;
	bool bCabeca = false, bPrimeiro = false, bVinganca = false, bSorte = false;
	int iAbates = 1;
	for ( int i = 2; i < args.ArgC(); ++i )
	{
		if ( !V_stricmp( args[ i ], "cabeca" ) )
			bCabeca = true;
		else if ( !V_stricmp( args[ i ], "primeiro" ) )
			bPrimeiro = true;
		else if ( !V_stricmp( args[ i ], "vinganca" ) )
			bVinganca = true;
		else if ( !V_stricmp( args[ i ], "sorte" ) )
			bSorte = true;
		else if ( !V_stricmp( args[ i ], "abates" ) && i + 1 < args.ArgC() )
			iAbates = atoi( args[ ++i ] );
	}
	pHud->Abate( pLocal->entindex(), iMulti, iAbates, bCabeca, bPrimeiro, bVinganca, bSorte );
}
