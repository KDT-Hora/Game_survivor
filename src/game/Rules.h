#pragma once
#include <vector>

#include "game/MapManager.h"

namespace game {

// ---------------------------------------------------------------------------
// 建築判定(§6)。プレビューと実行が同じ関数を使うので、表示と結果が食い違わない(§14.3)。
// この層は純粋関数: マップと「設置先にいる対象のリスト」だけを入力にし、状態を変えない。
// ---------------------------------------------------------------------------

enum class BuildFail {
    None,
    OutOfBounds,    // マップ外
    Rock,           // 岩
    ExistingWall,   // 既存の壁
    Terrain,        // 建築禁止地形(固定障害物含む)
    NoMaterial,     // 素材不足
    NoRoom,         // 押し出し先を確保できない
    NoDirection,    // 照準方向が未定
};

// 設置先にいる押し出し対象。敵は固体として、アイテムは自由配置物として扱う。
struct Occupant {
    EntityID id = ECS::NULL_ENTITY;
    Vec2 pos;
    bool isEnemy = true;
};

struct PushMove {
    EntityID id = ECS::NULL_ENTITY;
    Vec2 to;           // 移動先(タイル中心)
    Vec2i toTile;
    bool isEnemy = true;
};

struct BuildPlan {
    bool ok = false;
    BuildFail reason = BuildFail::None;
    Vec2i tile;                  // 設置予定タイル
    std::vector<PushMove> pushes;  // 全員分の押し出し先(ok のときだけ有効)
};

// 8方向の隣接タイルを返す(照準に最も近い方向)。dir=(0,0) なら NoDirection。
BuildPlan PlanWallPlacement(const MapManager& map, Vec2i playerTile, Vec2 aimDir,
                            const std::vector<Occupant>& occupants, bool hasMaterials);

// 周囲8マスから最も近い空きマスを全員分、重複なしで確保する。1人でも確保できなければ false(§6.3)。
// 距離はユークリッド(§22-5 の決定)。同距離は Dir8 の固定順。
bool AssignPushDestinations(const MapManager& map, Vec2i target, Vec2i playerTile,
                            const std::vector<Occupant>& occupants, std::vector<PushMove>& out);

// ---------------------------------------------------------------------------
// 岩押し(§7)。移動先が空いているときだけ押せる。連続押し・複数押しは不可。
// 斜めのときは、斜め移動先・横隣・縦隣の3か所すべてが空いている必要がある(§7.2)。
// ---------------------------------------------------------------------------
bool CanPushRock(const MapManager& map, Vec2i rockTile, Vec2i dir);

}  // namespace game
