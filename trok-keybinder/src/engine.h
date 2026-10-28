#pragma once

// Trok KeyBinder -- o motor: quando cada bind dispara e a fila do que vai para o chat. Sem nada do jogo aqui
// (o main.cpp entrega o estado de cada quadro), entao o teste do Linux roda o mesmo codigo.
//
// Nada sai de uma vez: as mensagens vao numa fila, uma a cada "intervalo" (o anti-flood). Uma tecla nunca dispara
// com o chat, um dialogo, o placar ou qualquer menu aberto (World::blocked); as automaticas esperam fechar (ate
// 15 s) e somem se a condicao deixar de valer antes (entrou no carro e ja saiu, por exemplo).

#include "binds.h"

#include <deque>
#include <map>
#include <string>
#include <vector>

namespace kb {

struct Me {
    bool connected = false;
    bool spawned = false; // nasceu e esta vivo (fora da selecao de classe)
    bool dead = false;
    bool inVehicle = false;
    bool driver = false;
    unsigned vehicle = 0; // qual veiculo (a lataria so conta no mesmo)
    float health = 0, armour = 0, vehicleHealth = 0;
    int weapon = 0;
    int interior = 0;
};

struct ChatLine {
    std::string text;   // UTF-8
    std::string author; // fala de jogador: o nick de quem falou
};

struct World {
    unsigned now = 0;       // ms
    bool blocked = false;   // chat, dialogo, placar, pausa, menus: as teclas nao disparam
    bool sendReady = false; // conectado e com o chat do SA-MP pronto
    Me me;
    std::vector<Player> players; // conectados (sem o local); distance > 0 so para quem esta perto (no stream)
    bool hasTarget = false;      // Na visao: o jogador na mira
    Player target;
    std::vector<ChatLine> chat; // mensagens que chegaram desde o ultimo quadro
};

class Keys {
public:
    virtual ~Keys() = default;
    virtual bool Down(int key) const = 0;
    // Os modificadores seguros agora sao os do atalho (Ctrl+L nao dispara com Shift+Ctrl+L).
    virtual bool ModsOk(const Hotkey& h) const = 0;
};

struct Notice {
    std::string title, text;
};

class Engine {
public:
    // Troca de servidor ou de preset: esquece teclas, agendadas e a fila.
    void Reset();
    // As binds mudaram de lugar (salvar, apagar): so o estado das teclas.
    void ResetKeys();

    // Um quadro. keys = nullptr: sem teclado (jogo em segundo plano).
    void Frame(const Preset& preset, const Settings& s, const World& w, const Keys* keys);

    // Roda uma bind agora (menu circular ou tecla). target = o jogador escolhido (Na visao). false = nao rodou
    // (o motivo vai para os avisos).
    bool Run(const Bind& b, const World& w, const Player* target = nullptr);

    // A proxima mensagem, se ja deu o intervalo desde a ultima.
    bool Next(unsigned now, int flood, std::string* out);

    std::vector<Notice> TakeNotices();
    size_t Queued() const { return queue_.size(); }
    size_t Scheduled() const { return pending_.size(); }

    static constexpr size_t MAX_QUEUE = 40;
    static constexpr int MAX_AROUND = 10;    // "Todos no raio": no maximo 10 jogadores por vez
    static constexpr unsigned WAIT_LIMIT = 15000;

private:
    struct KeyState {
        bool down = false;
        unsigned since = 0;
        bool fired = false;
        unsigned lastTap = 0;
    };
    struct Job {
        unsigned due;
        int cond;
        std::vector<std::string> lines;
    };

    void Enqueue(const std::vector<std::string>& lines);
    void Note(const std::string& title, const std::string& text);
    void Schedule(const Bind& b, unsigned now, const Player* who);
    bool Edge(const Bind& b, const Me& prev, const Me& me) const;
    static bool StillValid(int cond, const Me& me);

    std::vector<KeyState> keys_;
    std::vector<Job> pending_;
    std::deque<std::string> queue_;
    std::vector<Notice> notices_;
    std::map<std::string, unsigned> chatFired_; // trecho do chat -> ultimo disparo
    Me prev_;
    bool prevValid_ = false;
    unsigned lastSend_ = 0;
    bool sentAny_ = false;
};

} // namespace kb
