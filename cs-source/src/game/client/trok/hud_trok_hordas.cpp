//=============================================================================
// Trok: HUD do modo Hordas (trok_modo hordas; regras em server/trok/trok_hordas_diretor.cpp).
//
// - no alto, no meio: RODADA N (matilha/chefe marcados), zumbis que faltam e a contagem do
//   intervalo; no fim de jogo, a rodada alcançada e o recorde;
// - à direita: pontos de cada humano (o seu em destaque) com o "+10" subindo a cada ganho;
// - aviso da estação que você olha ("[E] LOJA", máquina, armadilha; vermelho se faltam pontos)
//   ou "SEGURE E — REVIVER" perto de um derrubado; o E na loja abre o menu (ui/trok_hordas_loja.cpp);
// - derrubado: contagem para sangrar e a barra de quem está te revivendo; colegas derrubados
//   marcados na tela;
// - bônus ativos com o tempo e a faixa do bônus pego;
// - nível de melhoria da arma na mão.
// Só lê o estado em rede (trok_hordas_estado) e as estações (CTrokHordasEstacao).
//=============================================================================

#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "iclientmode.h"
#include "ienginevgui.h"
#include "c_playerresource.h"
#include "view_scene.h"
#include "trok/trok_hordas.h"
#include "trok/trok_surto.h"
#include <vgui/ISurface.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using namespace vgui;

static ConVar trok_hordas_hud( "trok_hordas_hud", "1", FCVAR_ARCHIVE, "Hordas: mostra rodada, pontos, avisos das estações e bônus." );

static const Color COR_BRANCO( 236, 238, 241, 255 );
static const Color COR_CINZA( 190, 196, 204, 220 );
static const Color COR_DESTAQUE( 242, 181, 68, 255 );
static const Color COR_PERIGO( 217, 54, 43, 255 );	// vermelho-sangue da paleta (infecção/zumbi)
static const Color COR_FUNDO( 11, 13, 16, 170 );

static const char *s_pszBonusNome[ HORDAS_BONUS_NUM ] =
{
	"REABASTECIMENTO", "ABATE CERTO", "PONTOS EM DOBRO", "ATAQUE AÉREO", "REFORÇO", "CAIXA BARATA"
};

#define HORDAS_HUD_GANHOS 6

class CHudTrokHordas : public CHudElement, public Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokHordas, Panel );

public:
	CHudTrokHordas( const char *pElementName );

	virtual bool ShouldDraw( void );
	virtual void OnThink( void );

protected:
	virtual void ApplySchemeSettings( IScheme *pScheme );
	virtual void Paint( void );

private:
	int S( float px ) const { return (int)( px * ScreenHeight() / 720.0f + 0.5f ); }
	int Texto( HFont hFont, int x, int y, const Color &col, const char *pszUTF8, int iAlinha = 0 );	// -1 esq, 0 meio, 1 dir; devolve a largura
	int Largura( HFont hFont, const char *pszUTF8 );
	void Faixa( const char *pszUTF8, int y, const Color &corBarra );

	void PintarTopo( CTrokHordasEstado *pEstado );
	void PintarPontos( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal );
	void PintarAviso( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal );
	void PintarCaidos( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal );
	void PintarBonus( CTrokHordasEstado *pEstado );

	HFont m_hTexto, m_hPequena, m_hGrande, m_hNumero;

	// Ganhos de pontos do jogador local ("+10" subindo)
	int m_iPontosAntes;
	struct Ganho_t { int iValor; float flHora; };
	Ganho_t m_Ganhos[ HORDAS_HUD_GANHOS ];
	int m_iProxGanho;
};

DECLARE_HUDELEMENT( CHudTrokHordas );

