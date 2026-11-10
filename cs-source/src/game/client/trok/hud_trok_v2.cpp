//=============================================================================
// Trok: HUD em jogo, versão 2 (pedido do dono, 10/11/2026: "inspirado nos CoDs modernos,
// sofisticado"). Pesquisa em design/hud-pesquisa.md; prévia e medidas em design/ui.md (HUD).
//
// Um painel de tela cheia desenha a partida inteira (o hud_trok.cpp antigo passa a desenhar só
// a tela de morte; trok_hud_estilo 1 volta ao antigo):
// - radar quadrado em cima à esquerda (gira com a visão, "N" na borda);
// - bússola no topo (marcas a cada 15°, letras a cada 45°, rumo no meio, dano em vermelho);
// - placar do modo embaixo da bússola (Equipe, Solo, Surto, Corrida Armada) e fim de rodada do Surto;
// - lista de abates em cima à direita, só texto com véu escuro leve, a sua linha marcada;
// - vida (número com algarismos de largura fixa + barra em 10 partes com "rastro" do dano),
//   colete, fôlego, créditos da economia, tipo de zumbi (Surto) e "LENTO", embaixo à esquerda;
// - arma (nome, modo, pente grande / reserva, uma marca por bala, explosivos), embaixo à direita,
//   com a escada de armas da Corrida Armada acima;
// - mira que abre com a dispersão, marcador de acerto, direção do dano, aviso de granada,
//   eventos à direita da mira (ABATE +$300, NA CABEÇA), aviso de recarga e proteção.
// Medidas em px de uma tela de 720 de altura (S()), iguais às da prévia. Fontes criadas aqui
// (Barlow Condensed / Barlow, de resource/TrokScheme.res), no tamanho de trok_hud_escala.
// A faixa da série de abates continua em hud_trok_killstreak.cpp e o aviso de pegar arma em
// hud_trok_pegar.cpp (que usa TrokHudV2_Prompt).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "c_team.h"
#include "c_hl2mp_player.h"
#include "hl2mp_gamerules.h"
#include "weapon_parse.h"
#include "basegrenade_shared.h"
#include "trok/weapon_trok_gun.h"
#include "trok/trok_surto.h"
#include "trok/trok_zumbis.h"
#include "trok/trok_corrida.h"
#include "trok/trok_placar_extra.h"
#include "trok/hud_trok_v2.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include <wctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hud_estilo( "trok_hud_estilo", "2", FCVAR_ARCHIVE, "Estilo do HUD: 2 = novo (design/ui.md, seção HUD), 1 = o antigo." );
static ConVar trok_hud_escala( "trok_hud_escala", "1", FCVAR_ARCHIVE, "Tamanho do HUD novo (0.6 a 1.6).", true, 0.6f, true, 1.6f );
static ConVar trok_hud_opacidade( "trok_hud_opacidade", "1", FCVAR_ARCHIVE, "Opacidade do HUD novo (0.2 a 1).", true, 0.2f, true, 1.0f );
static ConVar trok_hud_minimalista( "trok_hud_minimalista", "0", FCVAR_ARCHIVE, "1 = HUD mínimo: sem bússola, rótulos, marcas de bala e créditos; radar menor." );
static ConVar trok_hud_bussola( "trok_hud_bussola", "1", FCVAR_ARCHIVE, "Bússola no topo da tela." );
static ConVar trok_hud_eventos( "trok_hud_eventos", "1", FCVAR_ARCHIVE, "Eventos à direita da mira (ABATE, NA CABEÇA, +$)." );
static ConVar trok_hud_granada( "trok_hud_granada", "1", FCVAR_ARCHIVE, "Aviso de granada perto (ícone em volta da mira apontando para ela)." );
static ConVar trok_hud_granada_raio( "trok_hud_granada_raio", "400", FCVAR_ARCHIVE, "Distância (u) até a qual a granada aparece no aviso." );
static ConVar trok_hud_margem( "trok_hud_margem", "28", FCVAR_ARCHIVE, "Margem do HUD novo até a borda (px numa tela de 720 de altura)." );

static const Color COR_BRANCO( 236, 238, 241, 255 );
static const Color COR_CINZA2( 163, 169, 177, 255 );
static const Color COR_CINZA3( 128, 134, 142, 255 );
static const Color COR_ALIADO( 90, 162, 255, 255 );
static const Color COR_INIMIGO( 255, 90, 90, 255 );
static const Color COR_ZUMBI( 140, 224, 90, 255 );
static const Color COR_COLETE( 157, 183, 208, 255 );
static const Color COR_FUNDO( 11, 13, 16, 255 );
static const Color COR_DESTAQUE_PADRAO( 242, 181, 68, 255 );

static const float UNIDADES_POR_METRO = 52.49f;
static const int MAX_KILLFEED = 5;
static const int MAX_EVENTOS = 4;
static const int MAX_GRANADAS = 4;

enum { AL_ESQ = 0, AL_DIR, AL_CENTRO };

bool TrokHudV2_Ativo( void )
{
	return trok_hud_estilo.GetInt() >= 2;
}

class CHudTrokV2;
static CHudTrokV2 *s_pHudV2 = NULL;

class CHudTrokV2 : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokV2, Panel );
public:
	CHudTrokV2( const char *pElementName );
	virtual ~CHudTrokV2() { if ( s_pHudV2 == this ) s_pHudV2 = NULL; }
	virtual void Init( void );
	virtual void VidInit( void ) { Reset(); }
	virtual void LevelInit( void ) { Reset(); }
	virtual void Reset( void );
	virtual bool ShouldDraw( void );
	virtual void FireGameEvent( IGameEvent *event );
	void MsgFunc_Damage( bf_read &msg );
	void MsgFunc_Battery( bf_read &msg );

	void Evento( const char *pszRotulo, const char *pszValor, const char *pszDetalhe, const Color &cor );
	void Prompt( const char *pszTecla, const char *pszVerbo, const char *pszObjeto, float flY );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void OnThink( void );
	virtual void Paint( void );

private:
	// Medidas e cor
	int S( float px ) const { return (int)( px * m_flEsc + 0.5f ); }
	int S1( float px ) const { return MAX( 1, S( px ) ); }
	Color A( const Color &c, float a = 1.0f ) const { return Color( c.r(), c.g(), c.b(), clamp( (int)( c.a() * a * m_flOpac ), 0, 255 ) ); }
	int Alfa( float a ) const { return clamp( (int)( 255.0f * a * m_flOpac ), 0, 255 ); }
	Color Accent( void ) const;
	Color CorJogador( int iPlayer ) const;
	bool IsAlly( C_BasePlayer *pLocal, int iPlayer ) const;

	// Desenho
	void CriarFontes( void );
	int TextoCore( HFont f, int x, int yBase, const Color &c, const wchar_t *wsz, int iAlinh, float flEsp, bool bMono, bool bDesenhar );
	int Texto( HFont f, int x, int yBase, const Color &c, const char *psz, int iAlinh = AL_ESQ, float flEsp = 0.0f, bool bMono = false );
	int Larg( HFont f, const char *psz, float flEsp = 0.0f, bool bMono = false );
	void Rect( int x, int y, int w, int h, const Color &c );
	void Veu( int x, int y, int w, int h, float a, int iLado );		// 0 escuro à direita, 1 à esquerda, 2 no meio
	void Poly( Vertex_t *v, int n, const Color &c );
	void LinhaGrossa( float x1, float y1, float x2, float y2, float th, const Color &c );
	void Arco( float cx, float cy, float r0, float r1, float a0, float a1, int iSeg, const Color &c );	// ângulo 0 = cima, horário
	void Circulo( float cx, float cy, float r, const Color &c );
	int Tecla( int x, int y, const char *pszTecla, const Color &c );
	void IconeExplosivo( int iTipo, int x, int y, const Color &c );
	void Vinheta( const Color &c, int a );
	void AvisoCentro( const char *pszTexto, const char *pszTecla, const Color &c, bool bPulsa );
	void NomeArma( const char *pszClassOrEventName, char *pszOut, int iOutSize );

	// Partes
	void PaintVinhetas( C_BasePlayer *pLocal );
	void PaintRadar( C_BasePlayer *pLocal );
	void PaintBussola( C_BasePlayer *pLocal );
	void PaintPlacar( C_BasePlayer *pLocal );
	void PaintPlacarEquipe( C_BasePlayer *pLocal, int y );
	void PaintPlacarSolo( C_BasePlayer *pLocal, int y );
	void PaintPlacarSurto( C_BasePlayer *pLocal, int y );
	void PaintPlacarCorrida( C_BasePlayer *pLocal, int y );
	void PaintKillfeed( void );
	void PaintVitais( C_BasePlayer *pLocal );
	void PaintArma( C_BasePlayer *pLocal );
	void PaintDegraus( C_BasePlayer *pLocal );
	void PaintMira( C_BasePlayer *pLocal );
	void PaintHitmarker( void );
	void PaintDano( C_BasePlayer *pLocal );
	void PaintGranadas( C_BasePlayer *pLocal );
	void PaintEventos( void );
	void PaintProtecao( C_BasePlayer *pLocal );
	void UpdateSpotted( C_BasePlayer *pLocal );
	void UpdateGranadas( C_BasePlayer *pLocal );

	// Fontes (criadas aqui no tamanho do HUD)
	HFont m_fBig, m_fScore, m_fTimer, m_fMed, m_fText, m_fLabel, m_fName, m_fSmall;
	int m_iFontesAlt;
	float m_flFontesEsc;
	int m_iTexBranca;

	float m_flEsc, m_flOpac;
	bool m_bMin;
	int m_iMargem;

	struct KillEntry_t
	{
		char szAtk[ 64 ], szVic[ 64 ], szWpn[ 48 ];
		Color corAtk, corVic;
		bool bHead, bInfeccao, bMeu;
		float flTime;
	};
	CUtlVector< KillEntry_t > m_Kills;
	int m_iHeadVictim;
	float m_flHeadTime;

	struct Evento_t
	{
		char szRotulo[ 48 ], szValor[ 24 ], szDetalhe[ 64 ];
		Color cor;
		float flTime;
	};
	CUtlVector< Evento_t > m_Eventos;
	int m_iDinheiroAnt;

	// Acerto
	float m_flHitTempo, m_flHitDur;
	bool m_bHitAbate, m_bHitCabeca;

	// Direção do dano
	struct DmgMark_t { Vector vecFrom; float flTime; };
	DmgMark_t m_Dmg[ 4 ];
	int m_iDmgNext;

	// Vida
	int m_iVidaAnt, m_iGhostDe;
	float m_flGhostTempo, m_flRegenTempo, m_flDanoTempo;
	int m_iColete;

	// Placar
	int m_iPlacarAnt;
	float m_flPlacarTempo;

	// Corrida
	int m_iDegrauAnt;
	float m_flDegrauTempo;
	char m_szDegrau[ 64 ];

	// Radar
	float m_flSpotted[ MAX_PLAYERS + 1 ];
	float m_flNextSpotCheck;

	// Granadas perto
	struct Granada_t { Vector vecPos; };
	Granada_t m_Gran[ MAX_GRANADAS ];
	int m_nGran;
	float m_flNextGran, m_flGranDesde;
};

DECLARE_HUDELEMENT( CHudTrokV2 );
DECLARE_HUD_MESSAGE( CHudTrokV2, Damage );
DECLARE_HUD_MESSAGE( CHudTrokV2, Battery );

CHudTrokV2::CHudTrokV2( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokV2" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	SetHiddenBits( 0 );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	m_fBig = m_fScore = m_fTimer = m_fMed = m_fText = m_fLabel = m_fName = m_fSmall = INVALID_FONT;
	m_iFontesAlt = 0;
	m_flFontesEsc = 0.0f;
	m_iTexBranca = -1;
	m_flEsc = 1.0f;
	m_flOpac = 1.0f;
	m_bMin = false;
	m_iMargem = 28;
	m_iColete = 0;
	s_pHudV2 = this;
	Reset();
}

void CHudTrokV2::Init( void )
{
	ListenForGameEvent( "player_death" );
	ListenForGameEvent( "player_hurt" );
	ListenForGameEvent( "trok_dano" );
	HOOK_HUD_MESSAGE( CHudTrokV2, Damage );
	HOOK_HUD_MESSAGE( CHudTrokV2, Battery );
	Reset();
}

void CHudTrokV2::Reset( void )
{
	m_Kills.RemoveAll();
	m_Eventos.RemoveAll();
	m_iHeadVictim = 0;
	m_flHeadTime = -100.0f;
	m_iDinheiroAnt = -1;
	m_flHitTempo = -100.0f;
	m_flHitDur = 0.0f;
	m_bHitAbate = m_bHitCabeca = false;
	for ( int i = 0; i < ARRAYSIZE( m_Dmg ); ++i )
		m_Dmg[ i ].flTime = -100.0f;
	m_iDmgNext = 0;
	m_iVidaAnt = -1;
	m_iGhostDe = 0;
	m_flGhostTempo = m_flRegenTempo = m_flDanoTempo = -100.0f;
	m_iPlacarAnt = -1;
	m_flPlacarTempo = -100.0f;
	m_iDegrauAnt = -1;
	m_flDegrauTempo = -100.0f;
	m_szDegrau[ 0 ] = 0;
	for ( int i = 0; i <= MAX_PLAYERS; ++i )
		m_flSpotted[ i ] = 0.0f;
	m_flNextSpotCheck = 0.0f;
	m_nGran = 0;
	m_flNextGran = 0.0f;
	m_flGranDesde = -100.0f;
}

