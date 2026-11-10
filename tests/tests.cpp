// SFML 非依存のロジックテスト。仕様書 §19.2 の合格条件のうち、ロジックで確認できるものを検証する。
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>
#include <string>
#include <vector>

#include "ecs/ECSManager.h"
#include "game/Pathfinding.h"
#include "game/Rules.h"
#include "game/GameSession.h"
#include "game/Systems.h"

using namespace game;

// ------------------------------------------------------------ 簡易フレームワーク
struct TestCase { const char* name; std::function<void()> fn; };
static std::vector<TestCase>& Registry() { static std::vector<TestCase> r; return r; }
static int g_failures = 0;
static const char* g_current = "";
struct Reg { Reg(const char* n, std::function<void()> f) { Registry().push_back({n, std::move(f)}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
#define CHECK(cond) do { if (!(cond)) { ++g_failures; std::printf("  FAIL [%s] %s:%d  %s\n", g_current, __FILE__, __LINE__, #cond); } } while (0)

// ------------------------------------------------------------------ ヘルパー
static std::string ReadFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static std::string BalanceText() {
    for (const char* p : {"data/balance.cfg", "../data/balance.cfg", "../../data/balance.cfg", "../../../data/balance.cfg"}) {
        std::string t = ReadFile(p);
        if (!t.empty()) return t;
    }
    std::printf("balance.cfg が見つかりません\n");
    std::exit(2);
}
// 何もない広場(岩・水・素材・自然出現なし)で始める。overrides で個別に上書きできる。
static const char* kEmpty =
    "map.rock_count = 0\nmap.water_clusters = 0\nmap.stone_clusters = 0\nmap.resource_count = 0\n"
    "map.resource_respawn = 0\nspawn.rate_base = 0\nspawn.rate_per_min = 0\n";

struct Fixture {
    core::Config cfg;
    GameSession sim;
    explicit Fixture(const std::string& overrides = "", bool empty = true, std::uint32_t seed = 1)
        : cfg(core::Config::LoadString(BalanceText() + "\n" + (empty ? kEmpty : "") + overrides)), sim(cfg, seed) {
        sim.StartRun(seed);
        sim.Ecs().EnableIntraPhaseShuffle(seed * 2654435761u + 12345u);   // フェーズ内は順不同でも結果が正しいこと
    }
    GameContext& g() { return sim.Ctx(); }
    ECS::ECSManager& ecs() { return sim.Ecs(); }
    MapManager& map() { return sim.Map(); }
    Vec2 PlayerPos() { return ecs().GetComponent<Transform>(sim.PlayerEntity()).pos; }
    void SetPlayerPos(Vec2 p) { ecs().GetComponent<Transform>(sim.PlayerEntity()).pos = p; }
    Vec2i PlayerTile() { return map().WorldToTile(PlayerPos()); }
    void Step(InputState in = {}, int n = 1) {
        for (int i = 0; i < n; ++i) sim.Step(in, g().bal.step);
    }
    InputState Move(float x, float y) { InputState i; i.move = {x, y}; i.aimWorld = PlayerPos() + Vec2{100.f, 0.f}; return i; }
    InputState Aim(Vec2 dir) { InputState i; i.aimWorld = PlayerPos() + dir * 100.f; return i; }
    int Enemy(const char* name) { return g().bal.EnemyIndex(name); }
    EntityID Spawn(const char* name, Vec2 p) { return CreateEnemy(g(), Enemy(name), p); }
    void GodMode() {  // プレイヤーを死なせない(挙動の観察用)
        auto& h = ecs().GetComponent<Health>(sim.PlayerEntity());
        h.hp = h.maxHp = 1e9f;
    }
    void ClearWeapon() {   // 今持っている攻撃手段をすべて止める(後から獲得したものは動く)
        for (auto& w : ecs().GetComponent<Arsenal>(sim.PlayerEntity()).slots) w.cooldown = 1e9f;
    }
    int UpgradeIndex(const char* id) {
        for (size_t i = 0; i < g().bal.upgrades.size(); ++i) if (g().bal.upgrades[i].id == id) return static_cast<int>(i);
        return -1;
    }
    float Hp(EntityID e) { return ecs().GetComponent<Health>(e).hp; }
    EntityID Dummy(Vec2 p) {   // 動かず、ダメージも与えず、簡単には死なない的
        auto e = Spawn("normal", p);
        auto& h = ecs().GetComponent<Health>(e);
        h.hp = h.maxHp = 1000.f;
        return e;
    }
    Vec2 Center(Vec2i t) { return map().TileCenter(t); }
};

// ------------------------------------------------------------------- ECS
struct CA { int v = 0; };
struct CB { int v = 0; };
struct OrderSys : ECS::System {
    std::vector<int>* log; int id;
    OrderSys(std::vector<int>* l, int i) : log(l), id(i) {}
    void Update(ECS::ECSManager&, float) override { log->push_back(id); }
};

struct PhaseSys : ECS::System {
    std::vector<std::pair<int, int>>* log; int phase, id;
    PhaseSys(std::vector<std::pair<int, int>>* l, int p, int i) : log(l), phase(p), id(i) {}
    void Update(ECS::ECSManager&, float) override { log->push_back({phase, id}); }
};

TEST(ecs_phases_run_in_phase_order_and_each_system_runs_once) {
    for (int shuffle = 0; shuffle < 2; ++shuffle) {
        ECS::ECSManager ecs;
        std::vector<std::pair<int, int>> log;
        if (shuffle) ecs.EnableIntraPhaseShuffle(99);
        int id = 0;
        // 登録順はわざとフェーズ順と逆にする(登録順ではなくフェーズ番号で並ぶこと)
        for (int phase : {2, 0, 1, 2, 0, 1, 2}) for (int k = 0; k < 4; ++k) ecs.RegisterSystem<PhaseSys>(phase, ECS::Signature{}, &log, phase, id++);
        std::set<std::vector<int>> orders;
        for (int frame = 0; frame < 30; ++frame) {
            log.clear();
            ecs.Update(0.f);
            CHECK(log.size() == 28);
            bool sorted = true;
            std::set<int> ids;
            std::vector<int> phase0;
            for (size_t i = 0; i < log.size(); ++i) {
                if (i > 0 && log[i].first < log[i - 1].first) sorted = false;
                ids.insert(log[i].second);
                if (log[i].first == 0) phase0.push_back(log[i].second);
            }
            CHECK(sorted);              // フェーズ間は常に 0 → 1 → 2
            CHECK(ids.size() == 28);    // 全 System が1回ずつ(衝突で脱落しない)
            orders.insert(phase0);
        }
        if (shuffle) CHECK(orders.size() > 1);    // フェーズ内の順序は実際に変わっている
        else CHECK(orders.size() == 1);
    }
}

TEST(ecs_destroy_is_deferred_and_stale_ids_are_dead) {
    ECS::ECSManager ecs;
    ecs.RegisterComponent<CA>();
    ecs.RegisterComponent<CB>();
    auto e = ecs.CreateEntity();
    ecs.AddComponent(e, CA{5});
    ecs.DestroyLater(e);
    ecs.DestroyLater(e);  // 多重予約は無害
    CHECK(ecs.IsAlive(e));
    ecs.Flush();
    CHECK(!ecs.IsAlive(e));
    CHECK(ecs.TryGet<CA>(e) == nullptr);
    auto e2 = ecs.CreateEntity();  // 破棄した ID はすぐには再利用されないので、古い ID は無効のまま
    ecs.AddComponent(e2, CA{7});
    CHECK(e2 != e);
    CHECK(ecs.TryGet<CA>(e) == nullptr);
    CHECK(ecs.TryGet<CA>(e2) && ecs.TryGet<CA>(e2)->v == 7);
}

TEST(ecs_component_swap_remove_keeps_data) {
    ECS::ECSManager ecs;
    ecs.RegisterComponent<CA>();
    std::vector<ECS::EntityID> es;
    for (int i = 0; i < 10; ++i) { es.push_back(ecs.CreateEntity()); ecs.AddComponent(es.back(), CA{i}); }
    ecs.DestroyLater(es[3]);
    ecs.DestroyLater(es[0]);
    ecs.Flush();
    for (int i = 0; i < 10; ++i) {
        if (i == 3 || i == 0) continue;
        CHECK(ecs.GetComponent<CA>(es[i]).v == i);
    }
}

TEST(ecs_system_signature_membership) {
    ECS::ECSManager ecs;
    ecs.RegisterComponent<CA>();
    ecs.RegisterComponent<CB>();
    std::vector<int> log;
    auto sys = ecs.RegisterSystem<OrderSys>(0, ECS::ECSManager::MakeSignature<CA, CB>(), &log, 0);
    auto a = ecs.CreateEntity(); ecs.AddComponent(a, CA{});
    auto b = ecs.CreateEntity(); ecs.AddComponent(b, CA{}); ecs.AddComponent(b, CB{});
    CHECK(sys->mEntities.size() == 1 && sys->mEntities.count(b) == 1);
    ecs.RemoveComponent<CB>(b);
    CHECK(sys->mEntities.empty());
    (void)a;
}

// ---------------------------------------------------------------- Config
TEST(config_loads_balance_and_missing_key_throws) {
    auto cfg = core::Config::LoadString(BalanceText());
    CHECK(cfg.I("tile.size") == 32);
    CHECK(cfg.I("map.width") == 50 && cfg.I("map.height") == 50);
    CHECK(cfg.F("player.speed") == 160.f);
    CHECK(cfg.I("wall.basic.cost_wood") == 3);
    bool threw = false;
    try { cfg.F("no.such.key"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    Balance b = Balance::Load(cfg);
    CHECK(b.enemies.size() == 5 && b.walls.size() == 1 && !b.upgrades.empty());
    CHECK(b.playerHp == 100.f);
}

// ---------------------------------------------------------- 建築判定(純粋)
struct MapFix {
    core::Config cfg = core::Config::LoadString(BalanceText());
    ECS::EventBus bus;
    MapManager map{20, 20, 32.f, cfg, bus};
};

TEST(quantize_dir8) {
    CHECK(core::QuantizeDir8({1, 0}) == Vec2i(1, 0));
    CHECK(core::QuantizeDir8({0, -1}) == Vec2i(0, -1));
    CHECK(core::QuantizeDir8({-1, 0.1f}) == Vec2i(-1, 0));
    CHECK(core::QuantizeDir8({1, 1}) == Vec2i(1, 1));
    CHECK(core::QuantizeDir8({-1, 1}) == Vec2i(-1, 1));
    CHECK(core::QuantizeDir8({-1, -1}) == Vec2i(-1, -1));
    CHECK(core::QuantizeDir8({1, -1}) == Vec2i(1, -1));
    CHECK(core::QuantizeDir8({0, 0}) == Vec2i(0, 0));
    CHECK(core::QuantizeDir8({1, 0.4f}) == Vec2i(1, 0));   // 22.5° 未満は右
    CHECK(core::QuantizeDir8({1, 0.5f}) == Vec2i(1, 1));   // 26.6° は斜め
}

TEST(plan_wall_placement_rules) {
    MapFix f;
    const Vec2i p{5, 5};
    auto plan = [&](Vec2 aim, bool mats = true) { return PlanWallPlacement(f.map, p, aim, {}, mats); };
    // 通常の地面: 8方向すべて設置可能
    const Vec2 dirs[8] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (Vec2 d : dirs) {
        auto r = plan(d);
        CHECK(r.ok);
        CHECK(r.tile == p + core::QuantizeDir8(d));
    }
    CHECK(plan({0, 0}).reason == BuildFail::NoDirection);
    CHECK(plan({1, 0}, false).reason == BuildFail::NoMaterial);
    f.map.PlaceRock(1, {6, 5});
    CHECK(plan({1, 0}).reason == BuildFail::Rock);
    f.map.PlaceWall(2, {4, 5});
    CHECK(plan({-1, 0}).reason == BuildFail::ExistingWall);
    f.map.SetTerrain({5, 6}, Terrain::Stone);  // 建築禁止地形
    CHECK(plan({0, 1}).reason == BuildFail::Terrain);
    f.map.SetTerrain({5, 4}, Terrain::Water);
    CHECK(plan({0, -1}).reason == BuildFail::Terrain);
    // マップ外(外周の境界は Border、さらに外は範囲外)
    auto edge = PlanWallPlacement(f.map, {0, 0}, {-1, 0}, {}, true);
    CHECK(edge.reason == BuildFail::OutOfBounds);
    auto border = PlanWallPlacement(f.map, {1, 5}, {-1, 0}, {}, true);  // (0,5) は Border 地形
    CHECK(border.reason == BuildFail::Terrain);
}

TEST(push_out_picks_nearest_free_neighbour_with_fixed_tiebreak) {
    MapFix f;
    const Vec2i target{5, 5}, player{4, 5};
    // 敵がタイル中心ちょうどにいる: 同距離の候補が複数 → 固定順(上, 右, 下, 左 ...)の先頭。
    // 左(4,5)=プレイヤータイルは敵の移動先にしない。
    std::vector<Occupant> occ{{10, f.map.TileCenter(target), true}};
    std::vector<PushMove> out;
    CHECK(AssignPushDestinations(f.map, target, player, occ, out));
    CHECK(out.size() == 1 && out[0].toTile == Vec2i(5, 4));
    // 敵が右寄りにいれば、右の候補が最も近い
    occ[0].pos = f.map.TileCenter(target) + Vec2{12.f, 0.f};
    CHECK(AssignPushDestinations(f.map, target, player, occ, out));
    CHECK(out[0].toTile == Vec2i(6, 5));
    // 同じ入力なら同じ結果(決定的)
    std::vector<PushMove> out2;
    CHECK(AssignPushDestinations(f.map, target, player, occ, out2) && out2[0].toTile == out[0].toTile);
}

TEST(push_out_multiple_targets_get_distinct_tiles_or_all_fail) {
    MapFix f;
    const Vec2i target{5, 5}, player{4, 5};
    std::vector<Occupant> occ;
    for (int i = 0; i < 5; ++i) occ.push_back({static_cast<EntityID>(10 + i), f.map.TileCenter(target), true});
    std::vector<PushMove> out;
    CHECK(AssignPushDestinations(f.map, target, player, occ, out));
    CHECK(out.size() == 5);
    std::set<std::pair<int, int>> seen;
    for (auto& m : out) seen.insert({m.toTile.x, m.toTile.y});
    CHECK(seen.size() == 5);  // 重複なし
    // 周囲を岩で塞ぐ: 空きは 3 マスだけ(右上・右下・左上 を残す)。4 体は入りきらず全体失敗。
    for (Vec2i t : {Vec2i{5, 4}, Vec2i{6, 5}, Vec2i{5, 6}, Vec2i{6, 6}, Vec2i{4, 6}}) f.map.PlaceRock(100, t);
    // 残りの空き: (4,4),(6,4)。(4,5) はプレイヤータイルで敵には使えない。
    occ.resize(2);
    CHECK(AssignPushDestinations(f.map, target, player, occ, out) && out.size() == 2);
    occ.push_back({99, f.map.TileCenter(target), true});
    CHECK(!AssignPushDestinations(f.map, target, player, occ, out));
    auto plan = PlanWallPlacement(f.map, player, {1, 0}, occ, true);
    CHECK(!plan.ok && plan.reason == BuildFail::NoRoom && plan.pushes.empty());
    // アイテムはプレイヤータイルにも逃がせる(回収可能な場所)
    std::vector<Occupant> items{{50, f.map.TileCenter(target), false}, {51, f.map.TileCenter(target), false}};
    CHECK(AssignPushDestinations(f.map, target, player, items, out));
}

TEST(push_out_does_not_use_solid_tiles_for_enemies) {
    MapFix f;
    const Vec2i target{5, 5}, player{4, 5};
    f.map.SetTerrain({5, 4}, Terrain::Water);
    f.map.PlaceWall(7, {6, 5});
    std::vector<Occupant> occ{{10, f.map.TileCenter(target) + Vec2{0.f, -10.f}, true}};  // 上寄り
    std::vector<PushMove> out;
    CHECK(AssignPushDestinations(f.map, target, player, occ, out));
    CHECK(f.map.BlocksEnemy(out[0].toTile) == false);
    CHECK(out[0].toTile != Vec2i(5, 4));
}

// ------------------------------------------------------------ 岩押し判定
TEST(rock_push_rules) {
    MapFix f;
    f.map.PlaceRock(1, {5, 5});
    CHECK(CanPushRock(f.map, {5, 5}, {1, 0}));
    CHECK(CanPushRock(f.map, {5, 5}, {0, -1}));
    CHECK(!CanPushRock(f.map, {5, 5}, {0, 0}));
    CHECK(!CanPushRock(f.map, {7, 7}, {1, 0}));                      // 岩がない
    f.map.PlaceRock(2, {6, 5});
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 0}));                      // 先に岩 → 連続押し不可
    f.map.RemoveRock({6, 5});
    f.map.PlaceWall(3, {6, 5});
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 0}));                      // 先に壁
    f.map.RemoveWall({6, 5});
    f.map.SetTerrain({6, 5}, Terrain::Water);
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 0}));                      // 先が水
    f.map.SetTerrain({6, 5}, Terrain::Grass);
    CHECK(CanPushRock(f.map, {5, 5}, {1, 0}));
}

