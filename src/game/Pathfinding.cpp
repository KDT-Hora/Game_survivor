#include "game/Pathfinding.h"

#include <algorithm>
#include <cmath>
#include <queue>

namespace game {

namespace {

// 通行コスト。<0 は通行不能。
float StepCost(const MapManager& map, Vec2i t, const PathProfile& p) {
    if (map.BlocksEnemyTerrain(t)) return -1.f;
    float c = 1.f;
    if (map.WallAt(t) != ECS::NULL_ENTITY && !p.ignoresWalls) {
        if (p.wallCost <= 0.f) return -1.f;
        c += p.wallCost;
    }
    if (map.RockAt(t) != ECS::NULL_ENTITY) {
        if (p.rockCost <= 0.f) return -1.f;
        c += p.rockCost;
    }
    return c;
}

float Octile(Vec2i a, Vec2i b) {
    const float dx = static_cast<float>(std::abs(a.x - b.x));
    const float dy = static_cast<float>(std::abs(a.y - b.y));
    return (dx + dy) + (1.41421356f - 2.f) * std::min(dx, dy);
}

}  // namespace

PathResult FindPath(const MapManager& map, Vec2i start, Vec2i goal, const PathProfile& profile, int maxExpansions) {
    PathResult res;
    const int W = map.Width(), H = map.Height();
    if (!map.InBounds(start) || !map.InBounds(goal)) return res;
    auto idx = [&](Vec2i t) { return t.y * W + t.x; };

    if (start == goal) { res.reachedGoal = true; res.found = true; return res; }

    struct Node { float f; int i; };
    struct Cmp { bool operator()(const Node& a, const Node& b) const { return a.f > b.f; } };
    std::priority_queue<Node, std::vector<Node>, Cmp> open;

    std::vector<float> g(static_cast<size_t>(W) * H, 1e30f);
    std::vector<int> parent(static_cast<size_t>(W) * H, -1);
    std::vector<char> closed(static_cast<size_t>(W) * H, 0);

    g[idx(start)] = 0.f;
    open.push({Octile(start, goal), idx(start)});
    int bestIdx = idx(start);
    float bestH = Octile(start, goal);

    while (!open.empty()) {
        const Node n = open.top();
        open.pop();
        if (closed[n.i]) continue;
        closed[n.i] = 1;
        const Vec2i cur{n.i % W, n.i / W};
        if (cur == goal) { bestIdx = n.i; res.reachedGoal = true; break; }
        if (++res.expansions > maxExpansions) break;

        const float h = Octile(cur, goal);
        if (h < bestH) { bestH = h; bestIdx = n.i; }

        for (int d = 0; d < 8; ++d) {
            const Vec2i dir = core::Dir8()[d];
            const Vec2i nb = cur + dir;
            if (!map.InBounds(nb)) continue;
            float cost = (nb == goal) ? 1.f : StepCost(map, nb, profile);
            if (cost < 0.f) continue;
            if (dir.x != 0 && dir.y != 0) {
                // 角抜け禁止: 両隣が通行可能なときだけ斜めに進む
                if (StepCost(map, {cur.x + dir.x, cur.y}, profile) < 0.f) continue;
                if (StepCost(map, {cur.x, cur.y + dir.y}, profile) < 0.f) continue;
                cost *= 1.41421356f;
            }
            const float ng = g[n.i] + cost;
            const int ni = idx(nb);
            if (ng < g[ni]) {
                g[ni] = ng;
                parent[ni] = n.i;
                open.push({ng + Octile(nb, goal), ni});
            }
        }
    }

    // 復元(到達不能なら最も近づけた地点まで)
    std::vector<Vec2i> rev;
    for (int i = bestIdx; i != -1 && i != idx(start); i = parent[i]) rev.push_back({i % W, i / W});
    std::reverse(rev.begin(), rev.end());
    res.tiles = std::move(rev);
    res.found = !res.tiles.empty();
    return res;
}

}  // namespace game
