//=============================================================================
// Trok: modo Hordas (trok_modo hordas). Cooperativo contra hordas de zumbis por rodadas,
// inspirado na Equipe Armada (Fireteam) do Combat Arms e nos zumbis do Call of Duty.
// Desenho completo: design/modo-hordas.md.
//
// - Diretor de rodadas (server/trok/trok_hordas_diretor.cpp): zumbis são bots NextBot do
//   time dos zumbis, num grupo reaproveitado (máximo vivos ao mesmo tempo, total por rodada);
//   vida, velocidade, quantidade e tipos crescem por rodada (cvars em cfg/hordas.cfg e a
//   tabela de tipos em scripts/trok_hordas.txt).
// - Pontos por jogador (separados do dinheiro da economia estilo CS): acerto, abate, cabeça,
//   faca, reparo de barricada. Gastos em estações físicas no mapa, com E (+use).
// - Estações (este arquivo, CTrokHordasEstacao, uma classe em rede para todas): arma na
//   parede, porta paga, máquina de melhoria (5 níveis, scripts/trok_upgrade.txt), caixa
//   misteriosa, bancada de equipamentos, chave de energia.
// - Derrubado e reviver, bônus que caem dos zumbis, rodadas especiais e chefe.
//
// Estado em rede numa entidade (trok_hordas_estado); o HUD (client/trok/hud_trok_hordas.cpp)
// só lê. Os tipos de zumbi (classe de cada zumbi, lentidão) continuam na entidade do Surto
// (trok_surto_estado), que o Hordas também cria, desligada (fase SURTO_DESLIGADO).
//=============================================================================
#ifndef TROK_HORDAS_H
#define TROK_HORDAS_H
#ifdef _WIN32
#pragma once
#endif

#ifdef CLIENT_DLL
	#define CTrokHordasEstado C_TrokHordasEstado
	#define CTrokHordasEstacao C_TrokHordasEstacao
#endif
class CBaseCombatWeapon;	// no cliente vira C_BaseCombatWeapon (shared_classnames.h)
class CBasePlayer;

#define TROK_HORDAS_SLOTS		6		// classes de arma do inventário do CA (bucket 0..5)
#define TROK_UPGRADE_MAX		5		// níveis da máquina de melhoria
#define TROK_DEFESA_MAX		3		// níveis de defesa (torreta, claymore, mina, remota) na mesma máquina

enum TrokHordasFase_t
{
	HORDAS_ESPERA = 0,		// ninguém jogando ainda
	HORDAS_INTERVALO,		// entre rodadas (contagem para a próxima)
	HORDAS_RODADA,			// zumbis nascendo
	HORDAS_FIM,				// todos caíram: fim de jogo, depois recomeça
};

enum TrokHordasRodadaTipo_t
{
	HORDAS_RODADA_NORMAL = 0,
	HORDAS_RODADA_MATILHA,	// rodada especial: só corredores fracos, prêmio no fim (os "cães" do CoD)
	HORDAS_RODADA_CHEFE,	// rodada com chefe (Colosso)
};

// Bônus que caem dos zumbis (os "power-ups" do CoD, com nomes nossos).
enum TrokHordasBonus_t
{
	HORDAS_BONUS_MUNICAO = 0,	// Reabastecimento: munição cheia para todos
	HORDAS_BONUS_ABATE_CERTO,	// Abate certo: um acerto derruba qualquer zumbi (por tempo)
	HORDAS_BONUS_PONTOS_DOBRO,	// Pontos em dobro (por tempo)
	HORDAS_BONUS_ATAQUE_AEREO,	// Ataque aéreo: mata os zumbis vivos, pontos para todos
	HORDAS_BONUS_REFORCO,		// Reforço: conserta todas as barricadas, pontos para todos
	HORDAS_BONUS_CAIXA_BARATA,	// Caixa barata: a caixa misteriosa custa pouco (por tempo)
	HORDAS_BONUS_NUM
};

enum TrokEstacaoTipo_t
{
	ESTACAO_PAREDE = 0,		// func_trok_compra_parede
	ESTACAO_PORTA,			// func_trok_porta_paga
	ESTACAO_UPGRADE,		// func_trok_maquina_upgrade
	ESTACAO_CAIXA,			// func_trok_caixa_misteriosa
	ESTACAO_BANCADA,		// func_trok_bancada
	ESTACAO_ENERGIA,		// func_trok_energia
	ESTACAO_ARMADILHA,		// func_trok_armadilha (painel; a zona é um trigger_trok_armadilha)
};