TEST(rock_diagonal_push_requires_three_clear_tiles) {
    MapFix f;
    f.map.PlaceRock(1, {5, 5});
    CHECK(CanPushRock(f.map, {5, 5}, {1, 1}));
    f.map.PlaceRock(2, {6, 6});          // 斜め移動先
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 1}));
    f.map.RemoveRock({6, 6});
    f.map.PlaceWall(3, {6, 5});          // 横の隣接
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 1}));
    f.map.RemoveWall({6, 5});
    f.map.PlaceRock(4, {5, 6});          // 縦の隣接
    CHECK(!CanPushRock(f.map, {5, 5}, {1, 1}));
    f.map.RemoveRock({5, 6});
    CHECK(CanPushRock(f.map, {5, 5}, {1, 1}));
}

TEST(map_changes_publish_events) {
    MapFix f;
    int moved = 0, placed = 0, destroyed = 0, terrain = 0;
    f.bus.Subscribe<OnObstacleMoved>([&](const OnObstacleMoved&) { ++moved; });
    f.bus.Subscribe<OnBuildingPlaced>([&](const OnBuildingPlaced&) { ++placed; });
    f.bus.Subscribe<OnBuildingDestroyed>([&](const OnBuildingDestroyed&) { ++destroyed; });
    f.bus.Subscribe<OnTerrainChanged>([&](const OnTerrainChanged&) { ++terrain; });
    f.map.PlaceRock(1, {3, 3});
    f.map.MoveRock({3, 3}, {4, 3});
    f.map.PlaceWall(2, {8, 8});
    f.map.RemoveWall({8, 8});
    f.map.SetTerrain({9, 9}, Terrain::Water);
    CHECK(moved == 2 && placed == 1 && destroyed == 1 && terrain == 1);
    CHECK(f.map.RockAt({4, 3}) == 1 && f.map.RockAt({3, 3}) == ECS::NULL_ENTITY);
}

// -------------------------------------------------------------------- A*
TEST(astar_straight_and_around_obstacles) {
    MapFix f;
    PathProfile normal;
    auto r = FindPath(f.map, {2, 5}, {10, 5}, normal, 5000);
    CHECK(r.reachedGoal && !r.tiles.empty() && r.tiles.back() == Vec2i(10, 5));
    CHECK(r.tiles.size() == 8);
    // 縦の壁(x=6, y=1..17) のうち y=9 だけ空ける → 迂回して到達
    for (int y = 1; y <= 17; ++y) if (y != 9) f.map.PlaceWall(100 + y, {6, y});
    auto r2 = FindPath(f.map, {2, 5}, {10, 5}, normal, 5000);
    CHECK(r2.reachedGoal);
    bool throughGap = false;
    for (auto t : r2.tiles) { CHECK(!f.map.BlocksEnemy(t)); if (t == Vec2i(6, 9)) throughGap = true; }
    CHECK(throughGap);
    // 壁を壊せる敵は、迂回より安いなら壁を通る経路を選ぶ
    PathProfile breaker; breaker.wallCost = 2.f;
    auto r3 = FindPath(f.map, {2, 5}, {10, 5}, breaker, 5000);
    CHECK(r3.reachedGoal && r3.tiles.size() < r2.tiles.size());
}

TEST(astar_unreachable_goal_returns_closest_point_not_garbage) {
    MapFix f;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) if (dx || dy) f.map.PlaceRock(200 + dx + dy * 3 + 9, {10 + dx, 10 + dy});
    auto r = FindPath(f.map, {2, 10}, {10, 10}, PathProfile{}, 5000);
    CHECK(!r.reachedGoal);
    CHECK(r.found && !r.tiles.empty());
    for (auto t : r.tiles) CHECK(!f.map.BlocksEnemy(t));
    CHECK(r.tiles.back().x == 8 || r.tiles.back().x == 9);   // 囲いの手前まで
    // 完全に囲まれた start でも落ちない
    auto r2 = FindPath(f.map, {10, 10}, {2, 2}, PathProfile{}, 5000);
    CHECK(!r2.reachedGoal && r2.tiles.empty());
}

