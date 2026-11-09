# Armas, movimento e HUD

## Armas

Uma arma do HL2MP tem três partes:

1. **Classe compartilhada** (`shared/hl2mp/weapon_smg1.cpp` é um bom modelo):
   ```cpp
   #ifdef CLIENT_DLL
   	#define CWeaponTrokRifle C_WeaponTrokRifle
   #endif
   class CWeaponTrokRifle : public CHL2MPMachineGun
   {
   public:
   	DECLARE_CLASS( CWeaponTrokRifle, CHL2MPMachineGun );
   	DECLARE_NETWORKCLASS();
   	DECLARE_PREDICTABLE();
   	DECLARE_ACTTABLE();
   	...
   };
   IMPLEMENT_NETWORKCLASS_ALIASED( WeaponTrokRifle, DT_WeaponTrokRifle )
   BEGIN_NETWORK_TABLE( CWeaponTrokRifle, DT_WeaponTrokRifle ) END_NETWORK_TABLE()
   BEGIN_PREDICTION_DATA( CWeaponTrokRifle ) END_PREDICTION_DATA()
   LINK_ENTITY_TO_CLASS( weapon_trok_rifle, CWeaponTrokRifle );
   PRECACHE_WEAPON_REGISTER( weapon_trok_rifle );
   acttable_t CWeaponTrokRifle::m_acttable[] = { { ACT_HL2MP_IDLE, ACT_HL2MP_IDLE_SMG1, false }, ... };
   IMPLEMENT_ACTTABLE( CWeaponTrokRifle );
   ```
   Ganchos: `PrimaryAttack`, `SecondaryAttack`, `Reload`, `ItemPostFrame`, `Deploy`, `Holster`,
   `GetBulletSpread`, `GetFireRate`, `AddViewKick`. Bases: `CWeaponHL2MPBase`,
   `CHL2MPMachineGun`, `CBaseHL2MPBludgeonWeapon` (arma branca), `CBaseHL2MPCombatWeapon`.
2. **Script** `game/mod_trok/scripts/weapon_trok_rifle.txt` (mesmo nome da classe): modelos
   (`viewmodel`, `playermodel`), `clip_size`, `primary_ammo`, `damage`, `bucket`/`bucket_position`,
   `weight`, sons (`SoundData`) e ícones do HUD (`TextureData`). Lido por `shared/weapon_parse.cpp`
   (`FileWeaponInfo_t`; o HL2MP estende em `hl2mp_weapon_parse.*`). Campos novos no script =
   estender a classe de parse.
3. **Munição**: tipos em `CHL2MPRules`/`GetAmmoDef()` (`shared/hl2mp/hl2mp_gamerules.cpp`,
   `def.AddAmmoType(...)`).

Os modelos de arma do CS:S (`models/weapons/v_rif_ak47.mdl`, `w_rif_ak47.mdl`...) ficam disponíveis
pelo gameinfo, mas as animações deles usam atividades do CS (`ACT_VM_PRIMARYATTACK` etc. existem;
as do jogador no CS:S são outro esqueleto). Teste a animação antes de assumir que funciona.

Recursos modernos (mira pelo cano, corrida tática, recuo com padrão...) normalmente mexem em:
`SetFOV` do jogador (zoom), `CalcViewModelView`/`CBaseViewModel::CalcViewModelView` (posição da arma
na mira), `GetBulletSpread`, `AddViewKick` e variáveis de rede previstas na arma. Tudo que muda o
tiro **precisa** ser previsto (ver `rede-e-predicao.md`).

## Movimento

- `shared/gamemovement.cpp` (`CGameMovement`): andar, pular, agachar, escada, água, atrito.
- `shared/hl2/hl_gamemovement.*` (`CHL2GameMovement`) é o que o HL2MP usa por cima.
- O mesmo código roda no servidor e no cliente (predição): estado novo de movimento (deslizar,
  stamina, escorregar) mora no jogador como variável de rede **e** na tabela de predição, ou
  dentro de `CMoveData`.
- Velocidades do HL2: ConVars `hl2_walkspeed`, `hl2_normspeed` (190) e `hl2_sprintspeed` (320),
  usadas por `CHL2_Player::StartSprinting/StopSprinting` (`server/hl2/hl2_player.cpp`), que
  reescrevem o `SetMaxSpeed`. Um `SetMaxSpeed` seu é desfeito na próxima troca de corrida; mude
  ali ou na sua classe de jogador.
- Teclas: `shared/in_buttons.h` (`IN_ATTACK`, `IN_SPEED`, `IN_DUCK`...). Tecla nova = bit livre
  em `in_buttons.h` + comando no cliente (`client/in_main.cpp`) + `kb_act.lst`.

## HUD

```cpp
// client/trok/hud_trok_exemplo.cpp
#include "cbase.h"
#include "hudelement.h"
#include "hud_macros.h"
#include <vgui_controls/Panel.h>
#include "tier0/memdbgon.h"

class CHudTrokExemplo : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudTrokExemplo, vgui::Panel );
public:
	CHudTrokExemplo( const char *pElementName ) : CHudElement( pElementName ), BaseClass( NULL, "HudTrokExemplo" )
	{
		SetParent( g_pClientMode->GetViewport() );
	}
	virtual bool ShouldDraw( void ) { return CHudElement::ShouldDraw(); }
	virtual void Paint( void ) { /* surface()->DrawSetColor, DrawFilledRect, DrawSetTextFont ... */ }
};
DECLARE_HUDELEMENT( CHudTrokExemplo );
```

- Posição e tamanho vêm de `scripts/HudLayout.res` (bloco `"HudTrokExemplo"` com `xpos`, `ypos`,
  `wide`, `tall`, `visible`, `enabled`). Cores e fontes: `resource/ClientScheme.res`.
- Animações do HUD: `scripts/HudAnimations.txt` (`g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("...")`).
- Esconder elementos do HL2MP: tire do `HudLayout.res` ou responda `false` no `ShouldDraw`.
- Menus em tela cheia (compra, escolha de equipamento): painéis VGUI em `client/game_controls/` e
  `client/hl2mp/ui/` (veja como o menu de time é aberto pelo `CBaseViewport`).
- A atualização de 2025 (64 bits) trouxe HUD escalável ao SDK; confira no código antes de fixar tamanhos em pixels.
- Exemplos: `client/hl2/hud_health.cpp`, `client/hl2/hud_ammo.cpp`, `client/hl2mp/hud_deathnotice.cpp`.