//-----------------------------------------------------------------------------
// Máquina de melhoria (scripts/trok_upgrade.txt), lida igual nos dois lados.
// niveis[0] é a arma sem melhoria (multiplicadores 1).
//-----------------------------------------------------------------------------
struct TrokUpgradeNivel_t
{
	char szNome[ 32 ];			// nome do nível ("MK II")
	int iPreco;					// pontos para chegar neste nível
	float flDano;				// x dano da bala
	float flPente;				// x tamanho do pente
	float flReserva;			// x reserva
	char szEfeito[ 32 ];		// "", "atordoante", "incendiaria", "explosiva"
	float flEfeitoChance;		// chance por acerto (0..1)
	float flEfeitoForca;		// lentidão (x velocidade), fração da vida por segundo (fogo) ou fração do dano (explosão)
	float flEfeitoTempo;		// s de lentidão ou de fogo
	float flEfeitoRaio;			// explosão
	int iCor[ 3 ];				// tom provisório da arma no mundo até haver camuflagem própria
};

struct TrokUpgrade_t
{
	int nNiveis;									// níveis carregados (até TROK_UPGRADE_MAX)
	int iMunicaoPreco;								// encher munição de arma melhorada na parede
	TrokUpgradeNivel_t niveis[ TROK_UPGRADE_MAX + 1 ];
	int nDefesa;									// níveis de defesa carregados (bloco "defesa")
	TrokUpgradeNivel_t defesa[ TROK_DEFESA_MAX + 1 ];	// dano das defesas postas pelo jogador
};

const TrokUpgrade_t &TrokUpgrade( bool bRecarregar = false );

//-----------------------------------------------------------------------------
// Estado em rede
//-----------------------------------------------------------------------------
class CTrokHordasEstado : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokHordasEstado, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CTrokHordasEstado();
	virtual ~CTrokHordasEstado();

#ifndef CLIENT_DLL
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }
#endif

	static bool Valido( int i ) { return i > 0 && i <= MAX_PLAYERS; }

	int GetFase( void ) const { return m_iFase; }
	float GetFimFase( void ) const { return m_flFimFase; }
	int GetRodada( void ) const { return m_iRodada; }
	int GetRodadaTipo( void ) const { return m_iRodadaTipo; }
	int GetRestantes( void ) const { return m_iRestantes; }		// zumbis que faltam matar nesta rodada
	int GetRecorde( void ) const { return m_iRecorde; }			// maior rodada alcançada no mapa
	bool IsEnergia( void ) const { return m_bEnergia; }
	int GetPontos( int i ) const { return Valido( i ) ? m_aPontos[ i ] : 0; }
	int GetAbates( int i ) const { return Valido( i ) ? m_aAbates[ i ] : 0; }
	float GetCaidoAte( int i ) const { return Valido( i ) ? m_aCaidoAte[ i ] : 0.0f; }	// 0 = de pé; senão, quando sangra até morrer
	float GetReviver( int i ) const { return Valido( i ) ? m_aReviver[ i ] : 0.0f; }		// progresso de quem o revive (0..1)
	bool TemKit( int i ) const { return Valido( i ) && m_aKit[ i ] != 0; }					// kit de reanimação comprado
	float GetVelZumbi( int i ) const { return Valido( i ) ? m_aVelZumbi[ i ] : 1.0f; }
	int GetNivelDefesa( int i ) const { return Valido( i ) ? m_aNivelDefesa[ i ] : 0; }	// torreta/claymore/mina/remota
	int GetNivel( int iJogador, int iSlot ) const
	{
		if ( !Valido( iJogador ) || iSlot < 0 || iSlot >= TROK_HORDAS_SLOTS )
			return 0;
		return m_aNivel[ iJogador * TROK_HORDAS_SLOTS + iSlot ];
	}
	float GetBonusAte( int b ) const { return ( b >= 0 && b < HORDAS_BONUS_NUM ) ? m_aBonusAte[ b ] : 0.0f; }
	bool BonusAtivo( int b ) const { return gpGlobals->curtime < GetBonusAte( b ); }
	int GetUltimoBonus( void ) const { return m_iUltimoBonus; }		// último bônus pego (faixa no HUD)
	float GetUltimoBonusHora( void ) const { return m_flUltimoBonus; }

	CNetworkVar( int, m_iFase );
	CNetworkVar( float, m_flFimFase );
	CNetworkVar( int, m_iRodada );
	CNetworkVar( int, m_iRodadaTipo );
	CNetworkVar( int, m_iRestantes );
	CNetworkVar( int, m_iRecorde );
	CNetworkVar( bool, m_bEnergia );
	CNetworkArray( int, m_aPontos, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aAbates, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aCaidoAte, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aReviver, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aKit, MAX_PLAYERS + 1 );
	CNetworkArray( float, m_aVelZumbi, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aNivelDefesa, MAX_PLAYERS + 1 );
	CNetworkArray( int, m_aNivel, ( MAX_PLAYERS + 1 ) * TROK_HORDAS_SLOTS );
	CNetworkArray( float, m_aBonusAte, HORDAS_BONUS_NUM );
	CNetworkVar( int, m_iUltimoBonus );
	CNetworkVar( float, m_flUltimoBonus );
};

