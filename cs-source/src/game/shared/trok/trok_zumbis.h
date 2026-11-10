//=============================================================================
// Trok: tipos de zumbi (Surto e, no futuro, modo cooperativo contra zumbis).
//
// Cada tipo é um bloco em scripts/trok_zumbis.txt (números e habilidades), lido no cliente e
// no servidor. Baseado nos infectados do Combat Arms (design/ca-pesquisa/mutagenicos.md):
//   normal  (Mauler / infectado do Quarantine)
//   striker (mutagênico: rápida, fôlego infinito; no cooperativo solta bolhas que explodem)
//   crusher (mutagênico: grande, mais vida, regenera 20%, golpe mais longo e lento)
//   charger (Equipe Armada: explode ao morrer)
//   splicer (Equipe Armada: golpe paralisa, nuvem tóxica ao morrer)
// O jogador escolhe com trok_zumbi_classe <nome> (ou trok_zumbi_classe_prox) e COMPRA o tipo
// com a FÚRIA, o dinheiro de infectado (moeda separada do dinheiro de humano), pago na hora em que vira
// zumbi; sem dinheiro, vira o normal. Ganha-se dinheiro de infectado infectando, ferindo humanos
// e no fim da rodada (trok_surto_inf_*). O estado vai pela entidade do Surto.
//=============================================================================
#ifndef TROK_ZUMBIS_H
#define TROK_ZUMBIS_H
#ifdef _WIN32
#pragma once
#endif

#define TROK_ZUMBIS_MAX 16

struct TrokZumbiClasse_t
{
	char szNome[ 32 ];				// nome interno (comando)
	char szTitulo[ 48 ];			// nome que o jogador vê
	bool bJogavel;					// pode ser escolhido no Surto
	int iPreco;						// dinheiro de infectado para virar este tipo (pago ao virar, a cada vez)
	float flVidaHospedeiro;			// <= 0: usa trok_surto_vida_hospedeiro
	float flVida;					// <= 0: usa trok_surto_vida_zumbi
	float flVelocidade;				// multiplica trok_surto_zumbi_velocidade
	bool bFolegoInfinito;
	float flRegen;					// < 0: usa trok_surto_regen
	float flGarraDano, flGarraAlcance, flGarraCadencia;	// multiplicadores das garras
	float flEscala;					// tamanho do corpo (multiplica a escala do corpo do CA)
	int iCor[ 3 ];
	// Habilidades (0 = não tem)
	float flExplodeDano, flExplodeRaio;					// ao morrer
	float flNuvemDps, flNuvemRaio, flNuvemTempo, flNuvemLentidao;	// nuvem tóxica ao morrer
	float flAuraDps, flAuraRaio;							// em volta enquanto vivo
	float flBombaIntervalo, flBombaDano, flBombaRaio, flBombaPavio;	// solta bolhas que explodem
	float flGolpeLentidao, flGolpeLentidaoTempo;			// golpe que deixa o humano lento
	// Bônus de quem tem o mutagênico escolhido, enquanto humano (CA: +3% velocidade)
	float flBonusHumanoVelocidade;
};

int TrokZumbis_Num( void );
const TrokZumbiClasse_t &TrokZumbis_Classe( int i );
int TrokZumbis_Achar( const char *pszNome );		// -1 se não existe

class CBasePlayer;
class CBaseEntity;
int TrokZumbis_ClasseDe( const CBaseEntity *pPlayer );	// tipo de quem é zumbi agora (0 se não é)
int TrokZumbis_EscolhaDe( const CBaseEntity *pPlayer );
float TrokZumbis_VelocidadeDe( const CBaseEntity *pPlayer );	// zumbi: tipo; humano: bônus x lentidão
float TrokZumbis_GarraDano( const CBaseEntity *pOwner );
float TrokZumbis_GarraAlcance( const CBaseEntity *pOwner );
float TrokZumbis_GarraCadencia( const CBaseEntity *pOwner );
int TrokZumbis_Dinheiro( const CBaseEntity *pPlayer );		// dinheiro de infectado

#ifndef CLIENT_DLL
class CTakeDamageInfo;
bool TrokZumbis_ClientCommand( CBasePlayer *pPlayer, const CCommand &args );
void TrokZumbis_Liberar( CBasePlayer *pPlayer, int iClasse );		// libera o tipo de graça (evento/admin)
void TrokZumbis_Ganhar( CBasePlayer *pPlayer, int iValor );			// dinheiro de infectado
void TrokZumbis_Infectou( CBasePlayer *pZumbi );					// zumbi infectou um humano
void TrokZumbis_FimRodada( bool bZumbisVenceram );
bool TrokZumbis_Permitido( CBasePlayer *pPlayer, int iClasse );
int TrokZumbis_AoVirar( CBasePlayer *pPlayer );			// define o tipo ao virar zumbi; devolve o tipo
void TrokZumbis_DefinirClasse( CBasePlayer *pPlayer, int iClasse );	// modo Hordas: o diretor escolhe o tipo
void TrokZumbis_AoVirarHumano( CBasePlayer *pPlayer );
void TrokZumbis_AoMorrer( CBasePlayer *pVictim );			// habilidades de morte
void TrokZumbis_AoAcertar( CBasePlayer *pVictim, const CTakeDamageInfo &info );	// golpe que paralisa
void TrokZumbis_Think( void );								// aura, bolhas, fôlego infinito
#endif

#endif // TROK_ZUMBIS_H
