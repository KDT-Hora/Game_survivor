#pragma once
#include <vector>

#include "game/MapManager.h"

namespace game {

// 経路探索のプロファイル。敵ごとの能力(壁を壊せる/岩を押せる)を、種別の条件分岐ではなくコストで表す。
struct PathProfile {
    float wallCost = 0.f;   // 0 = 壁は通れない / >0 = 壁を壊して進むコスト(1タイル)
    float rockCost = 0.f;
    bool ignoresWalls = false;   // true なら壁は障害物ではない(すり抜ける敵)   // 0 = 岩は通れない / >0 = 岩を押す・壊すコスト
};

struct PathResult {
    std::vector<Vec2i> tiles;   // start の次から goal(または最接近点)まで。start 自身は含まない
    bool reachedGoal = false;   // false なら到達不能で、最も近づけた地点までの経路
    bool found = false;         // 1歩でも進める経路があるか
    int expansions = 0;
};

// A*(8方向・角抜けなし)。AI と探索は分離してあり、将来フローフィールドへ差し替え可能(§8.2)。
// start / goal 自身は通行不能でも許可する(敵が押し出し直後に壁際にいる、プレイヤーが goal、など)。
PathResult FindPath(const MapManager& map, Vec2i start, Vec2i goal, const PathProfile& profile, int maxExpansions);

}  // namespace game