CTrokHordasEstado *TrokHordas_Estado( void );
bool TrokHordas_Ativo( void );							// o modo é o Hordas (o estado existe)
bool TrokHordas_Caido( const CBaseEntity *pEnt );		// humano derrubado esperando reviver
int TrokHordas_NivelArma( const CBaseCombatWeapon *pArma );		// nível de melhoria (0 = nenhum)
int TrokHordas_Pente( const CBaseCombatWeapon *pArma, int iBase );	// pente com a melhoria (weapon_trok_gun)
float TrokHordas_VelocidadeZumbi( const CBaseEntity *pPlayer );	// x velocidade do zumbi pela rodada (1 fora do modo)
int TrokHordas_PrecoCaixa( int iBase );
// Armas de defesa (postas no chão e atiram/explodem sozinhas): torreta, claymore, mina, remota.
// Na máquina de melhoria sobem o "nível de defesa" do jogador (vale para tudo o que ele põe).
bool TrokHordas_EhArmaDefesa( const char *pszClasse );						// preço da caixa (com o bônus Caixa barata)

//-----------------------------------------------------------------------------
// Estação de compra (todas as estações: uma classe só em rede; o tipo vem do nome da classe)
//-----------------------------------------------------------------------------
class CTrokHordasEstacao : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokHordasEstacao, CBaseEntity );
	DECLARE_NETWORKCLASS();
#ifndef CLIENT_DLL
	DECLARE_DATADESC();
#endif

	CTrokHordasEstacao();
	virtual ~CTrokHordasEstacao();

	int GetTipo( void ) const { return m_iTipo; }
	int GetPreco( void ) const { return m_iPreco; }
	int GetPreco2( void ) const { return m_iPreco2; }
	int GetEstadoCaixa( void ) const { return m_iEstado; }		// caixa: 0 parada, 1 girando, 2 oferecendo; armadilha: 0 pronta, 1 ligada, 2 recarregando
	int GetDono( void ) const { return m_iDono; }				// caixa: quem pagou
	bool PrecisaEnergia( void ) const { return m_bPrecisaEnergia; }
	const char *GetRotulo( void ) const { return m_szRotulo.Get(); }
	const char *GetArma( void ) const { return m_szArma.Get(); }	// parede: classe da arma; caixa: a oferecida
	bool IsUsada( void ) const { return m_bUsada; }				// porta aberta / energia ligada

	// Lista de todas (o HUD procura a que o jogador olha).
	static int Num( void );
	static CTrokHordasEstacao *Get( int i );
	// Texto do aviso para o jogador ("E — COMPRAR SUBMETRALHADORA: 1000"). false = nada a mostrar.
	bool Aviso( CBasePlayer *pPlayer, char *pszTexto, int iTam, bool *pbPode ) const;
	// A estação que o jogador olha a até trok_hordas_usar_distancia (mesma conta nos dois lados).
	static CTrokHordasEstacao *Mirada( CBasePlayer *pPlayer );

#ifndef CLIENT_DLL
	virtual void Precache( void );
	virtual void Spawn( void );
	virtual bool KeyValue( const char *szKeyName, const char *szValue );
	virtual int ObjectCaps( void ) { return BaseClass::ObjectCaps() | FCAP_IMPULSE_USE; }
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int UpdateTransmitState( void ) { return SetTransmitState( FL_EDICT_ALWAYS ); }

	void InputLigar( inputdata_t &inputdata );
	void InputDesligar( inputdata_t &inputdata );
	void InputAbrir( inputdata_t &inputdata );
	void CaixaPensar( void );
	void AbrirPorta( CBasePlayer *pPlayer );
	void LigarEnergia( CBasePlayer *pPlayer );
	void ArmadilhaPensar( void );
	bool IsDesligada( void ) const { return m_bDesligada; }

