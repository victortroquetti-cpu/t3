# Rede, predição, mensagens e ConVars

O Source tem duas DLLs que compilam o mesmo `shared/`: `server` (`GAME_DLL`) e `client`
(`CLIENT_DLL`). Estado vai do servidor para o cliente por **tabelas de rede** (delta por
snapshot). O cliente **prevê** o próprio jogador e as próprias armas, e o servidor corrige.

## Entidade compartilhada com variável de rede

```cpp
// shared/trok/trok_exemplo.h
#ifdef CLIENT_DLL
	#define CTrokExemplo C_TrokExemplo
#endif

class CTrokExemplo : public CBaseEntity
{
public:
	DECLARE_CLASS( CTrokExemplo, CBaseEntity );
	DECLARE_NETWORKCLASS();

	CNetworkVar( int, m_iEstado );          // muda -> marca para enviar
	CNetworkVar( float, m_flFim );
	CNetworkHandle( CBasePlayer, m_hDono );
};

// shared/trok/trok_exemplo.cpp
LINK_ENTITY_TO_CLASS( trok_exemplo, CTrokExemplo );
IMPLEMENT_NETWORKCLASS_ALIASED( TrokExemplo, DT_TrokExemplo )

BEGIN_NETWORK_TABLE( CTrokExemplo, DT_TrokExemplo )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iEstado ) ),
	RecvPropTime( RECVINFO( m_flFim ) ),
	RecvPropEHandle( RECVINFO( m_hDono ) ),
#else
	SendPropInt( SENDINFO( m_iEstado ), 3, SPROP_UNSIGNED ),   // 3 bits bastam para 0..7
	SendPropTime( SENDINFO( m_flFim ) ),
	SendPropEHandle( SENDINFO( m_hDono ) ),
#endif
END_NETWORK_TABLE()
```

- Mesmos nomes e mesma ordem lógica nos dois lados; bits a menos economizam banda.
- Só servidor: `IMPLEMENT_SERVERCLASS_ST( CFoo, DT_Foo ) ... END_SEND_TABLE()`.
  Só cliente: `IMPLEMENT_CLIENTCLASS_DT( C_Foo, DT_Foo, CFoo ) ... END_RECV_TABLE()`.
- Entidade que precisa ir para todos sempre (placar, estado do modo): `UpdateTransmitState()`
  retornando `SetTransmitState( FL_EDICT_ALWAYS )`.
- No cliente, reaja a mudança em `OnDataChanged( DataUpdateType_t )` ou `PostDataUpdate`.

## Estado das regras de jogo para o cliente (proxy)

`CHL2MPRules` não é entidade. O padrão (ver `shared/hl2mp/hl2mp_gamerules.cpp`) é:

1. `BEGIN_NETWORK_TABLE_NOBASE( CHL2MPRules, DT_HL2MPRules )` com as variáveis das regras;
   a classe declara `DECLARE_SERVERCLASS_NOBASE()` / `DECLARE_CLIENTCLASS_NOBASE()`.
2. Uma entidade proxy (`CHL2MPGameRulesProxy`, `LINK_ENTITY_TO_CLASS( hl2mp_gamerules, ... )`)
   com `SendPropDataTable( ..., SendProxy_HL2MPRules )` que devolve o ponteiro das regras, e o
   `RecvPropDataTable` equivalente no cliente.
3. `CreateStandardEntities()` cria a proxy.

Para o Trok ter variáveis próprias: tabela `DT_TrokRules` NOBASE em `CTrokRules`, proxy
`trok_gamerules` com send/recv proxy apontando para `TrokRules()`, e `CTrokRules::CreateStandardEntities()`
chamando a base e criando a proxy nova (`CBaseEntity::Create( "trok_gamerules", vec3_origin, vec3_angle )`).

## Predição (jogador e armas)

```cpp
// na classe (shared)
DECLARE_PREDICTABLE();

// no .cpp
BEGIN_PREDICTION_DATA( CTrokArma )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_iEstado, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flProximoTiro, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
END_PREDICTION_DATA()
```

- Toda variável que o código previsto muda **tem** que estar na tabela de predição, senão o
  cliente "salta" (erro de predição). `cl_pdump 1` / `cl_showerror 1` ajudam a achar.
- Código previsto roda várias vezes no cliente: efeitos e sons só na primeira
  (`IsFirstTimePredicted()` / `prediction->IsFirstTimePredicted()`); use `CDisablePredictionFiltering`
  e os filtros `CPASAttenuationFilter` como as armas do HL2MP fazem.
- Aleatoriedade prevista: `SharedRandomFloat/Int` (semente do comando), nunca `random->`.
- Exemplos: `shared/hl2mp/weapon_hl2mpbase.cpp`, `shared/hl2mp/hl2mp_player_shared.cpp`.

## User messages (servidor -> cliente, pontual)

- Registro (os dois lados): `RegisterUserMessages()` em `shared/hl2/hl2_usermessages.cpp`
  (`usermessages->Register( "Nome", tamanho )`, `-1` = variável). Mudar esse arquivo = patch.
- Servidor:
  ```cpp
  CSingleUserRecipientFilter filter( pPlayer );   // ou CReliableBroadcastRecipientFilter
  filter.MakeReliable();
  UserMessageBegin( filter, "TrokAviso" );
      WRITE_BYTE( iTipo );
      WRITE_STRING( pszTexto );
  MessageEnd();
  ```
- Cliente: em um `CHudElement`, `HOOK_HUD_MESSAGE( CHudTrokAviso, TrokAviso );` no `Init()` e
  `void MsgFunc_TrokAviso( bf_read &msg )` lendo na mesma ordem (`msg.ReadByte()`, `msg.ReadString(...)`).

## Eventos de jogo

- Declarados em `game/mod_trok/resource/modevents.res` (o do HL2MP é o padrão enquanto o mod
  não tiver o seu). Tipos: `byte`, `short`, `long`, `float`, `string`, `bool`.
- Servidor: `IGameEvent *e = gameeventmanager->CreateEvent( "trok_x" ); if ( e ) { e->SetInt( "userid", p->GetUserID() ); gameeventmanager->FireEvent( e ); }`
- Cliente/HUD: `ListenForGameEvent( "trok_x" )` e `FireGameEvent( IGameEvent *event )`
  (ver `client/hl2mp/hud_deathnotice.cpp`).
- Eventos servem também para bots (`CHL2MPBot` herda `CGameEventListener`) e plugins.

## ConVars

- `ConVar trok_x( "trok_x", "1", FCVAR_..., "ajuda" );` em arquivo de um lado; para valor igual
  nos dois lados, declare num arquivo **shared** com `FCVAR_REPLICATED`.
- `FCVAR_NOTIFY` (avisa no chat ao mudar), `FCVAR_CHEAT` (só com `sv_cheats 1`),
  `FCVAR_ARCHIVE` (cliente, salva em config.cfg), `FCVAR_GAMEDLL`.
- Comando: `CON_COMMAND_F( trok_y, "ajuda", FCVAR_CHEAT ) { ... args.Arg(1) ... }`;
  quem chamou, no servidor: `UTIL_GetCommandClient()`.
