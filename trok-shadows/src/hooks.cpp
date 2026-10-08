// Trok Shadows (.asi) -- Victor_Trok
// Os ganchos. Fazem o mesmo que o Shadows Extender 2.0 (DK22Pac), lido do binario dele, com estas diferencas:
//   - cada chamada trocada e conferida antes (no 1.0 US ou ja desviada por outro mod, que fica encadeado);
//   - DisplayShadowsAtLowSettings de [STENCIL_SHADOWS] vale para o stencil (o original lia a chave do
//     [REALTIME_SHADOWS] duas vezes);
//   - o shader so entra nas descargas do buffer que sao da sombra em tempo real (o original podia pintar com a
//     cor da sombra outros efeitos que enchessem o buffer);
//   - testes de ponteiro nulo onde o original podia travar o jogo;
//   - o modo combinado so liga com o shader carregado (o INI original ja pedia isso, mas sem o shader ele
//     ligava o stencil mesmo assim).

#include "trok.h"
#include "game.h"

#include <d3d9.h>
#include <cstdlib>
#include <cstring>

using namespace game;

extern "C" {
extern uintptr_t g_trokEventOriginal[EVENT_COUNT];
extern uintptr_t g_trokEventReturn[EVENT_COUNT];
extern float g_trokSunZLimit;
void TrokEventStub0();
void TrokEventStub1();
void TrokEventStub2();
void TrokEventStub3();
void TrokExtrasStub();
void TrokSunStub();
}

namespace {

typedef uint32_t u32;

typedef bool(__cdecl* BoolFn)();
typedef void(__cdecl* VoidFn)();
typedef void(__thiscall* DoShadowFn)(void* manager, void* physical);
typedef void*(__thiscall* ShadowUpdateFn)(void* shadow);
typedef void(__thiscall* ThisFn)(void* self);
typedef void*(__thiscall* SetLightFn)(void* shadow, u32 azimuth, u32 elevation, u32 setCamLight);
typedef void(__thiscall* BonePositionFn)(void* ped, void* out, u32 bone, u32 updateSkin);
typedef void(__thiscall* ColSphereSetFn)(void* sphere, u32 radius, u32 center, u32 material, u32 flags, u32 light);
typedef void(__cdecl* StoreShadowFn)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                                     u32);
typedef void(__cdecl* CastRealTimeFn)(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32,
                                      u32, u32, u32, u32);
typedef void(__cdecl* VehicleShadowFn)(void* vehicle, u32 type);
typedef void*(__cdecl* FrameRotateFn)(void* frame, void* axis, u32 angle, u32 combine);
typedef void*(__cdecl* MatrixTranslateFn)(void* matrix, void* translation, u32 combine);
typedef void*(__cdecl* MatrixRotateFn)(void* matrix, const void* axis, float angle, u32 combine);
typedef void*(__cdecl* ForAllAtomicsFn)(void* clump, void* callback, void* data);
typedef void*(__cdecl* AtomicCallbackFn)(void* atomic, void* data);
typedef void(__cdecl* DrawRectFn)(void* rect, void* color);
typedef void*(__cdecl* PtrFn)(void* object);
typedef int(__cdecl* AnimIdIndexFn)(void* hierarchy, int id);
typedef uint8_t(__thiscall* WeaponSkillFn)(void* ped);
typedef void*(__cdecl* WeaponInfoFn)(u32 type, u32 skill);
typedef void*(__thiscall* TaskJetPackFn)(void* intelligence);
typedef void(__thiscall* RenderJetPackFn)(void* task, void* ped);
typedef int(__cdecl* RenderStateSetFn)(int state, uintptr_t value);
typedef int(__cdecl* RenderStateGetFn)(int state, void* out);
typedef void(__cdecl* SetShaderFn)(void* shader);

template <class F>
F Fn(uintptr_t addr) {
    return reinterpret_cast<F>(addr);
}

u32 Bits(float f) {
    u32 u;
    memcpy(&u, &f, 4);
    return u;
}

// ------------------------------------------------------------------------------------------------ estado
bool g_applied = false;

// Valores que o codigo do jogo le por ponteiro (o original trocava as constantes por estes).
float g_stencilMaxDistance = 50.0f;
float g_stencilMaxDistanceSq = 2500.0f;
const float kShadowQuadScale = 2.15f; // jogo: 1.5 (tamanho do quadrado da sombra em tempo real)
const float kProjectionScale = 1.15f;  // CastShadowEntityXYZ (valor do Shadows Extender)

void* g_updateOwner = nullptr;  // dono da sombra em tempo real sendo desenhada na camera (CRealTimeShadow::Update)
void* g_castOwner = nullptr;    // dono da sombra sendo projetada no mundo (CastRealTimeShadowSectorList)
bool g_inRealtimeCast = false;  // projetando uma sombra em tempo real (o buffer so tem polys dela)
u32 g_lightElevation = 0;       // angulo de elevacao que o jogo calculou para a luz da sombra
bool g_clumpRendered = false;   // CShadowCamera::Update(RpClump*) desenhou e deixou a camera aberta (os NOPs)

// Limite de sombras em tempo real: so pedem sombra as entidades mais perto da camera (raio do quadro anterior).
constexpr int kMaxCandidates = 256;
float g_candidateDist[kMaxCandidates]; // distancia^2 de quem pediu sombra neste quadro
int g_candidateCount = 0;
int g_candidatesSeen = 0;          // pedidos neste quadro (inclusive acima de kMaxCandidates)
float g_limitRadiusSq = 1e30f;     // pedidos alem deste raio^2 ficam com a sombra simples

// Diagnostico (vai para o log uma vez).
bool g_loggedBusy = false, g_loggedDoubleUpdate = false, g_loggedCamera = false;
u32 g_slotFrame[MANAGER_SLOTS];
int g_updateRestores = 0; // vezes que a atualizacao das sombras em tempo real foi religada

void* g_stencilPool = nullptr;
int g_stencilPoolCount = 0;
uint8_t g_rectCalls[2][5]; // as duas chamadas do Im2D que o desenho do stencil desliga por um instante
bool g_rectCallsOk = false;

// ------------------------------------------------------------------------------------------------ chamadas
enum CallId {
    C_STENCIL_INIT,
    C_STENCIL_RECT,
    C_VEHICLE_1,
    C_VEHICLE_2,
    C_VEHICLE_3,
    C_VEHICLE_4,
    C_VEHICLE_5,
    C_SET_LIGHT,
    C_FRAME_ROTATE,
    C_MATRIX_TRANSLATE,
    C_OVERFLOW_1,
    C_OVERFLOW_2,
    C_CAST_REALTIME,
    C_FOR_ALL_ATOMICS,
    C_STORE_SHADOW,
    C_COL_SPHERE,
    C_CUTSCENE_SHADOW,
    C_PED_RPHANIM,
    C_SHADOW_UPDATE,
    C_BONE_POSITION,
    C_COUNT
};

