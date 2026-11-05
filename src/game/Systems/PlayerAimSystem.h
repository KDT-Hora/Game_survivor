#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// カーソルの方向・選択中の壁・建築の要求を Player に反映する(移動方向とは独立: §3.1)。
class PlayerAimSystem : public GameSystem {
public:
    explicit PlayerAimSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player, Transform>(); }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            auto& player = ecs.GetComponent<Player>(e);
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const Vec2 dir = (g.input.aimWorld - pos).Normalized();
            if (dir.LengthSq() > 0.f) player.aim = dir;
            if (g.input.wallSelect >= 0 && g.input.wallSelect < static_cast<int>(g.bal.walls.size()))
                player.selectedWall = g.input.wallSelect;
            if (g.input.buildPressed) player.buildRequested = true;  // その場では建てず、要求として保持(§16)
        }
    }
};

}  // namespace game
