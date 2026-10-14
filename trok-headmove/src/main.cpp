// Trok Headmove (.asi) -- Victor_Trok
// O /headmove do SA-MP 0.3.7 R1 sem o atraso dele. O SA-MP (CLocalPlayer::ProcessHead e CRemotePlayer::ProcessHead)
// guarda para onde a camera olha so uma vez por segundo, pede ao GTA a cada 200 ms que a cabeca olhe para la
// (IKChainManager_c::LookAt, "FollowPedSA") e pede o giro mais lento do GTA (0.1 por quadro, transicao de 500 ms).
// O mod deixa o SA-MP decidir QUANDO a cabeca segue a camera (o /headmove, morto, carregando...) e so muda o COMO:
//   - todo pedido "FollowPedSA" do SA-MP sai com a direcao de agora (a camera, ou o aim sync do outro jogador),
//     suavizada pelo atraso do .ini (0 = nenhuma), e com o giro do .ini;
//   - depois de cada ProcessHead, enquanto o SA-MP estiver pedindo (ultimo pedido ha menos de 400 ms), o mod repete o
//     pedido no quadro em que o SA-MP nao pediu, entao a cabeca anda todo quadro em vez de a cada 200 ms.
// Enderecos: samp.dll da SAMP-API (MIT, BlastHack), gta_sa.exe do gta-reversed / plugin-sdk.

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "MinHook.h"
#include "config.h"

void HeadLog(const char* fmt, ...); // Trok Headmove.log, ao lado do .asi

namespace {

constexpr const char* VERSION = "1.0";

// gta_sa.exe 1.0 US
constexpr DWORD GTA_VERSION_MARK = 0x82457C; // 0x94BF no 1.0 US (o mesmo teste dos plugins do GTA)
constexpr DWORD GTA_IK_LOOK_AT = 0x618970;   // IKChainManager_c::LookAt
constexpr DWORD GTA_IK_MANAGER = 0xC15448;   // g_ikChainMan: comeca com IKChain_c m_IKChains[32]
constexpr int IK_CHAIN_COUNT = 32;
constexpr DWORD IK_CHAIN_SIZE = 0x58, IKC_PED = 0x08, IKC_SPEED = 0x44, IKC_SLOT = 0x55; // IKChain_c
constexpr BYTE IK_SLOT_LOOK_AT = 0;
constexpr DWORD PED_POS = 0x04, PED_MATRIX = 0x14, MATRIX_POS = 0x30; // CPlaceable

// samp.dll 0.3.7 R1
constexpr DWORD SAMP_R1_ENTRY = 0x31DF13;
constexpr DWORD FN_LOCAL_HEAD = 0x2F80;   // CLocalPlayer::ProcessHead
constexpr DWORD FN_REMOTE_HEAD = 0x10EA0; // CRemotePlayer::ProcessHead
constexpr DWORD FN_GET_AIM = 0x98390;     // AimStuff::GetAim(int nPlayer): o aim sync de um jogador remoto
constexpr DWORD INTERNAL_AIM = 0xF1590;   // AimStuff::Aim* da camera do seu GTA
constexpr int PLAYER_NUMBERS = 210;       // AimStuff::ArrayPlayerAim
constexpr DWORD PL_PED = 0x00;            // CLocalPlayer / CRemotePlayer: m_pPed (CPed do SA-MP)
constexpr DWORD SP_GAME_PED = 0x2A4, SP_NUMBER = 0x2B0; // CPed do SA-MP: m_pGamePed, m_nPlayerNumber
constexpr DWORD SAMP_CHAT = 0x21A0E4, FN_ADD_ENTRY = 0x64010;

constexpr const char* SAMP_PURPOSE = "FollowPedSA";
constexpr float LOOK_DISTANCE = 20.0f; // como o SA-MP: um ponto 20 m a frente do personagem
constexpr double SAMP_SILENCE = 0.4;   // s sem pedido do SA-MP: o headmove parou (desligado, morto, de lado...)
constexpr int MIN_BLEND_MS = 150, MAX_BLEND_MS = 500; // a transicao do GTA precisa ser maior que um quadro

struct Vec3 {
    float x, y, z;
};

typedef void(__thiscall* LookAtFn)(void*, const char*, DWORD, void*, int, int, Vec3*, bool, float, int, int, bool);
typedef void(__thiscall* HeadFn)(void*);
typedef Vec3*(__stdcall* GetAimFn)(int);
typedef void(__thiscall* AddEntryFn)(void*, int, const char*, const char*, DWORD, DWORD);

// O estado de uma cabeca: a direcao suavizada e o ultimo pedido do SA-MP.
struct Head {
    DWORD ped = 0;      // CPed do GTA dono deste estado (mudou: comeca de novo)
    Vec3 dir = {0, 1, 0};
    double stepAt = 0;  // ultimo passo da suavizacao
    double sampAt = -1; // ultimo "FollowPedSA" do SA-MP para esta cabeca (-1 = nenhum ainda)
    bool sampNow = false; // o SA-MP pediu neste ProcessHead
    void* manager = nullptr;
    const char* purpose = nullptr; // a string do samp.dll
    int time = 2000, bone = -1, priority = 3;
    bool torso = false;
    int delayMs = 0;
    float sampTurn = 0.1f; // o giro que o SA-MP pediu (volta para ele quando o mod e desligado)
    bool changed = false;  // o mod mexeu no giro da corrente desta cabeca
};

HMODULE g_module = nullptr;
DWORD g_samp = 0;
char g_logPath[MAX_PATH] = "";
char g_iniPath[MAX_PATH] = "";
LookAtFn o_lookAt = nullptr;
HeadFn o_localHead = nullptr, o_remoteHead = nullptr;
Head g_local;
Head g_remotes[PLAYER_NUMBERS];
Head* g_context = nullptr; // a cabeca cujo ProcessHead esta rodando agora
DWORD g_configCheck = 0;
LARGE_INTEGER g_frequency = {};

// Para o log: o primeiro pedido de cada tipo e um resumo depois de um tempo.
bool g_firstLocal = true, g_firstRemote = true;
long g_adjusted = 0, g_repeated = 0;
double g_statsFrom = -1;
bool g_statsLogged = false;

template <class T>
T At(DWORD base, DWORD offset) {
    return *reinterpret_cast<T*>(base + offset);
}

bool Ptr(DWORD p) {
    return p >= 0x10000;
}

bool Readable(DWORD address, DWORD size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<void*>(address), &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        return false;
    }
    return address + size <= reinterpret_cast<DWORD>(mbi.BaseAddress) + mbi.RegionSize;
}

double Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) / static_cast<double>(g_frequency.QuadPart);
}

bool Finite(float v) {
    return v == v && v < 1e30f && v > -1e30f;
}

// Direcao valida (o aim sync pode chegar zerado antes do primeiro pacote): normaliza em out.
bool Direction(const Vec3* in, Vec3* out) {
    if (!in || !Finite(in->x) || !Finite(in->y) || !Finite(in->z)) {
        return false;
    }
    float len = std::sqrt(in->x * in->x + in->y * in->y + in->z * in->z);
    if (len < 0.5f || len > 1.5f) {
        return false;
    }
    *out = {in->x / len, in->y / len, in->z / len};
    return true;
}

DWORD GamePed(DWORD player) {
    DWORD sampPed = At<DWORD>(player, PL_PED);
    if (!Ptr(sampPed)) {
        return 0;
    }
    DWORD ped = At<DWORD>(sampPed, SP_GAME_PED);
    return Ptr(ped) ? ped : 0;
}

Vec3 PedPos(DWORD ped) {
    DWORD matrix = At<DWORD>(ped, PED_MATRIX);
    return Ptr(matrix) ? At<Vec3>(matrix, MATRIX_POS) : At<Vec3>(ped, PED_POS);
}

bool LocalFront(Vec3* out) {
    DWORD aim = At<DWORD>(g_samp, INTERNAL_AIM);
    return Ptr(aim) && Direction(reinterpret_cast<const Vec3*>(aim), out);
}

// O numero do jogador remoto no SA-MP (o indice do aim sync dele); -1 sem personagem.
int Number(DWORD player) {
    DWORD sampPed = At<DWORD>(player, PL_PED);
    if (!Ptr(sampPed)) {
        return -1;
    }
    int n = At<BYTE>(sampPed, SP_NUMBER);
    return n < PLAYER_NUMBERS ? n : -1;
}