patch::Call g_calls[C_COUNT] = {
    {"pool de sombras stencil", 0x53BCAB, CStencilShadows_Init, 0, false},
    {"cor da sombra stencil", 0x71167F, CSprite2d_DrawRect, 0, false},
    // Os PreRender das classes de veiculo que chamam CShadows::StoreShadowForVehicle.
    {"sombra de veiculo 1", 0x6ABCF5, CShadows_StoreShadowForVehicle, 0, false},
    {"sombra de veiculo 2", 0x6BD667, CShadows_StoreShadowForVehicle, 0, false},
    {"sombra de veiculo 3", 0x6C0B21, CShadows_StoreShadowForVehicle, 0, false},
    {"sombra de veiculo 4", 0x6C58A0, CShadows_StoreShadowForVehicle, 0, false},
    {"sombra de veiculo 5", 0x6CA73A, CShadows_StoreShadowForVehicle, 0, false},
    {"luz da sombra", 0x707E4F, CRealTimeShadow_SetLightProperties, 0, false},
    {"inclinacao da luz", 0x70596A, RwFrameRotate, 0, false},
    {"projecao da sombra", 0x70A1AC, RwMatrixTranslate, 0, false},
    {"buffer cheio 1", 0x7082A4, RenderBuffer_RenderStuffInBuffer, 0, false},
    {"buffer cheio 2", 0x7082BD, RenderBuffer_RenderStuffInBuffer, 0, false},
    {"projetar sombra em tempo real", 0x70AD0D, CShadows_CastRealTimeShadowSectorList, 0, false},
    {"partes desenhadas na sombra", 0x705C4A, RpClumpForAllAtomics, 0, false},
    {"distancia vertical da sombra", 0x707F2C, CShadows_StoreShadowToBeRendered, 0, false},
    {"raio da sombra", 0x70A2C8, CColSphere_Set, 0, false},
    {"sombra em cutscene", 0x5B1F3C, CRealTimeShadowManager_DoShadowThisFrame, 0, false},
    {"sombra dos pedestres", 0x5E6664, CEntity_UpdateRpHAnim, 0, false},
    {"atualizar sombra", 0x706B29, CRealTimeShadow_Update, 0, false},
    {"posicao do pedestre", 0x707CF1, CPed_GetBonePosition, 0, false},
};

patch::Call g_events[EVENT_COUNT] = {
    {"evento: RenderWare iniciado", EV_INIT_RW, 0, 0, false},
    {"evento: RenderWare desligado", EV_SHUTDOWN_RW, 0, 0, false},
    {"evento: jogo iniciado", EV_INIT_GAME, 0, 0, false},
    {"evento: quadro do jogo", EV_GAME_PROCESS, 0, 0, false},
};

template <class F>
F Original(CallId id) {
    return reinterpret_cast<F>(g_calls[id].original);
}

// ------------------------------------------------------------------------------------------------ liga/desliga
#define NOP2 {0x90, 0x90}
#define NOP5 {0x90, 0x90, 0x90, 0x90, 0x90}
#define NOP6 {0x90, 0x90, 0x90, 0x90, 0x90, 0x90}

// Os jcc que viram NOP ou jmp sao os testes do jogo (qualidade grafica, um objeto a cada quatro, sombra simples
// do veiculo, jogador principal).
patch::Toggle g_flagIgnoreSome = {"FlagIgnoreSomeShadows", 0x711E3D, 2, patch::EXPECT_SHORT_JCC, NOP2, {}, false,
                                  false, false};
patch::Toggle g_disableBuildings = {"DisableBuildingShadows", 0x711E41, 5, patch::EXPECT_CALL, NOP5, {}, false,
                                    false, false};
patch::Toggle g_stencilLow1 = {"stencil no grafico baixo (Process)", 0x711D9D, 2, patch::EXPECT_SHORT_JCC, NOP2, {},
                               false, false, false};
patch::Toggle g_stencilLow2 = {"stencil no grafico baixo (Render)", 0x7113C0, 2, patch::EXPECT_SHORT_JCC, NOP2, {},
                               false, false, false};
patch::Toggle g_realtimeLow1 = {"tempo real no grafico baixo", 0x706BCC, 1, patch::EXPECT_SHORT_JCC, {0xEB}, {},
                                false, false, false};
patch::Toggle g_realtimeLow2 = {"tempo real no grafico baixo (pedestre)", 0x5E6766, 1, patch::EXPECT_SHORT_JCC,
                                {0xEB}, {}, false, false, false};
patch::Toggle g_vehicleDefault = {"DrawVehicleDefaultShadowWithRealTime", 0x70BDAB, 6, patch::EXPECT_NEAR_JCC, NOP6,
                                  {}, false, false, false};
patch::Toggle g_morePlayers = {"MoreThanOnePlayer", 0x7069F5, 1, patch::EXPECT_SHORT_JCC, {0xEB}, {}, false, false,
                               false};

// ------------------------------------------------------------------------------------------------ calculos
// Quanto da forca sobra com nuvens e de noite (formula do Shadows Extender).
float Weather() {
    const float clouds = 1.0f - At<float>(CLOUD_COVERAGE);
    const float day = 1.0f - At<float>(DN_BALANCE);
    return (clouds > g_cfg.cloudsFactor ? clouds : g_cfg.cloudsFactor) *
           (day > g_cfg.nightFactor ? day : g_cfg.nightFactor);
}

bool ShaderOn() {
    return g_cfg.enableShader && ShadersReady();
}

bool CombineOn() {
    return ShaderOn() && g_cfg.combineWithStencil;
}

const float* EntityPosition(const void* entity) {
    const void* matrix = Field<void*>(entity, PLACEABLE_MATRIX);
    if (matrix) {
        return &Field<float>(matrix, MATRIX_POSITION);
    }
    return &Field<float>(entity, PLACEABLE_POSITION);
}

void DoShadowThisFrame(void* physical) {
    Fn<DoShadowFn>(CRealTimeShadowManager_DoShadowThisFrame)(reinterpret_cast<void*>(REALTIME_SHADOW_MAN),
                                                             physical);
}

// Distancia^2 ate a camera no plano (como o jogo mede em CShadows::StoreRealTimeShadow).
float DistanceSqToCamera(const void* entity) {
    const float* a = EntityPosition(entity);
    const float* b = EntityPosition(reinterpret_cast<void*>(THE_CAMERA));
    const float dx = a[0] - b[0], dy = a[1] - b[1];
    return dx * dx + dy * dy;
}

// O jogador e o veiculo dele sempre ganham sombra.
bool IsPlayerOrPlayerVehicle(const void* entity) {
    void* player = At<void*>(PLAYER_PED);
    return player && (entity == player || entity == VehicleOf(player));
}

// Pedido de sombra em tempo real, com dois filtros:
//   - alem de MaxDistance o jogo nao desenha a sombra, mas ela ocupava uma das 16 vagas e era redesenhada todo
//     quadro (o Shadows Extender pedia sombra para todo pedestre e veiculo carregado): quem esta longe nao pede;
//   - so as MaxRealTimeShadows entidades mais perto pedem (raio do quadro anterior). Quem ja tem sombra ganha uma
//     folga de ~14% no raio, para nao perder a vaga na borda. O jogador e o veiculo dele sempre pedem.
bool AllowShadow(void* entity) {
    if (!g_cfg.realtimeEnabled) {
        return false;
    }
    const float d2 = DistanceSqToCamera(entity);
    if (d2 > g_cfg.realtimeMaxDistance * g_cfg.realtimeMaxDistance) {
        return false;
    }
    const float rank = IsPlayerOrPlayerVehicle(entity) ? 0.0f : d2;
    g_candidatesSeen++;
    if (g_candidateCount < kMaxCandidates) {
        g_candidateDist[g_candidateCount++] = rank;
    }
    const bool hasShadow = Field<void*>(entity, PHYSICAL_SHADOW_DATA) != nullptr;
    return rank <= g_limitRadiusSq * (hasShadow ? 1.3f : 1.0f);
}

