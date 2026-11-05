#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 壁の破壊。HP が 0 以下の壁をマップから外して消す(§6.5)。
class DestructionSystem : public GameSystem {
public:
    explicit DestructionSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Building, Health, GridPosition>(); }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            if (ecs.GetComponent<Health>(e).hp > 0.f) continue;
            g.map.RemoveWall(ecs.GetComponent<GridPosition>(e).tile);   // 通行判定の更新とイベント発行は MapManager が行う
            ecs.DestroyLater(e);
        }
    }
};

}  // namespace game