void CHudTrokV2::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
	m_iFontesAlt = 0;	// recria as fontes
}

bool CHudTrokV2::ShouldDraw( void )
{
	static ConVarRef s_hud( "trok_hud" );
	if ( !TrokHudV2_Ativo() || ( s_hud.IsValid() && !s_hud.GetBool() ) || engine->IsLevelMainMenuBackground() )
		return false;
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;
	return CHudElement::ShouldDraw();
}

void CHudTrokV2::MsgFunc_Damage( bf_read &msg )
{
	msg.ReadByte();		// colete
	msg.ReadByte();		// dano
	msg.ReadLong();		// tipos de dano
	Vector vecFrom;
	vecFrom.x = msg.ReadFloat();
	vecFrom.y = msg.ReadFloat();
	vecFrom.z = msg.ReadFloat();
	if ( vecFrom == vec3_origin )
		return;
	m_Dmg[ m_iDmgNext ].vecFrom = vecFrom;
	m_Dmg[ m_iDmgNext ].flTime = gpGlobals->curtime;
	m_iDmgNext = ( m_iDmgNext + 1 ) % ARRAYSIZE( m_Dmg );
}

void CHudTrokV2::MsgFunc_Battery( bf_read &msg )
{
	m_iColete = MAX( 0, (int)msg.ReadShort() );
}

//-----------------------------------------------------------------------------
// Fontes e desenho básico
//-----------------------------------------------------------------------------
void CHudTrokV2::CriarFontes( void )
{
	if ( m_iFontesAlt == ScreenHeight() && m_flFontesEsc == m_flEsc )
		return;
	m_iFontesAlt = ScreenHeight();
	m_flFontesEsc = m_flEsc;

	struct Def_t { HFont *pFont; const char *pszNome; float flTall; int iPeso; };
	const Def_t defs[] =
	{
		{ &m_fBig,		"Barlow Condensed",				46.0f, 700 },
		{ &m_fScore,	"Barlow Condensed",				30.0f, 700 },
		{ &m_fTimer,	"Barlow Condensed SemiBold",	22.0f, 600 },
		{ &m_fMed,		"Barlow Condensed SemiBold",	18.0f, 600 },
		{ &m_fText,		"Barlow Condensed SemiBold",	15.0f, 600 },
		{ &m_fLabel,	"Barlow Condensed SemiBold",	11.0f, 600 },
		{ &m_fName,		"Barlow Medium",				13.0f, 500 },
		{ &m_fSmall,	"Barlow Medium",				12.0f, 500 },
	};
	for ( int i = 0; i < ARRAYSIZE( defs ); ++i )
	{
		if ( *defs[ i ].pFont == INVALID_FONT )
			*defs[ i ].pFont = surface()->CreateFont();
		surface()->SetFontGlyphSet( *defs[ i ].pFont, defs[ i ].pszNome, MAX( 6, S( defs[ i ].flTall ) ), defs[ i ].iPeso, 0, 0,
			ISurface::FONTFLAG_ANTIALIAS | ISurface::FONTFLAG_DROPSHADOW );
	}
}

// Texto letra a letra: espaçamento entre letras (px a 720) e, com bMono, algarismos em células
// de largura fixa (a do "0"), para os números não "pularem" quando mudam. y = linha de base.
int CHudTrokV2::TextoCore( HFont f, int x, int yBase, const Color &c, const wchar_t *wsz, int iAlinh, float flEsp, bool bMono, bool bDesenhar )
{
	if ( f == INVALID_FONT || !wsz || !wsz[ 0 ] )
		return 0;
	const int iEsp = (int)( flEsp * m_flEsc + 0.5f );
	const int iCel = bMono ? surface()->GetCharacterWidth( f, L'0' ) : 0;
	int w = 0;
	for ( const wchar_t *p = wsz; *p; ++p )
	{
		const bool bDig = bMono && *p >= L'0' && *p <= L'9';
		w += bDig ? iCel : surface()->GetCharacterWidth( f, *p );
		if ( p[ 1 ] )
			w += iEsp;
	}
	if ( !bDesenhar )
		return w;

	int px = ( iAlinh == AL_DIR ) ? x - w : ( iAlinh == AL_CENTRO ) ? x - w / 2 : x;
	const int py = yBase - surface()->GetFontAscent( f, L'0' );
	surface()->DrawSetTextFont( f );
	surface()->DrawSetTextColor( c );
	for ( const wchar_t *p = wsz; *p; ++p )
	{
		const int cw = surface()->GetCharacterWidth( f, *p );
		const bool bDig = bMono && *p >= L'0' && *p <= L'9';
		const int cel = bDig ? iCel : cw;
		surface()->DrawSetTextPos( px + ( cel - cw ) / 2, py );
		surface()->DrawUnicodeChar( *p );
		px += cel + iEsp;
	}
	return w;
}

int CHudTrokV2::Texto( HFont f, int x, int yBase, const Color &c, const char *psz, int iAlinh, float flEsp, bool bMono )
{
	wchar_t wsz[ 128 ];
	V_UTF8ToUnicode( psz, wsz, sizeof( wsz ) );
	return TextoCore( f, x, yBase, c, wsz, iAlinh, flEsp, bMono, true );
}

int CHudTrokV2::Larg( HFont f, const char *psz, float flEsp, bool bMono )
{
	wchar_t wsz[ 128 ];
	V_UTF8ToUnicode( psz, wsz, sizeof( wsz ) );
	return TextoCore( f, 0, 0, COR_BRANCO, wsz, AL_ESQ, flEsp, bMono, false );
}

void CHudTrokV2::Rect( int x, int y, int w, int h, const Color &c )
{
	if ( w <= 0 || h <= 0 || c.a() <= 0 )
		return;
	surface()->DrawSetColor( c );
	surface()->DrawFilledRect( x, y, x + w, y + h );
}

// Véu escuro com degradê (legibilidade sem caixa).
void CHudTrokV2::Veu( int x, int y, int w, int h, float a, int iLado )
{
	const int ia = Alfa( a );
	if ( ia <= 0 || w <= 0 || h <= 0 )
		return;
	surface()->DrawSetColor( COR_FUNDO );
	if ( iLado == 0 )
		surface()->DrawFilledRectFade( x, y, x + w, y + h, 0, ia, true );
	else if ( iLado == 1 )
		surface()->DrawFilledRectFade( x, y, x + w, y + h, ia, 0, true );
	else
	{
		const int q = w / 4;
		surface()->DrawFilledRectFade( x, y, x + q, y + h, 0, ia, true );
		surface()->DrawSetColor( Color( COR_FUNDO.r(), COR_FUNDO.g(), COR_FUNDO.b(), ia ) );
		surface()->DrawFilledRect( x + q, y, x + w - q, y + h );
		surface()->DrawSetColor( COR_FUNDO );
		surface()->DrawFilledRectFade( x + w - q, y, x + w, y + h, ia, 0, true );
	}
}

void CHudTrokV2::Poly( Vertex_t *v, int n, const Color &c )
{
	if ( c.a() <= 0 )
		return;
	if ( m_iTexBranca == -1 )
	{
		m_iTexBranca = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iTexBranca, "vgui/white", true, false );
	}
	surface()->DrawSetColor( c );
	surface()->DrawSetTexture( m_iTexBranca );
	surface()->DrawTexturedPolygon( n, v );
}

void CHudTrokV2::LinhaGrossa( float x1, float y1, float x2, float y2, float th, const Color &c )
{
	Vector2D d( x2 - x1, y2 - y1 );
	const float len = d.Length();
	if ( len < 0.01f )
		return;
	const Vector2D n( -d.y / len * th * 0.5f, d.x / len * th * 0.5f );
	Vertex_t v[ 4 ] =
	{
		Vertex_t( Vector2D( x1 + n.x, y1 + n.y ) ),
		Vertex_t( Vector2D( x2 + n.x, y2 + n.y ) ),
		Vertex_t( Vector2D( x2 - n.x, y2 - n.y ) ),
		Vertex_t( Vector2D( x1 - n.x, y1 - n.y ) ),
	};
	Poly( v, 4, c );
}

void CHudTrokV2::Arco( float cx, float cy, float r0, float r1, float a0, float a1, int iSeg, const Color &c )
{
	for ( int s = 0; s < iSeg; ++s )
	{
		const float b0 = a0 + ( a1 - a0 ) * s / iSeg;
		const float b1 = a0 + ( a1 - a0 ) * ( s + 1 ) / iSeg;
		Vertex_t v[ 4 ] =
		{
			Vertex_t( Vector2D( cx + sinf( b0 ) * r0, cy - cosf( b0 ) * r0 ) ),
			Vertex_t( Vector2D( cx + sinf( b0 ) * r1, cy - cosf( b0 ) * r1 ) ),
			Vertex_t( Vector2D( cx + sinf( b1 ) * r1, cy - cosf( b1 ) * r1 ) ),
			Vertex_t( Vector2D( cx + sinf( b1 ) * r0, cy - cosf( b1 ) * r0 ) ),
		};
		Poly( v, 4, c );
	}
}

void CHudTrokV2::Circulo( float cx, float cy, float r, const Color &c )
{
	const int N = 20;
	Vertex_t v[ N ];
	for ( int i = 0; i < N; ++i )
	{
		const float a = 2.0f * M_PI_F * i / N;
		v[ i ] = Vertex_t( Vector2D( cx + sinf( a ) * r, cy - cosf( a ) * r ) );
	}
	Poly( v, N, c );
}

// Tecla num quadradinho ("R", "E"). y = topo. Devolve a largura.
int CHudTrokV2::Tecla( int x, int y, const char *pszTecla, const Color &c )
{
	const int h = S( 18 );
	const int w = MAX( h, Larg( m_fLabel, pszTecla, 1.0f ) + S( 10 ) );
	surface()->DrawSetColor( c );
	surface()->DrawOutlinedRect( x, y, x + w, y + h );
	Texto( m_fLabel, x + w / 2, y + S( 13 ), c, pszTecla, AL_CENTRO, 1.0f );
	return w;
}

// Ícones de traço simples (granada, granada de luz, fumaça, outros).
void CHudTrokV2::IconeExplosivo( int iTipo, int x, int y, const Color &c )
{
	surface()->DrawSetColor( c );
	const int r = S( 6 );
	if ( iTipo == 0 )			// granada
	{
		surface()->DrawOutlinedCircle( x, y + S( 2 ), r, 16 );
		surface()->DrawFilledRect( x - S( 2 ), y - S( 7 ), x + S( 2 ), y - S( 4 ) );
		surface()->DrawLine( x + S( 2 ), y - S( 6 ), x + S( 6 ), y - S( 9 ) );
	}
	else if ( iTipo == 1 )		// luz
	{
		surface()->DrawOutlinedRect( x - S( 4 ), y - S( 6 ), x + S( 5 ), y + S( 8 ) );
		surface()->DrawFilledRect( x - S( 2 ), y - S( 9 ), x + S( 2 ), y - S( 6 ) );
		surface()->DrawLine( x - S( 4 ), y - S( 1 ), x + S( 4 ), y - S( 1 ) );
	}
	else if ( iTipo == 2 )		// fumaça
	{
		surface()->DrawOutlinedRect( x - S( 4 ), y - S( 6 ), x + S( 5 ), y + S( 8 ) );
		surface()->DrawLine( x - S( 4 ), y + S( 2 ), x + S( 4 ), y + S( 2 ) );
		surface()->DrawLine( x - S( 4 ), y - S( 2 ), x + S( 4 ), y - S( 2 ) );
	}
	else						// aparelho (mina, claymore, remota, torreta, foguete)
	{
		surface()->DrawOutlinedRect( x - S( 6 ), y - S( 4 ), x + S( 6 ), y + S( 5 ) );
		surface()->DrawFilledRect( x - S( 1 ), y - S( 8 ), x + S( 1 ), y - S( 4 ) );
	}
}

// Bordas da tela tingidas (vida baixa, zumbi, lento, dano).
void CHudTrokV2::Vinheta( const Color &c, int a )
{
	a = clamp( (int)( a * m_flOpac ), 0, 255 );
	if ( a <= 0 )
		return;
	const int w = ScreenWidth(), h = ScreenHeight();
	surface()->DrawSetColor( Color( c.r(), c.g(), c.b(), 255 ) );
	surface()->DrawFilledRectFade( 0, 0, w, h / 4, a, 0, false );
	surface()->DrawFilledRectFade( 0, h * 3 / 4, w, h, 0, a, false );
	surface()->DrawFilledRectFade( 0, 0, w / 5, h, a, 0, true );
	surface()->DrawFilledRectFade( w * 4 / 5, 0, w, h, 0, a, true );
}

Color CHudTrokV2::Accent( void ) const
{
	static ConVarRef s_destaque( "trok_ui_destaque" );
	int r = COR_DESTAQUE_PADRAO.r(), g = COR_DESTAQUE_PADRAO.g(), b = COR_DESTAQUE_PADRAO.b();
	if ( s_destaque.IsValid() )
		sscanf( s_destaque.GetString(), "%d %d %d", &r, &g, &b );
	return Color( clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
}

// Cor de cada jogador: no Surto humano azul e zumbi verde; com times o seu azul e o outro
// vermelho; sem times você azul e o resto vermelho.
Color CHudTrokV2::CorJogador( int iPlayer ) const
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal || !g_PR || iPlayer <= 0 )
		return COR_BRANCO;
	if ( TrokSurto_Ativo() )
		return g_PR->GetTeam( iPlayer ) == TEAM_TROK_ZUMBIS ? COR_ZUMBI : COR_ALIADO;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
		return g_PR->GetTeam( iPlayer ) == pLocal->GetTeamNumber() ? COR_ALIADO : COR_INIMIGO;
	return iPlayer == pLocal->entindex() ? COR_ALIADO : COR_INIMIGO;
}