void RequestShadow(void* entity) {
    if (AllowShadow(entity)) {
        DoShadowThisFrame(entity);
    }
}

// Silhueta de um clump na camera da sombra, como CShadowCamera::Update(RpClump*) faz com o dono: sem textura, luz
// nem cor na geometria durante o desenho.
void* __cdecl SilhouetteAtomic(void* atomic, void* data) {
    if (!(Field<uint8_t>(atomic, ATOMIC_FLAGS) & 4)) {
        return atomic;
    }
    void* geometry = Field<void*>(atomic, ATOMIC_GEOMETRY);
    const u32 flags = geometry ? Field<u32>(geometry, GEOMETRY_FLAGS) : 0;
    if (geometry) {
        Field<u32>(geometry, GEOMETRY_FLAGS) = flags & ~SHADOW_GEOMETRY_FLAGS;
    }
    Fn<AtomicCallbackFn>(game::atomicQuickRender)(atomic, data);
    if (geometry) {
        Field<u32>(geometry, GEOMETRY_FLAGS) = flags;
    }
    return atomic;
}

void RenderStateSet(int state, uintptr_t value) {
    Fn<RenderStateSetFn>(RwRenderStateSet)(state, value);
}

// ------------------------------------------------------------------------------------------------ stencil
// CStencilShadows::Init com MaxShadows objetos no lugar dos 64 fixos do jogo.
bool __cdecl StencilInitHook() {
    const int count = g_stencilPoolCount;
    if (!g_stencilPool) {
        g_stencilPool = calloc(count, 0x1C);
        if (!g_stencilPool) {
            Log("aviso: sem memoria para %d sombras stencil, ficou o padrao do jogo", count);
            return Original<BoolFn>(C_STENCIL_INIT)();
        }
    }
    uint8_t* pool = static_cast<uint8_t*>(g_stencilPool);
    memset(pool, 0, count * 0x1C);
    At<uint8_t>(RW_STENCIL_CLEAR) = 0;
    for (int i = 0; i < count; i++) {
        uint8_t* obj = pool + i * 0x1C;
        Field<void*>(obj, 0x14) = i + 1 < count ? obj + 0x1C : nullptr; // m_pNext
        Field<void*>(obj, 0x18) = i > 0 ? obj - 0x1C : nullptr;         // m_pPrev
    }
    At<void*>(STENCIL_FIRST_AVAILABLE) = pool;
    At<void*>(STENCIL_FIRST_ACTIVE) = nullptr;
    return true;
}

// O retangulo da tela inteira que escurece onde o stencil marcou sombra: cor e forca do INI.
void __cdecl StencilRectHook(void* rect, uint8_t* color) {
    float alpha = static_cast<float>(g_cfg.stencilColor[3]) * Weather();
    if (alpha > 255.0f) {
        alpha = 255.0f;
    }
    color[0] = static_cast<uint8_t>(g_cfg.stencilColor[0]);
    color[1] = static_cast<uint8_t>(g_cfg.stencilColor[1]);
    color[2] = static_cast<uint8_t>(g_cfg.stencilColor[2]);
    color[3] = static_cast<uint8_t>(alpha);

    // Como no Shadows Extender: o Im2D desenha sem trocar os shaders (as duas chamadas viram NOP por um
    // instante) e o mod zera vertex/pixel shader antes. As paginas ja ficaram graváveis na instalacao.
    if (g_rectCallsOk) {
        memset(reinterpret_cast<void*>(0x7FB81D), 0x90, 5);
        memset(reinterpret_cast<void*>(0x7FB824), 0x90, 5);
    }
    Fn<SetShaderFn>(rwD3D9SetVertexShader)(nullptr);
    Fn<SetShaderFn>(rwD3D9SetPixelShader)(nullptr);
    Original<DrawRectFn>(C_STENCIL_RECT)(rect, color);
    if (g_rectCallsOk) {
        memcpy(reinterpret_cast<void*>(0x7FB81D), g_rectCalls[0], 5);
        memcpy(reinterpret_cast<void*>(0x7FB824), g_rectCalls[1], 5);
    }
    Fn<SetShaderFn>(rwD3D9SetPixelShader)(nullptr);
}

// ------------------------------------------------------------------------------------------------ veiculos
// Os PreRender dos veiculos: pede sombra em tempo real e, se o INI deixar, a sombra simples do jogo.
template <int N>
void __cdecl VehicleShadowHook(void* vehicle, u32 type) {
    if (g_cfg.vehicleRealtime && HasRwObject(vehicle)) {
        RequestShadow(vehicle);
    }
    if (!g_cfg.disableVehicleDefaultShadow) {
        Original<VehicleShadowFn>(static_cast<CallId>(C_VEHICLE_1 + N))(vehicle, type);
    }
}

// ------------------------------------------------------------------------------------------------ pedestres
// CPed::PreRenderAfterTest: depois de atualizar os ossos, todo pedestre pede sombra em tempo real (o pedido
// condicional do jogo, mais abaixo na funcao, vira NOP). Dentro de um veiculo com sombra em tempo real, o
// pedestre entra na sombra do veiculo (TrokShadowExtras): duas sombras separadas escureciam dobrado onde se
// cruzavam (moto e piloto).
void __thiscall PedRpHAnimHook(void* ped) {
    Original<ThisFn>(C_PED_RPHANIM)(ped);
    if (!HasRwObject(ped)) {
        return;
    }
    void* vehicle = VehicleOf(ped);
    if (vehicle && g_cfg.vehicleRealtime && HasRwObject(vehicle)) {
        return;
    }
    RequestShadow(ped);
}

void __thiscall CutsceneShadowHook(void* manager, void* physical) {
    if (physical && HasRwObject(physical) && AllowShadow(physical)) {
        Original<DoShadowFn>(C_CUTSCENE_SHADOW)(manager, physical);
    }
}

// CRealTimeShadowManager::Update chamando CRealTimeShadow::Update: pula sombras de quem nao tem modelo.
void* __thiscall ShadowUpdateHook(void* shadow) {
    void* owner = Field<void*>(shadow, RTSHADOW_OWNER);
    g_updateOwner = owner;
    if (!owner || !HasRwObject(owner)) {
        return nullptr;
    }
    // Diagnostico: a mesma sombra atualizada duas vezes no mesmo quadro (o Update do gerente rodando duas vezes,
    // por outro mod) faz a sombra apagar e voltar.
    const u32 frame = At<u32>(TIMER_FRAME_COUNTER);
    void** slots = reinterpret_cast<void**>(REALTIME_SHADOW_MAN + MANAGER_SHADOWS);
    for (int i = 0; i < MANAGER_SLOTS; i++) {
        if (slots[i] == shadow) {
            if (g_slotFrame[i] == frame + 1 && !g_loggedDoubleUpdate) {
                g_loggedDoubleUpdate = true;
                Log("diagnostico: uma sombra em tempo real foi atualizada 2 vezes no mesmo quadro (outro mod chama o "
                    "CRealTimeShadowManager::Update?)");
            }
            g_slotFrame[i] = frame + 1; // +1: o quadro 0 nao se confunde com "nunca"
            break;
        }
    }
    return Original<ShadowUpdateFn>(C_SHADOW_UPDATE)(shadow);
}

