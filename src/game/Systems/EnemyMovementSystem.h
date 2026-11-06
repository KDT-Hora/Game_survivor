#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 敵の移動。目的地(target)へ向かい、敵同士が重ならないよう軽く離れ、壁・岩・水には入らない。
class EnemyMovementSystem : public GameSystem {
public:
    explicit EnemyMovementSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Enemy, Transform, Collider, Health>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        const bool hasPlayer = ecs.IsAlive(g.player);
        const Vec2 playerPos = hasPlayer ? ecs.GetComponent<Transform>(g.player).pos : Vec2{};
        const float playerRadius = hasPlayer ? ecs.GetComponent<Collider>(g.player).radius : 0.f;

        for (EntityID e : mEntities) {
            auto& enemy = ecs.GetComponent<Enemy>(e);
            auto& transform = ecs.GetComponent<Transform>(e);
            const float radius = ecs.GetComponent<Collider>(e).radius;
            const EnemyDef& def = g.bal.enemies[enemy.def];

            // 近くの敵と重なっていたら離れる(押し合いはしない: §5.1)
            Vec2 separation;
            g.enemyGrid.Query(transform.pos, radius * 2.f, [&](EntityID other) {
                if (other == e || !ecs.IsAlive(other)) return;
                auto* otherTransform = ecs.TryGet<Transform>(other);
                auto* otherCollider = ecs.TryGet<Collider>(other);
                if (!otherTransform || !otherCollider) return;
                Vec2 away = transform.pos - otherTransform->pos;
                const float dist = away.Length(), minDist = radius + otherCollider->radius;
                if (dist >= minDist) return;
                if (dist < 1e-3f) {   // 完全に重なっているときは、IDで決めた向きへ離す
                    away = Vec2{static_cast<float>(e % 7) - 3.f, static_cast<float>(e % 5) - 2.f};
                    separation += away.Normalized();
                } else {
                    separation += away * ((minDist - dist) / dist / minDist);
                }
            });

            // 目的地へ進む。プレイヤーに触れていたら止まる。
            Vec2 move;
            const bool touchingPlayer = Distance(transform.pos, playerPos) < radius + playerRadius;
            if (!enemy.hold && !touchingPlayer) {
                const Vec2 toTarget = enemy.target - transform.pos;
                const float dist = toTarget.Length();
                if (dist > 0.5f) move = toTarget.Normalized() * std::min(def.speed * dt, dist);
            }
            move += separation * (def.speed * dt * g.bal.separation);

            transform.pos = MoveAndSlide(g.map, transform.pos, move, radius, WhoOf(def));
            transform.pos = Depenetrate(g.map, transform.pos, radius, WhoOf(def));
        }
    }
};

}  // namespace game