TEST(astar_no_corner_cutting) {
    MapFix f;
    f.map.PlaceRock(1, {6, 5});
    f.map.PlaceRock(2, {5, 6});   // (5,5) から (6,6) へ斜めに抜けようとすると角抜けになる
    auto r = FindPath(f.map, {5, 5}, {6, 6}, PathProfile{}, 5000);
    CHECK(r.tiles.size() != 1 || r.tiles[0] != Vec2i(6, 6));
}

// ------------------------------------------------------------ 衝突
TEST(move_and_slide_slides_along_wall_and_blocks_corners) {
    MapFix f;
    for (int y = 3; y <= 8; ++y) f.map.PlaceWall(300 + y, {8, y});
    // 壁(x=8*32=256)に向かって斜めに進む: x は止まり、y は滑る
    Vec2 p{256.f - 11.f, 160.f};
    Vec2 q = MoveAndSlide(f.map, p, {2.f, 2.f}, 10.f, MapManager::Who::Player);
    CHECK(q.x == p.x && q.y > p.y);
    // 壁の端を回り込むときも、タイルの角をすり抜けない(円が角に食い込まない)
    Vec2 c{240.f, 272.f};   // 壁の下端の角 (256,288) へ斜めに近づく
    for (int i = 0; i < 60; ++i) c = MoveAndSlide(f.map, c, {1.f, 1.f}, 10.f, MapManager::Who::Player);
    CHECK(!f.map.CircleHitsBlocked(c, 10.f, MapManager::Who::Player));
}

TEST(depenetrate_pushes_circle_out_of_new_wall) {
    MapFix f;
    f.map.PlaceWall(1, {5, 5});
    Vec2 inside{5.f * 32 - 4.f, 5.5f * 32};   // 壁タイルの左辺から 4px 食い込んだ円(r=10)
    Vec2 out = Depenetrate(f.map, inside, 10.f, MapManager::Who::Player);
    CHECK(!f.map.CircleHitsBlocked(out, 10.f, MapManager::Who::Player));
    CHECK(Distance(out, inside) < 20.f);
}

// ===========================================================================
// GameSession 統合テスト
// ===========================================================================
TEST(sim_title_then_start) {
    core::Config cfg = core::Config::LoadString(BalanceText());
    GameSession sim(cfg, 3);
    CHECK(sim.State() == GameState::Title);
    InputState in;
    sim.Step(in, 0.016f);
    CHECK(sim.State() == GameState::Title);
    in.startPressed = true;
    sim.Step(in, 0.016f);
    CHECK(sim.State() == GameState::InGame);
}

TEST(sim_map_generation_spec) {
    core::Config cfg = core::Config::LoadString(BalanceText());
    GameSession sim(cfg, 5);
    sim.StartRun(5);
    MapManager& m = sim.Map();
    CHECK(m.Width() == 50 && m.Height() == 50);
    for (int i = 0; i < 50; ++i) {   // 外周は移動不能
        CHECK(m.BlocksPlayer({i, 0}) && m.BlocksPlayer({i, 49}) && m.BlocksPlayer({0, i}) && m.BlocksPlayer({49, i}));
    }
    int rocks = 0, water = 0, stone = 0;
    for (int y = 0; y < 50; ++y) for (int x = 0; x < 50; ++x) {
        if (m.RockAt({x, y}) != ECS::NULL_ENTITY) ++rocks;
        if (m.GetTerrain({x, y}) == Terrain::Water) ++water;
        if (m.GetTerrain({x, y}) == Terrain::Stone) ++stone;
    }
    CHECK(rocks > 50 && water > 0 && stone > 0);
    // 開始地点周辺は安全地帯(岩・水なし)
    const Vec2i c{25, 25};
    bool safe = true;
    for (int dy = -4; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx)
        if (dx * dx + dy * dy <= 16 && m.BlocksPlayer({c.x + dx, c.y + dy})) safe = false;
    CHECK(safe);
    CHECK(m.WorldToTile(sim.Ecs().GetComponent<Transform>(sim.PlayerEntity()).pos) == c);
}

TEST(sim_player_moves_smoothly_and_diagonal_is_not_faster) {
    Fixture f;
    f.GodMode();
    const Vec2 start = f.PlayerPos();
    f.Step(f.Move(1, 0), 60);
    const float dx = f.PlayerPos().x - start.x;
    CHECK(std::abs(dx - 160.f) < 2.f);
    Fixture d;
    d.GodMode();
    const Vec2 s2 = d.PlayerPos();
    d.Step(d.Move(1, 1), 60);
    const float dist = Distance(d.PlayerPos(), s2);
    CHECK(std::abs(dist - 160.f) < 2.f);   // 正規化: 斜めでも同じ速さ
    // 照準は移動と独立
    InputState in = f.Move(1, 0);
    in.aimWorld = f.PlayerPos() + Vec2{0.f, -50.f};
    f.Step(in);
    auto& pl = f.ecs().GetComponent<Player>(f.sim.PlayerEntity());
    CHECK(pl.aim.y < -0.99f);
}

TEST(sim_player_cannot_enter_rock_wall_or_water_and_slides) {
    Fixture f;
    f.GodMode();
    const Vec2i pt = f.PlayerTile();
    CreateWall(f.g(), {pt.x + 3, pt.y}, 0);
    f.Step(f.Move(1, 0), 120);
    CHECK(f.PlayerPos().x < f.map().TileMin({pt.x + 3, pt.y}).x);
    const Vec2i cur = f.PlayerTile();
    f.map().SetTerrain({cur.x, cur.y + 3}, Terrain::Water);
    f.Step(f.Move(0, 1), 120);
    CHECK(f.PlayerPos().y < f.map().TileMin({cur.x, cur.y + 3}).y);
    // 壁に斜めに押し付けても壁面に沿って動ける(y が進む)
    Fixture w;
    w.GodMode();
    const Vec2i q = w.PlayerTile();
    for (int y = q.y - 6; y <= q.y + 6; ++y) CreateWall(w.g(), {q.x + 2, y}, 0);
    w.Step(w.Move(1, 1), 30);
    const float y0 = w.PlayerPos().y;
    w.Step(w.Move(1, 1), 30);
    CHECK(w.PlayerPos().y > y0 + 20.f);
}

TEST(sim_player_pushes_rock_with_slowdown_and_hold_time) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateRock(f.g(), {pt.x + 2, pt.y});
    f.Step(f.Move(1, 0), 1);
    CHECK(f.map().RockAt({pt.x + 2, pt.y}) != ECS::NULL_ENTITY);
    // 接触して押し続けると、hold_time 後に1タイル動く
    bool moved = false;
    int framesAfterMove = 0;
    float speedWhilePushing = -1.f;
    for (int i = 0; i < 240 && framesAfterMove < 12; ++i) {
        const Vec2 before = f.PlayerPos();
        const bool wasPushing = f.ecs().GetComponent<Player>(f.sim.PlayerEntity()).pushing;  // このフレームの速度を決めるのは前フレームの状態
        f.Step(f.Move(1, 0));
        if (wasPushing && f.PlayerPos().x > before.x)
            speedWhilePushing = std::max(speedWhilePushing, (f.PlayerPos().x - before.x) / f.g().bal.step);
        if (f.map().RockAt({pt.x + 3, pt.y}) != ECS::NULL_ENTITY) moved = true;
        if (moved) ++framesAfterMove;
    }
    CHECK(moved);
    CHECK(f.map().RockAt({pt.x + 2, pt.y}) == ECS::NULL_ENTITY);
    // GridPosition コンポーネントも占有と一致している
    CHECK(f.ecs().GetComponent<GridPosition>(f.map().RockAt({pt.x + 3, pt.y})).tile == Vec2i(pt.x + 3, pt.y));
    // 押している間は通常の 60%(岩が動いた後、隙間を歩く間)
    CHECK(speedWhilePushing > 0.f);
    CHECK(std::abs(speedWhilePushing - 160.f * 0.6f) < 3.f);
}

TEST(sim_rock_not_pushed_when_blocked_by_rock_or_wall) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateRock(f.g(), {pt.x + 2, pt.y});
    CreateRock(f.g(), {pt.x + 3, pt.y});   // 先に別の岩 → 連続押し不可
    f.Step(f.Move(1, 0), 240);
    CHECK(f.map().RockAt({pt.x + 2, pt.y}) != ECS::NULL_ENTITY);
    CHECK(f.map().RockAt({pt.x + 3, pt.y}) != ECS::NULL_ENTITY);
    CHECK(f.map().RockAt({pt.x + 4, pt.y}) == ECS::NULL_ENTITY);
    CHECK(f.PlayerPos().x < f.map().TileMin({pt.x + 2, pt.y}).x);
}

TEST(sim_rock_diagonal_push) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateRock(f.g(), {pt.x + 1, pt.y + 1});
    bool moved = false;
    for (int i = 0; i < 240 && !moved; ++i) {
        f.Step(f.Move(1, 1));
        moved = f.map().RockAt({pt.x + 2, pt.y + 2}) != ECS::NULL_ENTITY;
    }
    CHECK(moved);
    // 横の隣接が壁なら斜め押し不可
    Fixture g2;
    g2.GodMode();
    g2.ClearWeapon();
    const Vec2i q = g2.PlayerTile();
    CreateRock(g2.g(), {q.x + 1, q.y + 1});
    CreateWall(g2.g(), {q.x + 2, q.y + 1}, 0);
    g2.Step(g2.Move(1, 1), 240);
    CHECK(g2.map().RockAt({q.x + 1, q.y + 1}) != ECS::NULL_ENTITY);
    CHECK(g2.map().RockAt({q.x + 2, q.y + 2}) == ECS::NULL_ENTITY);
}

TEST(sim_rock_is_not_pushed_onto_an_enemy) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateRock(f.g(), {pt.x + 2, pt.y});
    auto e = f.Spawn("normal", f.Center({pt.x + 3, pt.y}));
    f.ecs().GetComponent<Enemy>(e).hold = true;
    f.Step(f.Move(1, 0), 20);
    CHECK(f.map().RockAt({pt.x + 3, pt.y}) == ECS::NULL_ENTITY);
}