CHudTrokHordas::CHudTrokHordas( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokHordas" )
{
	SetParent( g_pClientMode->GetViewport() );
	SetScheme( scheme()->LoadSchemeFromFileEx( enginevgui->GetPanel( PANEL_CLIENTDLL ), "resource/TrokScheme.res", "TrokScheme" ) );
	SetHiddenBits( 0 );
	m_hTexto = m_hPequena = m_hGrande = m_hNumero = INVALID_FONT;
	m_iPontosAntes = -1;
	m_iProxGanho = 0;
	for ( int i = 0; i < HORDAS_HUD_GANHOS; ++i )
		m_Ganhos[ i ].flHora = -100.0f;
}

void CHudTrokHordas::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );
	SetPaintBackgroundEnabled( false );
	SetMouseInputEnabled( false );
	SetKeyBoardInputEnabled( false );
	SetPos( 0, 0 );
	SetSize( ScreenWidth(), ScreenHeight() );
	m_hTexto = pScheme->GetFont( "TrokHudText", true );
	m_hPequena = pScheme->GetFont( "TrokHudSmall", true );
	m_hGrande = pScheme->GetFont( "TrokHudScore", true );
	m_hNumero = pScheme->GetFont( "TrokHudNumber", true );
	if ( m_hNumero == INVALID_FONT )
		m_hNumero = m_hGrande;
}

bool CHudTrokHordas::ShouldDraw( void )
{
	if ( !trok_hordas_hud.GetBool() || engine->IsLevelMainMenuBackground() || !TrokHordas_Ativo() )
		return false;
	if ( !C_BasePlayer::GetLocalPlayer() )
		return false;
	return CHudElement::ShouldDraw();
}

void TrokHordasLoja_Pensar( void );	// ui/trok_hordas_loja.cpp: E na loja abre o menu

void CHudTrokHordas::OnThink( void )
{
	TrokHordasLoja_Pensar();
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pEstado || !pLocal )
		return;
	const int iPontos = pEstado->GetPontos( pLocal->entindex() );
	if ( m_iPontosAntes >= 0 && iPontos != m_iPontosAntes )
	{
		Ganho_t &g = m_Ganhos[ m_iProxGanho ];
		g.iValor = iPontos - m_iPontosAntes;
		g.flHora = gpGlobals->curtime;
		m_iProxGanho = ( m_iProxGanho + 1 ) % HORDAS_HUD_GANHOS;
	}
	m_iPontosAntes = iPontos;
}

int CHudTrokHordas::Largura( HFont hFont, const char *pszUTF8 )
{
	if ( hFont == INVALID_FONT || !pszUTF8[ 0 ] )
		return 0;
	wchar_t wsz[ 192 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	return w;
}

int CHudTrokHordas::Texto( HFont hFont, int x, int y, const Color &col, const char *pszUTF8, int iAlinha )
{
	if ( hFont == INVALID_FONT || !pszUTF8[ 0 ] )
		return 0;
	wchar_t wsz[ 192 ];
	V_UTF8ToUnicode( pszUTF8, wsz, sizeof( wsz ) );
	int w = 0, t = 0;
	surface()->GetTextSize( hFont, wsz, w, t );
	const int x0 = ( iAlinha < 0 ) ? x : ( iAlinha == 0 ? x - w / 2 : x - w );
	surface()->DrawSetTextFont( hFont );
	surface()->DrawSetTextColor( Color( 0, 0, 0, MIN( 200, col.a() ) ) );
	surface()->DrawSetTextPos( x0 + 1, y + 1 );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
	surface()->DrawSetTextColor( col );
	surface()->DrawSetTextPos( x0, y );
	surface()->DrawPrintText( wsz, V_wcslen( wsz ) );
	return w;
}

void CHudTrokHordas::Faixa( const char *pszUTF8, int y, const Color &corBarra )
{
	const int w = Largura( m_hGrande, pszUTF8 );
	if ( w <= 0 )
		return;
	const int t = surface()->GetFontTall( m_hGrande );
	const int cx = ScreenWidth() / 2;
	surface()->DrawSetColor( COR_FUNDO );
	surface()->DrawFilledRect( cx - w / 2 - S( 18 ), y - S( 6 ), cx + w / 2 + S( 18 ), y + t + S( 6 ) );
	surface()->DrawSetColor( corBarra );
	surface()->DrawFilledRect( cx - w / 2 - S( 18 ), y - S( 6 ), cx + w / 2 + S( 18 ), y - S( 4 ) );
	Texto( m_hGrande, cx, y, COR_BRANCO, pszUTF8 );
}

void CHudTrokHordas::Paint( void )
{
	CTrokHordasEstado *pEstado = TrokHordas_Estado();
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pEstado || !pLocal || !g_PR )
		return;
	PintarTopo( pEstado );
	PintarPontos( pEstado, pLocal );
	PintarBonus( pEstado );
	PintarCaidos( pEstado, pLocal );
	PintarAviso( pEstado, pLocal );
}

