// Terrain Survivor エントリポイント。
//  通常起動:    terrain_survivor.exe
//  確認用モード: terrain_survivor.exe --frames N --screenshot out.png [--seed S] [--script "..."] [--bot]
// 確認用モードはウィンドウを開かず、固定ステップで N フレーム進めて PNG を保存して終了する。
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "app/DebugUi.h"
#include "app/Renderer.h"
#include "core/Config.h"
#include "game/GameSession.h"

using namespace game;

namespace {

const sf::Vector2u kWindow{1280, 720};

// ------------------------------------------------------------ データの場所
std::string FindDataFile() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path();
    for (int i = 0; i < 6; ++i) {
        fs::path p = dir / "data" / "balance.cfg";
        if (fs::exists(p)) return p.string();
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return "data/balance.cfg";
}

// ------------------------------------------------------------ スクリプト入力(確認用)
struct Cmd {
    std::string kind, arg;
    int a = 0, b = 0;
    float x = 0.f, y = 0.f;
};

std::vector<Cmd> ParseScript(const std::string& text) {
    std::vector<Cmd> out;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) continue;
        Cmd c;
        const size_t at = item.find('@');
        std::string head = item.substr(0, at);
        if (at != std::string::npos) {
            const std::string range = item.substr(at + 1);
            const size_t dash = range.find('-');
            c.a = std::atoi(range.substr(0, dash).c_str());
            c.b = dash == std::string::npos ? c.a : std::atoi(range.substr(dash + 1).c_str());
        }
        const size_t colon = head.find(':');
        c.kind = head.substr(0, colon);
        if (colon != std::string::npos) {
            c.arg = head.substr(colon + 1);
            const size_t comma = c.arg.find(',');
            if (comma != std::string::npos) {
                c.x = static_cast<float>(std::atof(c.arg.substr(0, comma).c_str()));
                c.y = static_cast<float>(std::atof(c.arg.substr(comma + 1).c_str()));
            }
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace

// ============================================================================
int main(int argc, char** argv) {
    int frames = -1;
    std::string shot, scriptText, give;
    std::vector<std::pair<std::string, std::string>> overrides;
    std::uint32_t seed = 1;
    bool bot = false, noAutostart = false, debugDraw = false, debugPaths = false, startDebug = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--frames") frames = std::atoi(next().c_str());
        else if (a == "--screenshot") shot = next();
        else if (a == "--seed") seed = static_cast<std::uint32_t>(std::atoi(next().c_str()));
        else if (a == "--script") scriptText = next();
        else if (a == "--give") give = next();   // 確認用: 強化を最初から付ける(例: bomb,beam,multishot)
        else if (a == "--set") {   // 確認用: 設定の上書き(例: --set spawn.big_start=0)
            const std::string kv = next();
            const size_t eq = kv.find('=');
            if (eq != std::string::npos) overrides.push_back({kv.substr(0, eq), kv.substr(eq + 1)});
        }
        else if (a == "--debug") startDebug = true;   // 確認用: 最初からデバッグモード
        else if (a == "--debug-draw") debugDraw = true;    // 確認用: 当たり判定を重ねて描く
        else if (a == "--debug-paths") debugPaths = true;  // 確認用: 敵の経路も描く
        else if (a == "--bot") bot = true;
        else if (a == "--no-autostart") noAutostart = true;
    }

    core::Config cfg;
    std::string dataFile;
    try {
        dataFile = FindDataFile();
        cfg = core::Config::LoadFile(dataFile);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "設定の読み込みに失敗: %s\n", e.what());
        return 1;
    }
    for (const auto& kv : overrides) cfg.Set(kv.first, kv.second);
    GameSession sim(cfg, seed);
    app::Renderer renderer(std::filesystem::path(dataFile).parent_path().u8string());
    const float step = sim.Ctx().bal.step;
    renderer.debug.colliders = debugDraw || debugPaths;
    renderer.debug.paths = debugPaths;