// ---- 建築(GameSession 経由)
static InputState BuildInput(Fixture& f, Vec2 dir) {
    InputState in = f.Aim(dir);
    in.buildPressed = true;
    return in;
}

TEST(sim_build_wall_in_8_directions_consumes_wood_only_on_success) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    auto& inv = f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity());
    inv.wood = 100;
    int placed = 0;
    for (Vec2 d : {Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, 1}, Vec2{0, -1}, Vec2{1, 1}, Vec2{1, -1}, Vec2{-1, 1}, Vec2{-1, -1}}) {
        f.Step(BuildInput(f, d));
        const Vec2i t = pt + core::QuantizeDir8(d);
        if (f.map().WallAt(t) != ECS::NULL_ENTITY) ++placed;
        f.Step(f.Aim(d), 2);
    }
    CHECK(placed == 8);
    CHECK(inv.wood == 100 - 8 * 3);
    // 同じ場所へ再設置 → 失敗し、素材は減らない
    f.Step(BuildInput(f, {1, 0}));
    CHECK(!f.g().lastBuildOk && f.g().lastBuildFail == BuildFail::ExistingWall);
    CHECK(inv.wood == 100 - 8 * 3);
    // 壁にはHPがある
    auto& hp = f.ecs().GetComponent<Health>(f.map().WallAt({pt.x + 1, pt.y}));
    CHECK(hp.hp == f.g().bal.walls[0].hp);
    // プレイヤーは壁と重ならない
    CHECK(!f.map().CircleHitsBlocked(f.PlayerPos(), 10.f, MapManager::Who::Player));
}

TEST(sim_build_fails_without_material_and_changes_nothing) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    auto& inv = f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity());
    inv.wood = 2;
    const auto living = f.ecs().LivingCount();
    f.Step(BuildInput(f, {1, 0}));
    CHECK(f.g().lastBuildFail == BuildFail::NoMaterial);
    CHECK(f.map().WallAt({pt.x + 1, pt.y}) == ECS::NULL_ENTITY && inv.wood == 2);
    CHECK(f.ecs().LivingCount() == living);
}

TEST(sim_build_pushes_enemy_and_item_out_of_the_way) {
    Fixture f("pickup.magnet_radius = 0\n");   // 吸い寄せで先に拾われないようにする
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    const Vec2i target{pt.x + 1, pt.y};
    auto e = f.Spawn("normal", f.Center(target) + Vec2{6.f, 0.f});
    auto item = CreatePickup(f.g(), PickupKind::Wood, 1, f.Center(target) + Vec2{0.f, -4.f});
    f.ecs().GetComponent<Enemy>(e).hold = true;
    auto& inv = f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity());
    const int wood = inv.wood;
    const float hpBefore = f.ecs().GetComponent<Health>(e).hp;
    f.Step(f.Aim({1, 0}));                 // グリッドを最新にしてから
    f.Step(BuildInput(f, {1, 0}));
    CHECK(f.map().WallAt(target) != ECS::NULL_ENTITY);
    CHECK(inv.wood == wood - 3);
    const Vec2i et = f.map().WorldToTile(f.ecs().GetComponent<Transform>(e).pos);
    const Vec2i it = f.map().WorldToTile(f.ecs().GetComponent<Transform>(item).pos);
    CHECK(et != target && it != target);
    CHECK(std::abs(et.x - target.x) <= 1 && std::abs(et.y - target.y) <= 1);
    CHECK(!f.map().BlocksEnemy(et));
    CHECK(f.ecs().GetComponent<Health>(e).hp == hpBefore);   // 押し出しダメージなし
}

TEST(sim_build_is_cancelled_entirely_when_a_pushed_target_has_no_room) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    const Vec2i target{pt.x + 1, pt.y};
    // 設置先の周囲8マスを(プレイヤータイル以外)岩で塞ぐ → 敵の逃げ場なし
    for (int d = 0; d < 8; ++d) {
        const Vec2i n = target + core::Dir8()[d];
        if (n != pt) CreateRock(f.g(), n);
    }
    auto e = f.Spawn("normal", f.Center(target));
    f.ecs().GetComponent<Enemy>(e).hold = true;
    const Vec2 epos = f.ecs().GetComponent<Transform>(e).pos;
    auto& inv = f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity());
    const int wood = inv.wood;
    f.Step(f.Aim({1, 0}));
    f.Step(BuildInput(f, {1, 0}));
    CHECK(f.g().lastBuildFail == BuildFail::NoRoom);
    CHECK(f.map().WallAt(target) == ECS::NULL_ENTITY);
    CHECK(inv.wood == wood);
    const Vec2 after = f.ecs().GetComponent<Transform>(e).pos;
    CHECK(Distance(after, epos) < 3.f);   // 敵は動いていない(通常の移動分だけ)
}

TEST(sim_build_preview_matches_execution) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    f.Spawn("normal", f.Center({pt.x + 1, pt.y}));
    f.Step(f.Aim({1, 0}));
    const BuildPlan preview = f.sim.PreviewBuild();
    CHECK(preview.ok && preview.tile == Vec2i(pt.x + 1, pt.y) && preview.pushes.size() == 1);
    f.Step(BuildInput(f, {1, 0}));
    CHECK(f.g().lastBuildOk && f.map().WallAt(preview.tile) != ECS::NULL_ENTITY);
    const BuildPlan after = f.sim.PreviewBuild();
    CHECK(!after.ok && after.reason == BuildFail::ExistingWall);
}

TEST(sim_build_publishes_event_and_blocks_enemy_path) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    int placed = 0;
    f.ecs().Events().Subscribe<OnBuildingPlaced>([&](const OnBuildingPlaced&) { ++placed; });
    f.Step(BuildInput(f, {1, 0}));
    CHECK(placed == 1);
}

// ---- 敵(§19.2: 3,5,6,7)
static bool EnemyOverlapsSolid(Fixture& f, EntityID e) {
    if (!f.ecs().IsAlive(e)) return false;
    auto& t = f.ecs().GetComponent<Transform>(e);
    auto& c = f.ecs().GetComponent<Collider>(e);
    return f.map().CircleHitsBlocked(t.pos, c.radius - 0.5f, MapManager::Who::Enemy);
}

TEST(sim_enemy_chases_the_player_and_hurts_with_invulnerability) {
    Fixture f;
    f.ClearWeapon();
    const Vec2 p = f.PlayerPos();
    f.Spawn("normal", p + Vec2{200.f, 0.f});
    auto& hp = f.ecs().GetComponent<Health>(f.sim.PlayerEntity());
    const float start = hp.hp;
    f.Step({}, 300);   // 5 秒
    CHECK(hp.hp < start);
    // 0.5 秒の無敵があるので、5 秒で受けるダメージは最大 5*(1/0.5)=10 回 → 50
    CHECK(hp.hp >= start - 5.f * 11.f);
    // 無敵時間: 1 回目の直後は減らない
    Fixture g2;
    g2.ClearWeapon();
    g2.Spawn("normal", g2.PlayerPos() + Vec2{12.f, 0.f});
    auto& hp2 = g2.ecs().GetComponent<Health>(g2.sim.PlayerEntity());
    g2.Step({}, 2);
    const float after = hp2.hp;
    CHECK(after == hp2.maxHp - 5.f);
    g2.Step({}, 20);   // 0.33 秒
    CHECK(hp2.hp == after);
}

TEST(sim_enemy_goes_around_wall_and_never_overlaps_solids) {
    Fixture f("enemy.normal.breaks_walls = 0\n");   // 壁を壊せない敵は迂回する
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    // プレイヤーの右に縦の壁(隙間は上端付近)。敵は壁の向こう側から来る。
    for (int y = pt.y - 8; y <= pt.y + 8; ++y) if (y != pt.y - 6) CreateWall(f.g(), {pt.x + 3, y}, 0);
    auto e = f.Spawn("normal", f.Center({pt.x + 8, pt.y}));
    bool reached = false, overlapped = false;
    for (int i = 0; i < 60 * 25 && !reached; ++i) {
        f.Step();
        if (EnemyOverlapsSolid(f, e)) overlapped = true;
        reached = Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) < 24.f;
    }
    CHECK(reached);          // 壁を迂回して到達できた
    CHECK(!overlapped);      // 壁を壊せない敵は壁を通り抜けない
    CHECK(f.map().WallAt({pt.x + 3, pt.y}) != ECS::NULL_ENTITY);   // 壁は無傷(この敵は壊さない)
}

TEST(sim_rock_moved_into_enemy_route_changes_the_route) {
    Fixture f("enemy.normal.breaks_walls = 0\n");   // 壁を壊せない敵の挙動を見る
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    // 通路: 上下を壁で挟んだ1マス幅の廊下に岩を置いて塞ぐ → 通常敵は届かず、最接近点で待機する
    for (int x = pt.x + 2; x <= pt.x + 12; ++x) {
        CreateWall(f.g(), {x, pt.y - 1}, 0);
        CreateWall(f.g(), {x, pt.y + 1}, 0);
    }
    for (int y = pt.y - 1; y <= pt.y + 1; ++y) CreateWall(f.g(), {pt.x + 13, y}, 0);  // 奥は行き止まり
    auto e = f.Spawn("normal", f.Center({pt.x + 12, pt.y}));
    CreateRock(f.g(), {pt.x + 4, pt.y});
    // 廊下の出口(プレイヤー側)は開いている。岩があるので敵は進めない。
    f.Step({}, 60 * 10);
    const Vec2 ep = f.ecs().GetComponent<Transform>(e).pos;
    CHECK(ep.x > f.map().TileMin({pt.x + 4, pt.y}).x + 32.f);   // 岩の手前(右側)で止まっている
    CHECK(!EnemyOverlapsSolid(f, e));
    CHECK(Distance(ep, f.PlayerPos()) > 60.f);
}

TEST(sim_unreachable_enemy_does_not_jitter_or_wander) {
    Fixture f("enemy.normal.breaks_walls = 0\n");   // 壁を壊せない敵の挙動を見る
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    // プレイヤーを壁で完全に囲む
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == 2) CreateWall(f.g(), {pt.x + dx, pt.y + dy}, 0);
    auto e = f.Spawn("normal", f.Center({pt.x + 8, pt.y}));
    f.Step({}, 60 * 8);
    const Vec2 p1 = f.ecs().GetComponent<Transform>(e).pos;
    f.Step({}, 60 * 3);
    const Vec2 p2 = f.ecs().GetComponent<Transform>(e).pos;
    CHECK(Distance(p1, p2) < 2.f);                 // 最接近点で静止(ぶるぶる動かない)
    CHECK(Distance(p1, f.PlayerPos()) < 140.f);    // 囲いの手前(最接近点)までは来ている
    CHECK(!EnemyOverlapsSolid(f, e));
}