// Rodada, zumbis que faltam, contagem.
void CHudTrokHordas::PintarTopo( CTrokHordasEstado *pEstado )
{
	const int cx = ScreenWidth() / 2;
	const int y = S( 18 );
	char sz[ 128 ];

	if ( pEstado->GetFase() == HORDAS_FIM )
	{
		V_snprintf( sz, sizeof( sz ), "FIM DE JOGO  —  RODADA %d", pEstado->GetRodada() );
		Faixa( sz, S( 170 ), COR_PERIGO );
		V_snprintf( sz, sizeof( sz ), "RECORDE NESTE MAPA: RODADA %d", pEstado->GetRecorde() );
		Texto( m_hTexto, cx, S( 170 ) + S( 48 ), COR_CINZA, sz );
		return;
	}

	// Número da rodada grande, sóbrio, em âmbar (o CoD usa marcas de giz; aqui é número).
	const int R = pEstado->GetRodada();
	if ( R > 0 )
	{
		V_snprintf( sz, sizeof( sz ), "%d", R );
		const Color corR = ( pEstado->GetRodadaTipo() != HORDAS_RODADA_NORMAL ) ? COR_PERIGO : COR_DESTAQUE;
		Texto( m_hNumero, S( 34 ), ScreenHeight() - S( 150 ), corR, sz, -1 );
		Texto( m_hPequena, S( 36 ), ScreenHeight() - S( 166 ), COR_CINZA, "RODADA", -1 );
	}

	const float flFalta = MAX( 0.0f, pEstado->GetFimFase() - gpGlobals->curtime );
	switch ( pEstado->GetFase() )
	{
	case HORDAS_ESPERA:
		Texto( m_hTexto, cx, y, COR_CINZA, "ESPERANDO JOGADORES" );
		break;
	case HORDAS_INTERVALO:
		if ( R == 0 )
			V_snprintf( sz, sizeof( sz ), "PREPAREM-SE  ·  %d", (int)ceilf( flFalta ) );
		else
			V_snprintf( sz, sizeof( sz ), "RODADA %d EM %d", R + 1, (int)ceilf( flFalta ) );
		Texto( m_hTexto, cx, y, COR_BRANCO, sz );
		break;
	case HORDAS_RODADA:
	{
		const char *pszTipo = "";
		if ( pEstado->GetRodadaTipo() == HORDAS_RODADA_MATILHA )
			pszTipo = "MATILHA  ·  ";
		else if ( pEstado->GetRodadaTipo() == HORDAS_RODADA_CHEFE )
			pszTipo = "CHEFE  ·  ";
		V_snprintf( sz, sizeof( sz ), "%sRODADA %d  ·  %d ZUMBIS", pszTipo, R, pEstado->GetRestantes() );
		Texto( m_hTexto, cx, y, pszTipo[ 0 ] ? COR_PERIGO : COR_BRANCO, sz );
		break;
	}
	}
}

