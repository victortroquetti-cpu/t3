// Trok Shadows Menu (.asi) -- Victor_Trok
// Enderecos do gta_sa.exe 1.0 US usados pelo complemento. Os nomes vem do gta-reversed e do plugin-sdk.
#pragma once

#include <cstdint>

namespace game {

// --- Dados ---
constexpr uintptr_t RW_ENGINE_INSTANCE = 0xC97B24; // RwGlobals*; curCamera no offset 0
constexpr uintptr_t RW_D3D_DEVICE = 0xC97C28;      // IDirect3DDevice9*
constexpr uintptr_t RT_MAX_DISTANCE = 0x8D5240;    // MAX_DISTANCE_PED_SHADOWS (15.0)
constexpr uintptr_t RT_MAX_DISTANCE_SQ = 0xC4B6B0; // o mesmo ao quadrado
constexpr uintptr_t GRADIENT_MAX = 0x8D5218;       // intensidade do degrade da sombra em tempo real
constexpr uintptr_t GRADIENT_MIN = 0x8D521C;
constexpr uintptr_t REALTIME_SHADOW_MAN = 0xC40350; // g_realTimeShadowMan

// --- Funcoes ---
constexpr uintptr_t CEntity_UpdateRpHAnim = 0x532B20;
constexpr uintptr_t atomicQuickRender = 0x705620;
constexpr uintptr_t RpClumpForAllAtomics = 0x749B70;
constexpr uintptr_t RwRenderStateSet = 0x7FE420;

// --- Offsets ---
constexpr uint32_t ENTITY_RWOBJECT = 0x18;       // CEntity::m_pRwObject
constexpr uint32_t ENTITY_TYPE = 0x36;           // CEntity: tipo nos 3 bits de baixo
constexpr uint32_t RTSHADOW_OWNER = 0x00;        // CRealTimeShadow::m_pOwner
constexpr uint32_t RTSHADOW_INTENSITY = 0x05;    // CRealTimeShadow::m_nIntensity (0 a 100: acende e apaga 3 por quadro)
constexpr uint32_t RTSHADOW_CAMERA = 0x08;       // CRealTimeShadow::m_camera (CShadowCamera: RwCamera* no offset 0)
constexpr uint32_t ATOMIC_FLAGS = 0x02;          // RpAtomic::object.object.flags (bit 2 = rpATOMICRENDER)
constexpr uint32_t ATOMIC_GEOMETRY = 0x18;       // RpAtomic::geometry
constexpr uint32_t GEOMETRY_FLAGS = 0x08;        // RpGeometry::flags
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

} // namespace game