bool RemoteFront(int number, Vec3* out) {
    return Direction(reinterpret_cast<GetAimFn>(g_samp + FN_GET_AIM)(number), out);
}

// Um passo da suavizacao: com atraso, a cabeca chega a ~95% da direcao nova em delayMs (3 constantes de tempo).
void Step(Head& h, DWORD ped, const Vec3& front, int delayMs) {
    double now = Now();
    if (h.ped != ped) {
        h = Head();
        h.ped = ped;
        h.dir = front;
    } else if (delayMs <= 0 || now - h.stepAt > 1.0) {
        h.dir = front;
    } else {
        double tau = delayMs / 3000.0;
        float a = static_cast<float>(1.0 - std::exp(-(now - h.stepAt) / tau));
        Vec3 d = {h.dir.x + (front.x - h.dir.x) * a, h.dir.y + (front.y - h.dir.y) * a,
                  h.dir.z + (front.z - h.dir.z) * a};
        if (!Direction(&d, &h.dir)) {
            h.dir = front; // virou de costas num quadro so: a media zerou
        }
    }
    h.stepAt = now;
    h.delayMs = delayMs;
}

Vec3 Target(const Head& h) {
    Vec3 p = PedPos(h.ped);
    return {p.x + h.dir.x * LOOK_DISTANCE, p.y + h.dir.y * LOOK_DISTANCE, p.z + h.dir.z * LOOK_DISTANCE};
}

int Blend(int delayMs) {
    return delayMs < MIN_BLEND_MS ? MIN_BLEND_MS : delayMs > MAX_BLEND_MS ? MAX_BLEND_MS : delayMs;
}

// O giro do GTA fica na corrente de IK (criada no primeiro pedido): troca nela tambem, para valer sem esperar a
// corrente acabar e para o .ini novo valer na hora.
void SetTurn(DWORD ped, float turn) {
    for (int i = 0; i < IK_CHAIN_COUNT; ++i) {
        DWORD chain = GTA_IK_MANAGER + i * IK_CHAIN_SIZE;
        if (At<DWORD>(chain, IKC_PED) != ped || At<BYTE>(chain, IKC_SLOT) != IK_SLOT_LOOK_AT) {
            continue;
        }
        float& speed = *reinterpret_cast<float*>(chain + IKC_SPEED);
        if (speed > 0.0f && speed <= 10.0f) { // um valor estranho aqui: nao e a estrutura que o mod conhece
            speed = turn;
        }
    }
}

// Mod desligado (ou o .ini mudou para isso): a corrente desta cabeca volta ao giro do SA-MP.
void Restore(Head& h, DWORD ped) {
    if (h.changed && h.ped == ped) {
        SetTurn(ped, h.sampTurn);
    }
    h.changed = false;
}

void Chat(const char* text) {
    DWORD chat = At<DWORD>(g_samp, SAMP_CHAT);
    if (Ptr(chat)) {
        reinterpret_cast<AddEntryFn>(g_samp + FN_ADD_ENTRY)(reinterpret_cast<void*>(chat), 8, text, nullptr, 0xFFFFFFFF, 0);
    }
}

// A cada segundo: o .ini mudou? Vale na hora (e avisa no chat, para quem esta ajustando com o jogo aberto).
void FollowConfig() {
    DWORD tick = GetTickCount();
    if (tick - g_configCheck < 1000) {
        return;
    }
    g_configCheck = tick;
    if (!cfg::Reload()) {
        return;
    }
    char text[160], line[220];
    cfg::Describe(text, sizeof(text));
    HeadLog("Trok Headmove.ini mudou: %s", text);
    snprintf(line, sizeof(line), "{9AD8FF}[Trok Headmove]{FFFFFF} %s", text);
    Chat(line);
}

void Stats() {
    if (g_statsLogged || g_statsFrom < 0 || Now() - g_statsFrom < 15.0) {
        return;
    }
    g_statsLogged = true;
    double secs = Now() - g_statsFrom;
    HeadLog("em %.0f s: %ld pedidos do SA-MP ajustados (%.1f/s) e %ld repetidos pelo mod (%.1f/s)", secs, g_adjusted,
            g_adjusted / secs, g_repeated, g_repeated / secs);
}

