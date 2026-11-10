//=============================================================================
// Trok: modo Corrida Armada (trok_modo corrida), o "Arms Race" do Combat Arms (modo M14,
// design/chimera/specs/combat_arms/game_modes.md e missions.md F4.8; design/modos.md).
//
// Todos contra todos (o exército solo com uma escada de armas): cada degrau da escada
// começa num número de abates; quando os abates de inimigo do jogador ficam IGUAIS ao de
// um degrau, a arma dele vira a desse degrau (e ao nascer, a do degrau em que ele está).
// Vence quem chegar na meta de abates (mp_fraglimit: 15, 30 ou 45). A escada (uma por meta)
// fica em scripts/trok_corrida.txt; os números do modo em cfg/corrida.cfg.
//
// Estado em rede numa entidade só (trok_corrida_estado): meta, degrau e abates de cada
// jogador, quem chegou na última arma e o vencedor. O HUD (client/trok/hud_trok_corrida.cpp)
// e o placar só leem.
//=============================================================================
#ifndef TROK_CORRIDA_H
#define TROK_CORRIDA_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#define CTrokCorridaEstado C_TrokCorridaEstado
#endif

class CTakeDamageInfo;

//-----------------------------------------------------------------------------
// Escada de armas (scripts/trok_corrida.txt), lida igual nos dois lados.
//-----------------------------------------------------------------------------
#define TROK_CORRIDA_MAX_DEGRAUS	32
#define TROK_CORRIDA_MAX_ARMAS		8

struct TrokCorridaArma_t
{
	char szClasse[ 64 ];		// weapon_trok_fuzil, ...
	int iPeso;					// peso no sorteio do degrau (CA: Set_rate)
};

struct TrokCorridaDegrau_t
{
	int iAbates;				// abates em que o degrau começa (CA: Kill<i>)
	char szNome[ 32 ];			// nome mostrado no HUD
	int iMunicao;				// reserva dada com a arma (-1 = a de trok_municao_inicial)
	int nArmas;
	TrokCorridaArma_t armas[ TROK_CORRIDA_MAX_ARMAS ];
};

struct TrokCorridaEscada_t
{
	int iMeta;					// meta da escada (CA: Goal); 0 = nenhuma carregada
	int nDegraus;
	TrokCorridaDegrau_t degraus[ TROK_CORRIDA_MAX_DEGRAUS ];
};

// Escada da meta pedida (a de meta igual; senão a de meta mais perto). Guardada em cache;
// bRecarregar relê o arquivo. NULL se o arquivo não tem nenhuma escada válida.
const TrokCorridaEscada_t *TrokCorrida_Escada( int iMeta, bool bRecarregar = false );
// Degrau em que está quem tem iAbates (o último cujo início é <= iAbates).
int TrokCorrida_DegrauDosAbates( const TrokCorridaEscada_t *pEscada, int iAbates );

//-----------------------------------------------------------------------------
// Estado em rede
//-----------------------------------------------------------------------------
class CTrokCorridaEstado : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokCorridaEstado, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CTrokCorridaEstado();
	virtual ~CTrokCorridaEstado();

#ifndef CLIENT_DLL
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
#endif

	static bool Valido( int i ) { return i > 0 && i <= MAX_PLAYERS; }

	int GetMeta( void ) const { return m_iMeta; }
	int GetDegraus( void ) const { return m_iDegraus; }
	int GetDegrau( int i ) const { return Valido( i ) ? m_aDegrau[ i ] : 0; }		// 0 = primeira arma
	int GetAbates( int i ) const { return Valido( i ) ? m_aAbates[ i ] : 0; }		// abates de inimigo na partida
	int GetUltimoJogador( void ) const { return m_iUltimo; }						// último a chegar na última arma
	float GetUltimoHora( void ) const { return m_flUltimoHora; }
	int GetVencedor( void ) const { return m_iVencedor; }							// 0 = ninguém (ainda)
	bool IsFim( void ) const { return m_bFim; }

	CNetworkVar( int, m_iMeta );
	CNetworkVar( int, m_iDegraus );
	CNetworkArray( int, m_aDegrau, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aAbates, MAX_PLAYERS + 1 );
	CNetworkVar( int, m_iUltimo );
	CNetworkVar( float, m_flUltimoHora );
	CNetworkVar( int, m_iVencedor );
	CNetworkVar( bool, m_bFim );
};

CTrokCorridaEstado *TrokCorrida_Estado( void );
bool TrokCorrida_Ativo( void );		// existe o estado (o modo é a Corrida Armada)

#ifndef CLIENT_DLL
class CBasePlayer;
class CBaseCombatWeapon;

// Chamados por CTrokRules (trok_gamerules.cpp):
void TrokCorrida_Iniciar( bool bAtivo );		// construtor das regras: liga ou desliga o modo
void TrokCorrida_Think( void );				// todo frame
void TrokCorrida_Abate( CBasePlayer *pVictim, const CTakeDamageInfo &info, int iFragsAntes );	// PlayerKilled, depois da base
bool TrokCorrida_PodeTerArma( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );	// CanHavePlayerItem

// Chamados por hl2mp_player.cpp (patch, #ifdef TROK):
bool TrokCorrida_DarArmas( CBasePlayer *pPlayer );		// GiveDefaultItems: true = a Corrida deu as armas
void TrokCorrida_AntesDeMorrer( CBasePlayer *pPlayer );	// Event_Killed: sem arma caindo (CA)
#endif

#endif // TROK_CORRIDA_H