    // ---------------------------------------------------------- 確認用(ヘッドレス)モード
    if (frames >= 0) {
        sf::RenderTexture rt(kWindow);
        const std::vector<Cmd> cmds = ParseScript(scriptText);
        sf::Vector2f aimScreen{kWindow.x / 2.f + 100.f, kWindow.y / 2.f};
        if (!noAutostart) {
            InputState s;
            s.startPressed = true;
            sim.Step(s, step);
            if (sim.State() == GameState::InGame) {   // 指定 seed で作り直す(Title の乱数に依存しない)
                sim.StartRun(seed);
            }
        }
        if (!give.empty() && sim.State() == GameState::InGame) {
            std::stringstream gs(give);
            std::string id;
            while (std::getline(gs, id, ',')) {
                const auto& ups = sim.Ctx().bal.upgrades;
                for (size_t i = 0; i < ups.size(); ++i)
                    if (ups[i].id == id) sim.ApplyUpgrade(static_cast<int>(i));
            }
        }
        for (int f = 0; f < frames; ++f) {
            InputState in;
            const sf::View view = renderer.GameView(sim, sf::Vector2f(kWindow));
            // --- スクリプト
            for (const Cmd& c : cmds) {
                const bool active = f >= c.a && f <= c.b;
                if (c.kind == "hold" && active && !c.arg.empty()) {
                    switch (std::toupper(c.arg[0])) {
                        case 'W': in.move.y -= 1.f; break;
                        case 'S': in.move.y += 1.f; break;
                        case 'A': in.move.x -= 1.f; break;
                        case 'D': in.move.x += 1.f; break;
                    }
                } else if (c.kind == "aim" && f >= c.a) {
                    aimScreen = {c.x, c.y};
                } else if (f == c.a) {
                    if (c.kind == "build") in.buildPressed = true;
                    else if (c.kind == "start") in.startPressed = true;
                    else if (c.kind == "pause") in.pausePressed = true;
                    else if (c.kind == "wall") in.wallSelect = std::atoi(c.arg.c_str()) - 1;
                    else if (c.kind == "choose") in.choose = std::atoi(c.arg.c_str()) - 1;
                    else if (c.kind == "click") {
                        if (sim.State() == GameState::LevelUp) {
                            const int n = static_cast<int>(sim.Ctx().offer.size());
                            for (int i = 0; i < n; ++i)
                                if (app::Renderer::CardRect(i, n, sf::Vector2f(kWindow)).contains({c.x, c.y})) in.choose = i;
                        } else {
                            in.startPressed = true;
                        }
                    }
                }
            }
            // --- 簡易ボット: 周回しながら最寄りの敵を向き、時々壁を建て、強化は先頭を選ぶ
            if (bot) {
                const float ang = (f / 150) * 1.5707963f;
                in.move = {std::cos(ang), std::sin(ang)};
                if (sim.State() == GameState::LevelUp) in.choose = 0;
                if (sim.State() == GameState::InGame && f % 90 == 45) in.buildPressed = true;
                auto& ecs = sim.Ecs();
                if (ecs.IsAlive(sim.PlayerEntity())) {
                    const core::Vec2 pp = ecs.GetComponent<Transform>(sim.PlayerEntity()).pos;
                    float best = 1e30f;
                    core::Vec2 target = pp + in.move * 100.f;
                    sim.Ctx().enemyGrid.Query(pp, 300.f, [&](ECS::EntityID e) {
                        if (!ecs.IsAlive(e)) return;
                        const core::Vec2 ep = ecs.GetComponent<Transform>(e).pos;
                        const float d = (ep - pp).LengthSq();
                        if (d < best) { best = d; target = ep; }
                    });
                    in.aimWorld = target;
                }
            } else {
                const sf::Vector2f w = rt.mapPixelToCoords({static_cast<int>(aimScreen.x), static_cast<int>(aimScreen.y)}, view);
                in.aimWorld = {w.x, w.y};
            }
            sim.Step(in, step);
        }
        auto& ecs = sim.Ecs();
        std::printf("state=%d time=%.1f enemies=%d kills=%d pickups=%d entities=%u\n", static_cast<int>(sim.State()), sim.Ctx().time,
                    sim.Ctx().enemyCount, sim.Ctx().kills, sim.Ctx().pickupCount, ecs.LivingCount());
        if (ecs.IsAlive(sim.PlayerEntity())) {
            const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
            const auto& inv = ecs.GetComponent<Inventory>(sim.PlayerEntity());
            const auto& ex = ecs.GetComponent<Experience>(sim.PlayerEntity());
            std::printf("player hp=%.0f/%.0f level=%d wood=%d stone=%d\n", hp.hp, hp.maxHp, ex.level, inv.wood, inv.stone);
        }
        if (!shot.empty()) {
            renderer.Draw(rt, sim, false);
            rt.display();
            std::error_code ec;
            const std::filesystem::path p(shot);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
            if (!rt.getTexture().copyToImage().saveToFile(p)) {
                std::fprintf(stderr, "スクリーンショットの保存に失敗: %s\n", shot.c_str());
                return 1;
            }
            std::printf("saved %s\n", shot.c_str());
        }
        return 0;
    }

