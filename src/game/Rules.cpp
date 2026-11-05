#include "game/Rules.h"

#include <algorithm>

namespace game {

bool CanPushRock(const MapManager& map, Vec2i rock, Vec2i dir) {
    if (dir.x == 0 && dir.y == 0) return false;
    if (map.RockAt(rock) == ECS::NULL_ENTITY) return false;
    const Vec2i dest = rock + dir;
    if (map.BlocksRock(dest)) return false;
    if (dir.x != 0 && dir.y != 0) {
        // 斜め押し: 横方向・縦方向の隣接タイルも妨げられていないこと(角抜け防止)
        if (map.BlocksRock({rock.x + dir.x, rock.y})) return false;
        if (map.BlocksRock({rock.x, rock.y + dir.y})) return false;
    }
    return true;
}

bool AssignPushDestinations(const MapManager& map, Vec2i target, Vec2i playerTile,
                            const std::vector<Occupant>& occupants, std::vector<PushMove>& out) {
    out.clear();
    if (occupants.empty()) return true;

    // 押し出し先の候補は、設置先の周囲8マス(Dir8 の固定順)。
    std::vector<Vec2i> candidates;
    for (int i = 0; i < 8; ++i) candidates.push_back(target + core::Dir8()[i]);

    // この対象を置けるマスか。
    auto canPlace = [&](const Occupant& o, Vec2i tile) {
        if (!map.InBounds(tile)) return false;
        if (o.isEnemy) return !map.BlocksEnemy(tile) && tile != playerTile;   // 敵は、壁や水・プレイヤーのいるマスには動かさない
        return !map.BlocksPlayer(tile);                                       // アイテムは、拾える場所へ
    };

    // 置ける場所が少ない敵を先に割り当て、アイテムは後にする(同じ種類の中では ID の小さい順)。
    std::vector<size_t> order;
    for (size_t i = 0; i < occupants.size(); ++i) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (occupants[a].isEnemy != occupants[b].isEnemy) return occupants[a].isEnemy;
        return occupants[a].id < occupants[b].id;
    });

    std::vector<bool> used(candidates.size(), false);
    std::vector<PushMove> moves(occupants.size());
    for (size_t i : order) {
        const Occupant& o = occupants[i];
        // いちばん近い空きマスを選ぶ(同じ距離なら固定順の先)
        int best = -1;
        float bestDist = 0.f;
        for (size_t k = 0; k < candidates.size(); ++k) {
            if (used[k] || !canPlace(o, candidates[k])) continue;
            const float dist = core::Distance(o.pos, map.TileCenter(candidates[k]));
            if (best < 0 || dist < bestDist - 1e-4f) { best = static_cast<int>(k); bestDist = dist; }
        }
        if (best < 0) return false;   // 1つでも置けなければ、全体を取りやめる(§6.3)
        used[best] = true;
        moves[i].id = o.id;
        moves[i].toTile = candidates[best];
        moves[i].to = map.TileCenter(candidates[best]);
        moves[i].isEnemy = o.isEnemy;
    }
    out = std::move(moves);
    return true;
}

BuildPlan PlanWallPlacement(const MapManager& map, Vec2i playerTile, Vec2 aimDir,
                            const std::vector<Occupant>& occupants, bool hasMaterials) {
    BuildPlan plan;
    const Vec2i dir = core::QuantizeDir8(aimDir);
    if (dir.x == 0 && dir.y == 0) {
        plan.reason = BuildFail::NoDirection;
        return plan;
    }
    plan.tile = playerTile + dir;
    const Vec2i t = plan.tile;

    if (!map.InBounds(t)) { plan.reason = BuildFail::OutOfBounds; return plan; }
    if (map.RockAt(t) != ECS::NULL_ENTITY) { plan.reason = BuildFail::Rock; return plan; }
    if (map.WallAt(t) != ECS::NULL_ENTITY) { plan.reason = BuildFail::ExistingWall; return plan; }
    if (map.BlocksBuilding(t)) { plan.reason = BuildFail::Terrain; return plan; }
    if (!hasMaterials) { plan.reason = BuildFail::NoMaterial; return plan; }

    if (!AssignPushDestinations(map, t, playerTile, occupants, plan.pushes)) {
        plan.pushes.clear();
        plan.reason = BuildFail::NoRoom;
        return plan;
    }
    plan.ok = true;
    return plan;
}

}  // namespace game
