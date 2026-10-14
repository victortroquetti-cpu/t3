// Smoke test do Trok Headmove fora do jogo (Wine 32 bits, sem janela): um "GTA de mentira" que
//  - deixa os enderecos fixos do gta_sa.exe 1.0 US dentro da propria imagem (array gigante no .bss), com a marca de
//    versao, um IKChainManager_c::LookAt de mentira em 0x618970 e as 32 correntes de IK do g_ikChainMan em 0xC15448
//    (o primeiro pedido para um personagem cria a corrente com o giro pedido; os seguintes so trocam o alvo, como o
//    UpdateLookAtInfo do GTA). A cada quadro a cabeca de cada personagem anda para o alvo na fracao do giro da corrente;
//  - carrega o samp.dll falso (make_fake_samp.py) e poe nele, pelos enderecos do R1, um CLocalPlayer::ProcessHead e um
//    CRemotePlayer::ProcessHead que fazem o que o SA-MP faz: guardam a direcao da camera (ou do aim sync) uma vez por
//    segundo e pedem "FollowPedSA" ao LookAt a cada 200 ms, com giro 0.1 e transicao de 500 ms;
//  - gira a camera (e a mira dos outros) 90 graus de uma vez e mede quanto a cabeca demora para chegar.
//
// Uso: fake_gta.exe <asi|-> <cenario>     ("-": sem o .asi, o SA-MP puro)
//   sa-mp            sem o .asi: a referencia (a cabeca leva de ~0,4 a ~1,7 s)
//   zero             sem Trok Headmove.ini: o mod cria o arquivo com os padroes (atraso 0, giro 1.0); a cabeca chega
//                    em ate 2 quadros e o pedido sai todo quadro; as outras correntes de IK ficam intactas
//   atraso-300       atraso_ms=300: a cabeca chega em ~300 ms
//   outros           os outros jogadores com atraso_outros_ms=100; um deles troca de personagem no meio (saiu e voltou)
//   outros-desligado outros_jogadores=0: o seu rapido, os outros como o SA-MP
//   recarregar       com o jogo rodando, o .ini muda para ligado=0: o chat avisa e tudo volta ao jeito do SA-MP
//   headmove-off     o /headmove do SA-MP e desligado no meio: o mod para de pedir junto
//   outra-versao     samp.dll com o ponto de entrada do R3: o .asi nao instala nada
//   gta-errado       sem a marca do 1.0 US: o .asi nao instala nada

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// A imagem vai de 0x400000 ate ~0xD6x000 e cobre 0x618970..0xC15F60. Tem que ser o comeco do .bss.
unsigned char g_gtaMemory[0x960000];

template <typename T> T& Mem(DWORD address) {
    return *reinterpret_cast<T*>(address);
}

struct Vec3 {
    float x, y, z;
};

constexpr DWORD GTA_VERSION_MARK = 0x82457C, GTA_IK_LOOK_AT = 0x618970, GTA_IK_MANAGER = 0xC15448;
constexpr DWORD IK_CHAIN_SIZE = 0x58, IKC_PED = 0x08, IKC_SPEED = 0x44, IKC_SLOT = 0x55;
constexpr DWORD FN_LOCAL_HEAD = 0x2F80, FN_REMOTE_HEAD = 0x10EA0, FN_GET_AIM = 0x98390, INTERNAL_AIM = 0xF1590;
constexpr DWORD SAMP_CHAT = 0x21A0E4, FN_ADD_ENTRY = 0x64010;
constexpr int REMOTES = 3;

typedef void(__thiscall* LookAtFn)(void*, const char*, void*, void*, int, int, Vec3*, bool, float, int, int, bool);
typedef void(__thiscall* HeadFn)(void*);

struct Aim {
    Vec3 front, source, sourceBefore, up;
};

// Um personagem: o CPed do GTA (+0x14 CMatrix*), o CPed do SA-MP (+0x2A4 CPed do GTA, +0x2B0 numero) e o
// CLocalPlayer / CRemotePlayer (+0 CPed do SA-MP).
struct Sim {
    alignas(16) BYTE ped[0x800];
    alignas(16) BYTE otherPed[0x800]; // o personagem novo quando ele sai e volta
    alignas(16) float matrix[24];
    alignas(16) BYTE sampPed[0x300];
    alignas(16) BYTE player[0x200];
    Aim aim;
    bool remote = false;
    // o SA-MP de mentira
    DWORD dirAt = 0, lookAt = 0;
    bool started = false;
    Vec3 rel = {0, 20, 0};
    // a cabeca (a IK de mentira)
    int chain = -1;
    Vec3 target = {0, 0, 0};
    Vec3 head = {0, 1, 0};
    // o que chegou ao LookAt
    int calls = 0;
    float speed = 0;
    int blend = 0, time = 0;
};

