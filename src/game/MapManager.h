#pragma once
#include <vector>

#include "core/Config.h"
#include "core/Math.h"
#include "ecs/ECSConfig.h"
#include "ecs/EventBus.h"

namespace game {

using core::Vec2;
using core::Vec2i;
using ECS::EntityID;

enum class Terrain { Grass, Stone, Water, Border };

// 通行・衝突属性(§4.3)。単一の真偽値に集約しない。
struct TerrainProps {
    bool walkable = true;
    bool blocksPlayer = false;
    bool blocksEnemy = false;
    bool blocksProjectile = false;
    bool blocksBuilding = false;
};

// マップ変更イベント(§15.4)。経路探索などは購読して反応し、変更側と直接結合しない。
struct OnObstacleMoved : ECS::Event {
    Vec2i from, to;   // 破壊された場合 to は (-1,-1)
    OnObstacleMoved(Vec2i f, Vec2i t) : from(f), to(t) {}
};
struct OnBuildingPlaced : ECS::Event {
    Vec2i tile;
    explicit OnBuildingPlaced(Vec2i t) : tile(t) {}
};
struct OnBuildingDestroyed : ECS::Event {
    Vec2i tile;
    explicit OnBuildingDestroyed(Vec2i t) : tile(t) {}
};
struct OnTerrainChanged : ECS::Event {
    Vec2i tile;
    explicit OnTerrainChanged(Vec2i t) : tile(t) {}
};

// タイル占有の唯一の管理者(§5.1, §23-2)。他の System は独自に空きマス判定をしない。
// レイヤー: Terrain / Static Object(岩) / Building(壁)。動的 Entity は連続座標で管理し、ここには入れない。
class MapManager {
public:
    MapManager(int width, int height, float tileSize, const core::Config& cfg, ECS::EventBus& bus);

    int Width() const { return w; }
    int Height() const { return h; }
    float TileSize() const { return tile; }

    // --- 座標変換(連続座標とタイル座標の境界はここだけ) ---
    Vec2i WorldToTile(Vec2 p) const;
    Vec2 TileCenter(Vec2i t) const { return {(t.x + 0.5f) * tile, (t.y + 0.5f) * tile}; }
    Vec2 TileMin(Vec2i t) const { return {t.x * tile, t.y * tile}; }
    bool InBounds(Vec2i t) const { return t.x >= 0 && t.y >= 0 && t.x < w && t.y < h; }

    // --- 地形 ---
    Terrain GetTerrain(Vec2i t) const;
    void SetTerrain(Vec2i t, Terrain terrain);
    const TerrainProps& Props(Terrain t) const { return props[static_cast<int>(t)]; }

    // --- 物体 ---
    EntityID RockAt(Vec2i t) const { return InBounds(t) ? rocks[Idx(t)] : ECS::NULL_ENTITY; }
    EntityID WallAt(Vec2i t) const { return InBounds(t) ? walls[Idx(t)] : ECS::NULL_ENTITY; }
    void PlaceRock(EntityID e, Vec2i t);
    void MoveRock(Vec2i from, Vec2i to);     // 移動先が空いている前提(呼び出し側が検証)
    void RemoveRock(Vec2i t);
    void PlaceWall(EntityID e, Vec2i t);
    void RemoveWall(Vec2i t);

    // --- 用途別の通行判定(§4.3) ---
    bool BlocksPlayer(Vec2i t) const;
    bool BlocksEnemy(Vec2i t) const;          // 地形+岩+壁(通常敵が通れないもの)
    bool BlocksEnemyTerrain(Vec2i t) const;   // 地形のみ(壁・岩を壊せる敵の経路用)
    bool BlocksProjectile(Vec2i t) const;
    bool BlocksBuilding(Vec2i t) const;
    bool BlocksRock(Vec2i t) const;           // 岩の移動先として不可(地形が非 walkable、岩、壁、範囲外)

    // 円(連続座標)が、指定条件でブロックされるタイルと重なるか。
    enum class Who { Player, Enemy, Projectile };
    bool CircleHitsBlocked(Vec2 center, float radius, Who who) const;
    // 線分が(半径付きで)ブロックされるタイルを横切らないか。敵の見通し判定に使う。
    bool SegmentClear(Vec2 a, Vec2 b, float radius, Who who) const;

    // 地形変更の通し番号。経路キャッシュの無効化に使う。
    std::uint32_t Version() const { return version; }

private:
    size_t Idx(Vec2i t) const { return static_cast<size_t>(t.y) * w + t.x; }
    bool BlockedFor(Vec2i t, Who who) const {
        if (who == Who::Player) return BlocksPlayer(t);
        if (who == Who::Projectile) return BlocksProjectile(t);
        return BlocksEnemy(t);
    }

    int w, h;
    float tile;
    TerrainProps props[4];
    bool rockBlocksProjectile = true;
    bool wallBlocksProjectile = false;
    std::vector<Terrain> terrain;
    std::vector<EntityID> rocks, walls;
    ECS::EventBus& bus;
    std::uint32_t version = 0;
};

}  // namespace game
