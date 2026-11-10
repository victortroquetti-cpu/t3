//=============================================================================
// Trok: lista de servidores pela Steam (ver trok_servidores.h).
//
// A resposta de cada servidor chega por ISteamMatchmakingServerListResponse, chamada
// pelo SteamAPI_RunCallbacks que o motor já roda todo quadro. Só entram servidores da
// mesma pasta de mod (gamedir), então servidores de HL2DM e de outros mods do SDK ficam
// de fora.
//=============================================================================

#include "cbase.h"
#include "trok_servidores.h"
// Os cabeçalhos públicos da Steam usam strncpy de propósito (inline em matchmakingtypes.h);
// o SDK troca strncpy por um nome que não existe, o que quebra a ligação. Libera só aqui.
#ifdef strncpy
#undef strncpy
#endif
#include <string.h>
#include "steam/steam_api.h"
#include "steam/isteammatchmaking.h"
#include "tier1/utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar trok_servidores_pasta( "trok_servidores_pasta", "", FCVAR_ARCHIVE,
	"Pasta de mod dos servidores listados (vazio = a deste jogo)." );

class CTrokListaServidores : public ISteamMatchmakingServerListResponse
{
public:
	CTrokListaServidores() : m_hPedido( NULL ), m_bAtualizando( false ), m_bInternet( true ) { m_szErro[ 0 ] = 0; }
	// Sem Parar() no destrutor: no fim do processo a Steam pode já ter sido desligada.

	void Atualizar( bool bInternet );
	void Parar( void );

	// ISteamMatchmakingServerListResponse
	virtual void ServerResponded( HServerListRequest hRequest, int iServer );
	virtual void ServerFailedToRespond( HServerListRequest hRequest, int iServer ) {}
	virtual void RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response );

	HServerListRequest m_hPedido;
	bool m_bAtualizando;
	bool m_bInternet;
	char m_szErro[ 128 ];
	CUtlVector< TrokServidor_t > m_Lista;

private:
	const char *Pasta( void );
};

static CTrokListaServidores s_Lista;

static ISteamMatchmakingServers *TrokMMServidores( void )
{
	return ( steamapicontext ) ? steamapicontext->SteamMatchmakingServers() : NULL;
}

const char *CTrokListaServidores::Pasta( void )
{
	static char s_szPasta[ 64 ];
	if ( trok_servidores_pasta.GetString()[ 0 ] )
		return trok_servidores_pasta.GetString();
	// engine->GetGameDirectory() é o caminho inteiro; o servidor anuncia só o nome da pasta.
	V_FileBase( engine->GetGameDirectory(), s_szPasta, sizeof( s_szPasta ) );
	return s_szPasta;
}

void CTrokListaServidores::Parar( void )
{
	ISteamMatchmakingServers *pMM = TrokMMServidores();
	if ( m_hPedido && pMM )
	{
		pMM->CancelQuery( m_hPedido );
		pMM->ReleaseRequest( m_hPedido );
	}
	m_hPedido = NULL;
	m_bAtualizando = false;
}

void CTrokListaServidores::Atualizar( bool bInternet )
{
	Parar();
	m_Lista.RemoveAll();
	m_szErro[ 0 ] = 0;
	m_bInternet = bInternet;

	ISteamMatchmakingServers *pMM = TrokMMServidores();
	if ( !pMM || !steamapicontext->SteamUtils() )
	{
		V_strncpy( m_szErro, "Steam indisponível: abra o jogo pela Steam para ver a lista.", sizeof( m_szErro ) );
		return;
	}
	const AppId_t app = steamapicontext->SteamUtils()->GetAppID();
	if ( bInternet )
	{
		MatchMakingKeyValuePair_t filtro( "gamedir", Pasta() );
		MatchMakingKeyValuePair_t *pFiltros = &filtro;
		m_hPedido = pMM->RequestInternetServerList( app, &pFiltros, 1, this );
	}
	else
	{
		m_hPedido = pMM->RequestLANServerList( app, this );
	}
	m_bAtualizando = ( m_hPedido != NULL );
}

void CTrokListaServidores::ServerResponded( HServerListRequest hRequest, int iServer )
{
	ISteamMatchmakingServers *pMM = TrokMMServidores();
	if ( !pMM || hRequest != m_hPedido )
		return;
	gameserveritem_t *pItem = pMM->GetServerDetails( hRequest, iServer );
	if ( !pItem || !pItem->m_bHadSuccessfulResponse )
		return;
	if ( V_stricmp( pItem->m_szGameDir, Pasta() ) != 0 )
		return; // outro mod (a busca na LAN não tem filtro)

	TrokServidor_t s;
	V_strncpy( s.szNome, pItem->GetName(), sizeof( s.szNome ) );
	V_strncpy( s.szMapa, pItem->m_szMap, sizeof( s.szMapa ) );
	V_strncpy( s.szEndereco, pItem->m_NetAdr.GetConnectionAddressString(), sizeof( s.szEndereco ) );
	s.iJogadores = MAX( 0, pItem->m_nPlayers - pItem->m_nBotPlayers );
	s.iBots = pItem->m_nBotPlayers;
	s.iMaximo = pItem->m_nMaxPlayers;
	s.iPing = pItem->m_nPing;
	s.bSenha = pItem->m_bPassword;

	// Mesmo endereço já na lista: atualiza.
	FOR_EACH_VEC( m_Lista, i )
	{
		if ( !V_strcmp( m_Lista[ i ].szEndereco, s.szEndereco ) )
		{
			m_Lista.Remove( i );
			break;
		}
	}
	// Entra na ordem de ping.
	int iPos = 0;
	while ( iPos < m_Lista.Count() && m_Lista[ iPos ].iPing <= s.iPing )
		++iPos;
	m_Lista.InsertBefore( iPos, s );
}

void CTrokListaServidores::RefreshComplete( HServerListRequest hRequest, EMatchMakingServerResponse response )
{
	if ( hRequest != m_hPedido )
		return;
	m_bAtualizando = false;
	if ( response == eNoServersListedOnMasterServer && m_Lista.Count() == 0 && m_bInternet )
		V_strncpy( m_szErro, "Nenhum servidor na internet agora.", sizeof( m_szErro ) );
}

namespace TrokServidores
{
	void Atualizar( bool bInternet ) { s_Lista.Atualizar( bInternet ); }
	void Parar( void ) { s_Lista.Parar(); }
	bool Atualizando( void ) { return s_Lista.m_bAtualizando; }
	bool Internet( void ) { return s_Lista.m_bInternet; }
	int Quantos( void ) { return s_Lista.m_Lista.Count(); }
	const TrokServidor_t &Servidor( int i ) { return s_Lista.m_Lista[ i ]; }
	const char *Erro( void ) { return s_Lista.m_szErro; }
}
