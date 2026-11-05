#include "game/MapManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace game {

static TerrainProps LoadProps(const core::Config& c, const std::string& name) {
    const std::string p = "terrain." + name + ".";
    TerrainProps t;
    t.walkable = c.B(p + "walkable");
    t.blocksPlayer = c.B(p + "blocks_player");
    t.blocksEnemy = c.B(p + "blocks_enemy");
    t.blocksProjectile = c.B(p + "blocks_projectile");
    t.blocksBuilding = c.B(p + "blocks_building");
    return t;
}

MapManager::MapManager(int width, int height, float tileSize, const core::Config& cfg, ECS::EventBus& b)
    : w(width), h(height), tile(tileSize), bus(b) {
    props[static_cast<int>(Terrain::Grass)] = LoadProps(cfg, "grass");
    props[static_cast<int>(Terrain::Stone)] = LoadProps(cfg, "stone");
    props[static_cast<int>(Terrain::Water)] = LoadProps(cfg, "water");
    props[static_cast<int>(Terrain::Border)] = LoadProps(cfg, "border");
    rockBlocksProjectile = cfg.B("rock.blocks_projectile");
    wallBlocksProjectile = cfg.B("wall.basic.blocks_projectile");
    terrain.assign(static_cast<size_t>(w) * h, Terrain::Grass);
    rocks.assign(static_cast<size_t>(w) * h, ECS::NULL_ENTITY);
    walls.assign(static_cast<size_t>(w) * h, ECS::NULL_ENTITY);
    // 外周は移動不能な境界(§4.4)
    for (int x = 0; x < w; ++x) { terrain[Idx({x, 0})] = Terrain::Border; terrain[Idx({x, h - 1})] = Terrain::Border; }
    for (int y = 0; y < h; ++y) { terrain[Idx({0, y})] = Terrain::Border; terrain[Idx({w - 1, y})] = Terrain::Border; }
}

Vec2i MapManager::WorldToTile(Vec2 p) const {
    return {static_cast<int>(std::floor(p.x / tile)), static_cast<int>(std::floor(p.y / tile))};
}

Terrain MapManager::GetTerrain(Vec2i t) const { return InBounds(t) ? terrain[Idx(t)] : Terrain::Border; }

void MapManager::SetTerrain(Vec2i t, Terrain tr) {
    if (!InBounds(t) || terrain[Idx(t)] == tr) return;
    terrain[Idx(t)] = tr;
    ++version;
    bus.Publish<OnTerrainChanged>(t);
}

void MapManager::PlaceRock(EntityID e, Vec2i t) {
    assert(InBounds(t) && rocks[Idx(t)] == ECS::NULL_ENTITY);
    rocks[Idx(t)] = e;
    ++version;
    bus.Publish<OnObstacleMoved>(Vec2i{-1, -1}, t);
}

void MapManager::MoveRock(Vec2i from, Vec2i to) {
    assert(InBounds(from) && InBounds(to));
    assert(rocks[Idx(from)] != ECS::NULL_ENTITY && !BlocksRock(to));
    rocks[Idx(to)] = rocks[Idx(from)];
    rocks[Idx(from)] = ECS::NULL_ENTITY;
    ++version;
    bus.Publish<OnObstacleMoved>(from, to);
}

void MapManager::RemoveRock(Vec2i t) {
    if (!InBounds(t) || rocks[Idx(t)] == ECS::NULL_ENTITY) return;
    rocks[Idx(t)] = ECS::NULL_ENTITY;
    ++version;
    bus.Publish<OnObstacleMoved>(t, Vec2i{-1, -1});
}

void MapManager::PlaceWall(EntityID e, Vec2i t) {
    assert(InBounds(t) && !BlocksBuilding(t));
    walls[Idx(t)] = e;
    ++version;
    bus.Publish<OnBuildingPlaced>(t);
}

void MapManager::RemoveWall(Vec2i t) {
    if (!InBounds(t) || walls[Idx(t)] == ECS::NULL_ENTITY) return;
    walls[Idx(t)] = ECS::NULL_ENTITY;
    ++version;
    bus.Publish<OnBuildingDestroyed>(t);
}

bool MapManager::BlocksPlayer(Vec2i t) const {
    if (!InBounds(t)) return true;
    return Props(terrain[Idx(t)]).blocksPlayer || rocks[Idx(t)] != ECS::NULL_ENTITY || walls[Idx(t)] != ECS::NULL_ENTITY;
}
bool MapManager::BlocksEnemyTerrain(Vec2i t) const {
    if (!InBounds(t)) return true;
    return Props(terrain[Idx(t)]).blocksEnemy;
}
bool MapManager::BlocksEnemy(Vec2i t) const {
    if (BlocksEnemyTerrain(t)) return true;
    return rocks[Idx(t)] != ECS::NULL_ENTITY || walls[Idx(t)] != ECS::NULL_ENTITY;
}
bool MapManager::BlocksProjectile(Vec2i t) const {
    if (!InBounds(t)) return true;
    if (Props(terrain[Idx(t)]).blocksProjectile) return true;
    if (rockBlocksProjectile && rocks[Idx(t)] != ECS::NULL_ENTITY) return true;
    if (wallBlocksProjectile && walls[Idx(t)] != ECS::NULL_ENTITY) return true;
    return false;
}
bool MapManager::BlocksBuilding(Vec2i t) const {
    if (!InBounds(t)) return true;
    return Props(terrain[Idx(t)]).blocksBuilding || rocks[Idx(t)] != ECS::NULL_ENTITY || walls[Idx(t)] != ECS::NULL_ENTITY;
}
bool MapManager::BlocksRock(Vec2i t) const {
    if (!InBounds(t)) return true;
    return !Props(terrain[Idx(t)]).walkable || rocks[Idx(t)] != ECS::NULL_ENTITY || walls[Idx(t)] != ECS::NULL_ENTITY;
}

bool MapManager::CircleHitsBlocked(Vec2 c, float r, Who who) const {
    const Vec2i lo = WorldToTile({c.x - r, c.y - r});
    const Vec2i hi = WorldToTile({c.x + r, c.y + r});
    for (int ty = lo.y; ty <= hi.y; ++ty) {
        for (int tx = lo.x; tx <= hi.x; ++tx) {
            if (!BlockedFor({tx, ty}, who)) continue;
            const Vec2 mn = TileMin({tx, ty});
            const float cx = std::clamp(c.x, mn.x, mn.x + tile);
            const float cy = std::clamp(c.y, mn.y, mn.y + tile);
            const float dx = c.x - cx, dy = c.y - cy;
            if (dx * dx + dy * dy < r * r) return true;
        }
    }
    return false;
}

bool MapManager::SegmentClear(Vec2 a, Vec2 b, float radius, Who who) const {
    const float len = (b - a).Length();
    const int steps = std::max(1, static_cast<int>(len / (tile * 0.25f)));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / steps;
        const Vec2 p{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
        if (CircleHitsBlocked(p, radius, who)) return false;
    }
    return true;
}

}  // namespace game
