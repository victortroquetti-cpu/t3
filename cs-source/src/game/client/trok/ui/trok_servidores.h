//=============================================================================
// Trok: lista de servidores própria (tela SERVIDORES do menu, trok_menu.cpp), no lugar
// do navegador de servidores do GameUI. Pede a lista à Steam pelo cliente
// (ISteamMatchmakingServers): internet (servidor mestre, filtrado pela pasta do mod) ou
// rede local (broadcast na LAN; VPNs que repassam broadcast, como Radmin e ZeroTier,
// também aparecem). Fora disso, "conectar por IP" no próprio menu.
//=============================================================================
#ifndef TROK_SERVIDORES_H
#define TROK_SERVIDORES_H
#ifdef _WIN32
#pragma once
#endif

struct TrokServidor_t
{
	char szNome[ 64 ];
	char szMapa[ 32 ];
	char szEndereco[ 32 ];		// ip:porta para o "connect"
	int iJogadores;
	int iMaximo;
	int iBots;
	int iPing;
	bool bSenha;
};

namespace TrokServidores
{
	void Atualizar( bool bInternet );		// começa uma busca nova (cancela a anterior)
	void Parar( void );
	bool Atualizando( void );
	bool Internet( void );					// a última busca foi na internet
	int Quantos( void );
	const TrokServidor_t &Servidor( int i );	// ordenados por ping
	const char *Erro( void );				// "" ou o motivo de não ter lista (sem Steam...)
}

#endif // TROK_SERVIDORES_H