// CShadows::StoreRealTimeShadow pegando a posicao do osso raiz. Sem modelo, usa a posicao do pedestre (o
// Shadows Extender deixava o vetor sem valor).
void __thiscall BonePositionHook(void* ped, float* out, u32 bone, u32 updateSkin) {
    if (HasRwObject(ped)) {
        Original<BonePositionFn>(C_BONE_POSITION)(ped, out, bone, updateSkin);
        return;
    }
    const float* pos = EntityPosition(ped);
    out[0] = pos[0];
    out[1] = pos[1];
    out[2] = pos[2];
}

// Arma na mao (as duas, se forem pistolas duplas), paraquedas e mochila a jato desenhados na sombra.
void DrawPedExtras(void* ped) {
    void* weapon = Field<void*>(ped, PED_WEAPON_OBJECT);
    if (weapon) {
        const uint8_t slot = Field<uint8_t>(ped, PED_WEAPON_SLOT);
        const int type = Field<int>(ped, PED_WEAPONS + slot * WEAPON_SIZE);
        const bool parachute = type == WEAPON_PARACHUTE;
        void* hierarchy = Fn<PtrFn>(GetAnimHierarchyFromSkinClump)(Field<void*>(ped, ENTITY_RWOBJECT));
        void* frame = Field<void*>(weapon, OBJECT_PARENT);
        if (hierarchy && frame) {
            uint8_t* matrices = static_cast<uint8_t*>(Fn<PtrFn>(RpHAnimHierarchyGetMatrixArray)(hierarchy));
            const int bone = Fn<AnimIdIndexFn>(RpHAnimIDGetIndex)(hierarchy, parachute ? BONE_SPINE1 : BONE_R_HAND);
            void* modelling = static_cast<uint8_t*>(frame) + FRAME_MODELLING;
            if (matrices && bone >= 0) {
                memcpy(modelling, matrices + bone * 64, 64);
                if (parachute) {
                    Fn<MatrixTranslateFn>(RwMatrixTranslate)(modelling, reinterpret_cast<void*>(VEC_PARACHUTE_OFFSET),
                                                              1);
                    Fn<MatrixRotateFn>(RwMatrixRotate)(modelling, reinterpret_cast<void*>(VEC_PARACHUTE_AXIS), 90.0f,
                                                       1);
                }
                Fn<PtrFn>(RwFrameUpdateObjects)(frame);
                Fn<PtrFn>(RpClumpRender)(weapon);

                const uint8_t skill = Fn<WeaponSkillFn>(CPed_GetWeaponSkill)(ped);
                void* info = Fn<WeaponInfoFn>(CWeaponInfo_GetWeaponInfo)(static_cast<uint16_t>(type), skill);
                if (info && (Field<u32>(info, WEAPONINFO_FLAGS) & WEAPONFLAG_TWIN_PISTOL)) {
                    const int left = Fn<AnimIdIndexFn>(RpHAnimIDGetIndex)(hierarchy, BONE_L_HAND);
                    if (left >= 0) {
                        memcpy(modelling, matrices + left * 64, 64);
                        Fn<MatrixRotateFn>(RwMatrixRotate)(modelling, reinterpret_cast<void*>(VEC_TWIN_AXIS), 180.0f,
                                                           1);
                        Fn<MatrixTranslateFn>(RwMatrixTranslate)(modelling, reinterpret_cast<void*>(VEC_TWIN_OFFSET),
                                                                  1);
                        Fn<PtrFn>(RwFrameUpdateObjects)(frame);
                        Fn<PtrFn>(RpClumpRender)(weapon);
                    }
                }
            }
        }
    }
    void* intelligence = Field<void*>(ped, PED_INTELLIGENCE);
    if (intelligence) {
        void* task = Fn<TaskJetPackFn>(CPedIntelligence_GetTaskJetPack)(intelligence);
        if (task && Field<void*>(task, JETPACK_CLUMP)) {
            Fn<RenderJetPackFn>(CTaskSimpleJetPack_RenderJetPack)(task, ped);
        }
    }
}

// Cada parte do clump desenhada na camera da sombra (troca o atomicQuickRender do CShadowCamera::Update).
void* __cdecl ShadowAtomicCallback(void* atomic, void* data) {
    if (!(Field<uint8_t>(atomic, ATOMIC_FLAGS) & 4)) { // rpATOMICRENDER desligado
        return atomic;
    }
    const uintptr_t callback = Field<uintptr_t>(atomic, ATOMIC_RENDER_CB);
    if (callback == RenderVehicleReallyLowDetailCB_BigVehicle || callback == RenderVehicleReallyLowDetailCB ||
        callback == RenderVehicleLoDetailCB_Boat) {
        return atomic; // LOD do veiculo: desenharia a carroceria duas vezes
    }
    bool rotor = false;
    u32 alphaRef = 0;
    if (CombineOn() && (callback == RenderHeliRotorAlphaCB || callback == RenderHeliTailRotorAlphaCB)) {
        // Helice girando e um disco transparente: no stencil, so as pas quase opacas fazem sombra.
        Fn<RenderStateGetFn>(game::RwRenderStateGet)(RS_ALPHATESTFUNCTIONREF, &alphaRef);
        RenderStateSet(RS_ALPHATESTFUNCTIONREF, 0xFE);
        rotor = true;
    }
    void* pipeline = Field<void*>(atomic, ATOMIC_PIPELINE);
    void* owner = g_updateOwner;
    if ((owner && EntityType(owner) == ENTITY_VEHICLE) || pipeline == At<void*>(CAR_ENVMAP_OBJ_PIPELINE)) {
        Field<void*>(atomic, ATOMIC_PIPELINE) = nullptr; // pipeline padrao: so a silhueta importa
    }
    Fn<AtomicCallbackFn>(game::atomicQuickRender)(atomic, data);
    Field<void*>(atomic, ATOMIC_PIPELINE) = pipeline;
    if (rotor) {
        RenderStateSet(RS_ALPHATESTFUNCTIONREF, alphaRef);
    }
    return atomic;
}

// So roda dentro do RwCameraBeginUpdate que deu certo em CShadowCamera::Update(RpClump*): a camera fica aberta
// (os NOPs) ate o TrokShadowExtras fechar.
void* __cdecl ForAllAtomicsHook(void* clump, void* callback, void* data) {
    (void)callback;
    g_clumpRendered = true;
    return Original<ForAllAtomicsFn>(C_FOR_ALL_ATOMICS)(clump, reinterpret_cast<void*>(&ShadowAtomicCallback), data);
}

