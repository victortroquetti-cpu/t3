// Trok Chat (.asi) -- Victor_Trok
// O SA-MP 0.3.7 R1 por dentro (enderecos e estruturas da SAMP-API, github.com/kin4stat/SAMP-API, licenca MIT):
//   - CChat::AddEntry (0x64010): toda mensagem do chat passa por aqui; o original guarda as 100 dele e escreve o
//     chatlog.txt, e o Trok Chat guarda uma copia para desenhar (em UTF-8, ate 300);
//   - CChat::Draw (0x64230) e CChat::Render (0x63D70): o desenho do chat do SA-MP; pulados enquanto o Trok Chat estiver
//     no lugar e desenhando (se o desenho do Trok Chat parar, o do SA-MP volta sozinho);
//   - teclado: o SA-MP decide abrir a caixa dele no fim da corrente de janelas, em InputHandler::KeyPressHandler
//     (0x5D850, o F6) e InputHandler::CharInputHandler (0x5DA80, o caractere "t"). Os dois sao desviados: e la, onde
//     um mod com campo de texto na frente ja teria ficado com a tecla, que as duas teclas escolhidas no menu abrem a
//     barra do Trok Chat. O T e o F6 do SA-MP so abrem se forem elas;
//   - CInput::Open (0x657E0) e CInput::Close (0x658E0): scripts abrindo ou fechando o chat. Abre a barra do Trok Chat
//     (so o conteudo do Trok Chat aparece), com o texto que o script tiver posto na caixa do SA-MP;
//   - CInput::Send (0x65C60): o que a caixa do SA-MP faz com o texto digitado (comando do cliente, do servidor ou
//     mensagem); a barra do Trok Chat manda por ele;
//   - CInput::m_bEnabled (+0x14E0): "chat aberto", que os scripts e os outros mods consultam; fica ligado enquanto a
//     barra do Trok Chat estiver aberta;
//   - CGame::SetCursorMode (0x9BD30): o cursor do menu do /trokchat.

#include "tc.h"
#include "chat.h"
#include "menu.h"
#include "options.h"
#include "samp.h"

#include "MinHook.h"

#include <cstring>
#include <ctime>
#include <string>

