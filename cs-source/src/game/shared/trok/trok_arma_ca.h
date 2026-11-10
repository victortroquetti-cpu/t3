//=============================================================================
// Trok: números de arma no modelo do Combat Arms (sub-bloco "CA" do bloco "Trok"
// em scripts/weapon_*.txt). Lidos junto com o script (hl2mp_weapon_parse.cpp chama
// TrokArmaCA_Ler) e usados por weapon_trok_gun.cpp (armas de fogo) e
// weapon_trok_faca.cpp (faca).
//
// Fonte das leis e dos números: design/chimera/specs/combat_arms/weapon_firing.md,
// weapon_classes.md, inventory.md e games/combat_arms/data/weapons/*.toml (Chimera,
// MIT/Apache). Unidades de distância do CA 1:1 (o jogador tem o corpo do CA).
// Tempos em segundos (o CA guarda em ms), ângulos de recuo em graus como nos dados do
// CA, e os dois limiares "aleatórios" em radianos (como o CA compara).
//=============================================================================

#ifndef TROK_ARMA_CA_H
#define TROK_ARMA_CA_H
#ifdef _WIN32
#pragma once
#endif

class KeyValues;

struct TrokArmaCA_t
{
	bool bAtivo;					// "modelo_ca" 1 no bloco "Trok": a arma segue a lei do CA

	// Dano (por raio)
	float flDano;					// InstDamage
	float flDanoVariacao;			// DamageRFacter: dano x U(1-k, 1+k)
	float flAlcance;				// Range: raio termina aqui
	float flFaixa[ 3 ];				// Effectrange0..2 (u)
	float flFaixaDano[ 3 ];			// Rangedamage0..2 (degraus, não curva)

	// Cone (milésimos de unidade por unidade à frente; padrão quadrado)
	int iConeMin, iConeMax;			// MinPerturb / MaxPerturb
	int iConeMiraMin, iConeMiraMax;	// ZoomedMin/MaxPerturb (mirando pelo cano; 0 = usa o normal)
	float flConeSobe, flConeDesce;	// PerturbIncSpeed / PerturbDecSpeed
	float flConeMiraSobe, flConeMiraDesce;	// Zoomed*; 0 = usa o normal
	float flConeFator;				// DynamicPerturbFactor
	float flTeto[ 4 ];				// FirePerturb, FireMovePerturb, FireDuckPerturb, FireMoveDuckPerturb
	float flAlvoAndar, flAlvoCorrer, flAlvoPulo;	// WalkPerturb, RunPerturb, JumpPerturb
	float flEsperaRecuperar;		// DecDelay (s)

	// Recuo de câmera (do jogador, não da arma)
	float flRecuo;					// BaseCamRecoilPitch (graus)
	float flRecuoAcel;				// BaseCamRecoilAccelPitch
	float flRecuoMax;				// MaxCamRecoilPitch (graus; a subida real vai até o dobro)
	float flRecuoLadoInicio;		// MaxRandomPitch (rad)
	float flRecuoLadoDir, flRecuoLadoEsq;	// BaseCamRecoilWagR / WagL (graus)
	float flRecuoLadoMax;			// MaxCamRecoilWag (graus)
	float flRecuoLadoTroca;			// MaxRandomwag (rad)
	float flRecuoVoltaEspera;		// JitterDecDelay (s)
	float flRecuoVoltaLenta, flRecuoVoltaRapida;	// JitterWaitRecover / JitterRecover (rad/s)

	// Tempos
	float flTempoRecarga;			// animação "reload" (s); 0 = a do modelo
	float flTempoSacar;				// animação "select" (s); 0 = a do modelo
	float flRecargaBala[ 3 ];		// bala por bala (escopeta): início, cada bala, fim (s); [1] = 0 -> pente
	float flSaidaCorrida;			// PostRush (s) até atirar depois de correr; < 0 = trok_arma_correndo_atraso

	// Faca (weapon_trok_faca)
	float flFacaAlcance;			// 105
	float flFacaGolpe[ 4 ];			// golpe (botão 1): duração, dano, 1º acerto, 2º acerto (s)
	float flFacaEstocada[ 3 ];		// estocada (botão 2): duração, dano, acerto (s)
};

// Valores padrão (os do CA quando o registro não tem o campo) e leitura do sub-bloco "CA".
void TrokArmaCA_Padrao( TrokArmaCA_t &ca );
void TrokArmaCA_Ler( KeyValues *pTrok, TrokArmaCA_t &ca );

#endif // TROK_ARMA_CA_H
