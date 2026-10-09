// Trok Shadows Menu (.asi) -- Victor_Trok
// Enderecos do gta_sa.exe 1.0 US usados pelo complemento. Os nomes vem do gta-reversed e do plugin-sdk.
#pragma once

#include <cstdint>

namespace game {

// --- Dados ---
constexpr uintptr_t RW_D3D_DEVICE = 0xC97C28;      // IDirect3DDevice9*
constexpr uintptr_t RT_MAX_DISTANCE = 0x8D5240;    // MAX_DISTANCE_PED_SHADOWS (15.0)
constexpr uintptr_t RT_MAX_DISTANCE_SQ = 0xC4B6B0; // o mesmo ao quadrado
constexpr uintptr_t GRADIENT_MAX = 0x8D5218;       // intensidade do degrade da sombra em tempo real
constexpr uintptr_t GRADIENT_MIN = 0x8D521C;
constexpr uintptr_t REALTIME_SHADOW_MAN = 0xC40350; // g_realTimeShadowMan
constexpr uintptr_t CLOUD_COVERAGE = 0xC81304;      // CWeather::CloudCoverage (0..1)
constexpr uintptr_t DN_BALANCE = 0x8D12C0;          // CCustomBuildingDNPipeline::m_fDNBalanceParam (0 dia, 1 noite)
// Buffer de desenho temporario (RenderBuffer): os poligonos de cada sombra projetada ficam aqui ate o desenho.
constexpr uintptr_t TEMP_VERTICES_STORED = 0xC4B950; // uiTempBufferVerticesStored
constexpr uintptr_t TEMP_INDICES_STORED = 0xC4B954;  // uiTempBufferIndicesStored

// --- Funcoes ---
constexpr uintptr_t RwRenderStateSet = 0x7FE420;
constexpr uintptr_t RwRenderStateGet = 0x7FD810;

// --- Offsets ---
constexpr uint32_t RTSHADOW_OWNER = 0x00;     // CRealTimeShadow::m_pOwner
constexpr uint32_t RTSHADOW_INTENSITY = 0x05; // CRealTimeShadow::m_nIntensity (0 a 100: acende e apaga 3 por quadro)
constexpr uint32_t RTSHADOW_CAMERA = 0x08;    // CRealTimeShadow::m_camera (CShadowCamera: RwCamera* no offset 0)
constexpr uint32_t MANAGER_SHADOWS = 0x04;    // CRealTimeShadowManager::m_apShadows[16]
constexpr int MANAGER_SLOTS = 16;

template <class T>
inline T& At(uintptr_t addr) {
    return *reinterpret_cast<T*>(addr);
}

template <class T>
inline T& Field(const void* base, uint32_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(base) + offset);
}

} // namespace game
