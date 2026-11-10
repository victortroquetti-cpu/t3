//=============================================================================
// Trok: granadas e explosivos do Combat Arms (Chimera, specs/combat_arms/grenades.md e
// special_weapons.md).
//
// Uma arma (CWeaponTrokExplosivo, shared/trok/weapon_trok_explosivo.cpp) para tudo o
// que se arremessa, se planta ou se dispara como projétil; um objeto no servidor
// (CTrokProjetil, server/trok/trok_projetil.cpp) para tudo o que voa ou fica no chão.
// Os números de cada um ficam no bloco "TrokExplosivo" de scripts/weapon_trok_*.txt;
// as regras que valem para todos (gravidade, dureza das superfícies...) ficam nas
// ConVars trok_explosivo_* (cfg/explosivos.cfg).
//
// Distâncias e velocidades estão em unidades do CA e são multiplicadas por
// trok_ca_escala (cfg/movimento.cfg), como no movimento.
//=============================================================================

#ifndef TROK_EXPLOSIVOS_H
#define TROK_EXPLOSIVOS_H
#ifdef _WIN32
#pragma once
#endif

// Como o objeto se comporta depois que sai da mão.
enum TrokExplosivoTipo_t
{
	TROK_EXP_GRANADA = 0,	// quica, pavio fixo (M67, XM84, M18)
	TROK_EXP_IMPACTO,		// explode no primeiro toque (RPG-7)
	TROK_EXP_MINA,			// pousa no chão; inimigo encosta = explode (M16A1)
	TROK_EXP_CLAYMORE,		// plantada; detonador (botão 2) ou morte do dono
	TROK_EXP_REMOTA,		// arremessada, pousa; detonador (botão 2) ou morte do dono
	TROK_EXP_TORRETA,		// arremessada, pousa, arma-se e atira sozinha
};

// O que a explosão faz (granadas e impacto).
enum TrokExplosivoEfeito_t
{
	TROK_EFEITO_FRAG = 0,	// dano de área
	TROK_EFEITO_LUZ,		// cega quem vê (XM84)
	TROK_EFEITO_FUMACA,		// nuvem de fumaça, sem dano (M18)
};

struct TrokExplosivoInfo_t
{
	char szArma[ 64 ];			// nome da arma (weapon_trok_...)
	char szEntidade[ 64 ];		// nome do objeto criado (trok_granada, trok_foguete...)
	int iTipo;					// TrokExplosivoTipo_t
	int iEfeito;				// TrokExplosivoEfeito_t
	bool bLancador;				// arma de tiro (pente + reserva) em vez de arremesso

	// --- Arremesso (s, na velocidade de animação 1 do CA) ---
	float flSaque;				// sacar a arma
	float flPino;				// puxar o pino (pré-disparo); 0 = sem
	float flPinoSom;			// quando o som do pino toca, desde o começo do pino
	float flLaco;				// laço do arremesso enquanto segura o botão; 0 = não segura
	float flSoltar;				// do soltar o botão até o objeto sair da mão
	float flPos;				// animação depois de soltar (inclui flSoltar)
	float flRecarga;			// pegar a próxima (arremesso) ou recarregar o pente (lançador)
	float flCiclo;				// lançador: tempo entre disparos
	float flCoice;				// lançador: graus que cada tiro levanta a câmera
	float flDetonarAtraso;		// botão 2 até a explosão (claymore, remota)

	// --- Voo ---
	float flVelocidade;			// u/s ao sair da câmera (o movimento de quem joga não soma)
	bool bGravidade;			// cai com a gravidade de objeto (trok_explosivo_gravidade)
	float flDispersao;			// desvio máximo (fração) em cada eixo, direita e cima
	float flPavio;				// s desde a criação até explodir (granada) ou sumir
	float flVida;				// pontos de vida do objeto (tiro e explosão o detonam)
	Vector vecCaixa;			// meia-caixa (frente, lado, cima) para tiro e minas
	float flMeiaAltura;			// recuo ao parar no chão (grenades.md seção 6)
	float flVelocidadeMorte;	// granada com pino puxado solta ao morrer (u/s)

	// --- Explosão ---
	float flDano;				// dano no núcleo (já com o "duas vezes" do CA em jogador)
	float flRaio;				// raio (u)
	float flConeDano;			// claymore: dano no cone
	float flConeRaio;			// claymore: raio do cone
	float flConeAngulo;			// claymore: meio ângulo do cone (graus)
	float flMinaAtraso;			// mina: do toque até a explosão (s)

	// --- Luz (XM84) ---
	float flLuzTempo;			// tempo de clarão (s) de frente e perto
	float flLuzAtraso;			// s até cegar, de frente
	float flLuzCostas;			// fração do tempo de costas
	float flLuzAtrasoCostas;	// s até cegar, de costas
	float flLuzMinimo;			// clarão mais curto que isso não chega

	// --- Fumaça (M18) ---
	float flFumacaSome;			// s depois da explosão em que a fumaça começa a sumir
	float flFumacaFim;			// s depois da explosão em que não sobra nada

	// --- Torreta ---
	float flTorretaPreparo;		// s do pouso até começar a atirar
	float flTorretaIntervalo;	// s entre tiros
	float flTorretaAlcance;		// raio de busca (u)
	int iTorretaBalas;			// tiros até sumir
	float flTorretaBalaDano;	// dano por tiro (perto)
	float flTorretaErro;		// erro (u) em cada eixo no ponto mirado
	float flTorretaAngCima;		// graus acima da horizontal
	float flTorretaAngBaixo;	// graus abaixo
	float flTorretaSensor;		// altura do sensor acima do pé (u)
	bool bUmPorVez;				// um novo some com o anterior do mesmo dono

	// --- Aparência e som (provisórios, do HL2/CS:S montados) ---
	char szModelo[ 128 ];		// modelo do objeto
	char szSomQuique[ 64 ];
	char szSomExplosao[ 64 ];
	char szSomAviso[ 64 ];		// "granada!" para o time ao arremessar
	char szSomAtivo[ 64 ];		// mina pisada, torreta armando, detonador
	char szSomTiro[ 64 ];		// torreta
};

// Lê (uma vez) o bloco "TrokExplosivo" de scripts/<arma>.txt.
const TrokExplosivoInfo_t &TrokExplosivo_Info( const char *pszArma );

// trok_ca_escala (cfg/movimento.cfg).
float TrokExplosivo_Escala( void );

#ifndef CLIENT_DLL
class CBasePlayer;
// Cria o objeto. vecVel já em u/s do Source (com a escala).
CBaseEntity *TrokExplosivo_Criar( const TrokExplosivoInfo_t &info, CBasePlayer *pDono, const Vector &vecOrigem, const Vector &vecVel, const QAngle &angInicial );
// Detona (com atraso) todas as claymores ou remotas do dono criadas por esta arma.
int TrokExplosivo_Detonar( CBasePlayer *pDono, const char *pszArma, float flAtraso );

// Cobertura das explosões do CA (shared/trok/trok_gamerules.cpp).
bool TrokExplosaoBloqueada( const Vector &a, const Vector &b, CBaseEntity *pInflictor );
#endif

#endif // TROK_EXPLOSIVOS_H