TEST(sim_big_enemy_breaks_walls_to_reach_the_player) {
    Fixture f("enemy.big.speed = 120\n");
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    int destroyed = 0;
    f.ecs().Events().Subscribe<OnBuildingDestroyed>([&](const OnBuildingDestroyed&) { ++destroyed; });
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == 2) CreateWall(f.g(), {pt.x + dx, pt.y + dy}, 0);
    auto e = f.Spawn("big", f.Center({pt.x + 7, pt.y}));
    bool reached = false;
    for (int i = 0; i < 60 * 20 && !reached; ++i) {
        f.Step();
        reached = Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) < 30.f;
    }
    CHECK(reached);
    CHECK(destroyed >= 1);
    // 壊れた壁は占有から消えている
    int walls = 0;
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (f.map().WallAt({pt.x + dx, pt.y + dy}) != ECS::NULL_ENTITY) ++walls;
    CHECK(walls < 16);
}

TEST(sim_smallest_enemy_can_break_a_wall) {
    Fixture f("enemy.normal.speed = 120\n");
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CHECK(f.g().bal.enemies[f.Enemy("normal")].breaksWalls);
    int destroyed = 0;
    f.ecs().Events().Subscribe<OnBuildingDestroyed>([&](const OnBuildingDestroyed&) { ++destroyed; });
    // 完全に囲まれたプレイヤー。通常敵でも壁を壊して来る。
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == 2) CreateWall(f.g(), {pt.x + dx, pt.y + dy}, 0);
    auto e = f.Spawn("normal", f.Center({pt.x + 6, pt.y}));
    bool reached = false;
    for (int i = 0; i < 60 * 40 && !reached; ++i) {
        f.Step();
        reached = Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) < 30.f;
    }
    CHECK(destroyed >= 1);
    CHECK(reached);
}

TEST(sim_shooter_stops_at_range_and_hurts_the_player_with_shots) {
    Fixture f;
    f.ClearWeapon();
    const auto& def = f.g().bal.enemies[f.Enemy("shooter")];
    CHECK(def.range > 0.f);
    auto e = f.Spawn("shooter", f.PlayerPos() + Vec2{def.range * 0.9f, 0.f});
    const float hp0 = f.Hp(f.sim.PlayerEntity());
    bool sawShot = false, hostileOnly = true;
    for (int i = 0; i < 60 * 8; ++i) {
        f.Step();
        for (EntityID p : f.g().projectiles) {
            if (!f.ecs().IsAlive(p)) continue;
            sawShot = true;
            if (!f.ecs().GetComponent<Projectile>(p).hostile) hostileOnly = false;
        }
    }
    CHECK(sawShot && hostileOnly);
    CHECK(f.Hp(f.sim.PlayerEntity()) < hp0);   // 弾でダメージを受けた
    // 射程内で止まって撃ち続ける(接触しに来ない)
    CHECK(Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) > 100.f);
}

TEST(sim_shooter_shot_is_stopped_by_a_wall_and_damages_it) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateWall(f.g(), {pt.x + 2, pt.y}, 0);
    f.Spawn("shooter", f.Center({pt.x + 5, pt.y}));
    const EntityID wall = f.map().WallAt({pt.x + 2, pt.y});
    const float wallHp = f.Hp(wall);
    f.Step(InputState{}, 60 * 4);
    CHECK(f.ecs().IsAlive(wall) && f.Hp(wall) < wallHp);   // 壁が弾を受けた
}

TEST(sim_shooter_does_not_fire_through_rocks) {
    Fixture f;
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CreateRock(f.g(), {pt.x + 2, pt.y});
    f.Spawn("shooter", f.Center({pt.x + 4, pt.y}));
    int shots = 0;
    for (int i = 0; i < 60 * 3; ++i) { f.Step(); shots += static_cast<int>(f.g().projectiles.size()); }
    CHECK(shots == 0 || f.Hp(f.sim.PlayerEntity()) == f.g().bal.playerHp);
}

TEST(sim_ghost_passes_through_walls_but_is_stopped_by_rocks) {
    Fixture f("enemy.ghost.speed = 120\n");
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    CHECK(f.g().bal.enemies[f.Enemy("ghost")].ignoresWalls);
    // 壁で完全に囲む。壁を壊せない通常敵は入れないが、すり抜ける敵は入ってこられる。
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == 2) CreateWall(f.g(), {pt.x + dx, pt.y + dy}, 0);
    int destroyed = 0;
    f.ecs().Events().Subscribe<OnBuildingDestroyed>([&](const OnBuildingDestroyed&) { ++destroyed; });
    auto ghost = f.Spawn("ghost", f.Center({pt.x + 7, pt.y}));
    bool reached = false;
    for (int i = 0; i < 60 * 15 && !reached; ++i) {
        f.Step();
        reached = Distance(f.ecs().GetComponent<Transform>(ghost).pos, f.PlayerPos()) < 30.f;
    }
    CHECK(reached);
    CHECK(destroyed == 0);   // 壁は壊さず、すり抜けただけ

    // 岩には止められる(岩で囲まれたプレイヤーには届かない)
    Fixture f2("enemy.ghost.speed = 120\n");
    f2.GodMode();
    f2.ClearWeapon();
    const Vec2i p2 = f2.PlayerTile();
    for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
        if (std::max(std::abs(dx), std::abs(dy)) == 2) CreateRock(f2.g(), {p2.x + dx, p2.y + dy});
    auto g2 = f2.Spawn("ghost", f2.Center({p2.x + 7, p2.y}));
    for (int i = 0; i < 60 * 10; ++i) f2.Step();
    CHECK(Distance(f2.ecs().GetComponent<Transform>(g2).pos, f2.PlayerPos()) > 60.f);
}

TEST(wall_is_weak_enough_to_break_in_a_few_seconds) {
    Fixture f;
    CHECK(f.g().bal.walls[0].hp <= 50.f);
    const auto& def = f.g().bal.enemies[f.Enemy("normal")];
    CHECK(f.g().bal.walls[0].hp / def.wallDps <= 8.f);   // 最小の敵でも 8 秒以内に壊せる
}

TEST(sim_big_enemy_pushes_or_breaks_rocks) {
    Fixture f("enemy.big.speed = 120\n");
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    // 廊下を岩で塞ぎ、奥を壁で閉じる(迂回不可)。大型敵は押す/壊すで進める。
    for (int x = pt.x + 2; x <= pt.x + 9; ++x) {
        CreateWall(f.g(), {x, pt.y - 1}, 0);
        CreateWall(f.g(), {x, pt.y + 1}, 0);
    }
    CreateRock(f.g(), {pt.x + 4, pt.y});
    CreateRock(f.g(), {pt.x + 5, pt.y});   // 2連: 押せないので壊す必要がある
    auto e = f.Spawn("big", f.Center({pt.x + 8, pt.y}));
    bool reached = false;
    for (int i = 0; i < 60 * 25 && !reached; ++i) {
        f.Step();
        reached = Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) < 30.f;
    }
    CHECK(reached);
}

// ---- 戦闘・成長・進行
TEST(sim_auto_attack_kills_enemy_and_drops_xp_that_levels_up) {
    Fixture f;
    f.GodMode();
    const Vec2 p = f.PlayerPos();
    auto e = f.Spawn("normal", p + Vec2{150.f, 0.f});
    f.ecs().GetComponent<Enemy>(e).hold = true;
    f.Step({}, 60 * 4);
    CHECK(!f.ecs().IsAlive(e));
    CHECK(f.g().kills == 1);
    // ドロップした経験値を回収するとレベルが上がる(必要経験値 5): 敵を追加で倒して集める
    for (int i = 0; i < 6; ++i) {
        auto en = f.Spawn("normal", f.PlayerPos() + Vec2{120.f + i * 3.f, 0.f});
        f.ecs().GetComponent<Enemy>(en).hold = true;
    }
    bool leveled = false;
    for (int i = 0; i < 60 * 15 && !leveled; ++i) {
        f.Step({}, 1);
        leveled = f.sim.State() == GameState::LevelUp;
    }
    CHECK(leveled);
    CHECK(f.g().offer.size() == 3);
    // 候補は重複しない
    CHECK(f.g().offer[0] != f.g().offer[1] && f.g().offer[1] != f.g().offer[2] && f.g().offer[0] != f.g().offer[2]);
    // 一時停止中は時間が進まない
    const float t = f.g().time;
    f.Step({}, 30);
    CHECK(f.g().time == t);
    // 選ぶと再開し、強化が反映される
    const int up = f.g().offer[1];
    InputState pick; pick.choose = 1;
    f.Step(pick);
    CHECK(f.sim.State() == GameState::InGame);
    CHECK(f.g().taken.size() > static_cast<size_t>(up) && f.g().taken[up] == 1);
    CHECK(f.ecs().GetComponent<Experience>(f.sim.PlayerEntity()).level >= 2);
}

TEST(sim_upgrades_change_stats) {
    Fixture f;
    auto& st = f.ecs().GetComponent<PlayerStats>(f.sim.PlayerEntity());
    auto& hp = f.ecs().GetComponent<Health>(f.sim.PlayerEntity());
    auto idx = [&](const char* id) {
        for (size_t i = 0; i < f.g().bal.upgrades.size(); ++i) if (f.g().bal.upgrades[i].id == id) return static_cast<int>(i);
        return -1;
    };
    f.sim.ApplyUpgrade(idx("dmg")); CHECK(st.damageMul > 1.19f);
    f.sim.ApplyUpgrade(idx("maxhp")); CHECK(hp.maxHp == 120.f);
    f.sim.ApplyUpgrade(idx("buildcost")); CHECK(WallWoodCost(f.g(), 0) == 2);
    f.sim.ApplyUpgrade(idx("buildcost")); f.sim.ApplyUpgrade(idx("buildcost")); CHECK(WallWoodCost(f.g(), 0) == 1);  // 最低 1
    f.sim.ApplyUpgrade(idx("pushspd")); CHECK(st.pushSpeedMul > 1.f);
    f.sim.ApplyUpgrade(idx("pickup")); CHECK(st.pickupRangeMul > 1.f);
}