    // ---------------------------------------------------------- 通常起動
    sf::RenderWindow window(sf::VideoMode(kWindow), "Terrain Survivor");
    window.setFramerateLimit(60);
    sf::Clock clock;
    double acc = 0.0;
    bool tabHeld = false;
    // クリック・キー入力のエッジは、固定ステップが実際に実行されるまで保持する。
    // (そのフレームに Step が0回でも入力を落とさない。60fps 制限+固定 1/60 では起こりうる)
    InputState pending;

    // F1 で入るデバッグモード(ImGui)。ImGui がマウス・キーボードを使っているときは、ゲームへ入力を渡さない。
    app::DebugUi debugUi;
    if (!debugUi.Init(window)) std::fprintf(stderr, "警告: デバッグ GUI を初期化できませんでした(F1 は無効)\n");
    if (startDebug) debugUi.Toggle();

    while (window.isOpen()) {
        InputState& in = pending;
        while (const std::optional<sf::Event> ev = window.pollEvent()) {
            debugUi.ProcessEvent(window, *ev);
            if (ev->is<sf::Event::Closed>()) window.close();
            if (const auto* k = ev->getIf<sf::Event::KeyPressed>()) {
                if (k->code == sf::Keyboard::Key::F1) debugUi.Toggle();
            }
            const bool uiMouse = debugUi.WantsMouse(), uiKeys = debugUi.WantsKeyboard();
            if (const auto* k = ev->getIf<sf::Event::KeyPressed>(); k && !uiKeys) {
                using K = sf::Keyboard::Key;
                if (k->code == K::Escape) in.pausePressed = true;
                if (k->code == K::Enter || k->code == K::Space) in.startPressed = true;
                if (k->code >= K::Num1 && k->code <= K::Num9) {
                    const int n = static_cast<int>(k->code) - static_cast<int>(K::Num1);
                    in.wallSelect = n;
                    in.choose = n;
                }
                if (k->code == K::Tab) tabHeld = true;
            }
            if (const auto* k = ev->getIf<sf::Event::KeyReleased>()) {
                if (k->code == sf::Keyboard::Key::Tab) tabHeld = false;
            }
            if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>(); m && !uiMouse) {
                if (m->button == sf::Mouse::Button::Right) in.buildPressed = true;
                if (m->button == sf::Mouse::Button::Left) {
                    in.startPressed = true;
                    const sf::Vector2f p(static_cast<float>(m->position.x), static_cast<float>(m->position.y));
                    const int n = static_cast<int>(sim.Ctx().offer.size());
                    for (int i = 0; i < n; ++i)
                        if (app::Renderer::CardRect(i, n, sf::Vector2f(window.getSize())).contains(p)) in.choose = i;
                }
            }
        }
        // 連続入力(毎フレーム作り直す。エッジとは別)
        in.move = {};
        if (window.hasFocus() && !debugUi.WantsKeyboard()) {
            using K = sf::Keyboard::Key;
            if (sf::Keyboard::isKeyPressed(K::W)) in.move.y -= 1.f;
            if (sf::Keyboard::isKeyPressed(K::S)) in.move.y += 1.f;
            if (sf::Keyboard::isKeyPressed(K::A)) in.move.x -= 1.f;
            if (sf::Keyboard::isKeyPressed(K::D)) in.move.x += 1.f;
        }
        const sf::View view = renderer.GameView(sim, sf::Vector2f(window.getSize()));
        const sf::Vector2f w = window.mapPixelToCoords(sf::Mouse::getPosition(window), view);
        in.aimWorld = {w.x, w.y};

        const sf::Time frameTime = clock.restart();
        debugUi.BeginFrame(window, frameTime, sim, renderer);
        // 早送りはプレイ中だけ(レベルアップ選択やタイトルは等速)。速いほど1フレームに進めるステップ数の上限も増やす。
        const double speed = sim.State() == GameState::InGame ? debugUi.TimeScale() : 1.0;
        acc += std::min(0.1, static_cast<double>(frameTime.asSeconds())) * speed;
        const int maxSteps = std::max(5, static_cast<int>(std::ceil(5.0 * speed)));
        int steps = 0;
        InputState stepIn = in;
        while (acc >= step && steps < maxSteps) {
            sim.Step(stepIn, step);
            stepIn.buildPressed = stepIn.pausePressed = stepIn.startPressed = false;   // エッジは最初の1ステップだけ
            stepIn.choose = -1;
            stepIn.wallSelect = -1;
            acc -= step;
            ++steps;
        }
        if (steps > 0) {   // 実際に消費されたときだけエッジを捨てる
            pending.buildPressed = pending.pausePressed = pending.startPressed = false;
            pending.choose = -1;
            pending.wallSelect = -1;
        }
        renderer.Draw(window, sim, tabHeld);
        debugUi.Render(window);
        window.display();
    }
    debugUi.Shutdown(window);
    return 0;
}