bool CHudTrokV2::IsAlly( C_BasePlayer *pLocal, int iPlayer ) const
{
	if ( !g_PR || iPlayer <= 0 || !( HL2MPRules() && HL2MPRules()->IsTeamplay() ) )
		return false;
	return g_PR->GetTeam( iPlayer ) == pLocal->GetTeamNumber();
}

// "trok_fuzil" / "weapon_trok_fuzil" -> nome do script em caixa alta. Arma branca = FACA.
void CHudTrokV2::NomeArma( const char *pszName, char *pszOut, int iOutSize )
{
	char szClass[ 64 ];
	if ( V_strnicmp( pszName, "weapon_", 7 ) == 0 )
		V_strncpy( szClass, pszName, sizeof( szClass ) );
	else
		V_snprintf( szClass, sizeof( szClass ), "weapon_%s", pszName );

	if ( V_stricmp( szClass, "weapon_crowbar" ) == 0 || V_stricmp( szClass, "weapon_stunstick" ) == 0 )
	{
		V_strncpy( pszOut, "FACA", iOutSize );
		return;
	}
	if ( V_stricmp( szClass, "weapon_trok_garras" ) == 0 )
	{
		V_strncpy( pszOut, "GARRAS", iOutSize );
		return;
	}

	const char *pszPrint = pszName;
	WEAPON_FILE_INFO_HANDLE h = LookupWeaponInfoSlot( szClass );
	FileWeaponInfo_t *pInfo = GetFileWeaponInfoFromHandle( h );
	if ( pInfo && pInfo->szPrintName[ 0 ] && pInfo->szPrintName[ 0 ] != '#' )
		pszPrint = pInfo->szPrintName;

	wchar_t wsz[ 64 ];
	V_UTF8ToUnicode( pszPrint, wsz, sizeof( wsz ) );
	for ( wchar_t *p = wsz; *p; ++p )
		*p = (wchar_t)towupper( *p );
	V_UnicodeToUTF8( wsz, pszOut, iOutSize );
}

static void CaixaAlta( const char *pszIn, char *pszOut, int iOutSize )
{
	wchar_t wsz[ 64 ];
	V_UTF8ToUnicode( pszIn, wsz, sizeof( wsz ) );
	for ( wchar_t *p = wsz; *p; ++p )
		*p = (wchar_t)towupper( *p );
	V_UnicodeToUTF8( wsz, pszOut, iOutSize );
}

// Tecla ligada a um comando ("+reload" -> "R"); pszPadrao se não achar.
static void TeclaDe( const char *pszCmd, const char *pszPadrao, char *pszOut, int iOutSize )
{
	const char *psz = engine->Key_LookupBinding( pszCmd );
	V_strncpy( pszOut, ( psz && psz[ 0 ] ) ? psz : pszPadrao, iOutSize );
	V_strupr( pszOut );
}

// "1250" -> "1.250"
static void Milhar( int v, char *pszOut, int iOutSize )
{
	char sz[ 32 ];
	V_snprintf( sz, sizeof( sz ), "%d", abs( v ) );
	const int n = V_strlen( sz );
	char szR[ 48 ];
	int j = 0;
	if ( v < 0 )
		szR[ j++ ] = '-';
	for ( int i = 0; i < n; ++i )
	{
		szR[ j++ ] = sz[ i ];
		if ( ( n - i - 1 ) % 3 == 0 && i != n - 1 )
			szR[ j++ ] = '.';
	}
	szR[ j ] = 0;
	V_strncpy( pszOut, szR, iOutSize );
}

//-----------------------------------------------------------------------------
// Eventos
//-----------------------------------------------------------------------------
void CHudTrokV2::Evento( const char *pszRotulo, const char *pszValor, const char *pszDetalhe, const Color &cor )
{
	Evento_t e;
	V_memset( &e, 0, sizeof( e ) );
	V_strncpy( e.szRotulo, pszRotulo ? pszRotulo : "", sizeof( e.szRotulo ) );
	V_strncpy( e.szValor, pszValor ? pszValor : "", sizeof( e.szValor ) );
	V_strncpy( e.szDetalhe, pszDetalhe ? pszDetalhe : "", sizeof( e.szDetalhe ) );
	e.cor = cor.a() > 0 ? cor : COR_BRANCO;
	e.flTime = gpGlobals->curtime;
	m_Eventos.AddToTail( e );
	while ( m_Eventos.Count() > MAX_EVENTOS )
		m_Eventos.Remove( 0 );
}

void CHudTrokV2::FireGameEvent( IGameEvent *event )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	const int iLocal = pLocal ? pLocal->entindex() : 0;
	const char *pszName = event->GetName();

	if ( !V_strcmp( pszName, "trok_dano" ) )
	{
		const int iVic = engine->GetPlayerForUserID( event->GetInt( "vitima" ) );
		const int iAtk = engine->GetPlayerForUserID( event->GetInt( "atacante" ) );
		const bool bHead = event->GetInt( "parte" ) == HITGROUP_HEAD;
		if ( iAtk > 0 && iAtk <= MAX_PLAYERS )
			m_flSpotted[ iAtk ] = MAX( m_flSpotted[ iAtk ], gpGlobals->curtime + 2.0f );
		if ( event->GetInt( "vida" ) <= 0 && bHead )
		{
			m_iHeadVictim = iVic;
			m_flHeadTime = gpGlobals->curtime;
		}
		if ( iLocal && iAtk == iLocal && bHead )
			m_bHitCabeca = true;
		return;
	}

	if ( !V_strcmp( pszName, "player_hurt" ) )
	{
		const int iAtk = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		const int iVic = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		if ( !iLocal || iAtk != iLocal || iVic == iLocal || iVic <= 0 )
			return;
		static ConVarRef s_tempo( "trok_hitmarker_tempo" );
		const float flBase = s_tempo.IsValid() ? s_tempo.GetFloat() : 0.18f;
		const bool bNovo = gpGlobals->curtime - m_flHitTempo > 0.05f;
		m_bHitAbate = event->GetInt( "health" ) <= 0;
		if ( bNovo && !m_bHitAbate )
			m_bHitCabeca = false;
		m_flHitDur = flBase * ( m_bHitAbate ? 2.3f : 1.1f );
		m_flHitTempo = gpGlobals->curtime;
		return;
	}

	if ( !V_strcmp( pszName, "player_death" ) )
	{
		const int iVic = engine->GetPlayerForUserID( event->GetInt( "userid" ) );
		const int iAtk = engine->GetPlayerForUserID( event->GetInt( "attacker" ) );
		const bool bHead = ( iVic == m_iHeadVictim ) && ( gpGlobals->curtime - m_flHeadTime < 0.5f );

		KillEntry_t k;
		V_memset( &k, 0, sizeof( k ) );
		k.bHead = bHead;
		k.flTime = gpGlobals->curtime;
		k.corAtk = CorJogador( iAtk );
		k.corVic = CorJogador( iVic );
		k.bMeu = iLocal && ( iAtk == iLocal || iVic == iLocal );
		if ( g_PR && iAtk > 0 && iAtk != iVic )
			V_strncpy( k.szAtk, g_PR->GetPlayerName( iAtk ), sizeof( k.szAtk ) );
		if ( g_PR && iVic > 0 )
			V_strncpy( k.szVic, g_PR->GetPlayerName( iVic ), sizeof( k.szVic ) );
		NomeArma( event->GetString( "weapon" ), k.szWpn, sizeof( k.szWpn ) );
		// Surto: zumbi pegando humano é infecção (a vítima ainda é humana no evento).
		if ( TrokSurto_Ativo() && g_PR && iAtk > 0 && g_PR->GetTeam( iAtk ) == TEAM_TROK_ZUMBIS && g_PR->GetTeam( iVic ) == TEAM_TROK_HUMANOS )
		{
			k.bInfeccao = true;
			V_strncpy( k.szWpn, "INFECTOU", sizeof( k.szWpn ) );
			k.corVic = COR_ALIADO;
		}
		m_Kills.AddToTail( k );
		while ( m_Kills.Count() > MAX_KILLFEED )
			m_Kills.Remove( 0 );

		// Você abateu alguém: evento à direita da mira.
		if ( iLocal && iAtk == iLocal && iVic != iLocal && trok_hud_eventos.GetBool() )
		{
			if ( k.bInfeccao )
				Evento( "INFECTOU", "+1", k.szVic, COR_ZUMBI );
			else if ( TrokSurto_Ativo() && g_PR && g_PR->GetTeam( iVic ) == TEAM_TROK_ZUMBIS )
				Evento( "ZUMBI ABATIDO", NULL, k.szVic, COR_BRANCO );
			else
				Evento( "ABATE", NULL, k.szVic, COR_BRANCO );
			if ( bHead )
			{
				Evento( "NA CABEÇA", NULL, NULL, Accent() );
				m_bHitCabeca = true;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Estado por quadro
//-----------------------------------------------------------------------------
void CHudTrokV2::OnThink( void )
{
	if ( GetWide() != ScreenWidth() || GetTall() != ScreenHeight() )
		SetSize( ScreenWidth(), ScreenHeight() );
	m_flEsc = ScreenHeight() / 720.0f * trok_hud_escala.GetFloat();
	m_flOpac = trok_hud_opacidade.GetFloat();
	m_bMin = trok_hud_minimalista.GetBool();
	m_iMargem = (int)( MAX( 0.0f, trok_hud_margem.GetFloat() ) * ScreenHeight() / 720.0f + 0.5f );

	static ConVarRef s_life( "trok_hud_killfeed_tempo" );
	const float flLife = s_life.IsValid() ? s_life.GetFloat() : 6.0f;
	for ( int i = m_Kills.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime - m_Kills[ i ].flTime > flLife || gpGlobals->curtime < m_Kills[ i ].flTime )
			m_Kills.Remove( i );
	}
	for ( int i = m_Eventos.Count() - 1; i >= 0; --i )
	{
		if ( gpGlobals->curtime - m_Eventos[ i ].flTime > 2.0f || gpGlobals->curtime < m_Eventos[ i ].flTime )
			m_Eventos.Remove( i );
	}

	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;
	const int iLocal = pLocal->entindex();

	// Vida: rastro do dano e regeneração.
	const int iVida = pLocal->IsAlive() ? MAX( 0, pLocal->GetHealth() ) : 0;
	if ( m_iVidaAnt >= 0 && pLocal->IsAlive() )
	{
		if ( iVida < m_iVidaAnt )
		{
			if ( gpGlobals->curtime - m_flGhostTempo > 0.8f )
				m_iGhostDe = m_iVidaAnt;
			m_flGhostTempo = gpGlobals->curtime;
			m_flDanoTempo = gpGlobals->curtime;
		}
		else if ( iVida > m_iVidaAnt )
			m_flRegenTempo = gpGlobals->curtime;
	}
	m_iVidaAnt = pLocal->IsAlive() ? iVida : -1;
	if ( !pLocal->IsAlive() )
		m_iColete = 0;

	// Créditos ganhos: vão no último evento (ou num novo).
	CTrokPlacarExtra *pExtra = TrokPlacarExtra();
	if ( pExtra && pExtra->IsEconomia() )
	{
		const int iDin = pExtra->GetDinheiro( iLocal );
		if ( m_iDinheiroAnt >= 0 && iDin > m_iDinheiroAnt && trok_hud_eventos.GetBool() )
		{
			char szVal[ 24 ], szMil[ 16 ];
			Milhar( iDin - m_iDinheiroAnt, szMil, sizeof( szMil ) );
			V_snprintf( szVal, sizeof( szVal ), "+$%s", szMil );
			bool bJunto = false;
			for ( int i = m_Eventos.Count() - 1; i >= 0; --i )
			{
				if ( gpGlobals->curtime - m_Eventos[ i ].flTime < 0.6f && !m_Eventos[ i ].szValor[ 0 ] )
				{
					V_strncpy( m_Eventos[ i ].szValor, szVal, sizeof( m_Eventos[ i ].szValor ) );
					bJunto = true;
					break;
				}
			}
			if ( !bJunto )
				Evento( "CRÉDITOS", szVal, NULL, COR_CINZA2 );
		}
		m_iDinheiroAnt = iDin;
	}
	else
		m_iDinheiroAnt = -1;

	// Placar do seu lado: clareia um instante quando sobe.
	int iPlacar = g_PR ? g_PR->GetPlayerScore( iLocal ) : 0;
	if ( HL2MPRules() && HL2MPRules()->IsTeamplay() && !TrokSurto_Ativo() )
	{
		C_Team *pTeam = GetGlobalTeam( pLocal->GetTeamNumber() );
		iPlacar = pTeam ? pTeam->Get_Score() : 0;
	}
	if ( m_iPlacarAnt >= 0 && iPlacar > m_iPlacarAnt )
		m_flPlacarTempo = gpGlobals->curtime;
	m_iPlacarAnt = iPlacar;

	// Corrida Armada: subiu de degrau.
	CTrokCorridaEstado *pCorrida = TrokCorrida_Ativo() ? TrokCorrida_Estado() : NULL;
	if ( pCorrida )
	{
		const int iDeg = pCorrida->GetDegrau( iLocal );
		if ( m_iDegrauAnt >= 0 && iDeg > m_iDegrauAnt )
		{
			m_flDegrauTempo = gpGlobals->curtime;
			const TrokCorridaEscada_t *pEsc = TrokCorrida_Escada( pCorrida->GetMeta() );
			char szNome[ 48 ] = "";
			if ( pEsc && iDeg >= 0 && iDeg < pEsc->nDegraus )
				CaixaAlta( pEsc->degraus[ iDeg ].szNome, szNome, sizeof( szNome ) );
			V_snprintf( m_szDegrau, sizeof( m_szDegrau ), szNome[ 0 ] ? "DEGRAU %d  ·  %s" : "DEGRAU %d", iDeg + 1, szNome );
		}
		m_iDegrauAnt = iDeg;
	}
	else
		m_iDegrauAnt = -1;
}

//-----------------------------------------------------------------------------
// Desenho
//-----------------------------------------------------------------------------
void CHudTrokV2::Paint( void )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;
	CriarFontes();

	const bool bVivo = pLocal->IsAlive() && pLocal->GetTeamNumber() != TEAM_SPECTATOR;
	if ( bVivo )
	{
		PaintVinhetas( pLocal );
		PaintRadar( pLocal );
		PaintBussola( pLocal );
	}
	PaintPlacar( pLocal );
	PaintKillfeed();
	if ( !bVivo )
		return;
	PaintVitais( pLocal );
	PaintArma( pLocal );
	PaintDegraus( pLocal );
	PaintDano( pLocal );
	PaintMira( pLocal );
	PaintHitmarker();
	PaintGranadas( pLocal );
	PaintEventos();
	PaintProtecao( pLocal );
}

void CHudTrokV2::PaintVinhetas( C_BasePlayer *pLocal )
{
	const bool bZumbi = TrokSurto_IsZumbi( pLocal );
	const float t = gpGlobals->curtime;

	// Pouca vida: vermelho pulsando de leve (trok_hud_vida_baixa).
	static ConVarRef s_baixa( "trok_hud_vida_baixa" );
	const float flLimite = s_baixa.IsValid() ? s_baixa.GetFloat() : 0.35f;
	if ( flLimite > 0.0f && !bZumbi )
	{
		const float flFrac = pLocal->GetHealth() / (float)MAX( 1, pLocal->GetMaxHealth() );
		if ( flFrac < flLimite )
		{
			const float k = 1.0f - flFrac / flLimite;
			Vinheta( Color( 170, 20, 20, 255 ), (int)( k * ( 95.0f + 25.0f * sinf( t * 4.0f ) ) ) );
		}
	}
	if ( bZumbi )
		Vinheta( Color( 79, 138, 44, 255 ), 50 );

	// Acabou de levar dano: um clarão vermelho curto nas bordas.
	const float flDano = t - m_flDanoTempo;
	if ( flDano >= 0.0f && flDano < 0.6f )
		Vinheta( Color( 180, 20, 20, 255 ), (int)( 90.0f * ( 1.0f - flDano / 0.6f ) ) );

	// Lento (golpe do Splicer, nuvem tóxica): bordas azul-aço.
	CTrokSurtoEstado *pSurto = TrokSurto_Ativo() ? TrokSurto_Estado() : NULL;
	if ( pSurto && pSurto->GetLentoAte( pLocal->entindex() ) > t )
		Vinheta( COR_COLETE, (int)( 55.0f + 10.0f * sinf( t * 3.0f ) ) );
}

// Radar quadrado que gira com a visão: aliados sempre, inimigos quando vistos ou atirando.
void CHudTrokV2::UpdateSpotted( C_BasePlayer *pLocal )
{
	if ( gpGlobals->curtime < m_flNextSpotCheck )
		return;
	m_flNextSpotCheck = gpGlobals->curtime + 0.2f;
	const Vector eye = pLocal->EyePosition();
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !p->IsAlive() || IsAlly( pLocal, i ) )
			continue;
		trace_t tr;
		UTIL_TraceLine( eye, p->EyePosition(), MASK_VISIBLE, pLocal, COLLISION_GROUP_NONE, &tr );
		if ( tr.fraction > 0.98f || tr.m_pEnt == p )
			m_flSpotted[ i ] = MAX( m_flSpotted[ i ], gpGlobals->curtime + 1.0f );
	}
}