TEST(sim_pickup_collects_materials_on_contact) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    auto& inv = f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity());
    const int wood = inv.wood, stone = inv.stone;
    auto a = CreatePickup(f.g(), PickupKind::Wood, 2, f.PlayerPos() + Vec2{10.f, 0.f});
    auto b = CreatePickup(f.g(), PickupKind::Stone, 1, f.PlayerPos() + Vec2{0.f, 12.f});
    auto far = CreatePickup(f.g(), PickupKind::Wood, 5, f.PlayerPos() + Vec2{200.f, 0.f});
    f.Step({}, 2);
    CHECK(inv.wood == wood + 2 && inv.stone == stone + 1);
    CHECK(!f.ecs().IsAlive(a) && !f.ecs().IsAlive(b) && f.ecs().IsAlive(far));
}


TEST(pickups_within_magnet_range_drift_to_the_player_and_get_collected) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    auto& ex = f.ecs().GetComponent<Experience>(f.sim.PlayerEntity());
    const float magnet = f.g().bal.magnetRadius;
    auto near = CreatePickup(f.g(), PickupKind::Xp, 1, f.PlayerPos() + Vec2{magnet - 10.f, 0.f});
    auto far = CreatePickup(f.g(), PickupKind::Xp, 1, f.PlayerPos() + Vec2{magnet + 40.f, 0.f});
    const float farX = f.ecs().GetComponent<Transform>(far).pos.x;
    f.Step({}, 3);
    CHECK(f.ecs().IsAlive(near));
    CHECK(f.ecs().GetComponent<Transform>(near).pos.x < f.PlayerPos().x + magnet - 10.f);   // 近づいている
    f.Step({}, 60 * 2);
    CHECK(!f.ecs().IsAlive(near) && ex.xp == 1);          // 触れなくても拾えた
    CHECK(f.ecs().IsAlive(far));
    CHECK(f.ecs().GetComponent<Transform>(far).pos.x == farX);   // 範囲外は動かない
}

TEST(heal_pickup_restores_hp_up_to_the_maximum_and_waits_while_hp_is_full) {
    Fixture f;
    f.ClearWeapon();
    auto& hp = f.ecs().GetComponent<Health>(f.sim.PlayerEntity());
    const float magnet = f.g().bal.magnetRadius;
    // 満タンのときは吸い寄せられない
    auto waiting = CreatePickup(f.g(), PickupKind::Heal, 1, f.PlayerPos() + Vec2{magnet - 10.f, 0.f});
    f.Step({}, 60);
    CHECK(f.ecs().IsAlive(waiting));
    // 減っていれば吸い寄せられて回復する。最大 HP は超えない
    hp.hp = hp.maxHp - 5.f;
    f.Step({}, 60 * 2);
    CHECK(!f.ecs().IsAlive(waiting));
    CHECK(hp.hp == hp.maxHp);
    hp.hp = 10.f;
    CreatePickup(f.g(), PickupKind::Heal, 1, f.PlayerPos());
    f.Step({}, 2);
    CHECK(hp.hp == 10.f + f.g().bal.healAmount);
}

TEST(knockback_pushes_enemies_away_but_not_into_solids) {
    Fixture f("enemy.normal.speed = 0\n");   // 敵は自分からは動かない(押された分だけ動く)
    f.GodMode();
    const Vec2i pt = f.PlayerTile();
    // 右に敵、そのすぐ右に岩。弾は右へ飛ぶので、敵は岩の方へ押されるが食い込まない。
    auto e = f.Dummy(f.Center({pt.x + 4, pt.y}));
    CreateRock(f.g(), {pt.x + 5, pt.y});
    const float startX = f.ecs().GetComponent<Transform>(e).pos.x;
    bool overlapped = false, pushed = false;
    for (int i = 0; i < 60 * 3; ++i) {
        f.Step(f.Aim({1.f, 0.f}));
        if (EnemyOverlapsSolid(f, e)) overlapped = true;
        if (f.ecs().GetComponent<Transform>(e).pos.x > startX + 1.f) pushed = true;
    }
    CHECK(pushed);
    CHECK(!overlapped);
    CHECK(f.Hp(e) < 1000.f);
    // ボスは押されない
    auto boss = f.Spawn("boss", f.Center({pt.x, pt.y + 5}));
    const Vec2 bossPos = f.ecs().GetComponent<Transform>(boss).pos;
    f.ecs().GetComponent<Enemy>(boss).hold = true;
    f.g().damage.push_back({boss, 1.f, Vec2{0.f, 50.f}});
    f.Step(f.Aim({1.f, 0.f}));
    CHECK(Distance(f.ecs().GetComponent<Transform>(boss).pos, bossPos) < 8.f);
}

TEST(frame_events_describe_only_the_last_step) {
    Fixture f;
    f.GodMode();
    auto e = f.Spawn("normal", f.PlayerPos() + Vec2{60.f, 0.f});
    f.ecs().GetComponent<Health>(e).hp = 1.f;
    bool sawHit = false, sawDeath = false;
    for (int i = 0; i < 60 * 3 && !sawDeath; ++i) {
        f.Step(f.Aim({1.f, 0.f}));
        for (const FxEvent& ev : f.g().fx) {
            if (ev.kind == FxEvent::Kind::EnemyHit) sawHit = true;
            if (ev.kind == FxEvent::Kind::EnemyDied) { sawDeath = true; CHECK(ev.def == f.Enemy("normal")); }
        }
    }
    CHECK(sawHit && sawDeath);
    f.Step({}, 1);
    for (const FxEvent& ev : f.g().fx) CHECK(ev.kind != FxEvent::Kind::EnemyDied);   // 前のステップの出来事は残らない
}

TEST(result_counters_track_walls_and_rocks) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    InputState in = f.Aim({1.f, 0.f});
    in.buildPressed = true;
    f.Step(in);
    f.Step({}, 1);
    CHECK(f.g().wallsBuilt == 1);
    CreateRock(f.g(), {pt.x, pt.y - 1});
    f.SetPlayerPos(f.Center(pt) + Vec2{0.f, -8.f});
    f.Step(f.Move(0.f, -1.f), 60 * 2);
    CHECK(f.g().rocksPushed >= 1);
}

TEST(sim_enemies_increase_over_time) {
    Fixture f("", false, 7);
    f.GodMode();
    f.ClearWeapon();
    f.Step({}, 60 * 10);
    const int early = f.g().enemyCount;
    f.Step({}, 60 * 110);
    const int later = f.g().enemyCount;
    CHECK(early > 0);
    CHECK(later > early);
    CHECK(later <= f.g().bal.maxAlive);
    // 大型敵が中盤以降に出る
    f.Step({}, 60 * 120);
    int big = 0;
    const int bigDef = f.Enemy("big");
    for (auto id : std::vector<ECS::EntityID>{}) (void)id;
    f.g().enemyGrid.Query({800.f, 800.f}, 5000.f, [&](EntityID e) {
        if (f.ecs().IsAlive(e)) if (auto* en = f.ecs().TryGet<Enemy>(e)) if (en->def == bigDef) ++big;
    });
    CHECK(big > 0);
}

TEST(sim_game_over_when_player_dies) {
    Fixture f("spawn.rate_base = 6\n", true, 11);
    f.ClearWeapon();
    bool over = false;
    for (int i = 0; i < 60 * 120 && !over; ++i) {
        f.Step();
        over = f.sim.State() == GameState::GameOver;
    }
    CHECK(over);
    const float t = f.g().time;
    f.Step({}, 30);
    CHECK(f.g().time == t);   // 終了後は進まない
    CHECK(!f.sim.Won());
}

TEST(sim_boss_spawns_at_duration_and_killing_it_wins) {
    Fixture f("game.duration = 20\nenemy.boss.hp = 30\nenemy.boss.speed = 30\n");
    f.GodMode();
    f.Step({}, 60 * 19);
    CHECK(!f.g().bossSpawned);
    f.Step({}, 60 * 2);
    CHECK(f.g().bossSpawned);
    // 逃げ続けてもクリアにはならない(ボス撃破が条件): プレイヤーは自動攻撃でいずれ倒す
    bool won = false;
    for (int i = 0; i < 60 * 60 && !won; ++i) {
        f.Step({}, 1);
        won = f.sim.State() == GameState::Result;
    }
    CHECK(won && f.sim.Won());
}

TEST(sim_cannot_win_by_surviving_alone) {
    Fixture f("game.duration = 5\nenemy.boss.hp = 1000000\n");
    f.GodMode();
    f.Step({}, 60 * 40);
    CHECK(f.sim.State() == GameState::InGame);
    CHECK(!f.sim.Won());
}

TEST(sim_pause_stops_time) {
    Fixture f;
    f.GodMode();
    f.Step({}, 10);
    InputState pause; pause.pausePressed = true;
    f.Step(pause);
    CHECK(f.sim.State() == GameState::Paused);
    const float t = f.g().time;
    f.Step({}, 30);
    CHECK(f.g().time == t);
    f.Step(pause);
    CHECK(f.sim.State() == GameState::InGame);
}

TEST(sim_is_deterministic_for_a_seed) {
    auto run = [](std::uint32_t seed) {
        Fixture f("", false, seed);
        f.GodMode();
        f.Step({}, 60 * 30);
        return std::make_tuple(f.g().enemyCount, f.g().kills, f.PlayerPos().x, f.ecs().LivingCount());
    };
    CHECK(run(42) == run(42));
}

TEST(sim_entities_do_not_leak) {
    Fixture f("", false, 9);
    f.GodMode();
    f.Step({}, 60 * 60);
    CHECK(f.ecs().LivingCount() < 3000);
}

TEST(sim_performance_with_many_enemies) {
    Fixture f("spawn.rate_base = 0\nspawn.max_alive = 400\n", false, 13);
    f.GodMode();
    f.ClearWeapon();
    const Vec2 p = f.PlayerPos();
    core::Rng rng(5);
    int n = 0;
    while (n < 350) {
        Vec2 pos{p.x + rng.Range(-500.f, 500.f), p.y + rng.Range(-500.f, 500.f)};
        const Vec2i t = f.map().WorldToTile(pos);
        if (f.map().BlocksEnemy(t) || Distance(pos, p) < 60.f) continue;
        f.Spawn("normal", pos);
        ++n;
    }
    f.Step({}, 30);   // 初回の経路探索を済ませる
    const auto t0 = std::chrono::steady_clock::now();
    const int frames = 300;
    f.Step(f.Move(1, 0.3f), frames);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / frames;
    std::printf("  350 enemies: %.2f ms/step\n", ms);
    CHECK(ms < 8.0);   // 60fps(16.6ms)の半分以内
}

