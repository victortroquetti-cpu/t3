//=============================================================================
// Trok: base comum dos painéis próprios que ficam dentro da partida (seleção de time,
// boas-vindas do servidor, espectador). Mesmo visual e mesma ideia do menu
// (trok_menu.cpp): desenho imediato, Desenhar() roda para pintar e para tratar o
// clique com o mesmo código, então o que se vê é exatamente onde se clica.
// Cores e regras em design/ui.md.
//=============================================================================
#ifndef TROK_UI_COMUM_H
#define TROK_UI_COMUM_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui/IScheme.h>
#include <vgui_controls/Panel.h>
#include "Color.h"

namespace TrokUI
{
	// Paleta do concept (design/ui.md).
	extern const Color COR_FUNDO;
	extern const Color COR_PAINEL;
	extern const Color COR_PAINEL_HOVER;
	extern const Color COR_TEXTO;
	extern const Color COR_TEXTO2;
	extern const Color COR_TEXTO3;
	extern const Color COR_LINHA;
	extern const Color COR_LINHA_FORTE;
	extern const Color COR_ALIADO;
	extern const Color COR_INIMIGO;
	extern const Color COR_ZUMBI;

	Color Destaque( void );								// trok_ui_destaque
	Color CorDoTime( int iTeam );						// relativa ao jogador local (aliado azul, inimigo vermelho)
	const char *NomeDoTime( int iTeam );				// "MERCENÁRIOS", "REMANESCENTES", "HUMANOS"...
	bool ModoComTimes( void );							// mata-mata em equipe (não Solo, não Surto)
	bool ServidorLocal( void );							// conexão loopback (partida aberta por este PC)
	vgui::HScheme Esquema( void );						// resource/TrokScheme.res
	vgui::HFont Fonte( const char *pszNome );			// fonte do TrokScheme
	const wchar_t *W( const char *pszUTF8 );			// UTF-8 -> wchar_t (buffer rotativo)
}

// ESC com um painel do Trok aberto na partida: fecha o painel em vez de abrir o menu
// (chamado em CHLClient::HandleUiToggle; patch do sdk.patch).
bool TrokUI_CaptureESC( void );

// Pede a seleção de time logo que o modo da partida for conhecido (entrou no servidor).
void TrokTime_PedirAbrir( void );

//-----------------------------------------------------------------------------
// Painel de tela cheia com desenho imediato.
//-----------------------------------------------------------------------------
class CTrokPainel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CTrokPainel, vgui::Panel );
public:
	CTrokPainel( const char *pszNome );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void Paint( void );
	virtual void OnCursorMoved( int x, int y );
	virtual void OnMousePressed( vgui::MouseCode code );

protected:
	virtual void Desenhar( void ) = 0;		// pinta (Pintando()) ou trata o clique

	bool Pintando( void ) const { return !m_bModoClique; }
	int S( float flPx ) const { return (int)( flPx * m_flEscala + 0.5f ); }

	void Texto( vgui::HFont hFont, int x, int y, const Color &col, const char *psz );
	void TextoDir( vgui::HFont hFont, int xDir, int y, const Color &col, const char *psz );
	void TextoCentro( vgui::HFont hFont, int xCentro, int y, const Color &col, const char *psz );
	void TextoEspacado( vgui::HFont hFont, int x, int y, const Color &col, const char *psz, int iEspaco );
	int Largura( vgui::HFont hFont, const char *psz );
	int Altura( vgui::HFont hFont );
	void Ret( int x, int y, int w, int h, const Color &col );
	void Contorno( int x, int y, int w, int h, const Color &col );

	bool Sobre( int x, int y, int w, int h ) const;
	bool Clicou( int x, int y, int w, int h );
	// Botão com borda (ou cheio, na cor de destaque). pszTecla: atalho desenhado à esquerda.
	bool Botao( int x, int y, int w, int h, vgui::HFont hFont, const char *pszRotulo, bool bCheio, const char *pszTecla = NULL );
	// Dica de tecla no rodapé: [TECLA] Rótulo
	void Dica( int &x, int y, const char *pszTecla, const char *pszRotulo );

	vgui::HFont m_hLogo, m_hTitulo, m_hItem, m_hCabecalho, m_hBotao, m_hCorpo, m_hPequena, m_hNumero;
	int m_iLargura, m_iAltura;
	float m_flEscala;

private:
	bool m_bModoClique;
	bool m_bCliqueTratado;
	int m_iMouseX, m_iMouseY;
};

#endif // TROK_UI_COMUM_H
