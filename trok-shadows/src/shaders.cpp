// Trok Shadows (.asi) -- Victor_Trok
// Os dois pixel shaders do Shadows Extender, ja montados (ps_2_0). O original compilava shadows_pixel.fx e
// shadows_pixel_stencil.fx com o d3dx9_43.dll ao abrir o jogo: sem o DirectX de junho de 2010 instalado o
// .asi nem carregava. Aqui o bytecode vem dentro do .asi e o mod nao depende de d3dx nem dos .fx.
//
// Registradores: s0 = textura da sombra, c0 = cor (r, g, b, forca).

#include "trok.h"
#include "game.h"

#include <d3d9.h>

namespace {

// shadows_pixel.fx -- sombra em tempo real com cor propria (modo nao combinado):
//   transp = c0.a * tex.r;  saida = ((1 - c0.rgb) * transp, 1)
//
//   ps_2_0
//   def c1, 1, 0, 0, 0
//   dcl t0.xy
//   dcl_2d s0
//   texld r0, t0, s0
//   mul r0.x, r0.x, c0.w
//   add r1.xyz, -c0, c1.x
//   mul r1.xyz, r1, r0.x
//   mov r1.w, c1.x
//   mov oC0, r1
const DWORD kRealtimeShader[] = {
    0xFFFF0200,
    0x05000051, 0xA00F0001, 0x3F800000, 0x00000000, 0x00000000, 0x00000000,
    0x0200001F, 0x80000000, 0xB0030000,
    0x0200001F, 0x90000000, 0xA00F0800,
    0x03000042, 0x800F0000, 0xB0E40000, 0xA0E40800,
    0x03000005, 0x80010000, 0x80000000, 0xA0FF0000,
    0x03000002, 0x80070001, 0xA1E40000, 0xA0000001,
    0x03000005, 0x80070001, 0x80E40001, 0x80000000,
    0x02000001, 0x80080001, 0xA0000001,
    0x02000001, 0x800F0800, 0x80E40001,
    0x0000FFFF,
};

// shadows_pixel_stencil.fx -- modo combinado (so a forca importa; a cor vem do passe do stencil):
//   saida = (c0.rgb, c0.a * tex.r)
//
//   ps_2_0
//   dcl t0.xy
//   dcl_2d s0
//   texld r0, t0, s0
//   mul r0.w, r0.x, c0.w
//   mov r0.xyz, c0
//   mov oC0, r0
const DWORD kStencilShader[] = {
    0xFFFF0200,
    0x0200001F, 0x80000000, 0xB0030000,
    0x0200001F, 0x90000000, 0xA00F0800,
    0x03000042, 0x800F0000, 0xB0E40000, 0xA0E40800,
    0x03000005, 0x80080000, 0x80000000, 0xA0FF0000,
    0x02000001, 0x80070000, 0xA0E40000,
    0x02000001, 0x800F0800, 0x80E40000,
    0x0000FFFF,
};

IDirect3DPixelShader9* g_realtime = nullptr;
IDirect3DPixelShader9* g_stencil = nullptr;

} // namespace

bool ShadersCreate() {
    if (g_realtime && g_stencil) {
        return true;
    }
    IDirect3DDevice9* device = game::At<IDirect3DDevice9*>(game::RW_D3D_DEVICE);
    if (!device) {
        Log("shader: o device do jogo ainda nao existe");
        return false;
    }
    HRESULT hr = device->CreatePixelShader(kRealtimeShader, &g_realtime);
    if (FAILED(hr)) {
        Log("shader: a placa recusou o shader da sombra em tempo real (0x%08lX)", static_cast<unsigned long>(hr));
        g_realtime = nullptr;
        return false;
    }
    hr = device->CreatePixelShader(kStencilShader, &g_stencil);
    if (FAILED(hr)) {
        Log("shader: a placa recusou o shader do modo combinado (0x%08lX)", static_cast<unsigned long>(hr));
        g_realtime->Release();
        g_realtime = nullptr;
        g_stencil = nullptr;
        return false;
    }
    return true;
}

void ShadersRelease() {
    if (g_realtime) {
        g_realtime->Release();
        g_realtime = nullptr;
    }
    if (g_stencil) {
        g_stencil->Release();
        g_stencil = nullptr;
    }
}

bool ShadersReady() {
    return g_realtime && g_stencil;
}

void* ShaderRealtime() {
    return g_realtime;
}

void* ShaderStencil() {
    return g_stencil;
}