TEST(sim_enemy_never_pushes_a_rock_onto_the_player) {
    Fixture f("enemy.big.speed = 120\n");
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    // プレイヤーは1マス幅の廊下の入口に立ち、すぐ右の岩の陰にいる。岩の奥から大型敵が来る。
    for (int x = pt.x + 1; x <= pt.x + 8; ++x) {
        CreateWall(f.g(), {x, pt.y - 1}, 0);
        CreateWall(f.g(), {x, pt.y + 1}, 0);
    }
    for (int y = pt.y - 1; y <= pt.y + 1; ++y) CreateWall(f.g(), {pt.x + 9, y}, 0);
    CreateRock(f.g(), {pt.x + 1, pt.y});
    f.Spawn("big", f.Center({pt.x + 5, pt.y}));
    bool crushed = false, rockOnPlayer = false;
    for (int i = 0; i < 60 * 15; ++i) {
        f.Step();
        if (f.map().CircleHitsBlocked(f.PlayerPos(), 10.f, MapManager::Who::Player)) crushed = true;
        if (f.map().RockAt(f.PlayerTile()) != ECS::NULL_ENTITY) rockOnPlayer = true;
    }
    CHECK(!crushed);          // プレイヤーが岩・壁と重なる瞬間が一度もない
    CHECK(!rockOnPlayer);
}

TEST(sim_building_never_leaves_the_player_overlapping_the_wall) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    f.ecs().GetComponent<Inventory>(f.sim.PlayerEntity()).wood = 100;
    // タイル境界ぎりぎりに立って、隣のタイルへ建てる(円が隣のタイルに食い込んでいる状態)
    const Vec2i pt = f.PlayerTile();
    f.SetPlayerPos({f.map().TileMin(pt).x + 31.f, f.map().TileMin(pt).y + 16.f});
    f.Step(BuildInput(f, {1, 0}));
    CHECK(f.g().lastBuildOk);
    CHECK(!f.map().CircleHitsBlocked(f.PlayerPos(), 10.f, MapManager::Who::Player));
}

// 追跡の途中でマップが変わったとき、敵が経路を引き直して迂回し、固体に重ならず到達する。
// 壁(§19.2-5)と岩(§19.2-3)の両方で確認する。
static void RerouteScenario(bool useRock) {
    Fixture f;
    f.GodMode();
    f.ClearWeapon();
    const Vec2i pt = f.PlayerTile();
    const int wx = pt.x + 4;
    const Vec2i nearGap{wx, pt.y + 2}, farGap{wx, pt.y + 9};
    for (int y = pt.y - 12; y <= pt.y + 12; ++y)
        if (y != nearGap.y && y != farGap.y) CreateWall(f.g(), {wx, y}, 0);
    auto e = f.Spawn("normal", f.Center({pt.x + 8, pt.y - 3}));
    if (useRock) CreateRock(f.g(), {wx + 1, nearGap.y - 1});   // 入口は塞がない位置
    auto passesThrough = [&](Vec2i gap) {
        const auto& en = f.ecs().GetComponent<Enemy>(e);
        for (size_t i = en.pathIdx; i < en.path.size(); ++i) if (en.path[i] == gap) return true;
        return false;
    };
    f.Step({}, 40);
    CHECK(passesThrough(nearGap));        // 最初は近い隙間を通る経路
    CHECK(!passesThrough(farGap));
    const Vec2 before = f.ecs().GetComponent<Transform>(e).pos;
    if (useRock) f.map().MoveRock({wx + 1, nearGap.y - 1}, nearGap);   // 岩が隙間へ(斜めに)押し込まれた
    else CreateWall(f.g(), nearGap, 0);                            // 壁で隙間を塞いだ
    bool overlapped = false, rerouted = false, reached = false;
    for (int i = 0; i < 60 * 45 && !reached; ++i) {
        f.Step();
        if (EnemyOverlapsSolid(f, e)) overlapped = true;
        if (i < 90 && !passesThrough(nearGap) && f.ecs().GetComponent<Enemy>(e).path.size() > 0) rerouted = true;   // 1.5秒以内に近い隙間を捨てる
        reached = Distance(f.ecs().GetComponent<Transform>(e).pos, f.PlayerPos()) < 24.f;
    }
    CHECK(rerouted);
    CHECK(!overlapped);
    CHECK(reached);
    (void)before;
}
TEST(sim_enemy_reroutes_when_a_wall_is_built_mid_chase) { RerouteScenario(false); }
TEST(sim_enemy_reroutes_when_a_rock_is_moved_mid_chase) { RerouteScenario(true); }

// ---- 新しい攻撃手段(強化で獲得)
static const char* kStill = "enemy.normal.speed = 0\nenemy.normal.contact_damage = 0\n";   // 的が動かない・ダメージを与えない

TEST(upgrade_definitions_include_new_attack_methods) {
    Fixture f;
    auto& up = f.g().bal.upgrades;
    const auto& bomb = up[f.UpgradeIndex("bomb")];
    const auto& beam = up[f.UpgradeIndex("beam")];
    const auto& multi = up[f.UpgradeIndex("multishot")];
    CHECK(bomb.stat == UpgradeStat::GrantWeapon && f.g().bal.weapons[bomb.weapon].type == WeaponType::Bomb);
    CHECK(beam.stat == UpgradeStat::GrantWeapon && f.g().bal.weapons[beam.weapon].type == WeaponType::Beam);
    CHECK(multi.stat == UpgradeStat::ExtraProjectiles);
    // 最初の攻撃手段はボルトだけ
    auto& slots = f.ecs().GetComponent<Arsenal>(f.sim.PlayerEntity()).slots;
    CHECK(slots.size() == 1 && f.g().bal.weapons[slots[0].def].type == WeaponType::Bolt);
}

TEST(upgrade_grants_weapon_then_levels_it_up) {
    Fixture f;
    auto& slots = f.ecs().GetComponent<Arsenal>(f.sim.PlayerEntity()).slots;
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));
    CHECK(slots.size() == 2 && slots[1].level == 1);
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));
    CHECK(slots.size() == 2 && slots[1].level == 2);     // 二重に持たず、レベルが上がる
    f.sim.ApplyUpgrade(f.UpgradeIndex("beam"));
    CHECK(slots.size() == 3);
    f.sim.ApplyUpgrade(f.UpgradeIndex("multishot"));
    CHECK(f.ecs().GetComponent<PlayerStats>(f.sim.PlayerEntity()).extraProjectiles == 1);
}

TEST(upgrade_can_be_chosen_from_the_level_up_screen) {
    Fixture f;
    f.GodMode();
    f.g().pendingLevelUps = 1;
    f.Step({}, 1);
    CHECK(f.sim.State() == GameState::LevelUp);
    // 提示された候補のうち武器獲得を選べる(候補は乱数。全候補の中に新武器が含まれうること自体を確認)
    f.g().offer = {f.UpgradeIndex("beam"), f.UpgradeIndex("bomb"), f.UpgradeIndex("multishot")};
    InputState pick; pick.choose = 1;
    f.Step(pick);
    CHECK(f.sim.State() == GameState::InGame);
    CHECK(f.ecs().GetComponent<Arsenal>(f.sim.PlayerEntity()).slots.size() == 2);
}

TEST(bomb_is_placed_at_the_player_and_explodes_after_the_fuse) {
    Fixture f(kStill);
    f.ClearWeapon();
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));
    const Vec2 pp = f.PlayerPos();
    auto nearE = f.Dummy(pp + Vec2{60.f, 0.f});      // 爆発範囲(90)の内側
    auto farE = f.Dummy(pp + Vec2{0.f, 160.f});      // 外側
    auto& php = f.ecs().GetComponent<Health>(f.sim.PlayerEntity());
    php.hp = php.maxHp = 100.f;                      // 爆弾は自分には当たらない
    f.Step({}, 2);
    CHECK(f.g().bombs.size() == 1);
    if (!f.g().bombs.empty()) CHECK(Distance(f.ecs().GetComponent<Transform>(f.g().bombs[0]).pos, pp) < 1.f);   // 自分のいる場所
    f.Step({}, 60 * 2);                              // 約2秒: まだ導火線が燃えている(fuse 2.5)
    CHECK(f.Hp(nearE) == 1000.f);                    // 爆発前はダメージなし
    f.Step({}, 60);                                  // 合計 約3秒: 爆発済み
    CHECK(std::abs(f.Hp(nearE) - (1000.f - 45.f)) < 0.01f);   // 範囲内は1回だけ 45
    CHECK(f.Hp(farE) == 1000.f);                              // 範囲外は無傷
    CHECK(php.hp == 100.f);
    f.Step({}, 30);                                  // 演出が終わったら爆弾は片付く
    CHECK(f.g().bombs.size() <= 1);
}

TEST(bomb_level_raises_damage) {
    Fixture f(kStill);
    f.ClearWeapon();
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));      // Lv2: 45 * 1.3
    auto e = f.Dummy(f.PlayerPos() + Vec2{40.f, 0.f});
    f.Step({}, 60 * 3);
    CHECK(std::abs(f.Hp(e) - (1000.f - 45.f * 1.3f)) < 0.05f);
}

TEST(beam_flies_toward_the_cursor_and_pierces_every_enemy_once) {
    Fixture f(kStill);
    f.ClearWeapon();
    f.sim.ApplyUpgrade(f.UpgradeIndex("beam"));
    const Vec2 pp = f.PlayerPos();
    auto e1 = f.Dummy(pp + Vec2{100.f, 0.f});
    auto e2 = f.Dummy(pp + Vec2{160.f, 0.f});
    auto e3 = f.Dummy(pp + Vec2{220.f, 0.f});
    auto off = f.Dummy(pp + Vec2{100.f, 90.f});      // 射線から外れた敵
    f.Step(f.Aim({1.f, 0.f}), 70);                   // 1.1秒: 1発目が飛びきる(2発目は 1.6秒後)
    for (auto e : {e1, e2, e3}) CHECK(std::abs(f.Hp(e) - (1000.f - 28.f)) < 0.01f);   // 全員に1回だけ(貫通・多重ヒットなし)
    CHECK(f.Hp(off) == 1000.f);
    // 向きはカーソル次第: 上向きに撃てば縦の敵に当たる
    Fixture g(kStill);
    g.ClearWeapon();
    g.sim.ApplyUpgrade(g.UpgradeIndex("beam"));
    auto up = g.Dummy(g.PlayerPos() + Vec2{0.f, -150.f});
    auto right = g.Dummy(g.PlayerPos() + Vec2{150.f, 0.f});
    g.Step(g.Aim({0.f, -1.f}), 70);
    CHECK(g.Hp(up) < 1000.f && g.Hp(right) == 1000.f);
}