private:
	void UsarParede( CBasePlayer *pPlayer );
	void UsarUpgrade( CBasePlayer *pPlayer );
	void UsarCaixa( CBasePlayer *pPlayer );
	void UsarBancada( CBasePlayer *pPlayer );
	void UsarArmadilha( CBasePlayer *pPlayer );

	string_t m_iszArmaMapa;			// keyvalue "arma" / "item"
	string_t m_iszRotuloMapa;		// keyvalue "rotulo"
	int m_iArea;					// porta: área que abre
	int m_iUsos;					// caixa: quantas vezes foi aberta
	float m_flProxUso;
	float m_flFimEstado;			// caixa: quando acaba de girar / de oferecer
	bool m_bDesligada;				// entrada Desligar (o mapa pode esconder uma caixa)
	int m_iEnergiaMapa;				// keyvalue precisa_energia (-1 = padrão do tipo)
	// Armadilha (keyvalues): tipo eletrica/fogo/arame, zona (nome do trigger_trok_armadilha),
	// dano por segundo nos zumbis e nos humanos, lentidão, tempo ligada e de recarga.
	string_t m_iszZona;
	float m_flArmDps, m_flArmDpsHumano, m_flArmLentidao, m_flArmDuracao, m_flArmRecarga;
	CHandle< CBasePlayer > m_hAtivador;
	float m_flProxEfeito;
public:
	CBasePlayer *Ativador( void ) const { return m_hAtivador.Get(); }
	bool ArmadilhaLigada( void ) const { return m_iTipo == ESTACAO_ARMADILHA && m_iEstado == 1; }
private:
	COutputEvent m_OnUsada;			// comprou / abriu / ligou
	COutputEvent m_OnSemPontos;
#endif

private:
	CNetworkVar( int, m_iTipo );
	CNetworkVar( int, m_iPreco );
	CNetworkVar( int, m_iPreco2 );
	CNetworkVar( int, m_iEstado );
	CNetworkVar( int, m_iDono );
	CNetworkVar( bool, m_bPrecisaEnergia );
	CNetworkVar( bool, m_bUsada );
	CNetworkString( m_szRotulo, 48 );
	CNetworkString( m_szArma, 48 );
};

#ifndef CLIENT_DLL
class CBasePlayer;
class CTakeDamageInfo;
class CCommand;

// Chamados por CTrokRules (trok_gamerules.cpp):
void TrokHordas_Iniciar( bool bAtivo );		// construtor das regras
void TrokHordas_Think( void );				// todo frame
void TrokHordas_PlayerSpawn( CBasePlayer *pPlayer );		// ao nascer (aplica zumbi ou humano)
void TrokHordas_Abate( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// PlayerKilled, depois da base
void TrokHordas_Dano( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// depois de cada dano (OnPlayerDamaged)
bool TrokHordas_PodeLevarDano( CBasePlayer *pPlayer, CBaseEntity *pAttacker );	// FPlayerCanTakeDamage
bool TrokHordas_PodeRenascer( CBasePlayer *pPlayer );		// FPlayerCanRespawn
bool TrokHordas_PodeTerArma( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem );	// CanHavePlayerItem
bool TrokHordas_ClientCommand( CBasePlayer *pPlayer, const CCommand &args );

// Usados pelas estações e entidades de mapa (trok_hordas_estacao.cpp, trok_hordas_mapa.cpp, func_trok_grade):
bool TrokHordas_Pagar( CBasePlayer *pPlayer, int iPreco );	// tira os pontos se tiver (avisa se não)
void TrokHordas_DarPontos( CBasePlayer *pPlayer, int iPontos, bool bDobro = true );
void TrokHordas_PontosReparo( CBasePlayer *pPlayer );		// consertou uma tábua de barricada
void TrokHordas_AbrirArea( int iArea );
bool TrokHordas_AreaAberta( int iArea );
void TrokHordas_LigarEnergia( void );
void TrokHordas_DarArma( CBasePlayer *pPlayer, const char *pszArma );	// troca a da mesma classe
void TrokHordas_EncherMunicao( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma );
bool TrokHordas_Melhorar( CBasePlayer *pPlayer, CBaseCombatWeapon *pArma );	// sobe um nível
const char *TrokHordas_SortearArmaCaixa( CBasePlayer *pPlayer );
bool TrokHordas_MelhorarDefesa( CBasePlayer *pPlayer );				// sobe o nível de defesa do jogador
// Quantas de cada objeto um jogador pode ter postas ao mesmo tempo (server/trok/trok_projetil.cpp).
// Fora do Hordas devolve iPadrao (torreta do CA: 1, as outras sem limite).
int TrokHordas_LimitePostos( const char *pszEntidade, int iPadrao );
int TrokHordas_ContarPostos( CBasePlayer *pDono, const char *pszEntidade );
void TrokHordas_DarKit( CBasePlayer *pPlayer );
int TrokHordas_ArmadilhasLigadas( CBasePlayer *pPlayer );			// armadilhas que este jogador ligou e estão ligadas
void TrokHordas_LentoZumbi( CBasePlayer *pZumbi, float flFator, float flTempo );	// arame, munição atordoante
#endif

#endif // TROK_HORDAS_H
