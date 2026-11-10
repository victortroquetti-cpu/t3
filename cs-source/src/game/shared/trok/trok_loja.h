//=============================================================================
// Trok: dados da loja (economia estilo CS). Lidos de scripts/trok_loja.txt pelos dois
// lados: o servidor vende (server/trok/trok_economia.cpp) e o cliente mostra o menu de
// compra (client/trok/ui/trok_loja.cpp). Ver design/economia.md.
//=============================================================================
#ifndef TROK_LOJA_H
#define TROK_LOJA_H
#ifdef _WIN32
#pragma once
#endif

#include "utlvector.h"

#define TROK_LOJA_ARQUIVO "scripts/trok_loja.txt"

enum TrokLojaTipo_t
{
	LOJA_ARMA = 0,		// arma (entidade weapon_*), vem cheia de munição
	LOJA_COLETE,		// armadura + fôlego + velocidade (níveis: compra só nível acima)
	LOJA_CAPACETE,		// protege o tiro na cabeça (reduz ou impede o crítico)
	LOJA_MUNICAO,		// enche a reserva da arma de um slot (ou de todas)
	LOJA_MEDICO,		// devolve vida na hora
	LOJA_MUTAGENICO,	// Surto: libera um tipo de zumbi (trok_zumbis.cpp) para quando virar zumbi
};

struct TrokLojaItem_t
{
	char szId[ 32 ];			// nome no "trok_comprar <id>"
	char szNome[ 48 ];			// nome na tela
	char szCategoria[ 32 ];		// id da categoria
	char szDescricao[ 128 ];
	char szEntidade[ 64 ];		// arma: weapon_*
	int iTipo;					// TrokLojaTipo_t
	int iPreco;
	int iTime;					// 0 = todos; senão só esse time (2 ou 3)
	int iMunicao;				// arma: reserva dada junto (-1 = cheia)
	int iMax;					// arma de arremesso: quantas pode carregar (0 = uma arma só)
	int iSlot;					// munição: slot da arma (bucket; -1 = todas)
	int iNivel;					// colete: nível (só compra nível maior que o que tem)
	int iArmadura;				// colete: pontos de armadura
	float flFolego;				// colete: multiplica o fôlego da corrida (1 = normal)
	float flVelocidade;			// colete: multiplica a velocidade (1 = normal)
	float flProtecao;			// capacete: fração do dano extra da cabeça que tira (1 = sem crítico)
	int iGolpes;				// capacete: tiros na cabeça que aguenta (0 = não quebra)
	int iVida;					// médico: vida devolvida (até a máxima)
	char szZumbi[ 32 ];			// mutagênico: nome do tipo em scripts/trok_zumbis.txt
};

struct TrokLojaCategoria_t
{
	char szId[ 32 ];
	char szNome[ 48 ];
};

class CTrokLojaDados
{
public:
	CTrokLojaDados() : m_bCarregada( false ), m_iAbatePadrao( 300 ) {}

	void Carregar( void );
	bool IsCarregada( void ) const { return m_bCarregada; }

	const TrokLojaItem_t *Item( const char *pszId ) const;
	int ItemIndice( const char *pszId ) const;
	// Classe de uma arma ou projétil (weapon_trok_fuzil -> "fuzil"); "" se não está na lista.
	const char *Classe( const char *pszEntidade ) const;
	// Ganho por abate com essa arma (pela classe; senão o padrão).
	int GanhoAbate( const char *pszEntidade ) const;

	CUtlVector< TrokLojaCategoria_t > m_Categorias;
	CUtlVector< TrokLojaItem_t > m_Itens;

private:
	struct Par_t { char szChave[ 64 ]; char szValor[ 32 ]; int iValor; };
	CUtlVector< Par_t > m_Classes;		// entidade -> classe
	CUtlVector< Par_t > m_Abate;		// classe -> $
	bool m_bCarregada;
	int m_iAbatePadrao;
};

// Carrega na primeira vez.
CTrokLojaDados &TrokLoja( void );

#endif // TROK_LOJA_H
