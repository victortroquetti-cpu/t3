// Teste do Linux do leitor dos .ifp e da pose (ver test/anim/run.sh). Modos:
//   nomes: sem referencia (as novas pelos nomes da tabela do jogo), os quadros-chave e o filtro;
//   pasta: com a pasta Trok DevTools\originais (MOD = diferente, NOVA = nao existe la) e o .txt que ela gera;
//   txt:   so com o .txt gerado pelo modo pasta;
//   pose:  a conta da pele num esqueleto de 2 ossos.
#include "ifp.h"
#include "pose.h"

#include "../ifp_writer.h"

#include <sys/stat.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <string>

void DevToolsLog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("   log| ");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

int g_failures = 0;

void Check(bool ok, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    printf("%s: ", ok ? "ok" : "FALHA");
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
    g_failures += !ok;
}

void WriteFile(const std::string& path, const std::string& data) {
    std::ofstream(path, std::ios::binary) << data;
}

ifpw::Key K(float time, float ax, float ay, float az, float deg, float tx = 0, float ty = 0, float tz = 0) {
    ifpw::Key k = {time, {0, 0, 0, 1}, {tx, ty, tz}};
    if (deg != 0) {
        ifpw::Axis(k.q, ax, ay, az, deg);
    }
    return k;
}

ifpw::Anim Walk(bool altered) {
    ifpw::Anim a;
    a.name = "WALK_civi";
    a.seqs.push_back({"Root", 0, true, {K(0, 0, 0, 1, 0, 0, 0, 0), K(1, 0, 0, 1, 0, 0, altered ? 2.0f : 1.0f, 0)}});
    a.seqs.push_back({"L Thigh", 41, false, {K(0, 1, 0, 0, 0), K(0.5f, 1, 0, 0, 30), K(1, 1, 0, 0, 0)}});
    return a;
}

ifpw::Anim Idle() {
    ifpw::Anim a;
    a.name = "IDLE_stance";
    a.compressed = false;
    a.seqs.push_back({"Head", 5, false, {K(0, 0, 0, 1, 0), K(2.5f, 0, 0, 1, 20)}});
    return a;
}

ifpw::Anim Single(const char* name, int bone) {
    ifpw::Anim a;
    a.name = name;
    a.seqs.push_back({"Spine", bone, false, {K(0, 1, 0, 0, 0), K(1, 1, 0, 0, 90)}});
    return a;
}

void MakeGame(const std::string& dir, bool originals) {
    mkdir(dir.c_str(), 0755);
    mkdir((dir + "/anim").c_str(), 0755);
    WriteFile(dir + "/anim/ped.ifp", ifpw::Anp3("ped", {Walk(false), Idle(), Single("TROK_NOVA", 2)}));
    WriteFile(dir + "/anim/anim.img", ifpw::Img({{"bar.ifp", ifpw::Anpk("bar", {Single("BARMAN_IDLE", -1)})},
                                                 {"trokmod.ifp", ifpw::Anp3("trokmod", {Single("TROK_DANCA", 2)})},
                                                 {"dancing.ifp", ifpw::Anp3("dance", {Single("DANCE_LOOP", 2)})}}));
    mkdir((dir + "/Trok DevTools").c_str(), 0755);
    if (originals) {
        mkdir((dir + "/Trok DevTools/originais").c_str(), 0755);
        WriteFile(dir + "/Trok DevTools/originais/ped.ifp", ifpw::Anp3("ped", {Walk(true), Idle()}));
        WriteFile(dir + "/Trok DevTools/originais/anim.img", ifpw::Img({{"bar.ifp", ifpw::Anpk("bar", {Single("BARMAN_IDLE", -1)})}}));
    }
}

const ifp::Anim* Find(const char* lib, const char* name) {
    for (const ifp::Anim& a : ifp::Anims()) {
        if (ifp::FindFile(a.file)->lib == lib && a.name == name) {
            return &a;
        }
    }
    return nullptr;
}