// ------------------------------------------------------------------------------------------------ luz e projecao
// CShadows::StoreRealTimeShadow -> CRealTimeShadow::SetLightProperties(azimute, elevacao, ...): o jogo ignora a
// elevacao (a luz sempre aponta para baixo, -90). O mod guarda e usa a elevacao no RwFrameRotate do eixo X.
void* __thiscall SetLightHook(void* shadow, u32 azimuth, u32 elevation, u32 setCamLight) {
    g_lightElevation = elevation;
    return Original<SetLightFn>(C_SET_LIGHT)(shadow, azimuth, elevation, setCamLight);
}

void* __cdecl FrameRotateHook(void* frame, void* axis, u32 angle, u32 combine) {
    (void)angle;
    return Original<FrameRotateFn>(C_FRAME_ROTATE)(frame, axis, g_lightElevation, combine);
}

// CShadows::CastShadowEntityXYZ: deslocamento da projecao (y = 0.5, valor do Shadows Extender).
void* __cdecl MatrixTranslateHook(void* matrix, float* translation, u32 combine) {
    translation[1] = 0.5f;
    return Original<MatrixTranslateFn>(C_MATRIX_TRANSLATE)(matrix, translation, combine);
}

// Raio da esfera em volta da sombra onde ela e projetada.
void __thiscall ColSphereSetHook(void* sphere, u32 radius, u32 center, u32 material, u32 flags, u32 light) {
    (void)radius;
    const float r = IsAircraft(g_castOwner) ? g_cfg.boundSphereInAir : g_cfg.boundSphere;
    Original<ColSphereSetFn>(C_COL_SPHERE)(sphere, Bits(r), center, material, flags, light);
}

// CShadows::StoreShadowToBeRendered da sombra em tempo real: distancia vertical do INI. Sem dono com modelo, a
// sombra nao e guardada.
void __cdecl StoreShadowHook(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9,
                             u32 a10, u32 zDistance, u32 a12, u32 a13, u32 realtime, u32 a15) {
    (void)zDistance;
    void* shadow = reinterpret_cast<void*>(realtime);
    void* owner = shadow ? Field<void*>(shadow, RTSHADOW_OWNER) : nullptr;
    if (!owner || !HasRwObject(owner)) {
        return;
    }
    const float z = IsAircraft(owner) ? g_cfg.zLimitInAir : g_cfg.zLimit;
    Original<StoreShadowFn>(C_STORE_SHADOW)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, Bits(z), a12, a13,
                                            realtime, a15);
}

// ------------------------------------------------------------------------------------------------ shader
// Descarrega o buffer com os polys de uma sombra em tempo real usando o pixel shader do mod.
void FlushRealtimeShadow(VoidFn render) {
    IDirect3DDevice9* device = At<IDirect3DDevice9*>(RW_D3D_DEVICE);
    const bool shader = ShaderOn() && device;
    const bool combine = shader && g_cfg.combineWithStencil;
    if (shader) {
        // O RenderWare acha que nao ha pixel shader e nao troca o do mod durante o desenho.
        At<void*>(RW_LAST_PIXEL_SHADER) = nullptr;
        float color[4];
        if (combine) {
            color[0] = color[1] = color[2] = 0.0f;
            color[3] = 1.0f;
        } else {
            color[0] = g_cfg.realtimeColor[0] / 255.0f;
            color[1] = g_cfg.realtimeColor[1] / 255.0f;
            color[2] = g_cfg.realtimeColor[2] / 255.0f;
            color[3] = g_cfg.realtimeColor[3] / 255.0f * Weather();
        }
        device->SetPixelShader(static_cast<IDirect3DPixelShader9*>(combine ? ShaderStencil() : ShaderRealtime()));
        device->SetPixelShaderConstantF(0, color, 1);
    }
    if (combine) {
        // Nao pinta nada: so soma 1 no stencil onde a sombra passa no teste de alfa. O retangulo do stencil
        // depois escurece tudo junto, com a cor de STENCIL_SHADOWS_COLOR.
        RenderStateSet(RS_SRCBLEND, 1);  // rwBLENDZERO
        RenderStateSet(RS_DESTBLEND, 2); // rwBLENDONE
        RenderStateSet(RS_STENCILENABLE, 1);
        RenderStateSet(RS_STENCILFUNCTIONMASK, 0xFFFFFFFF);
        RenderStateSet(RS_STENCILFUNCTIONWRITEMASK, 0xFFFFFFFF);
        RenderStateSet(RS_STENCILFUNCTION, 7); // rwSTENCILFUNCTIONGREATEREQUAL
        RenderStateSet(RS_STENCILFUNCTIONREF, 0);
        RenderStateSet(RS_STENCILPASS, 4); // rwSTENCILOPERATIONINCRSAT
    }
    render();
    if (shader) {
        device->SetPixelShader(nullptr);
    }
    if (combine) {
        RenderStateSet(RS_STENCILENABLE, 0);
    }
}

void __cdecl CastRealTimeHook(u32 a0, u32 a1, u32 a2, u32 a3, u32 a4, u32 a5, u32 a6, u32 a7, u32 a8, u32 a9,
                              u32 a10, u32 a11, u32 a12, u32 a13, u32 a14, u32 a15, u32 a16, u32 realtime, u32 a18) {
    void* shadow = reinterpret_cast<void*>(realtime);
    g_castOwner = shadow ? Field<void*>(shadow, RTSHADOW_OWNER) : nullptr;
    g_inRealtimeCast = true;
    Original<CastRealTimeFn>(C_CAST_REALTIME)(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15,
                                              a16, realtime, a18);
    g_inRealtimeCast = false;
    FlushRealtimeShadow(Fn<VoidFn>(RenderBuffer_RenderStuffInBuffer));
}

// RenderBuffer::StartStoring com o buffer cheio.
template <int N>
void __cdecl OverflowHook() {
    VoidFn render = Original<VoidFn>(static_cast<CallId>(C_OVERFLOW_1 + N));
    if (g_inRealtimeCast) {
        FlushRealtimeShadow(render);
    } else {
        render();
    }
}

// ------------------------------------------------------------------------------------------------ instalacao
bool CheckCalls(const CallId* ids, int count) {
    for (int i = 0; i < count; i++) {
        if (!g_calls[ids[i]].Check()) {
            Log("  0x%06X nao tem a chamada esperada (%s)", static_cast<unsigned>(g_calls[ids[i]].site),
                g_calls[ids[i]].name);
            return false;
        }
    }
    return true;
}

void Install(CallId id, const void* hook) {
    g_calls[id].Install(hook);
}

void PointTo(const uintptr_t* sites, int count, const float* value) {
    for (int i = 0; i < count; i++) {
        patch::Write(sites[i], &value, 4);
    }
}

bool PointersOk(const uintptr_t* sites, int count) {
    for (int i = 0; i < count; i++) {
        if (!patch::Readable(sites[i], 4) || !patch::InImage(At<uintptr_t>(sites[i]))) {
            Log("  0x%06X nao aponta para uma constante do jogo", static_cast<unsigned>(sites[i]));
            return false;
        }
    }
    return true;
}

void SetByte(uintptr_t addr, int value) {
    const uint8_t b = static_cast<uint8_t>(value);
    patch::Write(addr, &b, 1);
}