// Pontos de cada humano, à direita, e o "+10" do jogador local.
void CHudTrokHordas::PintarPontos( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal )
{
	const int xDir = ScreenWidth() - S( 24 );
	int y = ScreenHeight() / 2 - S( 60 );
	const int iLinha = S( 22 );
	char sz[ 96 ];
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		if ( !g_PR->IsConnected( i ) || g_PR->GetTeam( i ) != TEAM_TROK_HUMANOS )
			continue;
		const bool bEu = ( i == pLocal->entindex() );
		const bool bCaido = pEstado->GetCaidoAte( i ) > 0.0f;
		V_snprintf( sz, sizeof( sz ), "%d", pEstado->GetPontos( i ) );
		const int w = Texto( m_hTexto, xDir, y, bEu ? COR_DESTAQUE : COR_BRANCO, sz, 1 );
		char szNome[ 64 ];
		V_snprintf( szNome, sizeof( szNome ), "%s%s", bCaido ? "CAÍDO  " : "", g_PR->GetPlayerName( i ) );
		Texto( m_hPequena, xDir - w - S( 10 ), y + S( 3 ), bCaido ? COR_PERIGO : COR_CINZA, szNome, 1 );

		if ( bEu )
		{
			// Ganhos recentes subindo ao lado do número.
			for ( int k = 0; k < HORDAS_HUD_GANHOS; ++k )
			{
				const float flIdade = gpGlobals->curtime - m_Ganhos[ k ].flHora;
				if ( flIdade < 0.0f || flIdade > 1.2f || m_Ganhos[ k ].iValor == 0 )
					continue;
				const int a = (int)( 255 * ( 1.0f - flIdade / 1.2f ) );
				V_snprintf( sz, sizeof( sz ), "%+d", m_Ganhos[ k ].iValor );
				const Color c = m_Ganhos[ k ].iValor > 0 ? Color( 242, 181, 68, a ) : Color( 214, 72, 58, a );
				Texto( m_hPequena, xDir - w - S( 12 ) - S( 110 ), y - (int)( flIdade * S( 30 ) ), c, sz, 1 );
			}
		}
		y += iLinha;
	}

	// Nível da arma na mão.
	C_BaseCombatWeapon *pArma = pLocal->GetActiveWeapon();
	const int iNivel = TrokHordas_NivelArma( pArma );
	if ( iNivel > 0 )
	{
		V_snprintf( sz, sizeof( sz ), "MELHORIA %s", TrokUpgrade().niveis[ iNivel ].szNome );
		Texto( m_hPequena, xDir, ScreenHeight() - S( 128 ), COR_DESTAQUE, sz, 1 );
	}
	// Nível de defesa (torreta, claymore, mina, remota postas por você).
	const int iDef = pEstado->GetNivelDefesa( pLocal->entindex() );
	if ( iDef > 0 )
	{
		V_snprintf( sz, sizeof( sz ), "DEFESAS %s", TrokUpgrade().defesa[ iDef ].szNome );
		Texto( m_hPequena, xDir, ScreenHeight() - S( 146 ), COR_DESTAQUE, sz, 1 );
	}
}

// Aviso da estação ou de reviver (no meio, embaixo da mira).
void CHudTrokHordas::PintarAviso( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal )
{
	if ( !pLocal->IsAlive() || pLocal->GetTeamNumber() != TEAM_TROK_HUMANOS )
		return;
	const int cx = ScreenWidth() / 2;
	const int y = (int)( ScreenHeight() * 0.68f );
	char sz[ 160 ];

	if ( TrokHordas_Caido( pLocal ) )
	{
		const float flFalta = MAX( 0.0f, pEstado->GetCaidoAte( pLocal->entindex() ) - gpGlobals->curtime );
		V_snprintf( sz, sizeof( sz ), "CAÍDO  —  %d", (int)ceilf( flFalta ) );
		Faixa( sz, S( 200 ), COR_PERIGO );
		const float flProg = pEstado->GetReviver( pLocal->entindex() );
		if ( flProg > 0.0f )
		{
			const int w = S( 220 );
			surface()->DrawSetColor( COR_FUNDO );
			surface()->DrawFilledRect( cx - w / 2, y, cx + w / 2, y + S( 8 ) );
			surface()->DrawSetColor( COR_BRANCO );
			surface()->DrawFilledRect( cx - w / 2, y, cx - w / 2 + (int)( w * clamp( flProg, 0.0f, 1.0f ) ), y + S( 8 ) );
			Texto( m_hPequena, cx, y + S( 12 ), COR_BRANCO, "SENDO REVIVIDO" );
		}
		else
		{
			Texto( m_hPequena, cx, y, COR_CINZA, pEstado->TemKit( pLocal->entindex() ) ? "O KIT VAI TE LEVANTAR" : "ESPERE ALGUÉM TE REVIVER" );
		}
		return;
	}

	// Derrubado perto: reviver tem preferência sobre comprar.
	const float flDist = 110.0f;
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !TrokHordas_Caido( p ) )
			continue;
		if ( ( p->GetAbsOrigin() - pLocal->GetAbsOrigin() ).LengthSqr() > flDist * flDist )
			continue;
		V_snprintf( sz, sizeof( sz ), "SEGURE E  —  REVIVER %s", g_PR->GetPlayerName( i ) );
		Texto( m_hTexto, cx, y, COR_BRANCO, sz );
		const float flProg = pEstado->GetReviver( i );
		if ( flProg > 0.0f )
		{
			const int w = S( 220 );
			const int yb = y + S( 26 );
			surface()->DrawSetColor( COR_FUNDO );
			surface()->DrawFilledRect( cx - w / 2, yb, cx + w / 2, yb + S( 8 ) );
			surface()->DrawSetColor( COR_DESTAQUE );
			surface()->DrawFilledRect( cx - w / 2, yb, cx - w / 2 + (int)( w * clamp( flProg, 0.0f, 1.0f ) ), yb + S( 8 ) );
		}
		return;
	}

	CTrokHordasEstacao *pEstacao = CTrokHordasEstacao::Mirada( pLocal );
	if ( !pEstacao )
		return;
	bool bPode = true;
	if ( !pEstacao->Aviso( pLocal, sz, sizeof( sz ), &bPode ) || !sz[ 0 ] )
		return;
	Texto( m_hTexto, cx, y, bPode ? COR_BRANCO : COR_PERIGO, sz );
}