namespace samp {
namespace {

constexpr DWORD R1_ENTRY = 0x31DF13; // ponto de entrada do samp.dll 0.3.7 R1
constexpr DWORD P_CHAT = 0x21A0E4, P_INPUT = 0x21A0E8, P_DIALOG = 0x21A0B8, P_INFO = 0x21A0F8, P_GAME = 0x21A10C;
constexpr DWORD F_ADD_ENTRY = 0x64010, F_DRAW = 0x64230, F_RENDER = 0x63D70;
constexpr DWORD F_SEND = 0x65C60, F_ADD_COMMAND = 0x65AD0, F_INPUT_OPEN = 0x657E0, F_INPUT_CLOSE = 0x658E0;
constexpr DWORD F_KEY_PRESS = 0x5D850, F_CHAR_INPUT = 0x5DA80;
constexpr DWORD F_SET_CURSOR = 0x9BD30, F_UNLOCK_CAM = 0x9BC10;
// CChat
constexpr DWORD CHAT_PAGE_SIZE = 0x0, CHAT_MODE = 0x8, CHAT_TIMESTAMPS = 0xC, CHAT_ENTRIES = 0x132, CHAT_REDRAW = 0x63DA;
constexpr DWORD ENTRY_SIZE = 0xFC, ENTRY_TIME = 0x0, ENTRY_PREFIX = 0x4, ENTRY_TEXT = 0x20, ENTRY_TYPE = 0xF0;
constexpr DWORD ENTRY_TEXT_COLOR = 0xF4, ENTRY_PREFIX_COLOR = 0xF8;
constexpr int CHAT_ENTRY_COUNT = 100; // a mais nova e a ultima
// CInput e CDialog
constexpr DWORD INPUT_EDITBOX = 0x8, INPUT_ENABLED = 0x14E0;
constexpr DWORD DIALOG_ACTIVE = 0x28;
// A caixa de texto do SA-MP (CDXUTEditBox, do DXUT do DirectX SDK): o tipo do controle e o texto (CUniBuffer).
constexpr DWORD DXUT_TYPE = 0x30, DXUT_TEXT = 0x50, DXUT_TEXT_SIZE = 0x54;
constexpr int DXUT_CONTROL_EDITBOX = 6, DXUT_CONTROL_IMEEDITBOX = 7;
constexpr int MAX_SEND = 128;      // a caixa do SA-MP manda no maximo 128 bytes
constexpr size_t MAX_TEXT = 1024;  // mensagens de scripts podem passar dos 144 do servidor
constexpr int PREFILL_FRAMES = 30; // quadros depois de um script abrir o chat em que o texto dele e procurado
// gta_sa.exe
constexpr DWORD GTA_MENU_ACTIVE = 0xBA67A4;
constexpr DWORD GTA_MOUSE_STATE = 0xB73418; // CPad::NewMouseControllerState (+3 roda para cima, +4 para baixo)
// Chamadas do desenho do SA-MP sem o Present do Trok Chat no meio: passou disso, o desenho dele parou.
constexpr int MAX_DRAWS_WITHOUT_BEAT = 30;

typedef void(__thiscall* AddEntryFn)(void* chat, int type, const char* text, const char* prefix, DWORD textColor,
                                     DWORD prefixColor);
typedef void(__thiscall* DrawFn)(void* chat);
typedef void(__thiscall* SendFn)(void* input, const char* text);
typedef void(__thiscall* InputFn)(void* input);
typedef BOOL(__cdecl* KeyFn)(unsigned int key);
typedef void(__cdecl* CmdProc)(const char* params);
typedef void(__thiscall* AddCommandFn)(void* input, const char* name, CmdProc proc);
typedef void(__thiscall* SetCursorFn)(void* game, int mode, BOOL hide);
typedef void(__thiscall* UnlockCamFn)(void* game);

DWORD g_samp = 0;
volatile bool g_hookPending = false;
bool g_ready = false;
bool g_beating = false; // o Present do Trok Chat ja passou por aqui
int g_drawsWithoutBeat = 0;
bool g_menuRequest = false;
AddEntryFn g_addEntry = nullptr;
DrawFn g_draw = nullptr;
DrawFn g_render = nullptr;
InputFn g_inputOpen = nullptr;
InputFn g_inputClose = nullptr;
KeyFn g_keyPress = nullptr;
KeyFn g_charInput = nullptr;

// A ultima tecla apertada que a janela passou adiante (overlay.cpp): so uma apertada nova (nao a repeticao de quem
// segura a tecla) abre o chat.
UINT g_lastKey = 0;
bool g_lastFresh = false;
bool g_lastExtended = false;
UINT g_openedBy = 0;     // a tecla que abriu a barra (0 = um script)
int g_prefillFrames = 0;
int g_editboxState = -1; // a caixa do SA-MP: -1 nao olhada, 0 nao reconhecida, 1 texto legivel

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

bool Readable(const void* address, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!address || !VirtualQuery(address, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        return false;
    }
    return reinterpret_cast<const BYTE*>(address) + size <=
           reinterpret_cast<const BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
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

bool Taken() {
    return g_ready && opt::Get().enabled && Drawing();
}

// O chat do SA-MP so some com o Trok Chat ligado E desenhando: se o Present dele parar de passar (outro mod cortou a
// corrente, o ImGui nao subiu), o do SA-MP volta sozinho.
bool Hidden() {
    if (!Taken()) {
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
    if (!Taken()) {
        g_render(chatObject);
    }
}

// ---------------------------------------------------------------- teclas

// A tecla do Windows como o ImGui (e o menu) chama (a mesma tabela do backend Win32 do ImGui).
ImGuiKey KeyOf(UINT vk, bool extended) {
    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0'));
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return static_cast<ImGuiKey>(ImGuiKey_F1 + (vk - VK_F1));
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (vk - VK_NUMPAD0));
    }
    switch (vk) {
    case VK_TAB: return ImGuiKey_Tab;
    case VK_LEFT: return ImGuiKey_LeftArrow;
    case VK_RIGHT: return ImGuiKey_RightArrow;
    case VK_UP: return ImGuiKey_UpArrow;
    case VK_DOWN: return ImGuiKey_DownArrow;
    case VK_PRIOR: return ImGuiKey_PageUp;
    case VK_NEXT: return ImGuiKey_PageDown;
    case VK_HOME: return ImGuiKey_Home;
    case VK_END: return ImGuiKey_End;
    case VK_INSERT: return ImGuiKey_Insert;
    case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK: return ImGuiKey_Backspace;
    case VK_SPACE: return ImGuiKey_Space;
    case VK_RETURN: return extended ? ImGuiKey_KeypadEnter : ImGuiKey_Enter;
    case VK_OEM_7: return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA: return ImGuiKey_Comma;
    case VK_OEM_MINUS: return ImGuiKey_Minus;
    case VK_OEM_PERIOD: return ImGuiKey_Period;
    case VK_OEM_2: return ImGuiKey_Slash;
    case VK_OEM_1: return ImGuiKey_Semicolon;
    case VK_OEM_PLUS: return ImGuiKey_Equal;
    case VK_OEM_4: return ImGuiKey_LeftBracket;
    case VK_OEM_5: return ImGuiKey_Backslash;
    case VK_OEM_6: return ImGuiKey_RightBracket;
    case VK_OEM_3: return ImGuiKey_GraveAccent;
    case VK_CAPITAL: return ImGuiKey_CapsLock;
    case VK_SCROLL: return ImGuiKey_ScrollLock;
    case VK_NUMLOCK: return ImGuiKey_NumLock;
    case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
    case VK_PAUSE: return ImGuiKey_Pause;
    case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
    case VK_DIVIDE: return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case VK_ADD: return ImGuiKey_KeypadAdd;
    case VK_APPS: return ImGuiKey_Menu;
    default: return ImGuiKey_None;
    }
}

// A tecla gera um caractere visivel (letra, numero, espaco, pontuacao)? Essas abrem o chat com o caractere, como o "t"
// do SA-MP; as outras (Enter, F1-F12...) com a tecla.
bool Printable(UINT vk) {
    return (MapVirtualKeyA(vk, MAPVK_VK_TO_CHAR) & 0x7FFFFFFF) >= 0x20;
}

bool CanOpen() {
    return Taken() && !chat::IsOpen() && !menu::IsOpen() && !*reinterpret_cast<BYTE*>(GTA_MENU_ACTIVE) &&
           !host::DialogOpen();
}

void OpenBy(UINT vk) {
    g_openedBy = vk;
    g_lastFresh = false;
    g_prefillFrames = 0;
    chat::Open();
}

BOOL __cdecl KeyPressHook(unsigned int vk) {
    if (!CanOpen()) {
        return g_keyPress(vk);
    }
    const bool last = vk == g_lastKey;
    if (opt::IsOpenKey(KeyOf(vk, last && g_lastExtended))) {
        if (last && g_lastFresh && !Printable(vk)) {
            OpenBy(vk);
        }
        // A com caractere abre com ele (CharInputHook). E segurando a tecla do chat nada acontece, nem no jogo (o Enter
        // segurado depois de mandar a mensagem nao entra no carro): so a proxima apertada abre de novo.
        return TRUE;
    }
    if (vk == VK_F6) {
        return TRUE; // o F6 do SA-MP so abre o chat se for uma das teclas escolhidas
    }
    return g_keyPress(vk);
}

BOOL __cdecl CharInputHook(unsigned int ch) {
    if (!CanOpen()) {
        return g_charInput(ch);
    }
    if (ch >= 0x20 && g_lastFresh && Printable(g_lastKey) && opt::IsOpenKey(KeyOf(g_lastKey, g_lastExtended))) {
        OpenBy(g_lastKey); // o caractere fica com o SA-MP (nao cai na barra), como o "t" dele
        return TRUE;
    }
    if (ch == 't' || ch == 'T') {
        return TRUE; // o "t" do SA-MP, idem
    }
    return g_charInput(ch);
}

// ---------------------------------------------------------------- scripts abrindo e fechando o chat

void __thiscall InputOpenHook(void* input) {
    if (!Taken()) {
        g_inputOpen(input);
        return;
    }
    // Tudo que abre o chat do SA-MP abre a barra do Trok Chat. Um script pode por um texto na caixa do SA-MP (antes ou
    // logo depois de abrir): ele passa para a barra nos proximos quadros.
    if (!chat::IsOpen()) {
        g_openedBy = 0;
        chat::Open();
    }
    g_prefillFrames = PREFILL_FRAMES;
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

// A caixa de texto do SA-MP, se for a do DXUT esperada (tipo do controle 6 ou 7 e o texto num buffer legivel).
BYTE* Editbox() {
    void* input = Object(P_INPUT);
    BYTE* edit = input ? Field<BYTE*>(input, INPUT_EDITBOX) : nullptr;
    if (!Readable(edit, DXUT_TEXT_SIZE + 4)) {
        return nullptr;
    }
    const int type = Field<int>(edit, DXUT_TYPE);
    if (type != DXUT_CONTROL_EDITBOX && type != DXUT_CONTROL_IMEEDITBOX) {
        return nullptr;
    }
    const wchar_t* text = Field<const wchar_t*>(edit, DXUT_TEXT);
    const int size = Field<int>(edit, DXUT_TEXT_SIZE);
    return size >= 1 && size <= 4096 && Readable(text, size * sizeof(wchar_t)) ? edit : nullptr;
}

// O texto que um script pos na caixa do SA-MP (UTF-8). A caixa fica vazia depois: o proximo script que abrir o chat sem
// texto nao herda este.
std::string TakeNativeText() {
    BYTE* edit = Editbox();
    if (!edit) {
        return std::string();
    }
    wchar_t* text = Field<wchar_t*>(edit, DXUT_TEXT);
    const int size = Field<int>(edit, DXUT_TEXT_SIZE);
    std::wstring wide;
    for (int i = 0; i < size && text[i]; i++) {
        wide += text[i] < 0x20 ? L' ' : text[i];
    }
    if (wide.empty()) {
        return std::string();
    }
    text[0] = 0;
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return std::string();
    }
    std::string out(bytes, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], bytes, nullptr, nullptr);
    out.resize(bytes - 1);
    return out;
}

void __cdecl CmdTrokChat(const char*) {
    g_menuRequest = true;
    Log("/trokchat: menu");
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

void InstallHooks() {
    void* chatObject = Object(P_CHAT);
    void* input = Object(P_INPUT);
    MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        Log("MinHook nao iniciou (%d) -- o Trok Chat fica desligado", init);
        return;
    }
    const struct {
        DWORD rva;
        void* hook;
        void** original;
        const char* name;
    } hooks[] = {
        {F_ADD_ENTRY, reinterpret_cast<void*>(AddEntryHook), reinterpret_cast<void**>(&g_addEntry), "AddEntry"},
        {F_DRAW, reinterpret_cast<void*>(DrawHook), reinterpret_cast<void**>(&g_draw), "Draw"},
        {F_RENDER, reinterpret_cast<void*>(RenderHook), reinterpret_cast<void**>(&g_render), "Render"},
        {F_INPUT_OPEN, reinterpret_cast<void*>(InputOpenHook), reinterpret_cast<void**>(&g_inputOpen), "Open"},
        {F_INPUT_CLOSE, reinterpret_cast<void*>(InputCloseHook), reinterpret_cast<void**>(&g_inputClose), "Close"},
        {F_KEY_PRESS, reinterpret_cast<void*>(KeyPressHook), reinterpret_cast<void**>(&g_keyPress), "KeyPress"},
        {F_CHAR_INPUT, reinterpret_cast<void*>(CharInputHook), reinterpret_cast<void**>(&g_charInput), "CharInput"},
    };
    bool ok = true;
    for (const auto& h : hooks) {
        const MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(g_samp + h.rva), h.hook, h.original);
        if (status != MH_OK) {
            Log("nao deu para desviar o %s do SA-MP (MinHook %d)", h.name, status);
            ok = false;
        }
    }
    for (const auto& h : hooks) {
        ok = ok && MH_EnableHook(reinterpret_cast<void*>(g_samp + h.rva)) == MH_OK;
    }
    if (!ok) {
        Log("o Trok Chat fica desligado (o chat do SA-MP continua)");
        for (const auto& h : hooks) {
            MH_DisableHook(reinterpret_cast<void*>(g_samp + h.rva));
            MH_RemoveHook(reinterpret_cast<void*>(g_samp + h.rva));
        }
        return;
    }
    // Na thread do jogo: nenhuma mensagem entra entre os desvios e a copia das que ja estavam la.
    Import(chatObject);
    reinterpret_cast<AddCommandFn>(g_samp + F_ADD_COMMAND)(input, "trokchat", CmdTrokChat);
    g_ready = true;
    BYTE* edit = Editbox();
    g_editboxState = edit ? 1 : 0;
    if (edit) {
        Log("caixa de texto do SA-MP reconhecida (tipo %d): o texto que scripts poem nela passa para a barra",
            Field<int>(edit, DXUT_TYPE));
    } else {
        Log("caixa de texto do SA-MP nao reconhecida: texto pronto de scripts fica de fora (o resto funciona)");
    }
    Log("ligado: o chat do SA-MP agora e desenhado pelo Trok Chat (/trokchat abre o menu)");
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

void Frame() {
    if (g_hookPending) {
        g_hookPending = false;
        InstallHooks();
    }
    if (g_prefillFrames > 0) {
        --g_prefillFrames;
        if (!chat::IsOpen() || !chat::InputEmpty()) {
            g_prefillFrames = 0; // fechou ou ja digitaram: o texto do script nao entra mais
        } else if (g_editboxState == 1) {
            const std::string text = TakeNativeText();
            if (!text.empty()) {
                chat::SetInput(text.c_str());
                g_prefillFrames = 0;
            }
        }
    }
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

void KeyDown(WPARAM vk, LPARAM lParam) {
    g_lastKey = static_cast<UINT>(vk);
    g_lastFresh = !(lParam & (1 << 30));
    g_lastExtended = (HIWORD(lParam) & KF_EXTENDED) != 0;
}

void CharDone() {
    g_lastFresh = false; // o caractere da tecla ja passou: so a proxima apertada abre o chat
}

UINT OpenedBy() {
    return g_openedBy;
}

bool IsChatKey(WPARAM vk, LPARAM lParam) {
    return opt::IsOpenKey(KeyOf(static_cast<UINT>(vk), (HIWORD(lParam) & KF_EXTENDED) != 0));
}

bool TakeMenuRequest() {
    const bool request = g_menuRequest;
    g_menuRequest = false;
    return request;
}

void Cursor(bool on) {
    void* game = Object(P_GAME);
    if (!game) {
        return;
    }
    // Modo 2 = cursor visivel com camera e controles travados (o mesmo dos menus da casa).
    reinterpret_cast<SetCursorFn>(g_samp + F_SET_CURSOR)(game, on ? 2 : 0, on ? FALSE : TRUE);
    if (!on) {
        reinterpret_cast<UnlockCamFn>(g_samp + F_UNLOCK_CAM)(game);
    }
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

void Apply(const ChatSettings& s) {
    void* chatObject = samp::Object(samp::P_CHAT);
    if (!chatObject) {
        return;
    }
    const int lines = s.pageSize < 5 ? 5 : s.pageSize > 20 ? 20 : s.pageSize;
    samp::Field<unsigned int>(chatObject, samp::CHAT_PAGE_SIZE) = static_cast<unsigned int>(lines);
    samp::Field<int>(chatObject, samp::CHAT_MODE) = s.mode < 0 ? 0 : s.mode > 2 ? 2 : s.mode;
    samp::Field<bool>(chatObject, samp::CHAT_TIMESTAMPS) = s.timestamps;
    samp::Field<BOOL>(chatObject, samp::CHAT_REDRAW) = TRUE; // o do SA-MP (com o Trok Chat desligado) se redesenha
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

int Wheel() {
    const BYTE* mouse = reinterpret_cast<const BYTE*>(samp::GTA_MOUSE_STATE);
    return (mouse[3] ? 1 : 0) - (mouse[4] ? 1 : 0);
}

} // namespace host