void ApplyToggles() {
    g_flagIgnoreSome.Set(g_cfg.flagIgnoreSomeShadows);
    g_disableBuildings.Set(g_cfg.disableBuildingShadows);
    g_stencilLow1.Set(g_cfg.stencilLowSettings);
    g_stencilLow2.Set(g_cfg.stencilLowSettings);
    g_realtimeLow1.Set(g_cfg.realtimeLowSettings);
    g_realtimeLow2.Set(g_cfg.realtimeLowSettings);
    g_vehicleDefault.Set(g_cfg.drawVehicleDefaultWithRealTime);
    g_morePlayers.Set(g_cfg.moreThanOnePlayer);
}

// O SA-MP desliga a atualizacao das sombras em tempo real: apaga a chamada em Idle (0x53EA08, NOPs) e poe um ret no
// comeco de CRealTimeShadowManager::Update. Sem ela, nenhuma sombra em tempo real e desenhada. O mod religa (como o
// Shadows Extender) quando o jogo inicia e confere de novo a cada quadro. So mexe se cada byte for o do jogo ou o
// que o SA-MP poe: um gancho de outro mod nesses lugares fica como esta.
bool OnlyDisabled(uintptr_t addr, const uint8_t* vanilla, int size, uint8_t off) {
    for (int i = 0; i < size; i++) {
        const uint8_t b = At<uint8_t>(addr + i);
        if (b != vanilla[i] && b != off) {
            return false;
        }
    }
    return true;
}

bool RestoreRealtimeUpdate() {
    // mov ecx, offset g_realTimeShadowMan; call CRealTimeShadowManager::Update
    static const uint8_t idleCall[10] = {0xB9, 0x50, 0x03, 0xC4, 0x00, 0xE8, 0x9E, 0x80, 0x1C, 0x00};
    static const uint8_t prologue[5] = {0x51, 0x53, 0x57, 0x8B, 0xF9}; // push ecx; push ebx; push edi; mov edi, ecx
    bool changed = false;
    if (memcmp(reinterpret_cast<void*>(0x53EA08), idleCall, 10) && OnlyDisabled(0x53EA08, idleCall, 10, 0x90)) {
        patch::Write(0x53EA08, idleCall, 10);
        changed = true;
    }
    if (memcmp(reinterpret_cast<void*>(CRealTimeShadowManager_Update), prologue, 5) &&
        OnlyDisabled(CRealTimeShadowManager_Update, prologue, 5, 0xC3)) {
        patch::Write(CRealTimeShadowManager_Update, prologue, 5);
        changed = true;
    }
    return changed;
}

void ApplyLiveValues() {
    g_stencilMaxDistance = g_cfg.stencilMaxDistance;
    g_stencilMaxDistanceSq = g_cfg.stencilMaxDistance * g_cfg.stencilMaxDistance;
    At<float>(RT_MAX_DISTANCE) = g_cfg.realtimeMaxDistance;
    At<float>(RT_MAX_DISTANCE_SQ) = g_cfg.realtimeMaxDistance * g_cfg.realtimeMaxDistance;
    g_trokSunZLimit = g_cfg.sunZLimit;
}

} // namespace

// Chamado pelo TrokExtrasStub em CRealTimeShadow::Update, com a camera da sombra ainda aberta (os NOPs em
// CShadowCamera::Update): desenha os extras, inverte o raster e fecha a camera, como o jogo faria.
// So a sombra de clump passa por aqueles NOPs; a de atomic (e a que o RwCameraBeginUpdate recusou) chega aqui
// com a camera fechada e fica como esta (o Shadows Extender invertia e fechava de novo).
extern "C" void TrokShadowExtras(void* shadow) {
    if (!g_clumpRendered) {
        return;
    }
    g_clumpRendered = false;
    void* camera = Field<void*>(shadow, RTSHADOW_CAMERA);
    void* globals = At<void*>(RW_ENGINE_INSTANCE);
    if (!g_loggedCamera && globals && *static_cast<void**>(globals) != camera) {
        g_loggedCamera = true;
        Log("diagnostico: a camera atual do RenderWare nao e a da sombra (0x%08X, sombra 0x%08X)",
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(*static_cast<void**>(globals))),
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(camera)));
    }
    void* owner = Field<void*>(shadow, RTSHADOW_OWNER);
    if (owner && HasRwObject(owner)) {
        if (EntityType(owner) == ENTITY_PED) {
            if (g_cfg.weaponsInShadow) {
                DrawPedExtras(owner);
            }
        } else if (EntityType(owner) == ENTITY_VEHICLE) {
            // Quem esta no veiculo entra na sombra dele (uma sombra so: nada de escurecer dobrado onde as duas
            // sombras se cruzavam).
            for (int i = 0; i < 9; i++) {
                void* ped = Field<void*>(owner, i == 0 ? VEHICLE_DRIVER : VEHICLE_PASSENGERS + (i - 1) * 4);
                if (!ped || !HasRwObject(ped) || VehicleOf(ped) != owner) {
                    continue;
                }
                Fn<ForAllAtomicsFn>(game::RpClumpForAllAtomics)(Field<void*>(ped, ENTITY_RWOBJECT),
                                                                reinterpret_cast<void*>(&SilhouetteAtomic), nullptr);
                if (g_cfg.weaponsInShadow) {
                    DrawPedExtras(ped);
                }
            }
        }
    }
    Fn<ThisFn>(CShadowCamera_InvertRaster)(static_cast<uint8_t*>(shadow) + RTSHADOW_CAMERA);
    Fn<PtrFn>(game::RwCameraEndUpdate)(camera);
}

// Uma vez por quadro (evento de quadro, antes do CRealTimeShadowManager::Update): raio do limite de sombras em tempo
// real a partir das distancias de quem pediu sombra no quadro anterior; e a atualizacao das sombras ligada.
void HooksFrame() {
    if (!g_applied) {
        return;
    }
    if (RestoreRealtimeUpdate() && ++g_updateRestores <= 3) {
        Log("religado de novo: alguem desligou a atualizacao das sombras em tempo real com o jogo aberto (%dx)",
            g_updateRestores);
    }
    const int limit = g_cfg.maxRealtime;
    if (g_candidateCount > limit) {
        // A limit-esima menor distancia (selecao parcial; ate 256 entradas).
        float* d = g_candidateDist;
        for (int i = 0; i < limit; i++) {
            int best = i;
            for (int j = i + 1; j < g_candidateCount; j++) {
                if (d[j] < d[best]) {
                    best = j;
                }
            }
            const float t = d[i];
            d[i] = d[best];
            d[best] = t;
        }
        g_limitRadiusSq = d[limit - 1];
    } else {
        g_limitRadiusSq = 1e30f;
    }
    if (g_candidatesSeen > MANAGER_SLOTS && !g_loggedBusy) {
        g_loggedBusy = true;
        Log("diagnostico: %d entidades pediram sombra em tempo real num quadro (o jogo tem %d vagas) -- o limite "
            "MaxRealTimeShadows=%d fica com as mais perto",
            g_candidatesSeen, MANAGER_SLOTS, limit);
    }
    g_candidateCount = 0;
    g_candidatesSeen = 0;
}

