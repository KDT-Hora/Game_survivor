#include "game/GameContext.h"

#include <algorithm>
#include <cmath>

namespace game {

// ---------------------------------------------------------------- SpatialGrid
void SpatialGrid::Reset(float cellSize, float worldW, float worldH) {
    cell = cellSize;
    cols = std::max(1, static_cast<int>(std::ceil(worldW / cell)));
    rows = std::max(1, static_cast<int>(std::ceil(worldH / cell)));
    const size_t n = static_cast<size_t>(cols) * rows;
    if (cells.size() != n) cells.assign(n, {});
    else for (auto& c : cells) c.clear();
}

void SpatialGrid::Insert(EntityID e, Vec2 p) {
    const int x = Clamp(static_cast<int>(p.x / cell), cols), y = Clamp(static_cast<int>(p.y / cell), rows);
    cells[static_cast<size_t>(y) * cols + x].push_back(e);
}

// ------------------------------------------------------------------- 衝突
Vec2 MoveAndSlide(const MapManager& map, Vec2 pos, Vec2 delta, float r, MapManager::Who who) {
    // 軸ごとに解決し、壁面に沿って滑る(§3.1, §5.2)。1ステップの移動量は半径より十分小さい前提。
    Vec2 p = pos;
    if (delta.x != 0.f) {
        Vec2 q{p.x + delta.x, p.y};
        if (!map.CircleHitsBlocked(q, r, who)) p = q;
    }
    if (delta.y != 0.f) {
        Vec2 q{p.x, p.y + delta.y};
        if (!map.CircleHitsBlocked(q, r, who)) p = q;
    }
    return p;
}

Vec2 Depenetrate(const MapManager& map, Vec2 pos, float r, MapManager::Who who) {
    // 壁が円に重なって置かれた/押し出された場合に、最も近い辺から押し戻す。
    const float T = map.TileSize();
    for (int iter = 0; iter < 4; ++iter) {
        bool moved = false;
        const Vec2i lo = map.WorldToTile({pos.x - r, pos.y - r});
        const Vec2i hi = map.WorldToTile({pos.x + r, pos.y + r});
        for (int ty = lo.y; ty <= hi.y; ++ty) {
            for (int tx = lo.x; tx <= hi.x; ++tx) {
                bool blocked = who == MapManager::Who::Player ? map.BlocksPlayer({tx, ty}) : map.BlocksEnemy({tx, ty});
                if (!blocked) continue;
                const Vec2 mn = map.TileMin({tx, ty});
                const float cx = std::clamp(pos.x, mn.x, mn.x + T);
                const float cy = std::clamp(pos.y, mn.y, mn.y + T);
                float dx = pos.x - cx, dy = pos.y - cy;
                const float d2 = dx * dx + dy * dy;
                if (d2 >= r * r) continue;
                if (d2 > 1e-6f) {
                    const float d = std::sqrt(d2);
                    pos.x += dx / d * (r - d + 0.01f);
                    pos.y += dy / d * (r - d + 0.01f);
                } else {
                    // 中心がタイル内部: 最も近い辺から出す
                    const float l = pos.x - mn.x, rr = mn.x + T - pos.x, t = pos.y - mn.y, b = mn.y + T - pos.y;
                    const float m = std::min({l, rr, t, b});
                    if (m == l) pos.x = mn.x - r - 0.01f;
                    else if (m == rr) pos.x = mn.x + T + r + 0.01f;
                    else if (m == t) pos.y = mn.y - r - 0.01f;
                    else pos.y = mn.y + T + r + 0.01f;
                }
                moved = true;
            }
        }
        if (!moved) break;
    }
    return pos;
}

bool CircleOverlapsTile(const MapManager& map, Vec2 c, float r, Vec2i tile) {
    const Vec2 mn = map.TileMin(tile);
    const float T = map.TileSize();
    const float cx = std::clamp(c.x, mn.x, mn.x + T), cy = std::clamp(c.y, mn.y, mn.y + T);
    const float dx = c.x - cx, dy = c.y - cy;
    return dx * dx + dy * dy < r * r;
}

bool EnemyOverlapsTile(GameContext& g, Vec2i tile) {
    bool hit = false;
    const Vec2 c = g.map.TileCenter(tile);
    g.enemyGrid.Query(c, g.map.TileSize(), [&](EntityID e) {
        if (hit || !g.ecs.IsAlive(e)) return;
        auto* t = g.ecs.TryGet<Transform>(e);
        auto* col = g.ecs.TryGet<Collider>(e);
        if (t && col && CircleOverlapsTile(g.map, t->pos, col->radius, tile)) hit = true;
    });
    return hit;
}

bool PlayerOverlapsTile(GameContext& g, Vec2i tile) {
    auto* t = g.ecs.TryGet<Transform>(g.player);
    auto* c = g.ecs.TryGet<Collider>(g.player);
    return t && c && CircleOverlapsTile(g.map, t->pos, c->radius, tile);
}

// ------------------------------------------------------------ ファクトリ
EntityID CreatePlayer(GameContext& g, Vec2 pos) {
    auto& e = g.ecs;
    EntityID id = e.CreateEntity();
    e.AddComponent(id, Transform{pos});
    e.AddComponent(id, Collider{g.bal.playerRadius});
    e.AddComponent(id, Health{g.bal.playerHp, g.bal.playerHp, 0.f});
    e.AddComponent(id, Player{});
    e.AddComponent(id, PlayerStats{});
    Inventory inv;
    inv.wood = g.bal.startWood;
    inv.stone = g.bal.startStone;
    e.AddComponent(id, inv);
    Experience ex;
    ex.next = g.bal.xpBase;
    e.AddComponent(id, ex);
    Arsenal arsenal;
    arsenal.slots.push_back({0, 1, 0.f});   // 最初の攻撃手段は weapons の先頭(ボルト)
    e.AddComponent(id, arsenal);
    return id;
}

EntityID CreateRock(GameContext& g, Vec2i tile) {
    EntityID id = g.ecs.CreateEntity();
    g.ecs.AddComponent(id, GridPosition{tile});
    g.ecs.AddComponent(id, RockTag{});
    g.map.PlaceRock(id, tile);
    return id;
}

EntityID CreateWall(GameContext& g, Vec2i tile, int def) {
    EntityID id = g.ecs.CreateEntity();
    auto* stats = g.ecs.TryGet<PlayerStats>(g.player);
    const float mul = stats ? stats->wallHpMul : 1.f;
    const float hp = g.bal.walls[def].hp * mul;
    g.ecs.AddComponent(id, GridPosition{tile});
    g.ecs.AddComponent(id, Building{def});
    g.ecs.AddComponent(id, Health{hp, hp, 0.f});
    g.map.PlaceWall(id, tile);
    return id;
}

EntityID CreateEnemy(GameContext& g, int def, Vec2 pos) {
    const EnemyDef& d = g.bal.enemies[def];
    const float hpScale = 1.f + g.bal.hpGrowthPerMin * (g.time / 60.f);
    const float hp = def == g.bal.EnemyIndex("boss") ? d.hp : d.hp * hpScale;
    EntityID id = g.ecs.CreateEntity();
    g.ecs.AddComponent(id, Transform{pos});
    g.ecs.AddComponent(id, Collider{d.radius});
    g.ecs.AddComponent(id, Health{hp, hp, 0.f});
    Enemy en;
    en.def = def;
    en.target = pos;
    en.shotTimer = g.rng.Range(0.f, d.shotCooldown);   // 撃つタイミングをずらす
    en.repathTimer = g.rng.Range(0.f, g.bal.repathInterval);  // 探索タイミングをずらす(§8.2)
    g.ecs.AddComponent(id, en);
    return id;
}

EntityID CreatePickup(GameContext& g, PickupKind kind, int amount, Vec2 pos) {
    EntityID id = g.ecs.CreateEntity();
    g.ecs.AddComponent(id, Transform{pos});
    g.ecs.AddComponent(id, Pickup{kind, amount});
    return id;
}

EntityID CreateProjectile(GameContext& g, Vec2 pos, Vec2 vel, float damage, float life, float radius, bool pierce, int kind, bool hostile) {
    EntityID id = g.ecs.CreateEntity();
    g.ecs.AddComponent(id, Transform{pos});
    g.ecs.AddComponent(id, Collider{radius});
    Projectile p;
    p.vel = vel;
    p.damage = damage;
    p.life = life;
    p.pierce = pierce;
    p.kind = kind;
    p.hostile = hostile;
    g.ecs.AddComponent(id, p);
    return id;
}

EntityID CreateBomb(GameContext& g, Vec2 pos, float fuse, float blastRadius, float damage, float flash) {
    EntityID id = g.ecs.CreateEntity();
    g.ecs.AddComponent(id, Transform{pos});
    Bomb b;
    b.fuse = fuse;
    b.blastRadius = blastRadius;
    b.damage = damage;
    b.flash = b.flashTotal = flash;
    g.ecs.AddComponent(id, b);
    return id;
}

// ----------------------------------------------------------------- 建築
int WallWoodCost(GameContext& g, int wallDef) {
    const int base = g.bal.walls[wallDef].costWood;
    int reduction = 0;
    if (auto* s = g.ecs.TryGet<PlayerStats>(g.player))
        reduction = s->buildCostReduction;
    return base <= 0 ? 0 : std::max(1, base - reduction);
}

BuildPlan ComputeBuildPlan(GameContext& g) {
    auto& ecs = g.ecs;
    const auto& pl = ecs.GetComponent<Player>(g.player);
    const Vec2 ppos = ecs.GetComponent<Transform>(g.player).pos;
    const auto& inv = ecs.GetComponent<Inventory>(g.player);
    const Vec2i ptile = g.map.WorldToTile(ppos);
    const Vec2i dir = core::QuantizeDir8(pl.aim);
    const Vec2i target = ptile + dir;

    // 設置先にいる敵・アイテムを集める(円とタイルの重なりで判定)
    std::vector<Occupant> occ;
    if (g.map.InBounds(target)) {
        const Vec2 c = g.map.TileCenter(target);
        const float q = g.map.TileSize() * 1.2f;
        g.enemyGrid.Query(c, q, [&](EntityID e) {
            auto* t = ecs.TryGet<Transform>(e);
            auto* col = ecs.TryGet<Collider>(e);
            if (t && col && CircleOverlapsTile(g.map, t->pos, col->radius, target)) occ.push_back({e, t->pos, true});
        });
        g.pickupGrid.Query(c, q, [&](EntityID e) {
            auto* t = ecs.TryGet<Transform>(e);
            if (t && g.map.WorldToTile(t->pos) == target) occ.push_back({e, t->pos, false});
        });
        std::sort(occ.begin(), occ.end(), [](const Occupant& a, const Occupant& b) { return a.id < b.id; });
    }
    const int wallDef = pl.selectedWall;
    const bool enough = inv.wood >= WallWoodCost(g, wallDef) && inv.stone >= g.bal.walls[wallDef].costStone;
    return PlanWallPlacement(g.map, ptile, pl.aim, occ, enough);
}

}  // namespace game
