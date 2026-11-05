#include "app/DebugUi.h"

#include <imgui-SFML.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace game;

namespace app {

namespace {

// ImGui の SliderFloat 等は UTF-8 の文字列をそのまま受け取る。
const float kPi = 3.14159265358979f;

PlayerStats* StatsOf(GameSession& sim) { return sim.Ecs().TryGet<PlayerStats>(sim.PlayerEntity()); }

// 全敵を列挙する(空間ハッシュはその時点のもの。1フレーム分古いが、デバッグ用途なら十分)。
std::vector<EntityID> AllEnemies(GameSession& sim) {
    std::vector<EntityID> out;
    auto& ecs = sim.Ecs();
    sim.Ctx().enemyGrid.Query({0.f, 0.f}, 1e7f, [&](EntityID e) {
        if (ecs.IsAlive(e) && ecs.HasComponent<Enemy>(e)) out.push_back(e);
    });
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// プレイヤーの近く(輪の形)の、敵が立てる場所に出す。
bool SpawnNearPlayer(GameSession& sim, int def) {
    auto& g = sim.Ctx();
    if (!g.ecs.IsAlive(g.player)) return false;
    const Vec2 center = g.ecs.GetComponent<Transform>(g.player).pos;
    for (int i = 0; i < 40; ++i) {
        const float angle = g.rng.Range(0.f, 2.f * kPi), dist = g.rng.Range(140.f, 280.f);
        const Vec2 p{center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist};
        const Vec2i tile = g.map.WorldToTile(p);
        if (!g.map.InBounds(tile) || g.map.BlocksEnemy(tile)) continue;
        const EntityID e = CreateEnemy(g, def, g.map.TileCenter(tile));
        if (def == g.bal.EnemyIndex("boss") && !g.bossSpawned) {   // 最初の1体は、倒すとクリアになるボスとして扱う
            g.boss = e;
            g.bossSpawned = true;
        }
        return true;
    }
    return false;
}

}  // namespace

// ----------------------------------------------------------------------------
bool DebugUi::Init(sf::RenderWindow& window) {
    if (!ImGui::SFML::Init(window, false)) return false;   // 既定のフォントは日本語が出ないので、あとで入れる
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;   // imgui.ini を作らない
    ImFont* font = nullptr;
    for (const char* path : {"C:/Windows/Fonts/meiryo.ttc", "C:/Windows/Fonts/YuGothM.ttc", "C:/Windows/Fonts/msgothic.ttc"}) {
        font = io.Fonts->AddFontFromFileTTF(path, 16.f, nullptr, io.Fonts->GetGlyphRangesJapanese());
        if (font) break;
    }
    if (!font) io.Fonts->AddFontDefault();
    if (!ImGui::SFML::UpdateFontTexture()) return false;
    ready = true;
    return true;
}

void DebugUi::Shutdown(sf::RenderWindow& window) {
    if (ready) ImGui::SFML::Shutdown(window);
    ready = false;
}

void DebugUi::ProcessEvent(sf::RenderWindow& window, const sf::Event& ev) {
    if (ready) ImGui::SFML::ProcessEvent(window, ev);
}

bool DebugUi::WantsMouse() const { return ready && enabled && ImGui::GetIO().WantCaptureMouse; }
bool DebugUi::WantsKeyboard() const { return ready && enabled && ImGui::GetIO().WantCaptureKeyboard; }

void DebugUi::BeginFrame(sf::RenderWindow& window, sf::Time dt, GameSession& sim, Renderer& renderer) {
    if (!ready) return;
    ImGui::SFML::Update(window, dt);   // デバッグモードでなくても毎フレーム呼ぶ(Update と Render を対にするため)
    ApplyEveryFrame(sim);
    if (!enabled) {
        renderer.debug = {};   // デバッグモードを抜けたら表示も消す
        return;
    }
    SystemWindow(sim, renderer, dt);
    PlayerWindow(sim);
    SpawnWindow(sim);
}

void DebugUi::Render(sf::RenderWindow& window) {
    if (ready) ImGui::SFML::Render(window);
}

// デバッグモード中だけ効く設定を、毎フレーム「いまのラン」に反映する。
void DebugUi::ApplyEveryFrame(GameSession& sim) {
    auto& g = sim.Ctx();
    g.spawnEnabled = enabled ? spawnEnabled : true;
    if (enabled && godMode && g.ecs.IsAlive(g.player)) {
        auto& h = g.ecs.GetComponent<Health>(g.player);
        h.hp = h.maxHp;
    }
}

// ----------------------------------------------------------------------------
void DebugUi::SystemWindow(GameSession& sim, Renderer& renderer, sf::Time dt) {
    auto& g = sim.Ctx();
    ImGui::SetNextWindowPos({10.f, 10.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({320.f, 0.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("システム [F1 で閉じる]");

    ImGui::SeparatorText("表示");
    ImGui::Checkbox("当たり判定を表示", &renderer.debug.colliders);
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("水色=プレイヤー 緑=敵 黄=弾 白=アイテム\n赤枠=プレイヤーが通れないタイル 橙枠=敵が通れないタイル");
    ImGui::Checkbox("敵の経路を表示", &renderer.debug.paths);
    if (renderer.debug.paths) renderer.debug.colliders = true;   // 経路は当たり判定の描画の中で描く

    ImGui::SeparatorText("ゲームの速さ(早送り)");
    ImGui::SliderFloat("倍率", &timeScale, 0.f, 10.f, "x%.2f");
    for (float s : {0.f, 0.25f, 0.5f, 1.f, 2.f, 4.f, 8.f}) {
        char label[16];
        std::snprintf(label, sizeof(label), s == 0.f ? "停止" : "x%g", s);
        if (ImGui::Button(label)) timeScale = s;
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::SeparatorText("状態");
    ImGui::Text("経過 %.1f 秒  / ボスまで %.0f 秒", g.time, std::max(0.f, g.bal.duration - g.time));
    ImGui::Text("敵 %d  アイテム %d  撃破 %d", g.enemyCount, g.pickupCount, g.kills);
    ImGui::Text("Entity %u  FPS %.0f", sim.Ecs().LivingCount(), dt.asSeconds() > 0.f ? 1.f / dt.asSeconds() : 0.f);
    if (ImGui::Button("+30秒")) g.time += 30.f;
    ImGui::SameLine();
    if (ImGui::Button("+60秒")) g.time += 60.f;
    ImGui::SameLine();
    if (ImGui::Button("新しいラン")) {
        sim.StartRun(g.rng.Next());
    }
    ImGui::TextDisabled("数値の変更は「いまのラン」だけ有効です");
    ImGui::End();
}

void DebugUi::PlayerWindow(GameSession& sim) {
    auto& g = sim.Ctx();
    ImGui::SetNextWindowPos({340.f, 10.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({330.f, 640.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("プレイヤー");
    PlayerStats* st = StatsOf(sim);
    if (!st || sim.State() == GameState::Title) {
        ImGui::TextDisabled("ゲーム中のみ使えます");
        ImGui::End();
        return;
    }
    auto& ecs = sim.Ecs();
    auto& health = ecs.GetComponent<Health>(sim.PlayerEntity());
    auto& inv = ecs.GetComponent<Inventory>(sim.PlayerEntity());
    auto& exp = ecs.GetComponent<Experience>(sim.PlayerEntity());

    ImGui::SeparatorText("体力");
    ImGui::SliderFloat("HP", &health.hp, 0.f, health.maxHp, "%.0f");
    ImGui::DragFloat("最大HP", &health.maxHp, 1.f, 1.f, 100000.f, "%.0f");
    ImGui::Checkbox("無敵(HP が減らない)", &godMode);

    ImGui::SeparatorText("ステータス");
    ImGui::SliderFloat("移動速度", &st->moveSpeedMul, 0.1f, 5.f, "x%.2f");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f px/s", g.bal.playerSpeed * st->moveSpeedMul);
    ImGui::SliderFloat("攻撃力", &st->damageMul, 0.1f, 20.f, "x%.2f");
    ImGui::SliderFloat("攻撃速度", &st->attackSpeedMul, 0.1f, 10.f, "x%.2f");
    ImGui::SliderInt("追加の玉", &st->extraProjectiles, 0, 20);
    ImGui::SliderFloat("被ダメージ軽減", &st->damageReduction, 0.f, 0.75f, "%.2f");
    ImGui::SliderFloat("拾う範囲", &st->pickupRangeMul, 0.1f, 10.f, "x%.2f");
    ImGui::SliderFloat("岩を押す速さ", &st->pushSpeedMul, 0.1f, 10.f, "x%.2f");
    ImGui::SliderFloat("壁の耐久(新しい壁から)", &st->wallHpMul, 0.1f, 10.f, "x%.2f");
    ImGui::SliderInt("建築コスト減", &st->buildCostReduction, 0, 10);
    if (ImGui::Button("ステータスを初期値に戻す")) *st = PlayerStats{};

    ImGui::SeparatorText("素材");
    ImGui::DragInt("木材", &inv.wood, 1.f, 0, 9999);
    ImGui::DragInt("石材", &inv.stone, 1.f, 0, 9999);
    ImGui::DragInt("鉄くず", &inv.scrap, 1.f, 0, 9999);

    ImGui::SeparatorText("レベル");
    ImGui::Text("Lv %d   EXP %d / %d", exp.level, exp.xp, exp.next);
    if (ImGui::Button("経験値を満タンにする(レベルアップ)")) exp.xp = exp.next;

    ImGui::SeparatorText("攻撃手段");
    auto& arsenal = ecs.GetComponent<Arsenal>(sim.PlayerEntity());
    for (size_t i = 0; i < arsenal.slots.size(); ++i) {
        WeaponSlot& slot = arsenal.slots[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::SliderInt(g.bal.weapons[slot.def].name.c_str(), &slot.level, 1, 20, "Lv %d");
        ImGui::PopID();
    }
    for (size_t i = 0; i < g.bal.upgrades.size(); ++i) {   // 強化を直接獲得する(攻撃手段の獲得・玉を増やす)
        const UpgradeDef& u = g.bal.upgrades[i];
        if (u.stat != UpgradeStat::GrantWeapon && u.stat != UpgradeStat::ExtraProjectiles) continue;
        ImGui::PushID(static_cast<int>(i) + 1000);
        if (ImGui::Button(("獲得: " + u.name).c_str())) sim.ApplyUpgrade(static_cast<int>(i));
        ImGui::PopID();
    }
    ImGui::End();
}

void DebugUi::SpawnWindow(GameSession& sim) {
    auto& g = sim.Ctx();
    ImGui::SetNextWindowPos({680.f, 10.f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({340.f, 700.f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("敵の出現");

    ImGui::Checkbox("敵を自然に出現させる", &spawnEnabled);
    ImGui::TextDisabled("OFF にすると(ボスも含めて)出現が止まります");

    // 種類ごとの数
    std::vector<int> counts(g.bal.enemies.size(), 0);
    const std::vector<EntityID> enemies = AllEnemies(sim);
    for (EntityID e : enemies) {
        const int def = sim.Ecs().GetComponent<Enemy>(e).def;
        if (def >= 0 && def < static_cast<int>(counts.size())) ++counts[def];
    }
    std::string line = "いる敵: ";
    for (size_t i = 0; i < counts.size(); ++i) line += g.bal.enemies[i].name + " " + std::to_string(counts[i]) + "  ";
    ImGui::TextWrapped("%s", line.c_str());

    ImGui::SeparatorText("出現のペース");
    const float minutes = g.time / 60.f;
    ImGui::Text("いまの出現: %.2f 体/秒", g.bal.rateBase + g.bal.ratePerMin * minutes);
    ImGui::SliderFloat("開始時の出現(体/秒)", &g.bal.rateBase, 0.f, 20.f, "%.2f");
    ImGui::SliderFloat("1分ごとの増加", &g.bal.ratePerMin, 0.f, 20.f, "%.2f");
    ImGui::SliderInt("同時に存在できる上限", &g.bal.maxAlive, 0, 3000);
    ImGui::SliderFloat("出現距離 最小", &g.bal.spawnMin, 50.f, 1200.f, "%.0f");
    ImGui::SliderFloat("出現距離 最大", &g.bal.spawnMax, 100.f, 1500.f, "%.0f");
    g.bal.spawnMax = std::max(g.bal.spawnMax, g.bal.spawnMin + 10.f);
    ImGui::SliderFloat("敵HPの増加(1分あたり)", &g.bal.hpGrowthPerMin, 0.f, 2.f, "%.2f");

    ImGui::SeparatorText("種類ごとの割合");
    ImGui::SliderFloat("遠距離敵 出始め(秒)", &g.bal.shooterStart, 0.f, 600.f, "%.0f");
    ImGui::SliderFloat("遠距離敵 割合", &g.bal.shooterRatioStart, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("遠距離敵 1分ごとの増加", &g.bal.shooterRatioPerMin, 0.f, 0.5f, "%.3f");
    ImGui::SliderFloat("大型 出始め(秒)", &g.bal.bigStart, 0.f, 600.f, "%.0f");
    ImGui::SliderFloat("大型 割合", &g.bal.bigRatioStart, 0.f, 1.f, "%.2f");
    ImGui::SliderFloat("大型 1分ごとの増加", &g.bal.bigRatioPerMin, 0.f, 0.5f, "%.3f");
    ImGui::SliderFloat("ボス出現(秒)", &g.bal.duration, 10.f, 1200.f, "%.0f");

    ImGui::SeparatorText("手動で出す(プレイヤーの近く)");
    ImGui::SliderInt("数", &spawnCount, 1, 100);
    ImGui::BeginDisabled(sim.State() != GameState::InGame);
    for (size_t i = 0; i < g.bal.enemies.size(); ++i) {
        const std::string& name = g.bal.enemies[i].name;
        if (ImGui::Button((name + " を出す").c_str())) {
            const int n = name == "boss" ? 1 : spawnCount;
            for (int k = 0; k < n; ++k) SpawnNearPlayer(sim, static_cast<int>(i));
        }
        if (i % 2 == 0 && i + 1 < g.bal.enemies.size()) ImGui::SameLine();
    }
    ImGui::SeparatorText("全滅");
    if (ImGui::Button("全敵を倒す")) {   // HP を 0 にする: ドロップ・撃破数に反映。ボスなら撃破(クリア)扱い
        for (EntityID e : enemies) sim.Ecs().GetComponent<Health>(e).hp = 0.f;
    }
    ImGui::SameLine();
    if (ImGui::Button("全敵を消去")) {   // 静かに消す(ドロップなし)。ボスは残す(消すとクリアできなくなるため)
        for (EntityID e : enemies)
            if (e != g.boss) sim.Ecs().DestroyLater(e);
    }
    ImGui::EndDisabled();
    ImGui::TextDisabled("「倒す」=ドロップあり / 「消去」=ドロップなし(ボスは残る)");
    ImGui::End();
}

}  // namespace app
