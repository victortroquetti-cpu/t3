// Trok Shadows (.asi) -- Victor_Trok
// Enderecos do gta_sa.exe 1.0 US usados pelo mod. Os nomes vem do gta-reversed e do plugin-sdk; os pontos de
// gancho sao os mesmos do Shadows Extender (DK22Pac), que este mod substitui.
#pragma once

#include <cstdint>

namespace game {

// --- Eventos (chamadas que o mod encadeia: roda a original e depois o mod) ---
constexpr uintptr_t EV_INIT_RW = 0x5BD779;      // CGame::InitialiseRenderWare, depois do device pronto
constexpr uintptr_t EV_SHUTDOWN_RW = 0x53BC21;  // CGame::ShutdownRenderWare
constexpr uintptr_t EV_INIT_GAME = 0x748CFB;    // WinMain, depois de iniciar o jogo
constexpr uintptr_t EV_GAME_PROCESS = 0x53E981; // Idle, depois de CGame::Process (todo quadro)

// --- Dados ---
constexpr uintptr_t RW_ENGINE_INSTANCE = 0xC97B24;   // RwGlobals*; curCamera no offset 0
constexpr uintptr_t RW_D3D_DEVICE = 0xC97C28;        // IDirect3DDevice9*
constexpr uintptr_t RW_LAST_PIXEL_SHADER = 0x8E244C; // _rwD3D9LastPixelShaderUsed (cache do RenderWare)
constexpr uintptr_t RW_STENCIL_CLEAR = 0xC97C44;     // valor de limpeza do stencil (RwD3D9SetStencilClear)
constexpr uintptr_t CLOUD_COVERAGE = 0xC81304;       // CWeather::CloudCoverage (0..1)
constexpr uintptr_t DN_BALANCE = 0x8D12C0;           // CCustomBuildingDNPipeline::m_fDNBalanceParam (0 dia, 1 noite)
constexpr uintptr_t RT_MAX_DISTANCE = 0x8D5240;      // MAX_DISTANCE_PED_SHADOWS (15.0)
constexpr uintptr_t RT_MAX_DISTANCE_SQ = 0xC4B6B0;   // o mesmo ao quadrado
constexpr uintptr_t GRADIENT_MAX = 0x8D5218;         // intensidade do degrade da sombra em tempo real
constexpr uintptr_t GRADIENT_MIN = 0x8D521C;
constexpr uintptr_t STENCIL_FIRST_AVAILABLE = 0xC6A168; // CStencilShadows::pFirstAvailableStencilShadowObject
constexpr uintptr_t STENCIL_FIRST_ACTIVE = 0xC6A16C;    // CStencilShadows::pFirstActiveStencilShadowObject
constexpr uintptr_t REALTIME_SHADOW_MAN = 0xC40350;     // g_realTimeShadowMan
constexpr uintptr_t CAR_ENVMAP_OBJ_PIPELINE = 0xC02D24; // CCustomCarEnvMapPipeline::ObjPipeline
constexpr uintptr_t TIMER_FRAME_COUNTER = 0xB7CB4C;     // CTimer::m_FrameCounter
constexpr uintptr_t THE_CAMERA = 0xB6F028;              // TheCamera (CPlaceable: posicao como a de uma entidade)
constexpr uintptr_t PLAYER_PED = 0xB7CD98;              // CWorld::Players[0].m_pPed
constexpr uintptr_t VEC_PARACHUTE_OFFSET = 0x8D60B0;    // vetores do proprio jogo para o paraquedas e a 2a pistola
constexpr uintptr_t VEC_PARACHUTE_AXIS = 0x8D2338;
constexpr uintptr_t VEC_TWIN_AXIS = 0x8D60A4;
constexpr uintptr_t VEC_TWIN_OFFSET = 0x8D6098;

// --- Funcoes ---
constexpr uintptr_t CStencilShadows_Init = 0x70F9E0;
constexpr uintptr_t CRealTimeShadowManager_Update = 0x706AB0;
constexpr uintptr_t CRealTimeShadowManager_DoShadowThisFrame = 0x706BA0;
constexpr uintptr_t CRealTimeShadow_Update = 0x706600;
constexpr uintptr_t CRealTimeShadow_SetLightProperties = 0x705900;
constexpr uintptr_t CShadowCamera_InvertRaster = 0x705660;
constexpr uintptr_t CShadows_StoreShadowToBeRendered = 0x707390;
constexpr uintptr_t CShadows_StoreShadowForVehicle = 0x70BDA0;
constexpr uintptr_t CShadows_CastRealTimeShadowSectorList = 0x70A7E0;
constexpr uintptr_t RenderBuffer_RenderStuffInBuffer = 0x707800;
constexpr uintptr_t CColSphere_Set = 0x40FD10;
constexpr uintptr_t CSprite2d_DrawRect = 0x727B60;
constexpr uintptr_t CEntity_UpdateRpHAnim = 0x532B20;
constexpr uintptr_t CPed_GetBonePosition = 0x5E4280;
constexpr uintptr_t CPed_GetWeaponSkill = 0x5E6580;
constexpr uintptr_t CWeaponInfo_GetWeaponInfo = 0x743C60;
constexpr uintptr_t CPedIntelligence_GetTaskJetPack = 0x601110;
constexpr uintptr_t CTaskSimpleJetPack_RenderJetPack = 0x67F6A0;
constexpr uintptr_t atomicQuickRender = 0x705620;
constexpr uintptr_t RpClumpForAllAtomics = 0x749B70;
constexpr uintptr_t RpClumpRender = 0x749B20;
constexpr uintptr_t GetAnimHierarchyFromSkinClump = 0x734A40;
constexpr uintptr_t RpHAnimIDGetIndex = 0x7C51A0;
constexpr uintptr_t RpHAnimHierarchyGetMatrixArray = 0x7C5120;
constexpr uintptr_t RwFrameUpdateObjects = 0x7F0910;
constexpr uintptr_t RwMatrixTranslate = 0x7F2450;
constexpr uintptr_t RwMatrixRotate = 0x7F1FD0;
constexpr uintptr_t RwFrameRotate = 0x7F1010;
constexpr uintptr_t RwCameraEndUpdate = 0x7EE180;
constexpr uintptr_t RwRenderStateSet = 0x7FE420;
constexpr uintptr_t RwRenderStateGet = 0x7FD810;
constexpr uintptr_t rwD3D9SetVertexShader = 0x7F9FB0;
constexpr uintptr_t rwD3D9SetPixelShader = 0x7F9FF0;

// Callbacks de desenho do CVisibilityPlugins que a sombra ignora (LOD de veiculo) ou trata a parte (helices).
constexpr uintptr_t RenderVehicleReallyLowDetailCB_BigVehicle = 0x732820;
constexpr uintptr_t RenderVehicleReallyLowDetailCB = 0x7331E0;
constexpr uintptr_t RenderVehicleLoDetailCB_Boat = 0x7334F0;
constexpr uintptr_t RenderHeliRotorAlphaCB = 0x7340B0;
constexpr uintptr_t RenderHeliTailRotorAlphaCB = 0x734170;

// Faixa da imagem do gta_sa.exe: um gancho que aponta para fora dela e de outro mod (encadeia).
constexpr uintptr_t EXE_BEGIN = 0x400000;
constexpr uintptr_t EXE_END = 0xD00000;

// --- Offsets ---
constexpr uint32_t ENTITY_RWOBJECT = 0x18;      // CEntity::m_pRwObject
constexpr uint32_t ENTITY_TYPE = 0x36;          // CEntity: tipo nos 3 bits de baixo
constexpr uint32_t PLACEABLE_POSITION = 0x04;   // CPlaceable::m_placement.m_vPosn
constexpr uint32_t PLACEABLE_MATRIX = 0x14;     // CPlaceable::m_matrix
constexpr uint32_t MATRIX_POSITION = 0x30;      // CMatrix::pos
constexpr uint32_t PED_INTELLIGENCE = 0x47C;    // CPed::m_pIntelligence
constexpr uint32_t PED_WEAPON_OBJECT = 0x4F4;   // CPed::m_pWeaponObject (RpClump da arma na mao)
constexpr uint32_t PED_WEAPONS = 0x5A0;         // CPed::m_aWeapons[13], 0x1C cada, tipo no offset 0
constexpr uint32_t PED_WEAPON_SLOT = 0x718;     // CPed::m_nSelectedWepSlot
constexpr uint32_t WEAPON_SIZE = 0x1C;
constexpr uint32_t WEAPONINFO_FLAGS = 0x18;     // CWeaponInfo::m_nFlags; bit 11 = duas pistolas
constexpr uint32_t VEHICLE_SUBTYPE = 0x594;     // CVehicle::m_nVehicleSubType
constexpr uint32_t JETPACK_CLUMP = 0x40;        // CTaskSimpleJetPack: clump da mochila
constexpr uint32_t RTSHADOW_OWNER = 0x00;       // CRealTimeShadow::m_pOwner
constexpr uint32_t RTSHADOW_CAMERA = 0x08;      // CRealTimeShadow::m_camera (CShadowCamera: RwCamera* no offset 0)
constexpr uint32_t ATOMIC_FLAGS = 0x02;         // RpAtomic::object.object.flags (bit 2 = rpATOMICRENDER)
constexpr uint32_t ATOMIC_RENDER_CB = 0x48;     // RpAtomic::renderCallBack
constexpr uint32_t ATOMIC_PIPELINE = 0x6C;      // RpAtomic::pipeline
constexpr uint32_t OBJECT_PARENT = 0x04;        // RwObject::parent (frame de um clump)
constexpr uint32_t FRAME_MODELLING = 0x10;      // RwFrame::modelling (RwMatrix, 64 bytes)
constexpr uint32_t ATOMIC_GEOMETRY = 0x18;      // RpAtomic::geometry
constexpr uint32_t GEOMETRY_FLAGS = 0x08;       // RpGeometry::flags
// Flags que CShadowCamera::Update(RpClump*) tira da geometria para desenhar a silhueta (textura, luz, cor).
constexpr uint32_t SHADOW_GEOMETRY_FLAGS = 0xEC;
constexpr uint32_t PHYSICAL_SHADOW_DATA = 0x134; // CPhysical::m_pShadowData (CRealTimeShadow*)
constexpr uint32_t PED_FLAGS_IN_VEHICLE = 0x46D; // CPed: byte dos flags com bInVehicle no bit 0
constexpr uint32_t PED_VEHICLE = 0x58C;          // CPed::m_pVehicle
constexpr uint32_t VEHICLE_DRIVER = 0x460;       // CVehicle::m_pDriver
constexpr uint32_t VEHICLE_PASSENGERS = 0x464;   // CVehicle::m_apPassengers[8]
constexpr uint32_t MANAGER_SHADOWS = 0x04;       // CRealTimeShadowManager::m_apShadows[16]
constexpr int MANAGER_SLOTS = 16;

constexpr int ENTITY_VEHICLE = 2;
constexpr int ENTITY_PED = 3;
constexpr int VEHICLE_HELI = 3;
constexpr int VEHICLE_PLANE = 4;
constexpr int WEAPON_PARACHUTE = 46;
constexpr int BONE_SPINE1 = 3;
constexpr int BONE_R_HAND = 24;
constexpr int BONE_L_HAND = 34;
constexpr uint32_t WEAPONFLAG_TWIN_PISTOL = 0x800;

// RwRenderState usados no modo combinado.
constexpr int RS_SRCBLEND = 10;
constexpr int RS_DESTBLEND = 11;
constexpr int RS_STENCILENABLE = 21;
constexpr int RS_STENCILPASS = 24;
constexpr int RS_STENCILFUNCTION = 25;
constexpr int RS_STENCILFUNCTIONREF = 26;
constexpr int RS_STENCILFUNCTIONMASK = 27;
constexpr int RS_STENCILFUNCTIONWRITEMASK = 28;
constexpr int RS_ALPHATESTFUNCTIONREF = 30;

template <class T>
inline T& At(uintptr_t addr) {
    return *reinterpret_cast<T*>(addr);
}

template <class T>
inline T& Field(const void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
}

inline int EntityType(const void* entity) {
    return Field<uint8_t>(entity, ENTITY_TYPE) & 7;
}

inline bool HasRwObject(const void* entity) {
    return Field<void*>(entity, ENTITY_RWOBJECT) != nullptr;
}

// Pedestre dentro de um veiculo (devolve o veiculo).
inline void* VehicleOf(const void* ped) {
    if (!(Field<uint8_t>(ped, PED_FLAGS_IN_VEHICLE) & 1)) {
        return nullptr;
    }
    return Field<void*>(ped, PED_VEHICLE);
}

// Helicoptero ou aviao: usam os limites "InAir".
inline bool IsAircraft(const void* entity) {
    if (!entity || EntityType(entity) != ENTITY_VEHICLE) {
        return false;
    }
    const int sub = Field<int>(entity, VEHICLE_SUBTYPE);
    return sub == VEHICLE_HELI || sub == VEHICLE_PLANE;
}

} // namespace game
