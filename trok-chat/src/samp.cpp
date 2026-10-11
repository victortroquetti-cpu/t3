// Trok Chat (.asi) -- Victor_Trok
// O SA-MP 0.3.7 R1 por dentro (enderecos e estruturas da SAMP-API, github.com/kin4stat/SAMP-API, licenca MIT):
//   - CChat::AddEntry (0x64010): toda mensagem do chat passa por aqui; o original guarda as 100 dele e escreve o
//     chatlog.txt, e o Trok Chat guarda uma copia para desenhar (em UTF-8, ate 300);
//   - CChat::Draw (0x64230) e CChat::Render (0x63D70): o desenho do chat do SA-MP; pulados enquanto o Trok Chat estiver
//     no lugar e desenhando (se o desenho do Trok Chat parar, o do SA-MP volta sozinho);
//   - CInput::Send (0x65C60): o que a caixa do SA-MP faz com o texto digitado (comando do cliente, do servidor ou
//     mensagem); a barra do Trok Chat manda por ele;
//   - CInput::Open (0x657E0) e CInput::Close (0x658E0): o SA-MP abre a caixa dele quando o T ou o F6 chegam ate ele
//     (um mod com um campo de texto na frente fica com a tecla, como sempre). Aberta pelo T ou F6, abre a barra do
//     Trok Chat no lugar; aberta por um script (com texto pronto, por exemplo), abre a caixa do SA-MP mesmo;
//   - CInput::m_bEnabled (+0x14E0): "chat aberto", que os scripts e os outros mods consultam; fica ligado enquanto a
//     barra do Trok Chat estiver aberta.

#include "tc.h"
#include "chat.h"
#include "samp.h"

#include "MinHook.h"

#include <cstring>
#include <ctime>
#include <string>