void CHudTrokV2::PaintRadar( C_BasePlayer *pLocal )
{
	static ConVarRef s_radar( "trok_hud_radar" );
	static ConVarRef s_alcance( "trok_hud_radar_alcance" );
	if ( ( s_radar.IsValid() && !s_radar.GetBool() ) || !g_PR )
		return;
	UpdateSpotted( pLocal );

	const int s = S( m_bMin ? 140 : 168 );
	const int x0 = m_iMargem, y0 = m_iMargem;
	const int cx = x0 + s / 2, cy = y0 + s / 2;
	const float flRange = MAX( 200.0f, s_alcance.IsValid() ? s_alcance.GetFloat() : 1600.0f );

	Rect( x0, y0, s, s, A( Color( 11, 13, 16, 133 ) ) );
	surface()->DrawSetColor( A( Color( 255, 255, 255, 26 ) ) );
	surface()->DrawOutlinedRect( x0, y0, x0 + s, y0 + s );
	// cantos marcados
	const int k = S( 10 ), th = S1( 1 );
	const Color cCanto = A( Color( 255, 255, 255, 115 ) );
	Rect( x0, y0, k, th, cCanto ); Rect( x0, y0, th, k, cCanto );
	Rect( x0 + s - k, y0, k, th, cCanto ); Rect( x0 + s - th, y0, th, k, cCanto );
	Rect( x0, y0 + s - th, k, th, cCanto ); Rect( x0, y0 + s - k, th, k, cCanto );
	Rect( x0 + s - k, y0 + s - th, k, th, cCanto ); Rect( x0 + s - th, y0 + s - k, th, k, cCanto );
	// anéis e cone da visão
	surface()->DrawSetColor( A( Color( 255, 255, 255, 15 ) ) );
	surface()->DrawOutlinedCircle( cx, cy, (int)( s * 0.22f ), 40 );
	surface()->DrawOutlinedCircle( cx, cy, (int)( s * 0.44f ), 56 );
	{
		const float r = s * 0.48f, a = 0.55f;
		Vertex_t v[ 3 ] =
		{
			Vertex_t( Vector2D( cx, cy ) ),
			Vertex_t( Vector2D( cx + sinf( -a ) * r, cy - cosf( -a ) * r ) ),
			Vertex_t( Vector2D( cx + sinf( a ) * r, cy - cosf( a ) * r ) ),
		};
		Poly( v, 3, A( Color( 236, 238, 241, 16 ) ) );
	}

	QAngle angView = pLocal->EyeAngles();
	angView.x = angView.z = 0.0f;
	Vector fwd, right;
	AngleVectors( angView, &fwd, &right, NULL );

	const bool bSurto = TrokSurto_Ativo();
	const bool bEuZumbi = TrokSurto_IsZumbi( pLocal );
	const int dot = MAX( 3, S( 6 ) );
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !p->IsAlive() )
			continue;
		const bool bAlly = bSurto ? ( TrokSurto_IsZumbi( p ) == bEuZumbi ) : IsAlly( pLocal, i );
		if ( !bAlly && gpGlobals->curtime > m_flSpotted[ i ] )
			continue;
		const Vector rel = p->GetAbsOrigin() - pLocal->GetAbsOrigin();
		const float rx = DotProduct( rel, right ) / flRange;
		const float ry = DotProduct( rel, fwd ) / flRange;
		if ( fabsf( rx ) > 1.0f || fabsf( ry ) > 1.0f )
			continue;
		const float px = cx + rx * ( s / 2 - dot );
		const float py = cy - ry * ( s / 2 - dot );
		if ( bAlly )
		{
			const Color c = A( bEuZumbi ? COR_ZUMBI : COR_ALIADO );
			const float t = S( 4 );
			Vertex_t v[ 3 ] = { Vertex_t( Vector2D( px, py - t ) ), Vertex_t( Vector2D( px + t * 0.9f, py + t * 0.75f ) ), Vertex_t( Vector2D( px - t * 0.9f, py + t * 0.75f ) ) };
			Poly( v, 3, c );
		}
		else
		{
			// some aos poucos no último meio segundo
			const float flResta = m_flSpotted[ i ] - gpGlobals->curtime;
			const float a = clamp( flResta / 0.5f, 0.2f, 1.0f );
			const Color c = ( bSurto && TrokSurto_IsZumbi( p ) ) ? COR_ZUMBI : COR_INIMIGO;
			Rect( (int)px - dot / 2, (int)py - dot / 2, dot, dot, A( c, a ) );
		}
	}

	// Você: seta
	{
		const float t = S( 7 );
		Vertex_t v[ 4 ] =
		{
			Vertex_t( Vector2D( cx, cy - t ) ),
			Vertex_t( Vector2D( cx + t * 0.7f, cy + t * 0.7f ) ),
			Vertex_t( Vector2D( cx, cy + t * 0.35f ) ),
			Vertex_t( Vector2D( cx - t * 0.7f, cy + t * 0.7f ) ),
		};
		Poly( v, 4, A( COR_BRANCO ) );
	}

	// "N" girando na borda: o norte do mapa (+Y) visto da sua frente.
	const float nx = DotProduct( Vector( 0, 1, 0 ), right ), ny = DotProduct( Vector( 0, 1, 0 ), fwd );
	const float r = s / 2 - S( 10 );
	const int lx = clamp( (int)( cx + nx * r ), x0 + S( 9 ), x0 + s - S( 9 ) );
	const int ly = clamp( (int)( cy - ny * r ), y0 + S( 11 ), y0 + s - S( 5 ) );
	Texto( m_fLabel, lx, ly + S( 4 ), A( Accent() ), "N", AL_CENTRO );
}

// Rumo da bússola (0 = norte do mapa, +Y; horário).
static float RumoDe( float flYaw )
{
	return AngleNormalizePositive( 90.0f - flYaw );
}

void CHudTrokV2::PaintBussola( C_BasePlayer *pLocal )
{
	if ( m_bMin || !trok_hud_bussola.GetBool() )
		return;
	const int cx = ScreenWidth() / 2;
	const int y = S( 14 );
	const float largura = (float)S( 400 );
	const float graus = 150.0f, meio = graus * 0.5f;
	Veu( cx - (int)largura / 2 - S( 30 ), y - S( 6 ), (int)largura + S( 60 ), S( 40 ), 0.28f, 2 );

	const float rumo = RumoDe( pLocal->EyeAngles().y );
	static const char *s_nomes[ 8 ] = { "N", "NE", "L", "SE", "S", "SO", "O", "NO" };
	const int iPrimeiro = (int)ceilf( ( rumo - meio ) / 15.0f );
	for ( int i = iPrimeiro; i * 15.0f <= rumo + meio; ++i )
	{
		const float a = i * 15.0f;
		const float d = a - rumo;
		const int x = cx + (int)( d / meio * largura * 0.5f );
		const float borda = 1.0f - powf( fabsf( d ) / meio, 3.0f );
		const int ang = ( ( i * 15 ) % 360 + 360 ) % 360;
		if ( ang % 45 == 0 )
		{
			const Color c = ang == 0 ? Accent() : COR_BRANCO;
			Texto( m_fText, x, y + S( 14 ), A( c, 0.9f * borda ), s_nomes[ ang / 45 ], AL_CENTRO, 1.0f );
			Rect( x, y + S( 20 ), S1( 1 ), S( 7 ), A( COR_BRANCO, 0.7f * borda ) );
		}
		else
			Rect( x, y + S( 22 ), S1( 1 ), S( 5 ), A( COR_BRANCO, 0.35f * borda ) );
	}

	// Dano: marca vermelha no rumo de onde veio.
	for ( int k = 0; k < ARRAYSIZE( m_Dmg ); ++k )
	{
		const float flAge = gpGlobals->curtime - m_Dmg[ k ].flTime;
		if ( flAge < 0.0f || flAge > 1.3f )
			continue;
		const float a = flAge < 0.8f ? 1.0f : ( 1.3f - flAge ) / 0.5f;
		const Vector rel = m_Dmg[ k ].vecFrom - pLocal->EyePosition();
		const float flRumoDano = RumoDe( RAD2DEG( atan2f( rel.y, rel.x ) ) );
		const float d = AngleNormalize( flRumoDano - rumo );
		const int x = cx + (int)( clamp( d / meio, -1.0f, 1.0f ) * largura * 0.5f );
		Rect( x - S( 10 ), y + S( 29 ), S( 20 ), S1( 3 ), A( COR_INIMIGO, a ) );
	}

	// Centro: traço e número do rumo
	Rect( cx, y + S( 18 ), S1( 1 ), S( 13 ), A( Accent() ) );
	char szRumo[ 8 ];
	V_snprintf( szRumo, sizeof( szRumo ), "%03d", ( (int)( rumo + 0.5f ) ) % 360 );
	Texto( m_fLabel, cx, y + S( 44 ), A( COR_CINZA2 ), szRumo, AL_CENTRO, 0.0f, true );
}

//-----------------------------------------------------------------------------
// Placar do topo
//-----------------------------------------------------------------------------
void CHudTrokV2::PaintPlacar( C_BasePlayer *pLocal )
{
	if ( !g_PR )
		return;
	const bool bBussola = !m_bMin && trok_hud_bussola.GetBool() && pLocal->IsAlive();
	const int y = bBussola ? S( 66 ) : S( 20 );
	if ( TrokSurto_Ativo() )
		PaintPlacarSurto( pLocal, y );
	else if ( TrokCorrida_Ativo() && TrokCorrida_Estado() )
		PaintPlacarCorrida( pLocal, y );
	else if ( HL2MPRules() && HL2MPRules()->IsTeamplay() )
		PaintPlacarEquipe( pLocal, y );
	else
		PaintPlacarSolo( pLocal, y );
}