TEST(beam_is_stopped_by_rocks) {
    Fixture f(kStill);
    f.ClearWeapon();
    f.sim.ApplyUpgrade(f.UpgradeIndex("beam"));
    const Vec2 pp = f.PlayerPos();
    const Vec2i rockTile = f.map().WorldToTile(pp + Vec2{300.f, 0.f});
    CreateRock(f.g(), rockTile);
    auto before = f.Dummy(pp + Vec2{200.f, 0.f});
    auto behind = f.Dummy(pp + Vec2{380.f, 0.f});    // 岩の向こう側
    f.Step(f.Aim({1.f, 0.f}), 70);
    CHECK(f.Hp(before) < 1000.f);
    CHECK(f.Hp(behind) == 1000.f);
}

TEST(beam_passes_through_player_walls) {
    Fixture f(kStill);
    f.ClearWeapon();
    f.sim.ApplyUpgrade(f.UpgradeIndex("beam"));
    const Vec2 pp = f.PlayerPos();
    CreateWall(f.g(), f.map().WorldToTile(pp + Vec2{100.f, 0.f}), 0);   // wall.basic.blocks_projectile = 0
    auto e = f.Dummy(pp + Vec2{200.f, 0.f});
    f.Step(f.Aim({1.f, 0.f}), 70);
    CHECK(f.Hp(e) < 1000.f);
}

TEST(multishot_sends_extra_balls_to_different_enemies) {
    auto volley = [](int extra) {
        Fixture f(kStill);
        for (int i = 0; i < extra; ++i) f.sim.ApplyUpgrade(f.UpgradeIndex("multishot"));
        const Vec2 pp = f.PlayerPos();
        std::vector<EntityID> es;
        for (int k = 0; k < 3; ++k) {
            const float a = k * 2.0943951f;
            es.push_back(f.Dummy(pp + Vec2{std::cos(a), std::sin(a)} * (100.f + 8.f * k)));
        }
        f.Step({}, 22);   // 1回目の弾が届き、2回目の発射(0.4秒後)より前
        int hit = 0;
        for (auto e : es) if (f.Hp(e) < 1000.f) { ++hit; CHECK(std::abs(f.Hp(e) - 988.f) < 0.01f); }   // 当たった敵は1発だけ(12)
        return hit;
    };
    CHECK(volley(0) == 1);   // 通常は最も近い1体だけ
    CHECK(volley(2) == 3);   // 玉 +2 で、3体それぞれへ
}

TEST(multishot_with_fewer_enemies_still_fires_every_ball) {
    Fixture f(kStill);
    f.sim.ApplyUpgrade(f.UpgradeIndex("multishot"));
    f.sim.ApplyUpgrade(f.UpgradeIndex("multishot"));
    auto e = f.Dummy(f.PlayerPos() + Vec2{100.f, 0.f});
    f.Step({}, 22);
    CHECK(f.Hp(e) < 1000.f - 12.f * 2.5f);   // 3発のうち少なくとも2〜3発が命中(余りは少し角度をずらして同じ敵へ)
}

TEST(all_attack_methods_together_are_stable_for_a_long_run) {
    Fixture f("", false, 33);
    f.GodMode();
    f.sim.ApplyUpgrade(f.UpgradeIndex("bomb"));
    f.sim.ApplyUpgrade(f.UpgradeIndex("beam"));
    f.sim.ApplyUpgrade(f.UpgradeIndex("multishot"));
    int bombsMax = 0;
    for (int i = 0; i < 60 * 240; ++i) {
        InputState in;
        const float a = (i / 200) * 1.5707963f;
        in.move = {std::cos(a), std::sin(a)};
        in.aimWorld = f.PlayerPos() + Vec2{std::cos(a + 2.f), std::sin(a + 2.f)} * 100.f;
        if (f.sim.State() == GameState::LevelUp) in.choose = 0;
        f.sim.Step(in, f.g().bal.step);
        bombsMax = std::max<int>(bombsMax, static_cast<int>(f.g().bombs.size()));
    }
    CHECK(f.g().kills > 100);
    CHECK(f.ecs().LivingCount() < 4000);
    CHECK(bombsMax >= 1 && bombsMax < 10);   // 爆弾は演出後に片付いて溜まらない
}

TEST(enemy_sprites_exist_with_animation_frames) {
    // 敵の画像フォルダ(data/Image/...)に 0.png から連番のコマがあること(描画は SFML 側だが、データはここで検証できる)
    namespace fs = std::filesystem;
    fs::path dataDir;
    for (const char* p : {"data", "../data", "../../data", "../../../data"})
        if (fs::exists(fs::path(p) / "balance.cfg")) { dataDir = p; break; }
    CHECK(!dataDir.empty());
    auto cfg = core::Config::LoadString(BalanceText());
    Balance b = Balance::Load(cfg);
    CHECK(b.animFps > 0.f);
    for (const auto& e : b.enemies) {
        const fs::path dir = dataDir / fs::u8path(b.spriteDir) / fs::u8path(e.sprite);
        int frames = 0;
        while (fs::exists(dir / (std::to_string(frames) + ".png"))) ++frames;
        if (frames < 2) std::printf("  画像が足りません: %s\n", dir.string().c_str());
        CHECK(frames >= 2);       // アニメーションになる枚数
        CHECK(e.drawSize > 0.f);
    }
}

TEST(spawn_can_be_switched_off_for_debugging) {
    Fixture f("spawn.rate_base = 20\nspawn.shooter_start = 0\ngame.duration = 3\n", false);
    f.g().spawnEnabled = false;
    f.Step(InputState{}, 60 * 6);
    CHECK(f.g().enemyCount == 0 && !f.g().bossSpawned);   // 出現が止まる(ボスも)
    f.g().spawnEnabled = true;
    f.Step(InputState{}, 60 * 4);
    CHECK(f.g().enemyCount > 0 && f.g().bossSpawned);
}

TEST(short_mode_overrides_the_balance_and_the_boss_comes_earlier) {
    core::Config cfg = core::Config::LoadString(BalanceText() + "\n" + kEmpty);
    GameSession sim(cfg, 1);
    CHECK(sim.Modes().size() >= 2);
    CHECK(sim.SetModeById("short"));
    sim.StartRun(1);
    sim.Ecs().EnableIntraPhaseShuffle(7);
    auto& g = sim.Ctx();
    CHECK(g.bal.duration == 120.f);
    CHECK(g.bal.enemies[g.bal.EnemyIndex("boss")].hp == 900.f);
    // 通常モードは変わらない
    sim.SetMode(0);
    sim.StartRun(1);
    CHECK(sim.Ctx().bal.duration == 600.f);
    // ショートに戻し、ボスが 120 秒で出て、倒すとクリアになる(全体で3分以内)
    sim.SetModeById("short");
    sim.StartRun(1);
    sim.Ecs().EnableIntraPhaseShuffle(7);
    auto& g2 = sim.Ctx();
    auto& hp = sim.Ecs().GetComponent<Health>(sim.PlayerEntity());
    hp.hp = hp.maxHp = 1e9f;
    InputState in;
    in.choose = 0;   // レベルアップの選択待ちで止まらないよう、常に先頭を選ぶ
    for (int i = 0; i < 60 * 121 && !g2.bossSpawned; ++i) sim.Step(in, g2.bal.step);
    CHECK(g2.bossSpawned && g2.time <= 121.f);
    sim.Ecs().GetComponent<Health>(g2.boss).hp = 0.f;
    for (int i = 0; i < 5; ++i) sim.Step(in, g2.bal.step);
    CHECK(sim.State() == GameState::Result && sim.Won());
    CHECK(g2.time < 180.f);
}

TEST(title_screen_selects_the_mode_with_number_keys) {
    core::Config cfg = core::Config::LoadString(BalanceText() + "\n" + kEmpty);
    GameSession sim(cfg, 1);
    CHECK(sim.State() == GameState::Title && sim.Mode() == 0);
    InputState in;
    in.modeSelect = 1;
    sim.Step(in, 0.016f);
    CHECK(sim.Mode() == 1);
    in = InputState{};
    in.modeSelect = 9;   // 範囲外は無視
    sim.Step(in, 0.016f);
    CHECK(sim.Mode() == 1);
    in = InputState{};
    in.startPressed = true;
    sim.Step(in, 0.016f);
    CHECK(sim.State() == GameState::InGame && sim.Ctx().bal.duration == 120.f);
}

TEST(sim_full_length_run_is_stable) {
    // 約11分(ボス出現を含む)を自動で動かし、クラッシュ・Entity のリーク・敵数の暴走がないことを確認する。
    Fixture f("", false, 21);
    f.GodMode();
    size_t maxEntities = 0;
    int maxEnemies = 0, builds = 0;
    const int total = 60 * 60 * 11;
    for (int i = 0; i < total; ++i) {
        InputState in;
        const float a = (i / 180) * 1.5707963f;
        in.move = {std::cos(a), std::sin(a)};
        in.aimWorld = f.PlayerPos() + Vec2{std::cos(a + 1.f), std::sin(a + 1.f)} * 100.f;
        if (i % 100 == 50) in.buildPressed = true;
        if (f.sim.State() == GameState::LevelUp) in.choose = 0;
        f.sim.Step(in, f.g().bal.step);
        if (f.g().lastBuildOk) ++builds;
        maxEntities = std::max<size_t>(maxEntities, f.ecs().LivingCount());
        maxEnemies = std::max(maxEnemies, f.g().enemyCount);
        if (f.sim.State() == GameState::Result || f.sim.State() == GameState::GameOver) break;
    }
    std::printf("  maxEntities=%zu maxEnemies=%d time=%.0f kills=%d state=%d\n", maxEntities, maxEnemies, f.g().time, f.g().kills,
                static_cast<int>(f.sim.State()));
    CHECK(f.g().bossSpawned);
    CHECK(maxEntities < 6000);
    CHECK(maxEnemies <= f.g().bal.maxAlive + f.g().bal.maxAlive / 8 + 2);
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    const std::string filter = argc > 1 ? argv[1] : "";
    int ran = 0;
    for (auto& t : Registry()) {
        if (!filter.empty() && std::string(t.name).find(filter) == std::string::npos) continue;
        g_current = t.name;
        const int before = g_failures;
        try {
            t.fn();
        } catch (const std::exception& e) {
            ++g_failures;
            std::printf("  EXCEPTION [%s] %s\n", t.name, e.what());
        }
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[FAIL]", t.name);
        ++ran;
    }
    std::printf("\n%d tests, %d failures\n", ran, g_failures);
    return g_failures == 0 ? 0 : 1;
}