namespace samp {
namespace {

constexpr DWORD R1_ENTRY = 0x31DF13; // ponto de entrada do samp.dll 0.3.7 R1
constexpr DWORD P_CHAT = 0x21A0E4, P_INPUT = 0x21A0E8, P_DIALOG = 0x21A0B8, P_INFO = 0x21A0F8;
constexpr DWORD F_ADD_ENTRY = 0x64010, F_DRAW = 0x64230, F_RENDER = 0x63D70;
constexpr DWORD F_SEND = 0x65C60, F_ADD_COMMAND = 0x65AD0, F_INPUT_OPEN = 0x657E0, F_INPUT_CLOSE = 0x658E0;
// CChat
constexpr DWORD CHAT_PAGE_SIZE = 0x0, CHAT_MODE = 0x8, CHAT_TIMESTAMPS = 0xC, CHAT_ENTRIES = 0x132;
constexpr DWORD ENTRY_SIZE = 0xFC, ENTRY_TIME = 0x0, ENTRY_PREFIX = 0x4, ENTRY_TEXT = 0x20, ENTRY_TYPE = 0xF0;
constexpr DWORD ENTRY_TEXT_COLOR = 0xF4, ENTRY_PREFIX_COLOR = 0xF8;
constexpr int CHAT_ENTRY_COUNT = 100; // a mais nova e a ultima
// CInput e CDialog
constexpr DWORD INPUT_ENABLED = 0x14E0;
constexpr DWORD DIALOG_ACTIVE = 0x28;
constexpr int MAX_SEND = 128; // a caixa do SA-MP manda no maximo 128 bytes
constexpr size_t MAX_TEXT = 1024; // mensagens de scripts podem passar dos 144 do servidor
// gta_sa.exe
constexpr DWORD GTA_MOUSE_STATE = 0xB73418; // CPad::NewMouseControllerState (+3 roda para cima, +4 para baixo)
// Chamadas do desenho do SA-MP sem o Present do Trok Chat no meio: passou disso, o desenho dele parou.
constexpr int MAX_DRAWS_WITHOUT_BEAT = 30;

typedef void(__thiscall* AddEntryFn)(void* chat, int type, const char* text, const char* prefix, DWORD textColor,
                                     DWORD prefixColor);
typedef void(__thiscall* DrawFn)(void* chat);
typedef void(__thiscall* SendFn)(void* input, const char* text);
typedef void(__thiscall* InputFn)(void* input);
typedef void(__cdecl* CmdProc)(const char* params);
typedef void(__thiscall* AddCommandFn)(void* input, const char* name, CmdProc proc);

DWORD g_samp = 0;
volatile bool g_hookPending = false;
bool g_ready = false;
bool g_active = true;
bool g_chatKey = false;  // a janela esta passando o T ou o F6 adiante (overlay.cpp)
bool g_nativeLogged = false;
bool g_beating = false;  // o Present do Trok Chat ja passou por aqui
int g_drawsWithoutBeat = 0;
AddEntryFn g_addEntry = nullptr;
DrawFn g_draw = nullptr;
DrawFn g_render = nullptr;
InputFn g_inputOpen = nullptr;
InputFn g_inputClose = nullptr;

template <class T>
T& Field(void* base, DWORD offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<BYTE*>(base) + offset);
}

void* Object(DWORD pointer) {
    return g_samp ? *reinterpret_cast<void**>(g_samp + pointer) : nullptr;
}

// O SA-MP guarda o texto na pagina de codigo do Windows (cp1252 no Brasil); o ImGui usa UTF-8.
std::string Convert(const char* text, UINT from, UINT to) {
    if (!text || !*text) {
        return std::string();
    }
    const int wide = MultiByteToWideChar(from, 0, text, -1, nullptr, 0);
    if (wide <= 0) {
        return std::string();
    }
    std::wstring w(wide, L'\0');
    MultiByteToWideChar(from, 0, text, -1, &w[0], wide);
    const int bytes = WideCharToMultiByte(to, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return std::string();
    }
    std::string out(bytes, '\0');
    WideCharToMultiByte(to, 0, w.c_str(), -1, &out[0], bytes, nullptr, nullptr);
    out.resize(bytes - 1); // sem o zero final
    return out;
}

void Add(int type, const char* text, const char* prefix, DWORD textColor, DWORD prefixColor, std::time_t when) {
    if (!type || !text || !*text) {
        return;
    }
    chat::Message m;
    m.type = type;
    m.time = when;
    const size_t length = strnlen(text, MAX_TEXT);
    m.text = Convert(std::string(text, length).c_str(), CP_ACP, CP_UTF8);
    m.prefix = Convert(prefix, CP_ACP, CP_UTF8);
    m.textColor = textColor;
    m.prefixColor = prefixColor;
    chat::Add(m);
}

void __thiscall AddEntryHook(void* chatObject, int type, const char* text, const char* prefix, DWORD textColor,
                             DWORD prefixColor) {
    g_addEntry(chatObject, type, text, prefix, textColor, prefixColor);
    Add(type, text, prefix, textColor, prefixColor, std::time(nullptr));
}

bool Drawing() {
    return g_beating && g_drawsWithoutBeat <= MAX_DRAWS_WITHOUT_BEAT;
}

// O chat do SA-MP so some com o Trok Chat ligado E desenhando: se o Present dele parar de passar (outro mod cortou a
// corrente, o ImGui nao subiu), o do SA-MP volta sozinho.
bool Hidden() {
    if (!g_active || !Drawing()) {
        return false;
    }
    if (++g_drawsWithoutBeat > MAX_DRAWS_WITHOUT_BEAT) {
        Log("o desenho do Trok Chat parou -- o chat do SA-MP voltou ate ele voltar");
        return false;
    }
    return true;
}

void __thiscall DrawHook(void* chatObject) {
    if (!Hidden()) {
        g_draw(chatObject);
    }
}

void __thiscall RenderHook(void* chatObject) {
    if (!(g_active && Drawing())) {
        g_render(chatObject);
    }
}

bool Taken() {
    return g_ready && g_active && Drawing();
}

void __thiscall InputOpenHook(void* input) {
    if (g_chatKey && Taken()) {
        if (!chat::IsOpen()) {
            chat::Open(); // o T ou o F6 do jogador: a barra do Trok Chat no lugar da caixa do SA-MP
        }
        return;
    }
    // Um script abrindo a caixa (com texto pronto, por exemplo): a do SA-MP mesmo.
    if (chat::IsOpen()) {
        chat::Close();
    }
    if (Taken() && !g_nativeLogged) {
        g_nativeLogged = true;
        Log("a caixa de digitacao do SA-MP foi aberta por um script ou mod (fora do T e do F6): ela fica na tela");
    }
    g_inputOpen(input);
}

void __thiscall InputCloseHook(void* input) {
    g_inputClose(input);
    if (chat::IsOpen()) {
        static int logged = 0;
        if (logged < 3) {
            ++logged;
            Log("CInput::Close com a barra aberta (script ou o proprio SA-MP): a barra fecha");
        }
        chat::Close();
    }
}

// Mensagem do proprio Trok Chat no chat (pelo AddEntry, como as do SA-MP).
void Say(const char* text) {
    void* chatObject = Object(P_CHAT);
    if (chatObject) {
        reinterpret_cast<AddEntryFn>(g_samp + F_ADD_ENTRY)(chatObject, chat::TYPE_DEBUG, text, nullptr, 0xFFFFFFFF, 0);
    }
}

void __cdecl CmdTrokChat(const char*) {
    g_active = !g_active;
    if (!g_active) {
        chat::Close();
    }
    Log("/trokchat: %s", g_active ? "Trok Chat no lugar do chat do SA-MP" : "chat do SA-MP de volta");
    Say(g_active ? "{9A9A9A}[Trok Chat]{FFFFFF} ligado. {9A9A9A}/trokchat volta o chat do SA-MP."
                 : "{9A9A9A}[Trok Chat]{FFFFFF} desligado: o chat do SA-MP voltou. {9A9A9A}/trokchat liga de novo.");
}

// As mensagens que ja estavam no chat do SA-MP quando o Trok Chat ligou.
void Import(void* chatObject) {
    int count = 0;
    for (int i = 0; i < CHAT_ENTRY_COUNT; i++) {
        BYTE* e = reinterpret_cast<BYTE*>(chatObject) + CHAT_ENTRIES + i * ENTRY_SIZE;
        char text[145], prefix[29];
        std::memcpy(text, e + ENTRY_TEXT, 144);
        text[144] = 0;
        std::memcpy(prefix, e + ENTRY_PREFIX, 28);
        prefix[28] = 0;
        const int type = Field<int>(e, ENTRY_TYPE);
        if (type && text[0]) {
            Add(type, text, prefix, Field<DWORD>(e, ENTRY_TEXT_COLOR), Field<DWORD>(e, ENTRY_PREFIX_COLOR),
                static_cast<std::time_t>(Field<int>(e, ENTRY_TIME)));
            count++;
        }
    }
    Log("%d mensagens que ja estavam no chat", count);
}

bool IsR1(DWORD base) {
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint == R1_ENTRY;
}

} // namespace