// Tempo do mapa "07:42" (vazio sem limite).
static void TempoMapa( char *psz, int iSize )
{
	psz[ 0 ] = 0;
	const float flLeft = HL2MPRules() ? HL2MPRules()->GetMapRemainingTime() : 0.0f;
	if ( flLeft > 0.0f )
	{
		const int iSec = (int)flLeft;
		V_snprintf( psz, iSize, "%02d:%02d", iSec / 60, iSec % 60 );
	}
}

static int LimiteAbates( void )
{
	static ConVarRef s_fraglimit( "mp_fraglimit" );
	return s_fraglimit.IsValid() ? s_fraglimit.GetInt() : 0;
}

void CHudTrokV2::PaintPlacarEquipe( C_BasePlayer *pLocal, int y )
{
	const int cx = ScreenWidth() / 2;
	int iMyTeam = pLocal->GetTeamNumber();
	if ( iMyTeam != TEAM_COMBINE && iMyTeam != TEAM_REBELS )
		iMyTeam = TEAM_COMBINE;
	const int iOther = ( iMyTeam == TEAM_COMBINE ) ? TEAM_REBELS : TEAM_COMBINE;
	C_Team *pMine = GetGlobalTeam( iMyTeam );
	C_Team *pOther = GetGlobalTeam( iOther );
	const int iMine = pMine ? pMine->Get_Score() : 0;
	const int iTheirs = pOther ? pOther->Get_Score() : 0;
	const int iLimit = LimiteAbates();
	const float flRef = iLimit > 0 ? (float)iLimit : (float)MAX( 1, MAX( iMine, iTheirs ) );

	Veu( cx - S( 200 ), y - S( 4 ), S( 400 ), S( 44 ), 0.32f, 2 );
	char szTime[ 16 ];
	TempoMapa( szTime, sizeof( szTime ) );
	if ( szTime[ 0 ] )
		Texto( m_fTimer, cx, y + S( 23 ), A( COR_BRANCO ), szTime, AL_CENTRO, 0.0f, true );

	const int bw = S( 104 ), gap = S( 44 ), bh = S1( 2 );
	char szNum[ 16 ];
	// nós (esquerda): clareia um instante quando sobe
	const float flPulso = clamp( 1.0f - ( gpGlobals->curtime - m_flPlacarTempo ) / 0.6f, 0.0f, 1.0f );
	const Color cNos( (int)Lerp( flPulso, (float)COR_ALIADO.r(), (float)COR_BRANCO.r() ), (int)Lerp( flPulso, (float)COR_ALIADO.g(), (float)COR_BRANCO.g() ),
		(int)Lerp( flPulso, (float)COR_ALIADO.b(), (float)COR_BRANCO.b() ), 255 );
	V_snprintf( szNum, sizeof( szNum ), "%d", iMine );
	Texto( m_fScore, cx - gap - S( 10 ), y + S( 26 ), A( cNos ), szNum, AL_DIR, 0.0f, true );
	Rect( cx - gap - bw, y + S( 33 ), bw, bh, A( Color( 255, 255, 255, 36 ) ) );
	const int wNos = (int)( bw * clamp( iMine / flRef, 0.0f, 1.0f ) );
	Rect( cx - gap - wNos, y + S( 33 ), wNos, bh, A( COR_ALIADO ) );
	// eles (direita)
	V_snprintf( szNum, sizeof( szNum ), "%d", iTheirs );
	Texto( m_fScore, cx + gap + S( 10 ), y + S( 26 ), A( COR_INIMIGO ), szNum, AL_ESQ, 0.0f, true );
	Rect( cx + gap, y + S( 33 ), bw, bh, A( Color( 255, 255, 255, 36 ) ) );
	Rect( cx + gap, y + S( 33 ), (int)( bw * clamp( iTheirs / flRef, 0.0f, 1.0f ) ), bh, A( COR_INIMIGO ) );

	if ( !m_bMin )
	{
		char szLabel[ 64 ];
		if ( iLimit > 0 )
			V_snprintf( szLabel, sizeof( szLabel ), "EQUIPE  ·  %d PARA VENCER", iLimit );
		else
			V_strncpy( szLabel, "EQUIPE", sizeof( szLabel ) );
		Texto( m_fLabel, cx, y + S( 54 ), A( COR_CINZA3 ), szLabel, AL_CENTRO, 1.6f );
	}
}

void CHudTrokV2::PaintPlacarSolo( C_BasePlayer *pLocal, int y )
{
	const int cx = ScreenWidth() / 2;
	const int iMine = g_PR->GetPlayerScore( pLocal->entindex() );
	int iTheirs = 0, iPos = 1, iTotal = 1;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( i == pLocal->entindex() || !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) == TEAM_SPECTATOR )
			continue;
		iTheirs = MAX( iTheirs, g_PR->GetPlayerScore( i ) );
		++iTotal;
		if ( g_PR->GetPlayerScore( i ) > iMine )
			++iPos;
	}
	const int iLimit = LimiteAbates();
	const float flRef = iLimit > 0 ? (float)iLimit : (float)MAX( 1, MAX( iMine, iTheirs ) );

	Veu( cx - S( 200 ), y - S( 4 ), S( 400 ), S( 44 ), 0.32f, 2 );
	char szTime[ 16 ];
	TempoMapa( szTime, sizeof( szTime ) );
	if ( szTime[ 0 ] )
		Texto( m_fTimer, cx, y + S( 23 ), A( COR_BRANCO ), szTime, AL_CENTRO, 0.0f, true );

	const int bw = S( 104 ), gap = S( 44 ), bh = S1( 2 );
	char szNum[ 16 ];
	V_snprintf( szNum, sizeof( szNum ), "%d", iMine );
	Texto( m_fScore, cx - gap - S( 10 ), y + S( 26 ), A( iPos == 1 ? Accent() : COR_ALIADO ), szNum, AL_DIR, 0.0f, true );
	Rect( cx - gap - bw, y + S( 33 ), bw, bh, A( Color( 255, 255, 255, 36 ) ) );
	const int wNos = (int)( bw * clamp( iMine / flRef, 0.0f, 1.0f ) );
	Rect( cx - gap - wNos, y + S( 33 ), wNos, bh, A( COR_ALIADO ) );
	V_snprintf( szNum, sizeof( szNum ), "%d", iTheirs );
	Texto( m_fScore, cx + gap + S( 10 ), y + S( 26 ), A( COR_INIMIGO ), szNum, AL_ESQ, 0.0f, true );
	Rect( cx + gap, y + S( 33 ), bw, bh, A( Color( 255, 255, 255, 36 ) ) );
	Rect( cx + gap, y + S( 33 ), (int)( bw * clamp( iTheirs / flRef, 0.0f, 1.0f ) ), bh, A( COR_INIMIGO ) );

	char szLabel[ 64 ];
	if ( iLimit > 0 )
		V_snprintf( szLabel, sizeof( szLabel ), "SOLO  ·  %dº DE %d  ·  %d PARA VENCER", iPos, iTotal, iLimit );
	else
		V_snprintf( szLabel, sizeof( szLabel ), "SOLO  ·  %dº DE %d", iPos, iTotal );
	Texto( m_fLabel, cx, y + S( 54 ), A( iPos == 1 ? Accent() : COR_CINZA3 ), szLabel, AL_CENTRO, 1.6f );
}

// Surto: relógio da fase no meio (âmbar pulsando no alarme), humanos e zumbis dos lados, a frase
// do momento embaixo; no fim da rodada, o vencedor no meio da tela.
void CHudTrokV2::PaintPlacarSurto( C_BasePlayer *pLocal, int y )
{
	CTrokSurtoEstado *pEstado = TrokSurto_Estado();
	if ( !pEstado )
		return;
	const int cx = ScreenWidth() / 2;
	const float flLeft = MAX( 0.0f, pEstado->GetFimFase() - gpGlobals->curtime );
	char szTime[ 16 ];
	V_snprintf( szTime, sizeof( szTime ), "%d:%02d", (int)flLeft / 60, (int)flLeft % 60 );

	Veu( cx - S( 230 ), y - S( 6 ), S( 460 ), S( 50 ), 0.34f, 2 );
	const bool bAlarme = pEstado->IsAlarme() && pEstado->GetFase() == SURTO_JOGO;
	const float flPulso = bAlarme ? 0.5f + 0.5f * sinf( gpGlobals->curtime * M_PI_F * 2.0f * 1.2f ) : 0.0f;
	Texto( m_fScore, cx, y + S( 28 ), bAlarme ? A( Accent(), 0.65f + 0.35f * flPulso ) : A( COR_BRANCO ), szTime, AL_CENTRO, 0.0f, true );
	if ( bAlarme )
	{
		surface()->DrawSetColor( A( Accent(), 0.25f + 0.35f * flPulso ) );
		surface()->DrawOutlinedRect( cx - S( 44 ), y + S( 1 ), cx + S( 44 ), y + S( 36 ) );
	}

	char szNum[ 16 ];
	V_snprintf( szNum, sizeof( szNum ), "%d", pEstado->GetHumanosVivos() );
	const int wH = Texto( m_fScore, cx - S( 64 ), y + S( 28 ), A( COR_ALIADO ), szNum, AL_DIR, 0.0f, true );
	if ( !m_bMin )
		Texto( m_fLabel, cx - S( 64 ) - wH - S( 8 ), y + S( 24 ), A( COR_CINZA2 ), "HUMANOS", AL_DIR, 1.6f );
	V_snprintf( szNum, sizeof( szNum ), "%d", pEstado->GetZumbisVivos() );
	const int wZ = Texto( m_fScore, cx + S( 64 ), y + S( 28 ), A( COR_ZUMBI ), szNum, AL_ESQ, 0.0f, true );
	if ( !m_bMin )
		Texto( m_fLabel, cx + S( 64 ) + wZ + S( 8 ), y + S( 24 ), A( COR_CINZA2 ), "ZUMBIS", AL_ESQ, 1.6f );

	const char *pszLinha = NULL;
	Color colLinha = COR_CINZA3;
	if ( pEstado->GetFase() == SURTO_PREPARO )
	{
		pszLinha = "PREPARE-SE  ·  OS INFECTADOS SERÃO SORTEADOS";
		colLinha = Accent();
	}
	else if ( pEstado->GetFase() == SURTO_JOGO )
	{
		if ( TrokSurto_IsZumbi( pLocal ) )
			pszLinha = "INFECTE OS HUMANOS ANTES DO FIM";
		else if ( pEstado->IsAbrigos() )
		{
			pszLinha = "ABRIGOS ABERTOS  ·  FECHE AS GRADES";
			colLinha = Accent();
		}
		else if ( bAlarme )
		{
			pszLinha = "ALARME  ·  OS ABRIGOS VÃO ABRIR";
			colLinha = Accent();
		}
		else
			pszLinha = "SOBREVIVA ATÉ O FIM DO TEMPO";
	}
	if ( pszLinha )
		Texto( m_fLabel, cx, y + S( 56 ), A( colLinha ), pszLinha, AL_CENTRO, 1.8f );

	// Fim da rodada
	if ( pEstado->GetFase() == SURTO_FIM )
	{
		const bool bHumanos = pEstado->GetVencedor() == SURTO_HUMANOS;
		const char *pszTitulo = bHumanos ? "HUMANOS SOBREVIVERAM" : "OS INFECTADOS VENCERAM";
		const int ty = ScreenHeight() / 2 - S( 40 );
		Veu( 0, ty - S( 70 ), ScreenWidth(), S( 130 ), 0.6f, 2 );
		Texto( m_fBig, cx, ty, A( bHumanos ? COR_ALIADO : COR_ZUMBI ), pszTitulo, AL_CENTRO, 2.0f );
		if ( pEstado->GetDestaque() > 0 && g_PR && g_PR->IsConnected( pEstado->GetDestaque() ) )
		{
			char szDest[ 160 ];
			const int v = pEstado->GetDestaqueValor();
			if ( pEstado->GetDestaqueTipo() == SURTO_DESTAQUE_INFECCOES )
				V_snprintf( szDest, sizeof( szDest ), "DESTAQUE  ·  %s  ·  %d %s", g_PR->GetPlayerName( pEstado->GetDestaque() ), v, v == 1 ? "INFECÇÃO" : "INFECÇÕES" );
			else
				V_snprintf( szDest, sizeof( szDest ), "DESTAQUE  ·  %s  ·  %d %s", g_PR->GetPlayerName( pEstado->GetDestaque() ), v, v == 1 ? "ZUMBI ABATIDO" : "ZUMBIS ABATIDOS" );
			Texto( m_fText, cx, ty - S( 50 ), A( Accent() ), szDest, AL_CENTRO, 1.6f );
		}
		char szProx[ 48 ];
		V_snprintf( szProx, sizeof( szProx ), "NOVA RODADA EM %d", (int)ceilf( flLeft ) );
		Texto( m_fLabel, cx, ty + S( 26 ), A( COR_CINZA2 ), szProx, AL_CENTRO, 1.8f );
	}
}