const ifp::File* FindLib(const char* lib) {
    for (const ifp::File& f : ifp::Files()) {
        if (f.lib == lib) {
            return &f;
        }
    }
    return nullptr;
}

int TagOf(const char* lib, const char* name) {
    const ifp::Anim* a = Find(lib, name);
    return a ? a->tag : -1;
}

void Catalog(const std::string& mode, const std::string& dir) {
    ifp::Start((dir + "/").c_str());
    Check(ifp::Ready(), "leitura terminou");
    Check(ifp::Files().size() == 4, "4 .ifp (ped.ifp e 3 do anim.img): %d", static_cast<int>(ifp::Files().size()));
    Check(ifp::Anims().size() == 6, "6 animacoes: %d", static_cast<int>(ifp::Anims().size()));
    const ifp::File* ped = FindLib("PED");
    const ifp::File* bar = FindLib("BAR");
    const ifp::File* mod = FindLib("TROKMOD");
    const ifp::File* dancing = FindLib("DANCING");
    Check(ped && bar && mod && dancing, "os 4 .ifp pelo nome do arquivo");
    if (!ped || !bar || !mod || !dancing) {
        return;
    }
    Check(ped->format == "ANP3" && bar->format == "ANPK", "formatos: ped %s, bar %s", ped->format.c_str(), bar->format.c_str());
    Check(ped->known && bar->known && !mod->known, "conhecidos: ped e bar sim, trokmod nao");
    Check(dancing->renamed && dancing->block == "dance", "dancing.ifp com o nome interno \"%s\"", dancing->block.c_str());
    Check(!ped->broken && !bar->broken && !mod->broken && !dancing->broken, "todos lidos ate o fim");
    const ifp::Anim* walk = Find("PED", "WALK_civi");
    Check(walk && walk->sampId == 1257, "WALK_civi com o indice do SA-MP 1257 (%d)", walk ? walk->sampId : -9);
    if (mode == "nomes") {
        Check(ifp::Reference()[0] == 0, "sem referencia");
        Check(TagOf("PED", "WALK_civi") == ifp::TAG_NONE && TagOf("PED", "IDLE_stance") == ifp::TAG_NONE,
              "nomes: WALK_civi e IDLE_stance sem marca");
        Check(TagOf("PED", "TROK_NOVA") == ifp::TAG_NEW, "nomes: TROK_NOVA nova (nao esta na lista do PED)");
        Check(TagOf("TROKMOD", "TROK_DANCA") == ifp::TAG_NEW, "nomes: o .ifp desconhecido todo novo");
        Check(TagOf("DANCING", "DANCE_LOOP") == ifp::TAG_NONE && dancing->missing.size() == 12,
              "nomes: DANCE_LOOP original, faltam 12 do DANCING (%d)", static_cast<int>(dancing->missing.size()));
        Check(ifp::ModCount() == 0 && ifp::NewCount() == 2, "nomes: 0 modificadas e 2 novas (%d, %d)", ifp::ModCount(),
              ifp::NewCount());
        // Filtro
        ifp::Filter f;
        f.query = "walk";
        Check(ifp::Apply(f).total == 1, "busca \"walk\": 1");
        f.query = "";
        f.changed = true;
        Check(ifp::Apply(f).total == 2, "so as marcadas: 2");
        f.changed = false;
        f.file = static_cast<int>(bar - &ifp::Files()[0]);
        Check(ifp::Apply(f).total == 1, "so o bar.ifp: 1");
        // Quadros-chave
        ifp::Clip clip;
        Check(ifp::LoadClip(walk->index, &clip) && clip.tracks.size() == 2, "WALK_civi: 2 ossos");
        Check(std::fabs(walk->seconds - 1.0f) < 0.02f && walk->moves && walk->compressed, "WALK_civi: 1 s, anda, comprimida");
        if (clip.tracks.size() == 2) {
            const ifp::Track& root = clip.tracks[0];
            Check(root.bone == 0 && root.translation && std::fabs(root.keys[1].t[1] - 1.0f) < 0.002f,
                  "WALK_civi: a raiz anda 1 m em y (%.3f)", root.keys.size() > 1 ? root.keys[1].t[1] : -1.0f);
            const ifp::Key& k = clip.tracks[1].keys[1];
            Check(std::fabs(k.q[0] - std::sin(15 * 3.14159265f / 180)) < 0.002f && std::fabs(k.time - 0.5f) < 0.02f,
                  "WALK_civi: a coxa a 30 graus em 0,5 s (q.x %.4f)", k.q[0]);
        }
        const ifp::Anim* idle = Find("PED", "IDLE_stance");
        Check(idle && !idle->compressed && std::fabs(idle->seconds - 2.5f) < 1e-4f, "IDLE_stance: sem compressao, 2,5 s");
        const ifp::Anim* barman = Find("BAR", "BARMAN_IDLE");
        Check(barman && ifp::LoadClip(barman->index, &clip) && clip.tracks.size() == 1, "BARMAN_IDLE (ANPK): 1 osso");
        if (clip.tracks.size() == 1 && clip.tracks[0].keys.size() == 2) {
            const ifp::Track& t = clip.tracks[0];
            Check(t.bone == -1 && t.name == "Spine", "ANPK: o osso pelo nome (%s)", t.name.c_str());
            Check(std::fabs(t.keys[1].q[0] - 0.70711f) < 1e-3f && std::fabs(t.keys[1].q[3] - 0.70711f) < 1e-3f,
                  "ANPK: a rotacao conjugada de volta (%.4f %.4f)", t.keys[1].q[0], t.keys[1].q[3]);
        }
    } else {
        std::string ref = ifp::Reference();
        Check(ref.find(mode == "pasta" ? "originais" : "animacoes_originais.txt") != std::string::npos, "referencia: %s",
              ref.c_str());
        Check(TagOf("PED", "WALK_civi") == ifp::TAG_MOD, "%s: WALK_civi modificada (outra translacao)", mode.c_str());
        Check(TagOf("PED", "IDLE_stance") == ifp::TAG_NONE && TagOf("BAR", "BARMAN_IDLE") == ifp::TAG_NONE,
              "%s: IDLE_stance e BARMAN_IDLE iguais as originais", mode.c_str());
        Check(TagOf("PED", "TROK_NOVA") == ifp::TAG_NEW && TagOf("TROKMOD", "TROK_DANCA") == ifp::TAG_NEW &&
                  TagOf("DANCING", "DANCE_LOOP") == ifp::TAG_NEW,
              "%s: as que nao estao nas originais sao novas", mode.c_str());
        Check(ifp::ModCount() == 1 && ifp::NewCount() == 3, "%s: 1 modificada e 3 novas (%d, %d)", mode.c_str(),
              ifp::ModCount(), ifp::NewCount());
        Check(ped->missing.empty(), "%s: nada faltando no ped.ifp", mode.c_str());
        if (mode == "pasta") {
            std::ifstream txt(dir + "/Trok DevTools/animacoes_originais.txt");
            std::string line, all;
            int lines = 0;
            while (std::getline(txt, line)) {
                lines += line[0] != '#';
                all += line;
            }
            Check(lines == 3 && all.find("PED:WALK_CIVI") != std::string::npos, "o .txt da referencia com 3 animacoes (%d)",
                  lines);
        }
    }
}