// ------------------------------------------------------------------------------------------------ API
bool HooksVersionOk() {
    // Pontos fixos do 1.0 US. Uma chamada ja desviada por outro mod (destino fora do exe) tambem vale.
    static const CallId anchors[] = {C_SHADOW_UPDATE, C_STORE_SHADOW, C_CAST_REALTIME, C_VEHICLE_1, C_STENCIL_RECT,
                                     C_FOR_ALL_ATOMICS, C_SET_LIGHT};
    for (CallId id : anchors) {
        if (!g_calls[id].Check()) {
            Log("versao: 0x%06X nao tem a chamada do 1.0 US", static_cast<unsigned>(g_calls[id].site));
            return false;
        }
    }
    static const uint8_t extras[] = {0x8B, 0x07, 0x8A, 0x4E, 0x10}; // mov eax, [edi]; mov cl, [esi+10h]
    if (!patch::Readable(0x706676, 5) || memcmp(reinterpret_cast<void*>(0x706676), extras, 5)) {
        Log("versao: 0x706676 diferente do 1.0 US");
        return false;
    }
    const uint8_t* sun = reinterpret_cast<const uint8_t*>(0x707E2B); // mov ecx, edi; fld dword ptr [eax+8]
    const bool movEcxEdi = patch::Readable(0x707E2B, 5) &&
                           ((sun[0] == 0x8B && sun[1] == 0xCF) || (sun[0] == 0x89 && sun[1] == 0xF9));
    if (!movEcxEdi || sun[2] != 0xD9 || sun[3] != 0x40 || sun[4] != 0x08) {
        Log("versao: 0x707E2B diferente do 1.0 US");
        return false;
    }
    for (const patch::Call& ev : g_events) {
        if (!ev.Check()) {
            Log("versao: 0x%06X (%s) nao tem uma chamada", static_cast<unsigned>(ev.site), ev.name);
            return false;
        }
    }
    return true;
}

void HooksInstallEvents() {
    static void (*const stubs[EVENT_COUNT])() = {TrokEventStub0, TrokEventStub1, TrokEventStub2, TrokEventStub3};
    for (int i = 0; i < EVENT_COUNT; i++) {
        if (g_events[i].Install(reinterpret_cast<const void*>(stubs[i]))) {
            g_trokEventOriginal[i] = g_events[i].original;
        }
    }
}