Sim g_local;
Sim g_remotes[REMOTES];
Aim g_cameraAim;
bool g_headMove = true;
int g_chains = 0;
int g_failures = 0;
DWORD g_samp = 0;
char g_chatLast[256] = "";
int g_chatCount = 0;
BYTE g_chatObject[0x100];

void Check(bool ok, const char* what) {
    printf("%s: %s\n", ok ? "ok" : "FALHA", what);
    fflush(stdout);
    if (!ok) {
        ++g_failures;
    }
}

void CheckN(bool ok, const char* what, double value) {
    char text[200];
    snprintf(text, sizeof(text), "%s (%.0f)", what, value);
    Check(ok, text);
}

Vec3 Norm(Vec3 v) {
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return l > 1e-6f ? Vec3{v.x / l, v.y / l, v.z / l} : Vec3{0, 1, 0};
}

float Angle(Vec3 a, Vec3 b) {
    a = Norm(a);
    b = Norm(b);
    float d = a.x * b.x + a.y * b.y + a.z * b.z;
    d = d > 1 ? 1 : d < -1 ? -1 : d;
    return std::acos(d) * 57.29578f;
}

Vec3 Heading(float degrees) {
    float r = degrees / 57.29578f;
    return {std::sin(r), std::cos(r), 0.0f};
}

BYTE* GamePed(Sim& s) {
    return *reinterpret_cast<BYTE**>(s.sampPed + 0x2A4);
}

Vec3 PosOf(BYTE* ped) {
    float* m = *reinterpret_cast<float**>(ped + 0x14);
    return {m[12], m[13], m[14]};
}