// Corrida Armada: degrau no meio, você (posição) à esquerda, líder à direita.
void CHudTrokV2::PaintPlacarCorrida( C_BasePlayer *pLocal, int y )
{
	CTrokCorridaEstado *pC = TrokCorrida_Estado();
	if ( !pC )
		return;
	const int cx = ScreenWidth() / 2;
	const int iMe = pLocal->entindex();
	const int iTotal = MAX( 1, pC->GetDegraus() );
	const int iMeuAbates = pC->GetAbates( iMe );
	int iPos = 1, iJog = 1, iLider = 0, iLiderAbates = -1;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( i == iMe || !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) == TEAM_SPECTATOR )
			continue;
		++iJog;
		if ( pC->GetAbates( i ) > iMeuAbates )
			++iPos;
		if ( pC->GetAbates( i ) > iLiderAbates )
		{
			iLiderAbates = pC->GetAbates( i );
			iLider = i;
		}
	}

	Veu( cx - S( 220 ), y - S( 4 ), S( 440 ), S( 46 ), 0.32f, 2 );
	Texto( m_fLabel, cx, y + S( 8 ), A( COR_CINZA2 ), "CORRIDA ARMADA", AL_CENTRO, 1.8f );
	char sz[ 96 ];
	V_snprintf( sz, sizeof( sz ), "%d/%d", pC->GetDegrau( iMe ) + 1, iTotal );
	Texto( m_fScore, cx, y + S( 36 ), A( COR_BRANCO ), sz, AL_CENTRO, 0.0f, true );

	Texto( m_fLabel, cx - S( 70 ), y + S( 20 ), A( COR_CINZA3 ), "VOCÊ", AL_DIR, 1.6f );
	V_snprintf( sz, sizeof( sz ), "%dº DE %d", iPos, iJog );
	Texto( m_fMed, cx - S( 70 ), y + S( 38 ), A( iPos == 1 ? Accent() : COR_ALIADO ), sz, AL_DIR, 0.0f, true );
	if ( iLider > 0 )
	{
		Texto( m_fLabel, cx + S( 70 ), y + S( 20 ), A( COR_CINZA3 ), iPos == 1 ? "PERSEGUIDOR" : "LÍDER", AL_ESQ, 1.6f );
		V_snprintf( sz, sizeof( sz ), "%s  %d", g_PR->GetPlayerName( iLider ), pC->GetDegrau( iLider ) + 1 );
		Texto( m_fMed, cx + S( 70 ), y + S( 38 ), A( COR_INIMIGO ), sz, AL_ESQ );
	}
}

//-----------------------------------------------------------------------------
// Lista de abates
//-----------------------------------------------------------------------------
void CHudTrokV2::PaintKillfeed( void )
{
	static ConVarRef s_life( "trok_hud_killfeed_tempo" );
	const float flLife = s_life.IsValid() ? s_life.GetFloat() : 6.0f;
	const int xr = ScreenWidth() - m_iMargem;
	const int rowH = S( 24 );
	int y = S( 30 );
	for ( int i = 0; i < m_Kills.Count(); ++i )
	{
		const KillEntry_t &k = m_Kills[ i ];
		const float flAge = gpGlobals->curtime - k.flTime;
		const float flEnt = 1.0f - powf( 1.0f - clamp( flAge / 0.25f, 0.0f, 1.0f ), 3.0f );
		const float a = flAge > flLife - 1.0f ? clamp( flLife - flAge, 0.0f, 1.0f ) : flEnt;

		const int gap = S( 8 );
		const int wA = k.szAtk[ 0 ] ? Larg( m_fName, k.szAtk ) + gap : 0;
		const int wW = Larg( m_fLabel, k.szWpn, 1.2f ) + gap;
		const int wCab = k.bHead ? S( 14 ) : 0;
		const int wV = Larg( m_fName, k.szVic );
		const int total = wA + wW + wCab + wV;
		int x = xr - total + (int)( ( 1.0f - flEnt ) * S( 30 ) );

		Veu( x - S( 70 ), y - S( 2 ), total + S( 70 ) + m_iMargem, rowH - S( 2 ), 0.38f * a, 0 );
		if ( k.bMeu )
			Rect( xr + S( 6 ), y + S( 2 ), S1( 2 ), rowH - S( 8 ), A( Accent(), a ) );
		if ( k.szAtk[ 0 ] )
		{
			Texto( m_fName, x, y + S( 15 ), A( k.corAtk, a ), k.szAtk );
			x += wA;
		}
		Texto( m_fLabel, x, y + S( 14 ), A( k.bInfeccao ? COR_ZUMBI : COR_CINZA3, a ), k.szWpn, AL_ESQ, 1.2f );
		x += wW;
		if ( k.bHead )
		{
			// losango âmbar = na cabeça
			const float mx = x + S( 5 ), my = y + S( 10 ), r = S( 5 );
			Vertex_t v[ 4 ] = { Vertex_t( Vector2D( mx, my - r ) ), Vertex_t( Vector2D( mx + r, my ) ), Vertex_t( Vector2D( mx, my + r ) ), Vertex_t( Vector2D( mx - r, my ) ) };
			Poly( v, 4, A( Accent(), a ) );
			x += wCab;
		}
		Texto( m_fName, x, y + S( 15 ), A( k.corVic, a ), k.szVic );
		y += rowH;
	}
}

//-----------------------------------------------------------------------------
// Vida, colete, fôlego, créditos, tipo de zumbi (embaixo à esquerda)
//-----------------------------------------------------------------------------
void CHudTrokV2::PaintVitais( C_BasePlayer *pLocal )
{
	const int x = m_iMargem, yb = ScreenHeight() - m_iMargem;
	const float t = gpGlobals->curtime;
	const bool bZumbi = TrokSurto_IsZumbi( pLocal );
	const int iVida = MAX( 0, pLocal->GetHealth() );
	const int iMax = MAX( 1, MAX( pLocal->GetMaxHealth(), iVida ) );
	const float flFrac = iVida / (float)iMax;
	const bool bBaixa = !bZumbi && flFrac <= 0.3f;

	// número (clarão vermelho logo depois do dano)
	const float flFlash = clamp( 1.0f - ( t - m_flDanoTempo ) / 0.35f, 0.0f, 1.0f );
	Color cNum = bBaixa ? COR_INIMIGO : COR_BRANCO;
	if ( flFlash > 0.0f && !bBaixa )
		cNum = Color( 255, (int)Lerp( flFlash, 238.0f, 120.0f ), (int)Lerp( flFlash, 241.0f, 110.0f ), 255 );
	char sz[ 64 ];
	V_snprintf( sz, sizeof( sz ), "%d", iVida );
	const int wNum = Texto( m_fBig, x, yb - S( 18 ), A( cNum ), sz, AL_ESQ, 0.0f, true );

	// rótulo: VIDA, ou o tipo de zumbi
	if ( bZumbi )
	{
		const int iClasse = TrokZumbis_ClasseDe( pLocal );
		char szTipo[ 48 ];
		CaixaAlta( ( iClasse >= 0 && iClasse < TrokZumbis_Num() ) ? TrokZumbis_Classe( iClasse ).szTitulo : "INFECTADO", szTipo, sizeof( szTipo ) );
		Texto( m_fLabel, x + wNum + S( 8 ), yb - S( 22 ), A( COR_ZUMBI ), szTipo[ 0 ] ? szTipo : "INFECTADO", AL_ESQ, 1.6f );
	}
	else if ( !m_bMin || flFrac < 1.0f )
		Texto( m_fLabel, x + wNum + S( 8 ), yb - S( 22 ), A( COR_CINZA3 ), "VIDA", AL_ESQ, 1.6f );

	// barra em 10 partes, com o rastro do dano
	const int bw = S( 220 ), bh = S1( 5 ), segs = 10, sg = S1( 2 ), by = yb - S( 10 );
	const float segW = ( bw - sg * ( segs - 1 ) ) / (float)segs;
	const float flGhost = ( gpGlobals->curtime - m_flGhostTempo < 0.8f ) ? clamp( 1.0f - ( t - m_flGhostTempo - 0.35f ) / 0.45f, 0.0f, 1.0f ) : 0.0f;
	const float flGhostFrac = clamp( ( iVida + ( m_iGhostDe - iVida ) * flGhost ) / (float)iMax, 0.0f, 1.0f );
	const Color cBarra = bZumbi ? COR_ZUMBI : ( bBaixa ? COR_INIMIGO : COR_BRANCO );
	for ( int i = 0; i < segs; ++i )
	{
		const int sx = x + (int)( i * ( segW + sg ) );
		const float ini = i / (float)segs, fim = ( i + 1 ) / (float)segs;
		Rect( sx, by, (int)segW, bh, A( Color( 255, 255, 255, 31 ) ) );
		const float ench = clamp( ( flFrac - ini ) / ( fim - ini ), 0.0f, 1.0f );
		if ( ench > 0.0f )
			Rect( sx, by, (int)( segW * ench ), bh, A( cBarra ) );
		if ( flGhost > 0.0f && flGhostFrac > flFrac )
		{
			const float a0 = ench, a1 = clamp( ( flGhostFrac - ini ) / ( fim - ini ), 0.0f, 1.0f );
			if ( a1 > a0 )
				Rect( sx + (int)( segW * a0 ), by, (int)( segW * ( a1 - a0 ) ) + 1, bh, A( COR_INIMIGO, 0.85f ) );
		}
	}
	// regenerando (zumbi do Surto)
	if ( bZumbi && t - m_flRegenTempo < 0.8f && flFrac < 1.0f )
		Texto( m_fLabel, x + bw + S( 10 ), by + S( 6 ), A( COR_ZUMBI ), "+ REGENERANDO", AL_ESQ, 1.4f );

	// colete: linha fina por cima da barra e o número
	if ( m_iColete > 0 && !bZumbi )
	{
		CTrokPlacarExtra *pExtra = TrokPlacarExtra();
		(void)pExtra;
		const int iColeteMax = MAX( 100, m_iColete );
		Rect( x, by - S( 7 ), bw, S1( 2 ), A( Color( 255, 255, 255, 26 ) ) );
		Rect( x, by - S( 7 ), (int)( bw * m_iColete / (float)iColeteMax ), S1( 2 ), A( COR_COLETE ) );
		V_snprintf( sz, sizeof( sz ), "COLETE %d", m_iColete );
		Texto( m_fLabel, x + bw, yb - S( 22 ), A( COR_COLETE ), sz, AL_DIR, 1.4f );
	}

	// fôlego da corrida: só quando gasto
	C_BaseHLPlayer *pHL = dynamic_cast< C_BaseHLPlayer * >( pLocal );
	if ( pHL && !bZumbi )
	{
		const float flPower = pHL->m_HL2Local.m_flSuitPower;
		if ( flPower < 99.0f )
		{
			Rect( x, by + S( 9 ), bw, S1( 1 ), A( Color( 255, 255, 255, 26 ) ) );
			Rect( x, by + S( 9 ), (int)( bw * clamp( flPower / 100.0f, 0.0f, 1.0f ) ), S1( 1 ), A( Accent(), 0.85f ) );
		}
	}

	// Linha de cima: LENTO, tipo de zumbi escolhido (Surto) e créditos (economia).
	int yTopo = yb - S( 58 );
	CTrokSurtoEstado *pSurto = TrokSurto_Ativo() ? TrokSurto_Estado() : NULL;
	if ( pSurto && pSurto->GetLentoAte( pLocal->entindex() ) > t )
	{
		const int lw = Larg( m_fLabel, "LENTO", 1.4f ) + S( 12 );
		surface()->DrawSetColor( A( COR_COLETE ) );
		surface()->DrawOutlinedRect( x, yTopo - S( 14 ), x + lw, yTopo + S( 2 ) );
		Texto( m_fLabel, x + S( 6 ), yTopo - S( 2 ), A( COR_COLETE ), "LENTO", AL_ESQ, 1.4f );
		yTopo -= S( 24 );
	}
	if ( pSurto && !bZumbi && ( !m_bMin || pSurto->GetFase() == SURTO_PREPARO ) && TrokZumbis_Num() > 0 )
	{
		const int iEsc = TrokZumbis_EscolhaDe( pLocal );
		if ( iEsc >= 0 && iEsc < TrokZumbis_Num() )
		{
			char szTecla[ 16 ], szTipo[ 48 ];
			TeclaDe( "trok_zumbi_classe_prox", "N", szTecla, sizeof( szTecla ) );
			CaixaAlta( TrokZumbis_Classe( iEsc ).szTitulo, szTipo, sizeof( szTipo ) );
			const int wk = Tecla( x, yTopo - S( 15 ), szTecla, A( COR_CINZA2 ) );
			const int wl = Texto( m_fLabel, x + wk + S( 8 ), yTopo - S( 2 ), A( COR_CINZA3 ), "ZUMBI:", AL_ESQ, 1.6f );
			Texto( m_fLabel, x + wk + S( 8 ) + wl + S( 6 ), yTopo - S( 2 ), A( COR_ZUMBI ), szTipo, AL_ESQ, 1.6f );
			yTopo -= S( 26 );
		}
	}
	CTrokPlacarExtra *pExtra = TrokPlacarExtra();
	if ( pExtra && pExtra->IsEconomia() && !m_bMin && !bZumbi )
	{
		char szMil[ 24 ];
		Milhar( pExtra->GetDinheiro( pLocal->entindex() ), szMil, sizeof( szMil ) );
		V_snprintf( sz, sizeof( sz ), "$ %s", szMil );
		const int wl = Texto( m_fLabel, x, yTopo - S( 2 ), A( COR_CINZA3 ), "CRÉDITOS", AL_ESQ, 1.6f );
		const bool bCompra = pExtra->GetCompraAte( pLocal->entindex() ) > t;
		Texto( m_fMed, x + wl + S( 8 ), yTopo, A( bCompra ? Accent() : COR_BRANCO ), sz, AL_ESQ, 0.0f, true );
	}
}

