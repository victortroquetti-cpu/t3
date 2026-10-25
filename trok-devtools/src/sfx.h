// Trok DevTools (.asi) -- Victor_Trok
// Os efeitos sonoros do jogo (audio\SFX): os pacotes do PakFiles.dat (GENRL, FEET, PAIN_A, SCRIPT, SPC_*), os bancos
// do BankLkup.dat e os sons de cada banco (o cabecalho do banco no pacote: ate 400 sons, com o inicio, o loop e a
// frequencia de cada um). Lidos numa thread; os nomes dos veiculos entram quando o catalogo de modelos fica pronto.
// Tambem le o PCM de um som (para tocar e para salvar em .wav).
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace sfx {

enum Kind { T_ENGINE_TRAFFIC, T_ENGINE_PLAYER, T_WEAPONS, T_FEET, T_PAIN, T_SCRIPT, T_SPEECH, T_OTHER, KIND_COUNT };

struct Sound {
    int index = 0;            // no banco (0 = sound_001)
    DWORD offset = 0, size = 0; // bytes, depois do cabecalho do banco
    DWORD loopStart = 0xFFFFFFFF;
    int rate = 22050;
    float seconds = 0;
    bool loop = false;
    std::string name;   // nome do jogo, quando tem ("EAGLE_SHOT_L")
    std::string weapons; // armas que usam ("M4, AK-47")
    std::string search;  // minusculas
};

struct Bank {
    int id = 0;     // indice no BankLkup.dat (o eSoundBank do jogo)
    int pack = 0;   // indice em Packs()
    int number = 0; // 1.. dentro do pacote (Bank_001)
    int pakFile = 0;
    DWORD fileOffset = 0, bytes = 0;
    bool ok = false; // cabecalho lido
    std::string name; // "90S_P"
    int kind = T_OTHER;
    std::string text; // "motor, com voce dirigindo"
    std::vector<int> vehicles; // modelos que usam
    std::string vehicleText;   // "Bravura, Previon, Solair e mais 9"
    std::vector<Sound> sounds;
    std::string search;
};

struct Pack {
    std::string name, description;
    std::vector<int> banks; // ids
    int sounds = 0;
};

void Start(const char* gameDir); // thread
void Update();                   // thread do jogo: junta os nomes dos veiculos quando o catalogo fica pronto
bool Ready();
const char* Status();
int Failed(); // bancos com cabecalho invalido

const std::vector<Pack>& Packs();
const std::vector<Bank>& Banks();
const Bank* FindBank(int id);
const Sound* FindSound(int bank, int sound);
const char* KindName(int kind);
int BankCount();
int SoundCount();

// Filtro da aba: busca (todas as palavras), tipo, veiculo (modelo), arma (indice em audio_data::WEAPONS) e loop
// (0 todos, 1 so com loop, 2 sem loop). Devolve, por banco, os sons que passam.
struct Filter {
    std::string query;
    int kind = -1, vehicle = -1, weapon = -1, loop = 0;
    bool operator==(const Filter& o) const {
        return query == o.query && kind == o.kind && vehicle == o.vehicle && weapon == o.weapon && loop == o.loop;
    }
};
struct Result {
    std::vector<std::vector<int>> sounds; // por banco (indice = id): os sons que passam
    std::vector<int> bankCount;           // por pacote: bancos com algo
    std::vector<int> soundCount;          // por pacote: sons que passam
    int banks = 0, total = 0;
};
Result Apply(const Filter& f);

// PCM 16 bits mono do som (do arquivo do pacote).
bool ReadPcm(int bank, int sound, std::vector<short>* out, int* rate);
// Salva em <jogo>\Trok DevTools\sons\<PACOTE>_Bank_NNN_<NOME>_sound_NNN.wav; *path = o caminho salvo.
bool ExportWav(int bank, int sound, std::string* path);
// "GENRL_Bank_002_90S_P_sound_001"
std::string Label(int bank, int sound);

} // namespace sfx