Sim* SimOf(void* ped) {
    if (GamePed(g_local) == ped) {
        return &g_local;
    }
    for (Sim& s : g_remotes) {
        if (GamePed(s) == ped) {
            return &s;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------- GTA de mentira

void __fastcall FakeLookAt(void*, void*, const char* purpose, void* ped, void*, int time, int, Vec3* offset, bool,
                           float speed, int blend, int, bool) {
    Sim* s = SimOf(ped);
    if (!s || !purpose || strcmp(purpose, "FollowPedSA") != 0 || !offset) {
        return;
    }
    ++s->calls;
    s->speed = speed;
    s->blend = blend;
    s->time = time;
    s->target = *offset;
    if (s->chain < 0) { // a corrente nasce com o giro do primeiro pedido
        s->chain = g_chains++;
        DWORD chain = GTA_IK_MANAGER + s->chain * IK_CHAIN_SIZE;
        Mem<void*>(chain + IKC_PED) = ped;
        Mem<BYTE>(chain + IKC_SLOT) = 0;
        Mem<float>(chain + IKC_SPEED) = speed;
    }
}

float ChainSpeed(const Sim& s) {
    return s.chain < 0 ? 0.0f : Mem<float>(GTA_IK_MANAGER + s.chain * IK_CHAIN_SIZE + IKC_SPEED);
}

// O quadro do GTA: a cabeca anda para o alvo na fracao do giro da corrente.
void StepHeads() {
    Sim* all[] = {&g_local, &g_remotes[0], &g_remotes[1], &g_remotes[2]};
    for (Sim* s : all) {
        if (s->chain < 0) {
            continue;
        }
        float k = ChainSpeed(*s);
        k = k > 1 ? 1 : k;
        Vec3 p = PosOf(GamePed(*s));
        Vec3 want = Norm({s->target.x - p.x, s->target.y - p.y, s->target.z - p.z});
        s->head = Norm({s->head.x + (want.x - s->head.x) * k, s->head.y + (want.y - s->head.y) * k,
                        s->head.z + (want.z - s->head.z) * k});
    }
}

// ---------------------------------------------------------------- SA-MP de mentira (a logica do ProcessHead)

void SampHead(Sim& s, const Aim* aim, int time) {
    if (!g_headMove) {
        return;
    }
    BYTE* ped = GamePed(s);
    Vec3 pos = PosOf(ped);
    DWORD tick = GetTickCount();
    if (!s.started || tick - s.dirAt > 1000) {
        s.rel = {aim->front.x * 20.0f, aim->front.y * 20.0f, aim->front.z * 20.0f};
        s.dirAt = tick;
    }
    if (!s.started || tick - s.lookAt > 200) {
        Vec3 target = {pos.x + s.rel.x, pos.y + s.rel.y, pos.z + s.rel.z};
        reinterpret_cast<LookAtFn>(GTA_IK_LOOK_AT)(reinterpret_cast<void*>(GTA_IK_MANAGER), "FollowPedSA", ped, nullptr,
                                                   time, -1, &target, false, 0.1f, 500, 3, false);
        s.lookAt = tick;
    }
    s.started = true;
}

void __fastcall FakeLocalHead(BYTE* self, void*) {
    BYTE* sampPed = *reinterpret_cast<BYTE**>(self);
    Sim* s = SimOf(*reinterpret_cast<BYTE**>(sampPed + 0x2A4));
    const Aim* aim = *reinterpret_cast<Aim**>(g_samp + INTERNAL_AIM);
    if (s) {
        SampHead(*s, aim, 2000);
    }
}

Aim* __stdcall FakeGetAim(int number);

void __fastcall FakeRemoteHead(BYTE* self, void*) {
    BYTE* sampPed = *reinterpret_cast<BYTE**>(self);
    Sim* s = SimOf(*reinterpret_cast<BYTE**>(sampPed + 0x2A4));
    if (s) {
        SampHead(*s, FakeGetAim(sampPed[0x2B0]), 1000);
    }
}

Aim* __stdcall FakeGetAim(int number) {
    for (Sim& s : g_remotes) {
        if (s.sampPed[0x2B0] == number) {
            return &s.aim;
        }
    }
    static Aim zero = {};
    return &zero;
}

void __fastcall FakeAddEntry(void*, void*, int, const char* text, const char*, DWORD, DWORD) {
    snprintf(g_chatLast, sizeof(g_chatLast), "%s", text ? text : "");
    ++g_chatCount;
}

bool Redirect(DWORD at, void* to) {
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(at & ~0xFFFu), 0x2000, PAGE_EXECUTE_READWRITE, &old)) {
        return false;
    }
    Mem<BYTE>(at) = 0xE9;
    Mem<DWORD>(at + 1) = reinterpret_cast<DWORD>(to) - (at + 5);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(at), 5);
    return true;
}

void SetupSim(Sim& s, float x, float y, int number, bool remote) {
    memset(s.matrix, 0, sizeof(s.matrix));
    s.matrix[0] = s.matrix[5] = s.matrix[10] = s.matrix[15] = 1.0f;
    s.matrix[12] = x;
    s.matrix[13] = y;
    *reinterpret_cast<float**>(s.ped + 0x14) = s.matrix;
    *reinterpret_cast<float**>(s.otherPed + 0x14) = s.matrix;
    *reinterpret_cast<BYTE**>(s.sampPed + 0x2A4) = s.ped;
    s.sampPed[0x2B0] = static_cast<BYTE>(number);
    *reinterpret_cast<BYTE**>(s.player) = s.sampPed;
    s.aim.front = {0, 1, 0};
    s.remote = remote;
}

void SetHeading(float degrees) {
    g_cameraAim.front = Heading(degrees);
    for (Sim& s : g_remotes) {
        s.aim.front = Heading(degrees);
    }
}

// Um quadro do jogo: o SA-MP processa as cabecas (pelos enderecos, onde o .asi poe os ganchos) e o GTA anda a IK.
void Frame() {
    reinterpret_cast<HeadFn>(g_samp + FN_LOCAL_HEAD)(g_local.player);
    for (Sim& s : g_remotes) {
        reinterpret_cast<HeadFn>(g_samp + FN_REMOTE_HEAD)(s.player);
    }
    StepHeads();
}

double Seconds() {
    static LARGE_INTEGER f = {}, t0 = {};
    LARGE_INTEGER t;
    if (!f.QuadPart) {
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&t0);
    }
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart - t0.QuadPart) / f.QuadPart;
}

// Roda quadros de ~16 ms ate "until" (s). Em cada um, fn(agora).
template <class F> void RunUntil(double until, F fn) {
    while (Seconds() < until) {
        fn(Seconds());
        Frame();
        Sleep(15);
    }
}