// Esqueleto: a raiz na origem e um osso (cabeca, 5) em z = 1; um vertice em z = 2 todo na cabeca.
void Pose() {
    mesh::Mesh m;
    m.ok = true;
    m.id = 1;
    m.min = {-1, -1, 0};
    m.max = {1, 1, 2};
    mesh::Batch b;
    b.verts.push_back({0, 0, 2, 0, 0, 1, 0, 0});
    b.verts.push_back({0, 0, 0.5f, 0, 0, 1, 0, 0});
    m.batches.push_back(b);
    m.vertices = 2;
    mesh::Bone root, head;
    float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    memcpy(root.skinToBone, identity, sizeof(identity));
    memcpy(head.skinToBone, identity, sizeof(identity));
    head.skinToBone[14] = -1; // o osso em z = 1
    root.tag = 0;
    head.tag = 5;
    head.parent = 0;
    m.skin.bones = {root, head};
    mesh::SkinBatch sb;
    sb.batch = 0;
    sb.pos = {{0, 0, 2}, {0, 0, 0.5f}};
    sb.normal = {{0, 0, 1}, {0, 0, 1}};
    sb.index = {1, 0, 0, 0, 0, 0, 0, 0};
    sb.weight = {1, 0, 0, 0, 1, 0, 0, 0};
    sb.toModel[0] = {1, 0, 0};
    sb.toModel[1] = {0, 1, 0};
    sb.toModel[2] = {0, 0, 1};
    sb.toModel[3] = {0, 0, 0};
    m.skin.batches.push_back(sb);

    ifp::Clip clip;
    ifp::Track t;
    t.bone = 5;
    t.keys.push_back({0, {0, 0, 0, 1}, {0, 0, 0}});
    ifp::Key k = {1, {0, 0, 0, 1}, {0, 0, 0}};
    ifpw::Axis(k.q, 1, 0, 0, 90);
    t.keys.push_back(k);
    clip.tracks.push_back(t);
    clip.seconds = 1;
    mesh::Mesh out;
    Check(pose::Apply(m, clip, 0, true, &out), "pose: aplicada");
    const mesh::Vertex& v = out.batches[0].verts[0];
    Check(std::fabs(v.x) < 1e-4f && std::fabs(v.y) < 1e-4f && std::fabs(v.z - 2) < 1e-4f, "pose em 0 s: o vertice parado (%.3f %.3f %.3f)",
          v.x, v.y, v.z);
    pose::Apply(m, clip, 1, true, &out);
    Check(std::fabs(v.y + 1) < 1e-4f && std::fabs(v.z - 1) < 1e-4f && std::fabs(v.nz) < 1e-4f && std::fabs(v.ny + 1) < 1e-4f,
          "pose em 1 s: a cabeca girou 90 graus em x (%.3f %.3f %.3f, normal %.2f %.2f)", v.x, v.y, v.z, v.ny, v.nz);
    pose::Apply(m, clip, 0.5f, true, &out);
    Check(std::fabs(v.y + 0.70711f) < 1e-3f && std::fabs(v.z - 1.70711f) < 1e-3f, "pose em 0,5 s: 45 graus (%.3f %.3f)", v.y,
          v.z);
    Check(pose::Matched(m, clip) == 1, "pose: 1 osso da animacao no esqueleto");
    // A raiz anda: no lugar, so a altura muda
    ifp::Track r;
    r.bone = 0;
    r.translation = true;
    r.keys.push_back({0, {0, 0, 0, 1}, {0, 0, 0}});
    r.keys.push_back({1, {0, 0, 0, 1}, {0, 3, 0.5f}});
    clip.tracks.push_back(r);
    pose::Apply(m, clip, 1, true, &out);
    const mesh::Vertex& w = out.batches[0].verts[1];
    Check(std::fabs(w.y) < 1e-4f && std::fabs(w.z - 1.0f) < 1e-4f, "no lugar: a raiz so sobe (%.3f %.3f)", w.y, w.z);
    pose::Apply(m, clip, 1, false, &out);
    Check(std::fabs(w.y - 3) < 1e-4f && std::fabs(w.z - 1.0f) < 1e-4f, "andando: a raiz vai 3 m em y (%.3f %.3f)", w.y, w.z);
    Check(out.max.x > m.max.x && out.min.z == m.min.z, "pose: a caixa com folga, o chao no mesmo lugar");
}