void __fastcall HookLookAt(void* manager, void*, const char* purpose, DWORD ped, void* entity, int time, int bone,
                           Vec3* offset, bool torso, float speed, int blend, int priority, bool force) {
    Head* h = g_context;
    // So o pedido do SA-MP de dentro do ProcessHead em andamento, para o personagem dele: o resto do GTA passa direto.
    if (h && ped == h->ped && purpose && strcmp(purpose, SAMP_PURPOSE) == 0) {
        bool local = h == &g_local;
        if (local ? g_firstLocal : g_firstRemote) {
            (local ? g_firstLocal : g_firstRemote) = false;
            HeadLog("primeiro pedido do SA-MP (%s): duracao %d ms, giro %.2f, transicao %d ms, prioridade %d -- "
                    "agora sai com a direcao de agora e giro %.2f",
                    local ? "seu personagem" : "outro jogador", time, speed, blend, priority, cfg::Get().turn);
            if (g_statsFrom < 0) {
                g_statsFrom = Now();
            }
        }
        h->sampAt = Now();
        h->sampNow = true;
        h->manager = manager;
        h->purpose = purpose;
        h->time = time;
        h->bone = bone;
        h->torso = torso;
        h->priority = priority;
        h->sampTurn = speed;
        h->changed = true;
        float turn = cfg::Get().turn;
        Vec3 target = Target(*h);
        o_lookAt(manager, purpose, ped, entity, time, bone, &target, torso, turn, Blend(h->delayMs), priority, force);
        SetTurn(ped, turn);
        ++g_adjusted;
        return;
    }
    o_lookAt(manager, purpose, ped, entity, time, bone, offset, torso, speed, blend, priority, force);
}

// Depois do ProcessHead: o SA-MP ainda quer a cabeca seguindo, mas nao pediu neste quadro? O mod pede.
void Repeat(Head& h) {
    if (h.sampNow || h.sampAt < 0 || Now() - h.sampAt > SAMP_SILENCE || !h.purpose) {
        return;
    }
    float turn = cfg::Get().turn;
    Vec3 target = Target(h);
    o_lookAt(h.manager, h.purpose, h.ped, nullptr, h.time, h.bone, &target, h.torso, turn, Blend(h.delayMs),
             h.priority, false);
    SetTurn(h.ped, turn);
    ++g_repeated;
}

void __fastcall HookLocalHead(void* self, void*) {
    FollowConfig();
    Stats();
    const cfg::Options& o = cfg::Get();
    DWORD ped = GamePed(reinterpret_cast<DWORD>(self));
    Head& h = g_local;
    Vec3 front;
    if (!ped || !o.enabled || !LocalFront(&front)) {
        Restore(h, ped);
        o_localHead(self);
        return;
    }
    Step(h, ped, front, o.delayMs);
    h.sampNow = false;
    g_context = &h;
    o_localHead(self);
    g_context = nullptr;
    Repeat(h);
}

void __fastcall HookRemoteHead(void* self, void*) {
    FollowConfig();
    const cfg::Options& o = cfg::Get();
    DWORD player = reinterpret_cast<DWORD>(self);
    DWORD ped = GamePed(player);
    int number = ped ? Number(player) : -1;
    if (number < 0) {
        o_remoteHead(self);
        return;
    }
    Head& h = g_remotes[number];
    Vec3 front;
    if (!o.enabled || !o.others || !RemoteFront(number, &front)) {
        Restore(h, ped);
        o_remoteHead(self);
        return;
    }
    Step(h, ped, front, o.othersDelayMs);
    h.sampNow = false;
    g_context = &h;
    o_remoteHead(self);
    g_context = nullptr;
    Repeat(h);
}

void Sibling(char* out, const char* name) {
    out[0] = 0;
    char path[MAX_PATH];
    if (!GetModuleFileNameA(g_module, path, MAX_PATH)) {
        return;
    }
    char* slash = strrchr(path, '\\');
    if (!slash) {
        return;
    }
    slash[1] = 0;
    snprintf(out, MAX_PATH, "%s%s", path, name);
}

bool Hook(DWORD at, void* hook, void** original, const char* name) {
    void* target = reinterpret_cast<void*>(at);
    MH_STATUS s = MH_CreateHook(target, hook, original);
    if (s == MH_OK) {
        s = MH_EnableHook(target);
        if (s != MH_OK) {
            MH_RemoveHook(target);
        }
    }
    if (s != MH_OK) {
        HeadLog("gancho %s (0x%08X) recusado (%d)", name, static_cast<unsigned>(at), s);
        return false;
    }
    return true;
}