// Gira a camera (e a mira dos outros) e devolve, em ms, quanto cada cabeca levou para ficar a menos de 5 graus.
struct Response {
    double local = 1e9, remote[REMOTES] = {1e9, 1e9, 1e9};
};

Response Turn(float degrees, double window) {
    Response r;
    double t0 = Seconds();
    SetHeading(degrees);
    Vec3 want = Heading(degrees);
    RunUntil(t0 + window, [&](double now) {
        double ms = (now - t0) * 1000.0;
        if (r.local > 1e8 && Angle(g_local.head, want) < 5.0f) {
            r.local = ms;
        }
        for (int i = 0; i < REMOTES; ++i) {
            if (r.remote[i] > 1e8 && Angle(g_remotes[i].head, want) < 5.0f) {
                r.remote[i] = ms;
            }
        }
    });
    // o ultimo quadro tambem conta
    if (r.local > 1e8 && Angle(g_local.head, want) < 5.0f) {
        r.local = window * 1000.0;
    }
    return r;
}

// Pedidos por segundo que chegaram ao LookAt num trecho.
double Rate(Sim& s, double secs) {
    int before = s.calls;
    double t0 = Seconds();
    RunUntil(t0 + secs, [](double) {});
    return (s.calls - before) / (Seconds() - t0);
}

int LogCount(const char* text) {
    FILE* f = fopen("Trok Headmove.log", "r");
    if (!f) {
        return 0;
    }
    char line[512];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, text)) {
            ++n;
        }
    }
    fclose(f);
    return n;
}

bool LogHas(const char* text) {
    return LogCount(text) > 0;
}