//-----------------------------------------------------------------------------
// Arma (embaixo à direita)
//-----------------------------------------------------------------------------
static int TipoExplosivo( const char *pszClasse )
{
	if ( !V_stricmp( pszClasse, "weapon_trok_granada" ) )
		return 0;
	if ( !V_stricmp( pszClasse, "weapon_trok_granada_luz" ) )
		return 1;
	if ( !V_stricmp( pszClasse, "weapon_trok_granada_fumaca" ) )
		return 2;
	if ( !V_stricmp( pszClasse, "weapon_trok_mina" ) || !V_stricmp( pszClasse, "weapon_trok_claymore" ) || !V_stricmp( pszClasse, "weapon_trok_remota" ) ||
		 !V_stricmp( pszClasse, "weapon_trok_torreta" ) || !V_stricmp( pszClasse, "weapon_trok_foguete" ) )
		return 3;
	return -1;
}

void CHudTrokV2::PaintArma( C_BasePlayer *pLocal )
{
	C_BaseCombatWeapon *pWeapon = pLocal->GetActiveWeapon();
	if ( !pWeapon )
		return;
	const int xr = ScreenWidth() - m_iMargem, yb = ScreenHeight() - m_iMargem;
	const float t = gpGlobals->curtime;

	char szName[ 48 ];
	NomeArma( pWeapon->GetClassname(), szName, sizeof( szName ) );

	// Garras do zumbi
	if ( TrokSurto_IsZumbi( pLocal ) )
	{
		Texto( m_fMed, xr, yb - S( 4 ), A( COR_BRANCO ), szName, AL_DIR, 1.2f );
		if ( !m_bMin )
			Texto( m_fLabel, xr, yb - S( 26 ), A( COR_ZUMBI ), "CORPO A CORPO", AL_DIR, 1.6f );
		return;
	}

	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pWeapon );
	// linha de baixo: NOME [MODO]
	int x = xr;
	if ( pGun )
	{
		const char *pszModo = pGun->IsAutomatic() ? "AUTO" : "SEMI";
		const int w = Larg( m_fLabel, pszModo, 1.4f ) + S( 10 );
		surface()->DrawSetColor( A( Color( 236, 238, 241, 90 ) ) );
		surface()->DrawOutlinedRect( x - w, yb - S( 15 ), x, yb );
		Texto( m_fLabel, x - w / 2, yb - S( 4 ), A( COR_CINZA2 ), pszModo, AL_CENTRO, 1.4f );
		x -= w + S( 8 );
	}
	Texto( m_fMed, x, yb - S( 2 ), A( COR_BRANCO ), szName, AL_DIR, 1.2f );

	const int yNum = yb - S( 30 );
	int xEquip = xr;
	if ( pWeapon->UsesClipsForAmmo1() )
	{
		const int iClip = MAX( 0, pWeapon->Clip1() );
		const int iMax = MAX( 1, pWeapon->GetMaxClip1() );
		const int iReserve = pLocal->GetAmmoCount( pWeapon->GetPrimaryAmmoType() );
		const bool bBaixo = iClip <= ( iMax + 3 ) / 4;

		char szRes[ 16 ], szClip[ 16 ];
		V_snprintf( szRes, sizeof( szRes ), "/%d", iReserve );
		V_snprintf( szClip, sizeof( szClip ), "%d", iClip );
		const int wRes = Texto( m_fTimer, xr, yNum, A( COR_CINZA3 ), szRes, AL_DIR, 0.0f, true );
		const int wClip = Texto( m_fBig, xr - wRes - S( 6 ), yNum + S( 2 ), A( bBaixo ? COR_INIMIGO : COR_BRANCO ), szClip, AL_DIR, 0.0f, true );
		xEquip = xr - wRes - S( 6 ) - wClip - S( 26 );

		// uma marca por bala
		if ( !m_bMin && iMax <= 40 )
		{
			const int pw = S1( 2 ), ph = S( 7 ), pg = S1( 2 );
			int px = xr - iMax * ( pw + pg ) + pg;
			const int py = yNum - S( 52 );
			for ( int i = 0; i < iMax; ++i )
			{
				Rect( px, py, pw, ph, i < iClip ? A( bBaixo ? COR_INIMIGO : Color( 236, 238, 241, 217 ) ) : A( Color( 255, 255, 255, 36 ) ) );
				px += pw + pg;
			}
		}

		// avisos de recarga (só armas de fogo do Trok, fora da recarga)
		if ( pGun && !pWeapon->m_bInReload )
		{
			char szTecla[ 16 ];
			TeclaDe( "+reload", "R", szTecla, sizeof( szTecla ) );
			if ( iClip == 0 && iReserve == 0 )
				AvisoCentro( "SEM MUNIÇÃO", NULL, COR_INIMIGO, false );
			else if ( bBaixo && iReserve > 0 )
				AvisoCentro( "RECARREGAR", szTecla, iClip == 0 ? COR_INIMIGO : COR_BRANCO, true );
		}
	}
	else
		xEquip = xr - Larg( m_fMed, szName, 1.2f ) - S( 26 );
	(void)t;

	// explosivos que o jogador carrega (à esquerda do número)
	for ( int i = pLocal->WeaponCount() - 1; i >= 0; --i )
	{
		C_BaseCombatWeapon *pW = pLocal->GetWeapon( i );
		if ( !pW )
			continue;
		const int iTipo = TipoExplosivo( pW->GetClassname() );
		if ( iTipo < 0 )
			continue;
		int n = pLocal->GetAmmoCount( pW->GetPrimaryAmmoType() );
		if ( pW->UsesClipsForAmmo1() && pW->Clip1() > 0 )
			n += pW->Clip1();
		char szN[ 8 ];
		V_snprintf( szN, sizeof( szN ), "%d", n );
		const Color c = A( n > 0 ? ( pW == pWeapon ? Accent() : COR_BRANCO ) : COR_CINZA3 );
		const int wN = Texto( m_fText, xEquip, yNum - S( 4 ), c, szN, AL_DIR, 0.0f, true );
		IconeExplosivo( iTipo, xEquip - wN - S( 10 ), yNum - S( 14 ), c );
		xEquip -= wN + S( 26 );
	}
}

// Corrida Armada: a escada de armas acima da arma e o aviso de degrau novo.
void CHudTrokV2::PaintDegraus( C_BasePlayer *pLocal )
{
	CTrokCorridaEstado *pC = TrokCorrida_Ativo() ? TrokCorrida_Estado() : NULL;
	if ( !pC )
		return;
	const int iMe = pLocal->entindex();
	const int n = MAX( 1, pC->GetDegraus() );
	const int iDeg = pC->GetDegrau( iMe ) + 1;	// 1..n
	const int xr = ScreenWidth() - m_iMargem;
	const int y = ScreenHeight() - m_iMargem - S( 112 );

	const int w = n > 24 ? S( 5 ) : S( 7 ), gp = S1( 3 );
	int x = xr - n * ( w + gp ) + gp;
	const int x0 = x;
	for ( int i = 1; i <= n; ++i )
	{
		Color c( 255, 255, 255, 36 );
		if ( i < iDeg )
			c = Color( 236, 238, 241, 190 );
		if ( i == iDeg )
			c = Accent();
		const int h = i == iDeg ? S( 10 ) : S( 6 );
		Rect( x, y + ( S( 10 ) - h ), w, h, A( c ) );
		x += w + gp;
	}
	char sz[ 96 ];
	if ( !m_bMin )
	{
		V_snprintf( sz, sizeof( sz ), "DEGRAU %d/%d", iDeg, n );
		Texto( m_fLabel, x0 - S( 6 ), y + S( 10 ), A( COR_CINZA3 ), sz, AL_DIR, 1.6f );
	}

	// próxima arma e quantos abates faltam
	const TrokCorridaEscada_t *pEsc = TrokCorrida_Escada( pC->GetMeta() );
	if ( pEsc && iDeg < pEsc->nDegraus )
	{
		char szNome[ 48 ];
		CaixaAlta( pEsc->degraus[ iDeg ].szNome, szNome, sizeof( szNome ) );
		const int iFalta = MAX( 0, pEsc->degraus[ iDeg ].iAbates - pC->GetAbates( iMe ) );
		V_snprintf( sz, sizeof( sz ), "%s  ·  %d %s", szNome, iFalta, iFalta == 1 ? "ABATE" : "ABATES" );
		const int wv = Texto( m_fLabel, xr, y - S( 8 ), A( COR_CINZA2 ), sz, AL_DIR, 1.4f );
		Texto( m_fLabel, xr - wv - S( 8 ), y - S( 8 ), A( COR_CINZA3 ), "PRÓXIMA", AL_DIR, 1.6f );
	}

	// subiu de degrau: linha âmbar sob a mira
	const float d = gpGlobals->curtime - m_flDegrauTempo;
	if ( d >= 0.0f && d < 1.4f && m_szDegrau[ 0 ] )
	{
		const float a = d < 0.1f ? d / 0.1f : ( d > 0.9f ? ( 1.4f - d ) / 0.5f : 1.0f );
		const int dy = (int)( ( 1.0f - clamp( d / 0.3f, 0.0f, 1.0f ) ) * S( 10 ) );
		Texto( m_fText, ScreenWidth() / 2, ScreenHeight() / 2 + S( 108 ) + dy, A( Accent(), a ), m_szDegrau, AL_CENTRO, 2.0f );
	}
}

//-----------------------------------------------------------------------------
// Centro da tela
//-----------------------------------------------------------------------------
extern ConVar trok_mira_calibrar; // weapon_trok_gun.cpp

void CHudTrokV2::PaintMira( C_BasePlayer *pLocal )
{
	const int cx = ScreenWidth() / 2, cy = ScreenHeight() / 2;
	if ( trok_mira_calibrar.GetBool() )
	{
		Rect( cx, 0, 1, ScreenHeight(), Color( 255, 40, 40, 200 ) );
		Rect( 0, cy, ScreenWidth(), 1, Color( 255, 40, 40, 200 ) );
		return;
	}
	static ConVarRef s_mira( "trok_hud_mira" );
	if ( s_mira.IsValid() && !s_mira.GetBool() )
		return;

	// Zumbi: ponto + dois colchetes curvos (garras).
	if ( TrokSurto_IsZumbi( pLocal ) )
	{
		Rect( cx - S1( 1 ), cy - S1( 1 ), S1( 1 ) * 2, S1( 1 ) * 2, A( COR_BRANCO ) );
		const float r = (float)S( 16 ), th = (float)MAX( 1, S( 1.5f ) );
		Arco( cx, cy, r - th * 0.5f, r + th * 0.5f, DEG2RAD( 225.0f ), DEG2RAD( 315.0f ), 6, A( COR_BRANCO, 0.7f ) );
		Arco( cx, cy, r - th * 0.5f, r + th * 0.5f, DEG2RAD( 45.0f ), DEG2RAD( 135.0f ), 6, A( COR_BRANCO, 0.7f ) );
		return;
	}

	CWeaponTrokGun *pGun = dynamic_cast< CWeaponTrokGun * >( pLocal->GetActiveWeapon() );
	if ( !pGun )
	{
		Rect( cx - 1, cy - 1, 2, 2, A( Color( 236, 238, 241, 200 ) ) );
		return;
	}
	const float flVis = ( 1.0f - pGun->GetADSBlend() ) * ( 1.0f - pGun->GetSprintBlend() );
	if ( flVis < 0.05f )
		return;

	// distância do centro = meia abertura do cone de dispersão projetada na tela
	const float flFov = MAX( 10, pLocal->GetFOV() );
	const float flTanHalfV = tanf( DEG2RAD( flFov * 0.5f ) ) * 0.75f;
	const float flHalfSpread = DEG2RAD( 0.5f * pGun->CurrentSpreadDegrees() );
	int gap = (int)( ( ScreenHeight() * 0.5f ) * tanf( flHalfSpread ) / flTanHalfV );
	gap = clamp( gap, S( 5 ), S( 90 ) );
	const int len = S( 8 ), th = S1( 2 );
	const Color cor = A( Color( 236, 238, 241, 235 ), flVis ), borda = A( Color( 0, 0, 0, 115 ), flVis );
	struct R_t { int x, y, w, h; };
	const R_t r[ 4 ] =
	{
		{ cx - th / 2, cy - gap - len, th, len },
		{ cx - th / 2, cy + gap, th, len },
		{ cx - gap - len, cy - th / 2, len, th },
		{ cx + gap, cy - th / 2, len, th },
	};
	for ( int i = 0; i < 4; ++i )
		Rect( r[ i ].x - 1, r[ i ].y - 1, r[ i ].w + 2, r[ i ].h + 2, borda );
	for ( int i = 0; i < 4; ++i )
		Rect( r[ i ].x, r[ i ].y, r[ i ].w, r[ i ].h, cor );
	Rect( cx - S1( 1 ) / 2, cy - S1( 1 ) / 2, S1( 1 ), S1( 1 ), A( Color( 236, 238, 241, 150 ), flVis ) );
}