// Colegas derrubados marcados na tela (cruz e distância).
void CHudTrokHordas::PintarCaidos( CTrokHordasEstado *pEstado, C_BasePlayer *pLocal )
{
	for ( int i = 1; i <= gpGlobals->maxClients; ++i )
	{
		C_BasePlayer *p = UTIL_PlayerByIndex( i );
		if ( !p || p == pLocal || p->IsDormant() || !TrokHordas_Caido( p ) )
			continue;
		int x, y;
		if ( !GetVectorInScreenSpace( p->GetAbsOrigin() + Vector( 0, 0, 40 ), x, y ) )
			continue;
		const int t = S( 7 );
		surface()->DrawSetColor( COR_PERIGO );	// fixo: nada piscando (decisão do dono)
		surface()->DrawFilledRect( x - t, y - S( 2 ), x + t, y + S( 2 ) );
		surface()->DrawFilledRect( x - S( 2 ), y - t, x + S( 2 ), y + t );
		char sz[ 32 ];
		V_snprintf( sz, sizeof( sz ), "%d", (int)( ( p->GetAbsOrigin() - pLocal->GetAbsOrigin() ).Length() / 39.37f ) );
		Texto( m_hPequena, x, y + S( 10 ), COR_PERIGO, sz );
	}
}

// Bônus ativos com o tempo e a faixa do último pego.
void CHudTrokHordas::PintarBonus( CTrokHordasEstado *pEstado )
{
	const float flIdade = gpGlobals->curtime - pEstado->GetUltimoBonusHora();
	if ( flIdade >= 0.0f && flIdade < 2.5f )
		Faixa( s_pszBonusNome[ clamp( pEstado->GetUltimoBonus(), 0, HORDAS_BONUS_NUM - 1 ) ], S( 120 ), COR_DESTAQUE );

	int x = ScreenWidth() / 2;
	const int y = ScreenHeight() - S( 70 );
	char sz[ 64 ];
	int n = 0;
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
		if ( pEstado->BonusAtivo( b ) )
			++n;
	if ( n == 0 )
		return;
	x -= ( n - 1 ) * S( 90 );
	for ( int b = 0; b < HORDAS_BONUS_NUM; ++b )
	{
		if ( !pEstado->BonusAtivo( b ) )
			continue;
		const float flFalta = pEstado->GetBonusAte( b ) - gpGlobals->curtime;
		const bool bPisca = flFalta < 5.0f && ( (int)( gpGlobals->curtime * 4.0f ) % 2 ) == 0;
		V_snprintf( sz, sizeof( sz ), "%s %d", s_pszBonusNome[ b ], (int)ceilf( flFalta ) );
		if ( !bPisca )
			Texto( m_hPequena, x, y, COR_DESTAQUE, sz );
		x += S( 180 );
	}
}