void HooksApply() {
    if (g_applied) {
        return;
    }
    g_applied = true;

    // --- Sombras stencil ---
    static const CallId poolCalls[] = {C_STENCIL_INIT};
    if (CheckCalls(poolCalls, 1)) {
        g_stencilPoolCount = g_cfg.maxShadows;
        Install(C_STENCIL_INIT, reinterpret_cast<const void*>(&StencilInitHook));
        Log("aplicado: ate %d sombras stencil", g_stencilPoolCount);
    }

    static const uintptr_t distanceSites[] = {0x711943, 0x711954, 0x711969, 0x711977};
    static const uintptr_t distanceSqSites[] = {0x711827, 0x71183B, 0x7118DC, 0x711C05, 0x711D22};
    if (PointersOk(distanceSites, 4) && PointersOk(distanceSqSites, 5)) {
        PointTo(distanceSites, 4, &g_stencilMaxDistance);
        PointTo(distanceSqSites, 5, &g_stencilMaxDistanceSq);
        Log("aplicado: distancia das sombras stencil");
    }

    static const CallId rectCalls[] = {C_STENCIL_RECT};
    if (CheckCalls(rectCalls, 1)) {
        // As duas chamadas do Im2D que ficam desligadas durante o retangulo (como no Shadows Extender).
        const uintptr_t vs = patch::CallTarget(0x7FB81D), ps = patch::CallTarget(0x7FB824);
        g_rectCallsOk = vs && ps && game::At<uint8_t>(0x7FB81D) == 0xE8 && game::At<uint8_t>(0x7FB824) == 0xE8;
        if (g_rectCallsOk) {
            memcpy(g_rectCalls[0], reinterpret_cast<void*>(0x7FB81D), 5);
            memcpy(g_rectCalls[1], reinterpret_cast<void*>(0x7FB824), 5);
            DWORD old;
            VirtualProtect(reinterpret_cast<void*>(0x7FB81D), 12, PAGE_EXECUTE_READWRITE, &old);
        }
        Install(C_STENCIL_RECT, reinterpret_cast<const void*>(&StencilRectHook));
        Log("aplicado: cor e forca da sombra stencil");
    }

    // O Shadows Extender tira a sombra stencil dos veiculos (eles ganham sombra em tempo real).
    patch::Fill(0x711E26, 0x90, 5);

    // --- Sombras em tempo real: qualidade (vale na proxima vez que o jogo criar as sombras) ---
    struct ByteSite {
        uintptr_t addr;
        int value;
    };
    const ByteSite bytes[] = {
        {0x706814, g_cfg.createBlur1},  {0x706810, g_cfg.createBlur2},   {0x706812, g_cfg.blurLevel},
        {0x7064C2, g_cfg.rasterSize},   {0x7064F9, g_cfg.blurRasterSize}, {0x706825, g_cfg.rasterSize2},
        {0x706832, g_cfg.blurRasterSize2},
    };
    bool pushesOk = true;
    for (const ByteSite& b : bytes) {
        if (At<uint8_t>(b.addr - 1) != 0x6A) { // push imm8
            Log("  0x%06X nao e um push", static_cast<unsigned>(b.addr - 1));
            pushesOk = false;
        }
    }
    if (pushesOk) {
        for (const ByteSite& b : bytes) {
            SetByte(b.addr, b.value);
        }
        At<int>(GRADIENT_MAX) = g_cfg.gradientMax;
        At<int>(GRADIENT_MIN) = g_cfg.gradientMin;
        Log("aplicado: resolucao, desfoque e degrade da sombra em tempo real");
    }

    // --- Veiculos com sombra em tempo real ---
    static const CallId vehicleCalls[] = {C_VEHICLE_1, C_VEHICLE_2, C_VEHICLE_3, C_VEHICLE_4, C_VEHICLE_5};
    if (CheckCalls(vehicleCalls, 5)) {
        Install(C_VEHICLE_1, reinterpret_cast<const void*>(&VehicleShadowHook<0>));
        Install(C_VEHICLE_2, reinterpret_cast<const void*>(&VehicleShadowHook<1>));
        Install(C_VEHICLE_3, reinterpret_cast<const void*>(&VehicleShadowHook<2>));
        Install(C_VEHICLE_4, reinterpret_cast<const void*>(&VehicleShadowHook<3>));
        Install(C_VEHICLE_5, reinterpret_cast<const void*>(&VehicleShadowHook<4>));
        Log("aplicado: veiculos com sombra em tempo real");
    }

    // --- Arma, paraquedas e mochila a jato na sombra ---
    // CShadowCamera::Update(RpClump*) deixa a camera aberta (InvertRaster e RwCameraEndUpdate viram NOP) e o
    // mod desenha os extras e fecha a camera logo depois, em CRealTimeShadow::Update.
    // O RpClumpForAllAtomics trocado (0x705C4A) marca que a camera ficou aberta: sem ele, nada de NOP.
    static const CallId extrasCalls[] = {C_FOR_ALL_ATOMICS};
    const bool cameraCallsOk = At<uint8_t>(0x705C57) == 0xE8 && At<uint8_t>(0x705C5F) == 0xE8 &&
                               CheckCalls(extrasCalls, 1);
    if (cameraCallsOk) {
        if (patch::CallTarget(0x705C57) != CShadowCamera_InvertRaster ||
            patch::CallTarget(0x705C5F) != game::RwCameraEndUpdate) {
            Log("  aviso: 0x705C57/0x705C5F chamam 0x%08X/0x%08X (outro mod?)",
                static_cast<unsigned>(patch::CallTarget(0x705C57)), static_cast<unsigned>(patch::CallTarget(0x705C5F)));
        }
        Install(C_FOR_ALL_ATOMICS, reinterpret_cast<const void*>(&ForAllAtomicsHook));
        patch::Fill(0x705C57, 0x90, 5);
        patch::Fill(0x705C5F, 0x90, 5);
        patch::SetJump(0x706676, reinterpret_cast<const void*>(&TrokExtrasStub));
        Log("aplicado: arma, paraquedas, mochila a jato e quem esta no veiculo dentro da sombra");
    } else {
        Log("  pulado: arma na sombra (0x705C57/0x705C5F diferentes do 1.0 US)");
    }

    // --- Pedestres e cutscenes ---
    static const CallId pedCalls[] = {C_PED_RPHANIM};
    // 0x5E68A2: "mov ecx, offset g_realTimeShadowMan; push esi; call DoShadowThisFrame" (11 bytes).
    const bool pedNopOk = patch::Readable(0x5E68A2, 11) && At<uint8_t>(0x5E68A8) == 0xE8;
    if (pedNopOk && CheckCalls(pedCalls, 1)) {
        if (patch::CallTarget(0x5E68A8) != CRealTimeShadowManager_DoShadowThisFrame) {
            Log("  aviso: 0x5E68A8 chama 0x%08X (outro mod?)", static_cast<unsigned>(patch::CallTarget(0x5E68A8)));
        }
        patch::Fill(0x5E68A2, 0x90, 11);
        Install(C_PED_RPHANIM, reinterpret_cast<const void*>(&PedRpHAnimHook));
        Log("aplicado: sombra em tempo real em todo pedestre");
    } else {
        Log("  pulado: sombra em todo pedestre (0x5E68A2/0x5E6664 diferentes do 1.0 US)");
    }
    static const CallId safetyCalls[] = {C_CUTSCENE_SHADOW, C_SHADOW_UPDATE, C_BONE_POSITION};
    if (CheckCalls(safetyCalls, 3)) {
        Install(C_CUTSCENE_SHADOW, reinterpret_cast<const void*>(&CutsceneShadowHook));
        Install(C_SHADOW_UPDATE, reinterpret_cast<const void*>(&ShadowUpdateHook));
        Install(C_BONE_POSITION, reinterpret_cast<const void*>(&BonePositionHook));
        Log("aplicado: protecao contra sombra de quem nao tem modelo");
    }

    // --- Luz, sol e projecao ---
    static const CallId lightCalls[] = {C_SET_LIGHT, C_FRAME_ROTATE};
    if (CheckCalls(lightCalls, 2)) {
        Install(C_SET_LIGHT, reinterpret_cast<const void*>(&SetLightHook));
        Install(C_FRAME_ROTATE, reinterpret_cast<const void*>(&FrameRotateHook));
        patch::SetJump(0x707E2B, reinterpret_cast<const void*>(&TrokSunStub));
        Log("aplicado: luz da sombra segue a altura do sol");
    }

    static const uintptr_t quadSites[] = {0x707EF7, 0x707F05, 0x707F13, 0x707F21};
    static const uintptr_t projectionSites[] = {0x70A211, 0x70A228};
    static const CallId projectionCalls[] = {C_MATRIX_TRANSLATE, C_COL_SPHERE, C_STORE_SHADOW};
    if (PointersOk(quadSites, 4) && PointersOk(projectionSites, 2) && CheckCalls(projectionCalls, 3)) {
        PointTo(quadSites, 4, &kShadowQuadScale);
        PointTo(projectionSites, 2, &kProjectionScale);
        patch::Fill(0x70A0C9, 0x90, 5);
        Install(C_MATRIX_TRANSLATE, reinterpret_cast<const void*>(&MatrixTranslateHook));
        Install(C_COL_SPHERE, reinterpret_cast<const void*>(&ColSphereSetHook));
        Install(C_STORE_SHADOW, reinterpret_cast<const void*>(&StoreShadowHook));
        Log("aplicado: tamanho, raio e alcance da projecao");
    }

    // --- Shader: cor da sombra em tempo real e modo combinado ---
    static const CallId shaderCalls[] = {C_CAST_REALTIME, C_OVERFLOW_1, C_OVERFLOW_2};
    if (CheckCalls(shaderCalls, 3)) {
        Install(C_CAST_REALTIME, reinterpret_cast<const void*>(&CastRealTimeHook));
        Install(C_OVERFLOW_1, reinterpret_cast<const void*>(&OverflowHook<0>));
        Install(C_OVERFLOW_2, reinterpret_cast<const void*>(&OverflowHook<1>));
        if (ShadersCreate()) {
            Log("aplicado: pixel shaders (cor propria e modo combinado)");
        } else {
            Log("aviso: sem pixel shader -- a sombra em tempo real fica com a cor do jogo e sem modo combinado");
        }
    }

    ApplyLiveValues();
    ApplyToggles();
    if (g_cfg.combineWithStencil && !CombineOn()) {
        Log("aviso: CombineRealTimeShadowsWithStencil precisa de EnableShadowsShader=1 e do shader carregado");
    }
}

void HooksApplyLive(const Config& before) {
    if (!g_applied) {
        return;
    }
    ApplyLiveValues();
    ApplyToggles();
    if (before.maxShadows != g_cfg.maxShadows || before.createBlur1 != g_cfg.createBlur1 ||
        before.createBlur2 != g_cfg.createBlur2 || before.blurLevel != g_cfg.blurLevel ||
        before.rasterSize != g_cfg.rasterSize || before.blurRasterSize != g_cfg.blurRasterSize ||
        before.rasterSize2 != g_cfg.rasterSize2 || before.blurRasterSize2 != g_cfg.blurRasterSize2 ||
        before.gradientMax != g_cfg.gradientMax || before.gradientMin != g_cfg.gradientMin) {
        Log("  MaxShadows, Raster, Blur e Gradient mudam quando o jogo abrir de novo");
    }
    if (g_cfg.combineWithStencil && !CombineOn()) {
        Log("aviso: CombineRealTimeShadowsWithStencil precisa de EnableShadowsShader=1 e do shader carregado");
    }
}

void HooksRestoreRealtimeUpdate() {
    if (RestoreRealtimeUpdate()) {
        Log("religado: atualizacao das sombras em tempo real (estava desligada -- o SA-MP faz isso)");
    }
}

void HooksShutdown() {
    ShadersRelease();
}