void Unhook(DWORD at) {
    MH_DisableHook(reinterpret_cast<void*>(at));
    MH_RemoveHook(reinterpret_cast<void*>(at));
}

bool IsSampR1(DWORD base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint == SAMP_R1_ENTRY;
}

DWORD WINAPI Boot(LPVOID) {
    for (int i = 0; i < 300 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        HeadLog("samp.dll nao apareceu (single player?) -- o /headmove e do SA-MP, nada foi alterado");
        return 0;
    }
    if (!IsSampR1(g_samp)) {
        g_samp = 0;
        HeadLog("samp.dll nao e o 0.3.7 R1 -- o Trok Headmove ficou desligado (nada foi alterado)");
        return 0;
    }
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        HeadLog("MinHook nao iniciou (%d) -- desistindo", init);
        return 0;
    }
    // O LookAt primeiro: com ele no lugar, os dois ProcessHead podem chegar a qualquer momento.
    if (!Hook(GTA_IK_LOOK_AT, reinterpret_cast<void*>(HookLookAt), reinterpret_cast<void**>(&o_lookAt),
              "IKChainManager_c::LookAt")) {
        HeadLog("o Trok Headmove ficou desligado (nada foi alterado)");
        return 0;
    }
    bool local = Hook(g_samp + FN_LOCAL_HEAD, reinterpret_cast<void*>(HookLocalHead),
                      reinterpret_cast<void**>(&o_localHead), "CLocalPlayer::ProcessHead");
    bool remote = Hook(g_samp + FN_REMOTE_HEAD, reinterpret_cast<void*>(HookRemoteHead),
                       reinterpret_cast<void**>(&o_remoteHead), "CRemotePlayer::ProcessHead");
    if (!local && !remote) {
        Unhook(GTA_IK_LOOK_AT);
        HeadLog("o Trok Headmove ficou desligado (nada foi alterado)");
        return 0;
    }
    char text[160];
    cfg::Describe(text, sizeof(text));
    HeadLog("ganchos instalados (LookAt do GTA, ProcessHead %s%s%s) -- %s", local ? "seu" : "",
            local && remote ? " e " : "", remote ? "dos outros" : "", text);
    return 0;
}

} // namespace

void HeadLog(const char* fmt, ...) {
    if (!g_logPath[0]) {
        return;
    }
    FILE* file = fopen(g_logPath, "a");
    if (!file) {
        return;
    }
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(file, "[%02d:%02d:%02d] ", t.wHour, t.wMinute, t.wSecond);
    va_list args;
    va_start(args, fmt);
    vfprintf(file, fmt, args);
    va_end(args);
    fputc('\n', file);
    fclose(file);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }
    g_module = module;
    DisableThreadLibraryCalls(module);
    QueryPerformanceFrequency(&g_frequency);
    Sibling(g_logPath, "Trok Headmove.log");
    Sibling(g_iniPath, "Trok Headmove.ini");
    if (g_logPath[0]) {
        FILE* fresh = fopen(g_logPath, "w"); // um log por sessao de jogo
        if (fresh) {
            fclose(fresh);
        }
    }
    HeadLog("Trok Headmove .asi v%s -- esperando o samp.dll", VERSION);

    // Duas copias (com nomes diferentes) desviariam o mesmo pedido duas vezes.
    char marker[8];
    if (GetEnvironmentVariableA("TROK_HEADMOVE_LOADED", marker, sizeof(marker))) {
        HeadLog("outra copia do Trok Headmove ja esta carregada -- esta ficou desligada");
        return TRUE;
    }
    SetEnvironmentVariableA("TROK_HEADMOVE_LOADED", "1");
    if (!Readable(GTA_VERSION_MARK, 4) || At<DWORD>(GTA_VERSION_MARK, 0) != 0x94BF) {
        HeadLog("o gta_sa.exe nao e o 1.0 US -- o Trok Headmove ficou desligado (nada foi alterado)");
        return TRUE;
    }
    HeadLog("gta_sa.exe 1.0 US confirmado");
    cfg::Init(g_iniPath);
    HANDLE thread = CreateThread(nullptr, 0, Boot, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    }
    return TRUE;
}