bool FileHas(const char* path, const char* text) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return false;
    }
    char line[512];
    bool found = false;
    while (!found && fgets(line, sizeof(line), f)) {
        found = strstr(line, text) != nullptr;
    }
    fclose(f);
    return found;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("uso: fake_gta.exe <asi|-> <cenario>\n");
        return 2;
    }
    const char* asiPath = argv[1];
    const char* scenario = argv[2];
    bool withAsi = strcmp(asiPath, "-") != 0;
    bool wrongGta = strcmp(scenario, "gta-errado") == 0;
    bool otherSamp = strcmp(scenario, "outra-versao") == 0;
    bool inactive = !withAsi || wrongGta || otherSamp;

    DWORD lo = reinterpret_cast<DWORD>(g_gtaMemory);
    DWORD hi = lo + sizeof(g_gtaMemory);
    if (lo > GTA_IK_LOOK_AT || hi < GTA_IK_MANAGER + 32 * IK_CHAIN_SIZE) {
        printf("FALHA: memoria falsa do GTA fora do lugar (0x%08lX..0x%08lX)\n", lo, hi);
        return 2;
    }
    if (!wrongGta) {
        Mem<DWORD>(GTA_VERSION_MARK) = 0x94BF;
    }
    Check(Redirect(GTA_IK_LOOK_AT, reinterpret_cast<void*>(FakeLookAt)), "LookAt de mentira no lugar");
    // Outras correntes que o mod nao pode tocar: a de outro personagem e um braco do seu.
    g_chains = 2;
    Mem<DWORD>(GTA_IK_MANAGER + IKC_PED) = 0x12345678;
    Mem<BYTE>(GTA_IK_MANAGER + IKC_SLOT) = 0;
    Mem<float>(GTA_IK_MANAGER + IKC_SPEED) = 0.25f;
    Mem<void*>(GTA_IK_MANAGER + IK_CHAIN_SIZE + IKC_PED) = g_local.ped;
    Mem<BYTE>(GTA_IK_MANAGER + IK_CHAIN_SIZE + IKC_SLOT) = 1;
    Mem<float>(GTA_IK_MANAGER + IK_CHAIN_SIZE + IKC_SPEED) = 0.5f;

    SetupSim(g_local, 0, 0, 0, false);
    SetupSim(g_remotes[0], 3, 5, 5, true);
    SetupSim(g_remotes[1], -3, 5, 6, true);
    SetupSim(g_remotes[2], 0, 9, 7, true);
    g_cameraAim.front = {0, 1, 0};

    HMODULE samp = LoadLibraryA("samp.dll");
    Check(samp != nullptr, "samp.dll falso carregado");
    if (!samp) {
        return 1;
    }
    g_samp = reinterpret_cast<DWORD>(samp);
    Check(Redirect(g_samp + FN_LOCAL_HEAD, reinterpret_cast<void*>(FakeLocalHead)) &&
              Redirect(g_samp + FN_REMOTE_HEAD, reinterpret_cast<void*>(FakeRemoteHead)) &&
              Redirect(g_samp + FN_GET_AIM, reinterpret_cast<void*>(FakeGetAim)) &&
              Redirect(g_samp + FN_ADD_ENTRY, reinterpret_cast<void*>(FakeAddEntry)),
          "funcoes de mentira do SA-MP no lugar (ProcessHead, GetAim, AddEntry)");
    Mem<Aim*>(g_samp + INTERNAL_AIM) = &g_cameraAim;
    Mem<BYTE*>(g_samp + SAMP_CHAT) = g_chatObject;

    if (withAsi) {
        HMODULE asi = LoadLibraryA(asiPath);
        Check(asi != nullptr, "asi carregado");
        if (!asi) {
            return 1;
        }
        for (int i = 0; i < 100 && !LogHas("ganchos instalados") && !LogHas("desligado"); ++i) {
            Sleep(20);
        }
    }
    if (wrongGta) {
        Check(LogHas("nao e o 1.0 US"), "log: gta_sa.exe que nao e o 1.0 US recusado");
    }
    if (otherSamp) {
        Check(LogHas("nao e o 0.3.7 R1"), "log: samp.dll que nao e o R1 recusado");
    }
    if (!inactive) {
        Check(LogHas("ganchos instalados (LookAt do GTA, ProcessHead seu e dos outros)"), "log: os tres ganchos");
    }

    double start = Seconds();
    RunUntil(start + 1.2, [](double) {}); // a cabeca assenta olhando para a frente

    if (inactive || strcmp(scenario, "sa-mp") == 0) {
        Response r = Turn(90, 2.6);
        CheckN(r.local >= 400, "SA-MP puro: a sua cabeca demora (ms)", r.local);
        CheckN(r.remote[0] >= 400, "SA-MP puro: a cabeca do outro demora (ms)", r.remote[0]);
        double rate = Rate(g_local, 1.5);
        CheckN(rate > 3.5 && rate < 6.5, "SA-MP puro: ~5 pedidos por segundo (x10)", rate * 10);
        Check(std::fabs(g_local.speed - 0.1f) < 1e-4f && std::fabs(ChainSpeed(g_local) - 0.1f) < 1e-4f,
              "SA-MP puro: giro 0.1 no pedido e na corrente");
    } else if (strcmp(scenario, "zero") == 0 || strcmp(scenario, "atraso-300") == 0) {
        bool zero = strcmp(scenario, "zero") == 0;
        if (zero) {
            Check(FileHas("Trok Headmove.ini", "atraso_ms=0") && FileHas("Trok Headmove.ini", "giro=1.0"),
                  "Trok Headmove.ini criado com os padroes");
            Check(LogHas("primeiro pedido do SA-MP (seu personagem): duracao 2000 ms, giro 0.10, transicao 500 ms"),
                  "log: o pedido original do SA-MP (2000 ms, giro 0.1, 500 ms)");
        }
        Response a = Turn(90, 1.6);
        Response b = Turn(0, 1.6);
        if (zero) {
            CheckN(a.local <= 60 && b.local <= 60, "atraso 0: a sua cabeca chega em ate ~2 quadros (ms, o pior)",
                   a.local > b.local ? a.local : b.local);
        } else {
            CheckN(a.local >= 150 && a.local <= 480 && b.local >= 150 && b.local <= 480,
                   "atraso 300: a cabeca chega em ~300 ms (ms, a primeira)", a.local);
        }
        double rate = Rate(g_local, 1.5);
        CheckN(rate > 30, "um pedido por quadro, nao a cada 200 ms (por segundo)", rate);
        Check(std::fabs(g_local.speed - 1.0f) < 1e-4f && std::fabs(ChainSpeed(g_local) - 1.0f) < 1e-4f,
              "giro 1.0 no pedido e na corrente da sua cabeca");
        CheckN(g_local.blend >= 150 && g_local.blend <= 500, "transicao dentro de 150..500 ms", g_local.blend);
        Check(g_local.time == 2000, "a duracao do SA-MP (2000 ms) continua a mesma");
        Check(std::fabs(Mem<float>(GTA_IK_MANAGER + IKC_SPEED) - 0.25f) < 1e-6f &&
                  std::fabs(Mem<float>(GTA_IK_MANAGER + IK_CHAIN_SIZE + IKC_SPEED) - 0.5f) < 1e-6f,
              "as outras correntes de IK (outro personagem, o seu braco) ficaram intactas");
    } else if (strcmp(scenario, "outros") == 0) {
        Response a = Turn(90, 1.6);
        double worst = 0;
        for (double v : a.remote) {
            worst = v > worst ? v : worst;
        }
        CheckN(a.local <= 60, "a sua cabeca na hora (ms)", a.local);
        CheckN(worst >= 30 && worst <= 300, "a dos outros com atraso 100 (ms, o pior)", worst);
        Check(std::fabs(ChainSpeed(g_remotes[0]) - 1.0f) < 1e-4f, "giro 1.0 na corrente dos outros");
        Check(g_remotes[0].time == 1000, "a duracao do SA-MP para os outros (1000 ms) continua a mesma");
        // O jogador 7 sai e volta: personagem novo no mesmo numero. O mod recomeca do zero para ele.
        Sim& back = g_remotes[2];
        *reinterpret_cast<BYTE**>(back.sampPed + 0x2A4) = back.otherPed;
        back.chain = -1;
        back.head = Heading(-90);
        back.started = false;
        Response b = Turn(0, 1.6);
        CheckN(b.remote[2] <= 300, "quem saiu e voltou tambem segue a mira (ms)", b.remote[2]);
    } else if (strcmp(scenario, "outros-desligado") == 0) {
        Response a = Turn(90, 2.6);
        CheckN(a.local <= 60, "a sua cabeca na hora (ms)", a.local);
        CheckN(a.remote[0] >= 400, "a dos outros como o SA-MP (ms)", a.remote[0]);
        Check(std::fabs(g_remotes[0].speed - 0.1f) < 1e-4f && std::fabs(ChainSpeed(g_remotes[0]) - 0.1f) < 1e-4f,
              "os outros com o giro 0.1 do SA-MP");
    } else if (strcmp(scenario, "recarregar") == 0) {
        Response a = Turn(90, 1.0);
        CheckN(a.local <= 60, "antes: a sua cabeca na hora (ms)", a.local);
        FILE* f = fopen("Trok Headmove.ini", "w");
        if (f) {
            fputs("[headmove]\r\nligado=0\r\n", f);
            fclose(f);
        }
        RunUntil(Seconds() + 1.6, [](double) {});
        Check(g_chatCount == 1 && strstr(g_chatLast, "[Trok Headmove]") && strstr(g_chatLast, "desligado"),
              "o chat avisa que o .ini mudou (desligado)");
        Check(LogHas("Trok Headmove.ini mudou: desligado"), "log: .ini relido");
        double rate = Rate(g_local, 1.5);
        CheckN(rate > 3.5 && rate < 6.5, "desligado: de volta aos ~5 pedidos por segundo do SA-MP (x10)", rate * 10);
        Check(std::fabs(ChainSpeed(g_local) - 0.1f) < 1e-4f && std::fabs(ChainSpeed(g_remotes[0]) - 0.1f) < 1e-4f,
              "desligado: as correntes voltam ao giro 0.1 do SA-MP");
        Response b = Turn(0, 2.6);
        CheckN(b.local >= 400, "desligado: a cabeca volta a demorar como no SA-MP (ms)", b.local);
    } else if (strcmp(scenario, "headmove-off") == 0) {
        Response a = Turn(90, 1.0);
        CheckN(a.local <= 60, "com o /headmove: a sua cabeca na hora (ms)", a.local);
        g_headMove = false;
        RunUntil(Seconds() + 0.5, [](double) {});
        int before = g_local.calls + g_remotes[0].calls;
        RunUntil(Seconds() + 1.5, [](double) {});
        CheckN(g_local.calls + g_remotes[0].calls == before, "/headmove desligado: o mod para de pedir (pedidos)",
               g_local.calls + g_remotes[0].calls - before);
    } else {
        printf("cenario desconhecido: %s\n", scenario);
        return 2;
    }
    if (withAsi && !inactive) {
        Check(!LogHas("recusado"), "log: nenhum gancho recusado");
    }

    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), g_failures ? 1 : 0);
    return 0;
}
