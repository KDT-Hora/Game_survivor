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
            const Vec2i tile = ecs.GetComponent<GridPosition>(e).tile;
            g.map.RemoveWall(tile);   // 通行判定の更新とイベント発行は MapManager が行う
            g.fx.push_back({FxEvent::Kind::WallBroken, g.map.TileCenter(tile)});
            ecs.DestroyLater(e);
        }
    }
};

}  // namespace game
