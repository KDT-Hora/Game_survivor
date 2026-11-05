#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// プレイヤーの移動。壁・岩・水の前では止まり、面に沿って滑る。
class PlayerMovementSystem : public GameSystem {
public:
    explicit PlayerMovementSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player, Transform, Collider, PlayerStats>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            auto& player = ecs.GetComponent<Player>(e);
            auto& transform = ecs.GetComponent<Transform>(e);
            const auto& collider = ecs.GetComponent<Collider>(e);
            const auto& stats = ecs.GetComponent<PlayerStats>(e);

            player.wish = g.input.move.Normalized();  // 斜めでも速くならない(§3.1)
            float speed = g.bal.playerSpeed * stats.moveSpeedMul;
            if (player.pushing) speed *= g.bal.pushSpeedMul;   // 岩を押している間は遅い
            transform.pos = MoveAndSlide(g.map, transform.pos, player.wish * (speed * dt), collider.radius, MapManager::Who::Player);
            transform.pos = Depenetrate(g.map, transform.pos, collider.radius, MapManager::Who::Player);
        }
    }
};

}  // namespace game