bool Start() {
    for (int i = 0; i < 300 && !g_samp; ++i) {
        g_samp = reinterpret_cast<DWORD>(GetModuleHandleA("samp.dll"));
        if (!g_samp) {
            Sleep(100);
        }
    }
    if (!g_samp) {
        Log("samp.dll nao apareceu (single player?) -- o Trok Chat fica desligado");
        return false;
    }
    if (!IsR1(g_samp)) {
        Log("samp.dll nao e o 0.3.7 R1 -- o Trok Chat fica desligado (o chat do SA-MP continua)");
        g_samp = 0;
        return false;
    }
    for (int i = 0; i < 1200; ++i) {
        if (Object(P_CHAT) && Object(P_INPUT) && Object(P_INFO)) {
            g_hookPending = true;
            Log("SA-MP 0.3.7 R1 em 0x%08X: chat 0x%08X, caixa 0x%08X", static_cast<unsigned>(g_samp),
                reinterpret_cast<DWORD>(Object(P_CHAT)), reinterpret_cast<DWORD>(Object(P_INPUT)));
            return true;
        }
        Sleep(100);
    }
    Log("o chat do SA-MP nao apareceu em 2 minutos -- o Trok Chat fica desligado");
    g_samp = 0;
    return false;
}

void FrameStart() {
    if (!g_hookPending) {
        return;
    }
    g_hookPending = false;
    void* chatObject = Object(P_CHAT);
    void* input = Object(P_INPUT);
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        Log("MinHook nao iniciou (%d) -- o Trok Chat fica desligado", init);
        return;
    }
    void* addEntry = reinterpret_cast<void*>(g_samp + F_ADD_ENTRY);
    void* draw = reinterpret_cast<void*>(g_samp + F_DRAW);
    void* render = reinterpret_cast<void*>(g_samp + F_RENDER);
    void* open = reinterpret_cast<void*>(g_samp + F_INPUT_OPEN);
    void* close = reinterpret_cast<void*>(g_samp + F_INPUT_CLOSE);
    const struct {
        void* target;
        void* hook;
        void** original;
    } hooks[] = {
        {addEntry, reinterpret_cast<void*>(AddEntryHook), reinterpret_cast<void**>(&g_addEntry)},
        {draw, reinterpret_cast<void*>(DrawHook), reinterpret_cast<void**>(&g_draw)},
        {render, reinterpret_cast<void*>(RenderHook), reinterpret_cast<void**>(&g_render)},
        {open, reinterpret_cast<void*>(InputOpenHook), reinterpret_cast<void**>(&g_inputOpen)},
        {close, reinterpret_cast<void*>(InputCloseHook), reinterpret_cast<void**>(&g_inputClose)},
    };
    MH_STATUS status[5];
    bool ok = true;
    for (int i = 0; i < 5; i++) {
        status[i] = MH_CreateHook(hooks[i].target, hooks[i].hook, hooks[i].original);
        ok = ok && status[i] == MH_OK;
    }
    for (int i = 0; ok && i < 5; i++) {
        ok = MH_EnableHook(hooks[i].target) == MH_OK;
    }
    if (!ok) {
        Log("nao deu para desviar o chat do SA-MP (AddEntry %d, Draw %d, Render %d, Open %d, Close %d) -- o Trok Chat "
            "fica desligado",
            status[0], status[1], status[2], status[3], status[4]);
        for (const auto& h : hooks) {
            MH_DisableHook(h.target);
            MH_RemoveHook(h.target);
        }
        return;
    }
    // Na thread do jogo: nenhuma mensagem entra entre os desvios e a copia das que ja estavam la.
    Import(chatObject);
    reinterpret_cast<AddCommandFn>(g_samp + F_ADD_COMMAND)(input, "trokchat", CmdTrokChat);
    g_ready = true;
    Log("ligado: o chat do SA-MP agora e desenhado pelo Trok Chat (/trokchat volta o dele)");
}

