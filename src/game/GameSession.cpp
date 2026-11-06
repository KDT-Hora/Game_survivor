#include "game/GameSession.h"

#include <algorithm>
#include <queue>

#include "game/Systems.h"

namespace game {

namespace {
float TileSizeOf(const core::Config& c) { return c.F("tile.size"); }
}  // namespace

GameSession::World::World(const core::Config& cfg, std::uint32_t seed)
    : ecs(), map(cfg.I("map.width"), cfg.I("map.height"), TileSizeOf(cfg), cfg, ecs.Events()), ctx(cfg, ecs, map, seed) {}

GameSession::GameSession(const core::Config& c, std::uint32_t seed) : cfg(c) {
    for (const std::string& id : cfg.List("modes"))
        modes.push_back({id, cfg.S("mode." + id + ".label"), cfg.S("mode." + id + ".desc")});
    Build(seed);
}

bool GameSession::SetModeById(const std::string& id) {
    for (size_t i = 0; i < modes.size(); ++i)
        if (modes[i].id == id) { mode = static_cast<int>(i); return true; }
    return false;
}

void GameSession::Build(std::uint32_t seed) {
    // 選んだモードの設定(`mode.<id>.<キー> = 値`)で、基本の設定を上書きする。
    activeCfg = cfg;
    if (mode >= 0 && mode < static_cast<int>(modes.size()))
        for (const auto& kv : cfg.WithPrefix("mode." + modes[mode].id + "."))
            if (kv.first != "label" && kv.first != "desc") activeCfg.Set(kv.first, kv.second);
    world = std::make_unique<World>(activeCfg, seed);
    auto& ecs = world->ecs;
    ecs.RegisterComponent<Transform>();
    ecs.RegisterComponent<Collider>();
    ecs.RegisterComponent<Health>();
    ecs.RegisterComponent<GridPosition>();
    ecs.RegisterComponent<RockTag>();
    ecs.RegisterComponent<Building>();
    ecs.RegisterComponent<Player>();
    ecs.RegisterComponent<PlayerStats>();
    ecs.RegisterComponent<Inventory>();
    ecs.RegisterComponent<Experience>();
    ecs.RegisterComponent<Arsenal>();
    ecs.RegisterComponent<Bomb>();
    ecs.RegisterComponent<Enemy>();
    ecs.RegisterComponent<Pickup>();
    ecs.RegisterComponent<Projectile>();
    RegisterSystems(world->ctx);
    GenerateMap(world->ctx);
    won = false;
}

void GameSession::StartRun(std::uint32_t seed) {
    Build(seed);
    world->ctx.state = GameState::InGame;
}

BuildPlan GameSession::PreviewBuild() { return ComputeBuildPlan(world->ctx); }

void GameSession::ApplyUpgrade(int idx) {
    auto& g = world->ctx;
    const UpgradeDef& u = g.bal.upgrades[idx];
    auto& st = g.ecs.GetComponent<PlayerStats>(g.player);
    auto& hp = g.ecs.GetComponent<Health>(g.player);
    switch (u.stat) {
        case UpgradeStat::Damage: st.damageMul += u.amount; break;
        case UpgradeStat::AttackSpeed: st.attackSpeedMul += u.amount; break;
        case UpgradeStat::MaxHp: hp.maxHp += u.amount; hp.hp += u.amount; break;
        case UpgradeStat::DamageReduction: st.damageReduction = std::min(0.75f, st.damageReduction + u.amount); break;
        case UpgradeStat::MoveSpeed: st.moveSpeedMul += u.amount; break;
        case UpgradeStat::WallHp: st.wallHpMul += u.amount; break;
        case UpgradeStat::BuildCost: st.buildCostReduction += static_cast<int>(u.amount); break;
        case UpgradeStat::PushSpeed: st.pushSpeedMul += u.amount; break;
        case UpgradeStat::PickupRange: st.pickupRangeMul += u.amount; break;
        case UpgradeStat::ExtraProjectiles: st.extraProjectiles += static_cast<int>(u.amount); break;
        case UpgradeStat::GrantWeapon: {   // 新しい攻撃手段を獲得。すでに持っていれば +1 レベル
            auto& ars = g.ecs.GetComponent<Arsenal>(g.player);
            auto it = std::find_if(ars.slots.begin(), ars.slots.end(), [&](const WeaponSlot& w) { return w.def == u.weapon; });
            if (it != ars.slots.end()) ++it->level;
            else ars.slots.push_back({u.weapon, 1, 0.f});
            break;
        }
    }
    if (g.taken.size() < g.bal.upgrades.size()) g.taken.resize(g.bal.upgrades.size(), 0);
    ++g.taken[idx];
}

void GameSession::MakeOffer() {
    auto& g = world->ctx;
    std::vector<int> pool;
    for (size_t i = 0; i < g.bal.upgrades.size(); ++i) pool.push_back(static_cast<int>(i));
    g.offer.clear();
    const int n = std::min<int>(g.bal.levelChoices, static_cast<int>(pool.size()));
    for (int i = 0; i < n; ++i) {
        const int k = g.rng.Int(0, static_cast<int>(pool.size()) - 1);
        g.offer.push_back(pool[k]);
        pool.erase(pool.begin() + k);
    }
}

void GameSession::RunFrame(float dt) {
    auto& g = world->ctx;
    auto& ecs = world->ecs;
    ecs.Update(dt);
    ecs.Flush();
    g.time += dt;

    // 勝敗判定
    if (ecs.GetComponent<Health>(g.player).hp <= 0.f) {
        g.state = GameState::GameOver;
        return;
    }
    if (g.bossKilled) {
        won = true;
        g.state = GameState::Result;
        return;
    }
    if (g.pendingLevelUps > 0) {  // レベルアップ時はゲームを一時停止(§11.2)
        MakeOffer();
        g.state = GameState::LevelUp;
    }
}

void GameSession::Step(const InputState& in, float dt) {
    auto& g = world->ctx;
    g.input = in;
    switch (g.state) {
        case GameState::Title:
            SetMode(in.modeSelect);   // 数字キーでモードを選ぶ
            if (in.startPressed) StartRun(g.rng.Next());
            break;
        case GameState::InGame:
            if (in.pausePressed) { g.state = GameState::Paused; break; }
            RunFrame(dt);
            break;
        case GameState::Paused:
            if (in.pausePressed) g.state = GameState::InGame;
            break;
        case GameState::LevelUp:
            if (in.choose >= 0 && in.choose < static_cast<int>(g.offer.size())) {
                ApplyUpgrade(g.offer[in.choose]);
                --g.pendingLevelUps;
                if (g.pendingLevelUps > 0) MakeOffer();
                else { g.offer.clear(); g.state = GameState::InGame; }
            }
            break;
        case GameState::GameOver:
        case GameState::Result:
            if (in.startPressed) { Build(g.rng.Next()); world->ctx.state = GameState::Title; }
            break;
    }
}

// ------------------------------------------------------------ マップ生成
static void GrowCluster(GameContext& g, Vec2i seed, int size, Terrain t, Vec2i safeCenter) {
    std::vector<Vec2i> frontier{seed};
    for (int i = 0; i < size && !frontier.empty(); ++i) {
        const int k = g.rng.Int(0, static_cast<int>(frontier.size()) - 1);
        const Vec2i c = frontier[k];
        const int dx = c.x - safeCenter.x, dy = c.y - safeCenter.y;
        if (g.map.InBounds(c) && g.map.GetTerrain(c) == Terrain::Grass && dx * dx + dy * dy > (g.bal.safeRadius + 1) * (g.bal.safeRadius + 1)
            && c.x > 1 && c.y > 1 && c.x < g.map.Width() - 2 && c.y < g.map.Height() - 2)
            g.map.SetTerrain(c, t);
        for (int d = 0; d < 4; ++d) frontier.push_back(c + core::Dir8()[d]);
    }
}

void GenerateMap(GameContext& g) {
    MapManager& m = g.map;
    const Vec2i center{m.Width() / 2, m.Height() / 2};
    auto interior = [&]() { return Vec2i{g.rng.Int(1, m.Width() - 2), g.rng.Int(1, m.Height() - 2)}; };
    auto farFromStart = [&](Vec2i t) {
        const int dx = t.x - center.x, dy = t.y - center.y;
        return dx * dx + dy * dy > g.bal.safeRadius * g.bal.safeRadius;  // 開始地点周辺は安全地帯(§4.4)
    };

    for (int i = 0; i < g.bal.waterClusters; ++i) GrowCluster(g, interior(), g.bal.clusterSize, Terrain::Water, center);
    for (int i = 0; i < g.bal.stoneClusters; ++i) GrowCluster(g, interior(), g.bal.clusterSize, Terrain::Stone, center);

    int placed = 0;
    for (int tries = 0; placed < g.bal.rockCount && tries < g.bal.rockCount * 20; ++tries) {
        const Vec2i t = interior();
        if (!farFromStart(t) || m.BlocksPlayer(t) || m.GetTerrain(t) == Terrain::Water) continue;
        CreateRock(g, t);
        ++placed;
    }

    // 開始地点から到達できない空きマス(水や岩で囲まれた飛び地)は水にして、敵の出現・素材配置に使わない。
    std::vector<char> seen(static_cast<size_t>(m.Width()) * m.Height(), 0);
    std::queue<Vec2i> q;
    q.push(center);
    seen[center.y * m.Width() + center.x] = 1;
    while (!q.empty()) {
        const Vec2i c = q.front(); q.pop();
        for (int d = 0; d < 4; ++d) {
            const Vec2i n = c + core::Dir8()[d];
            if (!m.InBounds(n) || seen[n.y * m.Width() + n.x] || m.BlocksPlayer(n)) continue;
            seen[n.y * m.Width() + n.x] = 1;
            q.push(n);
        }
    }
    std::vector<Vec2i> reachable;
    for (int y = 0; y < m.Height(); ++y)
        for (int x = 0; x < m.Width(); ++x) {
            const Vec2i t{x, y};
            if (seen[y * m.Width() + x]) { reachable.push_back(t); continue; }
            if (!m.BlocksPlayer(t)) m.SetTerrain(t, Terrain::Water);
        }

    g.player = CreatePlayer(g, m.TileCenter(center));

    // マップ上の素材(§9.2)
    for (int i = 0; i < g.bal.resourceCount && !reachable.empty(); ++i) {
        const Vec2i t = reachable[g.rng.Int(0, static_cast<int>(reachable.size()) - 1)];
        const bool wood = g.rng.Chance(0.7f);
        CreatePickup(g, wood ? PickupKind::Wood : PickupKind::Stone, wood ? 2 : 1, m.TileCenter(t));
    }
}

}  // namespace game