// O espaco da pele deitado (como nos personagens do jogo): x da pele = z do modelo. A skinToBone[0] e o frame raiz
// poem de pe; sem o frame (o jeito antigo), o modelo ficava deitado.
void PoseLying() {
    mesh::Mesh m;
    m.ok = true;
    m.id = 2;
    m.min = {-1, -1, 0};
    m.max = {1, 1, 2};
    mesh::Batch b;
    b.verts.push_back({2, 0, 0, 1, 0, 0, 0, 0});
    m.batches.push_back(b);
    m.vertices = 1;
    // Q: x -> z, y -> y, z -> -x (linhas: direita, cima, frente, posicao)
    float q[16] = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 0, 1};
    mesh::Bone root, head;
    memcpy(root.skinToBone, q, sizeof(q));
    memcpy(head.skinToBone, q, sizeof(q));
    head.skinToBone[14] = -1; // a cabeca em z = 1 no modelo
    root.tag = 0;
    head.tag = 5;
    head.parent = 0;
    m.skin.bones = {root, head};
    m.skin.framed = true; // o frame raiz na origem, sem rotacao
    mesh::SkinBatch sb;
    sb.batch = 0;
    sb.pos = {{2, 0, 0}};
    sb.normal = {{1, 0, 0}};
    sb.index = {1, 0, 0, 0};
    sb.weight = {1, 0, 0, 0};
    sb.toModel[0] = {1, 0, 0};
    sb.toModel[1] = {0, 1, 0};
    sb.toModel[2] = {0, 0, 1};
    sb.toModel[3] = {0, 0, 0};
    m.skin.batches.push_back(sb);
    ifp::Clip none;
    mesh::Mesh out;
    pose::Apply(m, none, 0, true, &out);
    const mesh::Vertex& v = out.batches[0].verts[0];
    Check(std::fabs(v.x) < 1e-4f && std::fabs(v.z - 2) < 1e-4f && std::fabs(v.nz - 1) < 1e-4f,
          "pele deitada: o modelo parado fica de pe (%.3f %.3f %.3f)", v.x, v.y, v.z);
    // Uma animacao so na cabeca (a raiz sem quadros, como o abseil do jogo): continua de pe
    ifp::Clip clip;
    ifp::Track t;
    t.bone = 5;
    ifp::Key k = {0, {0, 0, 0, 1}, {0, 0, 0}};
    ifpw::Axis(k.q, 1, 0, 0, 90);
    t.keys.push_back(k);
    clip.tracks.push_back(t);
    clip.seconds = 0.03f;
    pose::Apply(m, clip, 0, true, &out);
    Check(std::fabs(v.x) < 1e-4f && std::fabs(v.y + 1) < 1e-4f && std::fabs(v.z - 1) < 1e-4f,
          "pele deitada, raiz sem quadros: a cabeca gira em pe (%.3f %.3f %.3f)", v.x, v.y, v.z);
    m.skin.framed = false;
    out = mesh::Mesh();
    pose::Apply(m, none, 0, true, &out);
    Check(std::fabs(out.batches[0].verts[0].x - 2) < 1e-4f, "sem o frame raiz: o espaco da pele (o caso antigo)");
}

int main(int argc, char** argv) {
    std::string mode = argc > 1 ? argv[1] : "nomes", dir = argc > 2 ? argv[2] : "jogo";
    printf("== %s\n", mode.c_str());
    if (mode == "pose") {
        Pose();
        PoseLying();
    } else {
        MakeGame(dir, mode == "pasta");
        if (mode == "txt") {
            // O .txt que o modo pasta gerou (o modo pasta roda antes)
            std::string from = dir.substr(0, dir.rfind('_')) + "_pasta/Trok DevTools/animacoes_originais.txt";
            std::ifstream in(from, std::ios::binary);
            std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            WriteFile(dir + "/Trok DevTools/animacoes_originais.txt", data);
        }
        Catalog(mode, dir);
    }
    printf(g_failures ? "FALHOU (%d)\n" : "PASSOU\n", g_failures);
    return g_failures ? 1 : 0;
}