bool Ready() {
    return g_ready;
}

void Heartbeat() {
    if (g_drawsWithoutBeat > MAX_DRAWS_WITHOUT_BEAT) {
        Log("o desenho do Trok Chat voltou");
    }
    g_drawsWithoutBeat = 0;
    g_beating = true;
}

bool Active() {
    return Taken();
}

void ChatKey(bool passing) {
    g_chatKey = passing;
}

} // namespace samp

namespace host {

ChatSettings Settings() {
    ChatSettings s;
    void* chatObject = samp::Object(samp::P_CHAT);
    if (chatObject) {
        s.pageSize = static_cast<int>(samp::Field<unsigned int>(chatObject, samp::CHAT_PAGE_SIZE));
        s.mode = samp::Field<int>(chatObject, samp::CHAT_MODE);
        s.timestamps = samp::Field<bool>(chatObject, samp::CHAT_TIMESTAMPS);
    }
    return s;
}

void Send(const char* utf8) {
    void* input = samp::Object(samp::P_INPUT);
    if (!input) {
        return;
    }
    std::string text = samp::Convert(utf8, CP_UTF8, CP_ACP);
    if (text.size() > static_cast<size_t>(samp::MAX_SEND)) {
        // Corta sem partir um caractere de dois bytes (paginas de codigo asiaticas).
        size_t cut = 0;
        while (cut < text.size()) {
            const size_t next = cut + (IsDBCSLeadByte(static_cast<BYTE>(text[cut])) ? 2 : 1);
            if (next > static_cast<size_t>(samp::MAX_SEND)) {
                break;
            }
            cut = next;
        }
        text.resize(cut);
    }
    reinterpret_cast<samp::SendFn>(samp::g_samp + samp::F_SEND)(input, text.c_str());
}

int Wheel() {
    const BYTE* mouse = reinterpret_cast<const BYTE*>(samp::GTA_MOUSE_STATE);
    return (mouse[3] ? 1 : 0) - (mouse[4] ? 1 : 0);
}

void InputOpened(bool open) {
    void* input = samp::Object(samp::P_INPUT);
    if (input) {
        samp::Field<BOOL>(input, samp::INPUT_ENABLED) = open ? TRUE : FALSE;
    }
}

bool DialogOpen() {
    void* dialog = samp::Object(samp::P_DIALOG);
    return dialog && samp::Field<int>(dialog, samp::DIALOG_ACTIVE) != 0;
}

} // namespace host