// X fino no acerto; abate vermelho e maior (entra 35% maior); abate na cabeça em âmbar.
void CHudTrokV2::PaintHitmarker( void )
{
	static ConVarRef s_hit( "trok_hitmarker" );
	if ( s_hit.IsValid() && !s_hit.GetBool() )
		return;
	const float d = gpGlobals->curtime - m_flHitTempo;
	if ( d < 0.0f || d > m_flHitDur || m_flHitDur <= 0.0f )
		return;
	const float a = clamp( ( m_flHitDur - d ) / ( m_flHitDur * 0.5f ), 0.0f, 1.0f );
	const float esc = m_bHitAbate ? 1.0f + 0.35f * ( 1.0f - clamp( d / 0.12f, 0.0f, 1.0f ) ) : 1.0f;
	const float cx = ScreenWidth() * 0.5f, cy = ScreenHeight() * 0.5f;
	const float gap = S( 6 ) * esc, len = ( m_bHitAbate ? S( 11 ) : S( 8 ) ) * esc;
	const Color cor = m_bHitAbate ? ( m_bHitCabeca ? Accent() : COR_INIMIGO ) : Color( 245, 245, 245, 255 );
	static const int s_dirs[ 4 ][ 2 ] = { { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
	for ( int i = 0; i < 4; ++i )
		LinhaGrossa( cx + s_dirs[ i ][ 0 ] * gap, cy + s_dirs[ i ][ 1 ] * gap, cx + s_dirs[ i ][ 0 ] * ( gap + len ), cy + s_dirs[ i ][ 1 ] * ( gap + len ),
			(float)S( 4 ), A( Color( 0, 0, 0, 128 ), a ) );
	for ( int i = 0; i < 4; ++i )
		LinhaGrossa( cx + s_dirs[ i ][ 0 ] * gap, cy + s_dirs[ i ][ 1 ] * gap, cx + s_dirs[ i ][ 0 ] * ( gap + len ), cy + s_dirs[ i ][ 1 ] * ( gap + len ),
			(float)S1( 2 ), A( cor, a ) );
}

// Cunha vermelha em volta da mira, apontando para de onde veio o dano (degradê em 3 faixas).
void CHudTrokV2::PaintDano( C_BasePlayer *pLocal )
{
	const float cx = ScreenWidth() * 0.5f, cy = ScreenHeight() * 0.5f;
	const float R = (float)S( 118 );
	QAngle angView = pLocal->EyeAngles();
	angView.x = angView.z = 0.0f;
	Vector fwd, right;
	AngleVectors( angView, &fwd, &right, NULL );
	for ( int k = 0; k < ARRAYSIZE( m_Dmg ); ++k )
	{
		const float flAge = gpGlobals->curtime - m_Dmg[ k ].flTime;
		if ( flAge < 0.0f || flAge > 1.3f )
			continue;
		const float a = flAge < 0.8f ? 1.0f : ( 1.3f - flAge ) / 0.5f;
		Vector rel = m_Dmg[ k ].vecFrom - pLocal->EyePosition();
		rel.z = 0.0f;
		const float ang = atan2f( DotProduct( rel, right ), DotProduct( rel, fwd ) );
		const float span = DEG2RAD( 22.0f );
		const Color c( 255, 72, 60, 255 );
		Arco( cx, cy, R - S( 10 ), R - S( 3 ), ang - span * 0.75f, ang + span * 0.75f, 8, A( c, 0.25f * a ) );
		Arco( cx, cy, R - S( 3 ), R + S( 3 ), ang - span * 0.9f, ang + span * 0.9f, 8, A( c, 0.55f * a ) );
		Arco( cx, cy, R + S( 3 ), R + S( 8 ), ang - span, ang + span, 8, A( c, 0.95f * a ) );
	}
}

// Granadas vivas perto (C_BaseGrenade: o CTrokProjetil e as do HL2MP). Fumaça e aparelhos
// plantados (mina, claymore, torreta) não avisam; granada de aliado também não (com times).
void CHudTrokV2::UpdateGranadas( C_BasePlayer *pLocal )
{
	if ( gpGlobals->curtime < m_flNextGran )
		return;
	m_flNextGran = gpGlobals->curtime + 0.1f;
	const int nAntes = m_nGran;
	m_nGran = 0;
	const float flRaio = MAX( 50.0f, trok_hud_granada_raio.GetFloat() );
	const bool bTimes = HL2MPRules() && HL2MPRules()->IsTeamplay();
	for ( C_BaseEntity *pEnt = ClientEntityList().FirstBaseEntity(); pEnt && m_nGran < MAX_GRANADAS; pEnt = ClientEntityList().NextBaseEntity( pEnt ) )
	{
		if ( pEnt->IsDormant() )
			continue;
		C_BaseGrenade *pGren = dynamic_cast< C_BaseGrenade * >( pEnt );
		if ( !pGren )
			continue;
		if ( ( pGren->GetAbsOrigin() - pLocal->GetAbsOrigin() ).LengthSqr() > flRaio * flRaio )
			continue;
		const char *pszModelo = modelinfo->GetModelName( pGren->GetModel() );
		if ( pszModelo )
		{
			char szLow[ 128 ];
			V_strncpy( szLow, pszModelo, sizeof( szLow ) );
			V_strlower( szLow );
			if ( V_strstr( szLow, "smoke" ) || V_strstr( szLow, "fumaca" ) || V_strstr( szLow, "mina" ) || V_strstr( szLow, "mine" ) ||
				 V_strstr( szLow, "claymore" ) || V_strstr( szLow, "torreta" ) || V_strstr( szLow, "turret" ) )
				continue;
		}
		C_BaseEntity *pDono = pGren->GetThrower();
		if ( bTimes && pDono && pDono != pLocal && pDono->GetTeamNumber() == pLocal->GetTeamNumber() )
			continue;
		m_Gran[ m_nGran++ ].vecPos = pGren->GetAbsOrigin();
	}
	if ( m_nGran > 0 && nAntes == 0 )
		m_flGranDesde = gpGlobals->curtime;
}

void CHudTrokV2::PaintGranadas( C_BasePlayer *pLocal )
{
	if ( !trok_hud_granada.GetBool() )
		return;
	UpdateGranadas( pLocal );
	if ( m_nGran <= 0 )
		return;
	const float cx = ScreenWidth() * 0.5f, cy = ScreenHeight() * 0.5f;
	const float R = (float)S( 78 );
	const float d = gpGlobals->curtime - m_flGranDesde;
	const float a = clamp( d / 0.12f, 0.0f, 1.0f );
	const float pulso = 0.6f + 0.4f * sinf( d * M_PI_F * 2.0f * 2.2f );
	QAngle angView = pLocal->EyeAngles();
	angView.x = angView.z = 0.0f;
	Vector fwd, right;
	AngleVectors( angView, &fwd, &right, NULL );
	for ( int i = 0; i < m_nGran; ++i )
	{
		Vector rel = m_Gran[ i ].vecPos - pLocal->GetAbsOrigin();
		const float flDist = rel.Length() / UNIDADES_POR_METRO;
		rel.z = 0.0f;
		const float ang = atan2f( DotProduct( rel, right ), DotProduct( rel, fwd ) );
		const float px = cx + sinf( ang ) * R, py = cy - cosf( ang ) * R;
		Circulo( px, py, (float)S( 15 ), A( Color( 11, 13, 16, 140 ), a ) );
		surface()->DrawSetColor( A( COR_INIMIGO, a * pulso ) );
		surface()->DrawOutlinedCircle( (int)px, (int)py, S( 15 ), 24 );
		IconeExplosivo( 0, (int)px, (int)py, A( Color( 255, 120, 110, 255 ), a ) );
		// seta para fora
		const float sr = R + S( 22 ), t = (float)S( 6 );
		const float sx = cx + sinf( ang ) * sr, sy = cy - cosf( ang ) * sr;
		const float fx = sinf( ang ), fy = -cosf( ang ), lx = cosf( ang ), ly = sinf( ang );
		Vertex_t v[ 3 ] =
		{
			Vertex_t( Vector2D( sx + fx * t, sy + fy * t ) ),
			Vertex_t( Vector2D( sx - fx * t * 0.33f + lx * t, sy - fy * t * 0.33f + ly * t ) ),
			Vertex_t( Vector2D( sx - fx * t * 0.33f - lx * t, sy - fy * t * 0.33f - ly * t ) ),
		};
		Poly( v, 3, A( COR_INIMIGO, a * pulso ) );
		char sz[ 16 ];
		V_snprintf( sz, sizeof( sz ), "%d m", (int)( flDist + 0.5f ) );
		Texto( m_fLabel, (int)px, (int)py + S( 30 ), A( Color( 255, 140, 130, 255 ), a ), sz, AL_CENTRO, 1.2f );
	}
}

// Eventos à direita da mira: entram deslizando, ficam 1,4 s e somem em 0,5 s.
void CHudTrokV2::PaintEventos( void )
{
	if ( !trok_hud_eventos.GetBool() )
		return;
	const int x0 = ScreenWidth() / 2 + S( 44 );
	int y = ScreenHeight() / 2 + S( 30 );
	for ( int i = 0; i < m_Eventos.Count(); ++i )
	{
		const Evento_t &e = m_Eventos[ i ];
		const float d = gpGlobals->curtime - e.flTime;
		if ( d < 0.0f || d > 1.9f )
			continue;
		const float ent = 1.0f - powf( 1.0f - clamp( d / 0.16f, 0.0f, 1.0f ), 3.0f );
		const float a = d > 1.4f ? clamp( ( 1.9f - d ) / 0.5f, 0.0f, 1.0f ) : ent;
		int x = x0 - (int)( ( 1.0f - ent ) * S( 8 ) );
		x += Texto( m_fText, x, y, A( e.cor, a ), e.szRotulo, AL_ESQ, 1.6f ) + S( 10 );
		if ( e.szValor[ 0 ] )
			x += Texto( m_fText, x, y, A( Accent(), a ), e.szValor, AL_ESQ, 0.0f, true ) + S( 10 );
		if ( e.szDetalhe[ 0 ] )
			Texto( m_fSmall, x, y, A( COR_CINZA2, a * 0.9f ), e.szDetalhe );
		y += S( 20 );
	}
}

// Aviso sob a mira ("[R] RECARREGAR"), com véu leve; pulsa sem sumir.
void CHudTrokV2::AvisoCentro( const char *pszTexto, const char *pszTecla, const Color &c, bool bPulsa )
{
	const float a = bPulsa ? 0.78f + 0.22f * sinf( gpGlobals->curtime * M_PI_F * 2.0f * 1.5f ) : 1.0f;
	const int y = ScreenHeight() / 2 + S( 74 );
	const int wk = pszTecla ? MAX( S( 18 ), Larg( m_fLabel, pszTecla, 1.0f ) + S( 10 ) ) + S( 8 ) : 0;
	const int wt = Larg( m_fText, pszTexto, 2.0f );
	const int x = ScreenWidth() / 2 - ( wk + wt ) / 2;
	Veu( x - S( 40 ), y - S( 15 ), wk + wt + S( 80 ), S( 24 ), 0.35f * a, 2 );
	if ( pszTecla )
		Tecla( x, y - S( 13 ), pszTecla, A( c, a ) );
	Texto( m_fText, x + wk, y + S( 1 ), A( c, a ), pszTexto, AL_ESQ, 2.0f );
}

// Proteção ao nascer: rótulo âmbar e barra que esvazia.
void CHudTrokV2::PaintProtecao( C_BasePlayer *pLocal )
{
	C_HL2MP_Player *pHL2MP = dynamic_cast< C_HL2MP_Player * >( pLocal );
	if ( !pHL2MP || pHL2MP->m_flTrokProtegidoAte <= gpGlobals->curtime )
		return;
	const float flTotal = MAX( 0.5f, trok_protecao.GetFloat() );
	const float flResta = clamp( ( pHL2MP->m_flTrokProtegidoAte - gpGlobals->curtime ) / flTotal, 0.0f, 1.0f );
	const int cx = ScreenWidth() / 2, y = ScreenHeight() / 2 + S( 72 );
	Texto( m_fLabel, cx, y, A( Accent() ), "PROTEGIDO", AL_CENTRO, 2.0f );
	Rect( cx - S( 40 ), y + S( 6 ), S( 80 ), S1( 2 ), A( Color( 255, 255, 255, 38 ) ) );
	Rect( cx - S( 40 ), y + S( 6 ), (int)( S( 80 ) * flResta ), S1( 2 ), A( Accent() ) );
}

// Prompt de interação: [tecla] VERBO OBJETO, centrado, véu escuro leve.
void CHudTrokV2::Prompt( const char *pszTecla, const char *pszVerbo, const char *pszObjeto, float flY )
{
	CriarFontes();
	const int y = (int)( ScreenHeight() * flY );
	const int wk = S( 26 );
	const int wv = Larg( m_fText, pszVerbo, 2.0f ), wo = Larg( m_fText, pszObjeto, 1.0f );
	const int tot = wk + wv + S( 8 ) + wo;
	const int x = ScreenWidth() / 2 - tot / 2;
	Veu( x - S( 50 ), y - S( 16 ), tot + S( 100 ), S( 26 ), 0.45f, 2 );
	Tecla( x, y - S( 13 ), pszTecla, A( Accent() ) );
	Texto( m_fText, x + wk, y + S( 1 ), A( COR_CINZA2 ), pszVerbo, AL_ESQ, 2.0f );
	Texto( m_fText, x + wk + wv + S( 8 ), y + S( 1 ), A( COR_BRANCO ), pszObjeto, AL_ESQ, 1.0f );
}

//-----------------------------------------------------------------------------
// Ganchos
//-----------------------------------------------------------------------------
void TrokHudV2_Prompt( const char *pszTecla, const char *pszVerbo, const char *pszObjeto, float flY )
{
	if ( s_pHudV2 )
		s_pHudV2->Prompt( pszTecla, pszVerbo, pszObjeto, flY );
}

void TrokHudV2_Evento( const char *pszRotulo, const char *pszValor, const char *pszDetalhe, Color cor )
{
	if ( s_pHudV2 && trok_hud_eventos.GetBool() )
		s_pHudV2->Evento( pszRotulo, pszValor, pszDetalhe, cor );
}
