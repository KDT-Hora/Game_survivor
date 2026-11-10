#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 壁の建築(§6)。検証はプレビューと同じ ComputeBuildPlan を使い、失敗したら何も変えない(§23-1)。
class BuildingSystem : public GameSystem {
public:
    explicit BuildingSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() {
        return ECS::ECSManager::MakeSignature<Player, Transform, Collider, Inventory, PlayerStats>();
    }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            auto& player = ecs.GetComponent<Player>(e);
            if (!player.buildRequested) continue;
            player.buildRequested = false;

            const BuildPlan plan = ComputeBuildPlan(g);
            g.lastBuildOk = plan.ok;
            g.lastBuildFail = plan.reason;
            if (!plan.ok) {
                g.fx.push_back({FxEvent::Kind::BuildFailed, g.map.TileCenter(plan.tile), 0.f, static_cast<int>(plan.reason)});
                continue;
            }

            // ここから先は失敗しない(押し出し先は確保済み)
            for (const PushMove& m : plan.pushes) {
                if (auto* t = ecs.TryGet<Transform>(m.id)) t->pos = m.to;
            }
            auto& inventory = ecs.GetComponent<Inventory>(e);
            inventory.wood -= WallWoodCost(g, player.selectedWall);
            inventory.stone -= g.bal.walls[player.selectedWall].costStone;
            CreateWall(g, plan.tile, player.selectedWall);
            ++g.wallsBuilt;
            g.fx.push_back({FxEvent::Kind::WallBuilt, g.map.TileCenter(plan.tile)});

            // 壁にプレイヤーの円が食い込んでいたら押し戻す(§6.2: 自分と重ならない)
            auto& transform = ecs.GetComponent<Transform>(e);
            transform.pos = Depenetrate(g.map, transform.pos, ecs.GetComponent<Collider>(e).radius, MapManager::Who::Player);
        }
    }
};

}  // namespace game
