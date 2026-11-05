#pragma once
#include <algorithm>

#include "game/Pathfinding.h"
#include "game/Systems/GameSystem.h"

namespace game {

// 経路の再探索。次の2つのとき、その敵の経路を探し直す。
//   ・一定時間ごと(repathTimer)
//   ・マップが変わったとき(壁・岩・地形の変化。通知イベントで知る)
// 全員が毎フレーム A* を走らせないよう、1フレームに探索する本数には上限がある。プレイヤーに近い敵を先にする。
class PathfindingSystem : public GameSystem {
public:
    explicit PathfindingSystem(GameContext& c) : GameSystem(c) {
        // マップが変わったことは「通知」で知る。変える側(MapManager)はこの System を知らない(§15.4)。
        auto& bus = g.ecs.Events();
        bus.Subscribe<OnObstacleMoved>([this](const OnObstacleMoved&) { mapChanged = true; });
        bus.Subscribe<OnBuildingPlaced>([this](const OnBuildingPlaced&) { mapChanged = true; });
        bus.Subscribe<OnBuildingDestroyed>([this](const OnBuildingDestroyed&) { mapChanged = true; });
        bus.Subscribe<OnTerrainChanged>([this](const OnTerrainChanged&) { mapChanged = true; });
    }
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Enemy, Transform>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        if (!ecs.IsAlive(g.player)) return;
        const Vec2 playerPos = ecs.GetComponent<Transform>(g.player).pos;
        const Vec2i goal = g.map.WorldToTile(playerPos);

        // 探索が必要な敵を集める
        struct Candidate { float dist; EntityID e; };
        std::vector<Candidate> candidates;
        for (EntityID e : mEntities) {
            auto& enemy = ecs.GetComponent<Enemy>(e);
            enemy.repathTimer -= dt;
            if (!enemy.usingPath) continue;   // 見通しがあって直進している敵は、経路を使わない
            if (mapChanged) enemy.needsPath = true;
            if (enemy.needsPath || enemy.repathTimer <= 0.f)
                candidates.push_back({Distance(ecs.GetComponent<Transform>(e).pos, playerPos), e});
        }
        mapChanged = false;

        // プレイヤーに近い敵から(同じ距離なら ID 順)
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.dist != b.dist ? a.dist < b.dist : a.e < b.e;
        });

        int budget = g.bal.pathBudget;
        for (const Candidate& c : candidates) {
            if (budget-- <= 0) break;
            auto& enemy = ecs.GetComponent<Enemy>(c.e);
            const Vec2 pos = ecs.GetComponent<Transform>(c.e).pos;
            PathResult result = FindPath(g.map, g.map.WorldToTile(pos), goal, ProfileFor(enemy), g.bal.pathMaxExpansions);
            enemy.path = std::move(result.tiles);
            enemy.pathIdx = 0;
            enemy.needsPath = false;
            enemy.repathTimer = g.bal.repathInterval * g.rng.Range(0.75f, 1.25f);   // 少しばらつかせる
        }
    }

private:
    // 敵ごとの能力(壁を壊せる/岩を押せる)を、経路のコストで表す。
    PathProfile ProfileFor(const Enemy& enemy) const {
        const EnemyDef& def = g.bal.enemies[enemy.def];
        PathProfile p;
        p.wallCost = def.breaksWalls ? g.bal.wallPathCost : 0.f;
        p.rockCost = def.pushesRocks ? g.bal.rockPathCost : 0.f;
        return p;
    }

    bool mapChanged = false;
};

}  // namespace game
