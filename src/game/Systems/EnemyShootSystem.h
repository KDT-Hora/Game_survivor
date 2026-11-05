#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 遠距離攻撃をする敵(def.range > 0)の射撃。射程内で、間に岩などがなければ、プレイヤーへ向けて弾を撃つ。
// 弾の移動と命中は ProjectileSystem が行う。
class EnemyShootSystem : public GameSystem {
public:
    explicit EnemyShootSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Enemy, Transform, Collider>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        if (!ecs.IsAlive(g.player)) return;
        const Vec2 playerPos = ecs.GetComponent<Transform>(g.player).pos;
        for (EntityID e : mEntities) {
            auto& enemy = ecs.GetComponent<Enemy>(e);
            const EnemyDef& def = g.bal.enemies[enemy.def];
            if (def.range <= 0.f) continue;

            enemy.shotTimer -= dt;
            if (enemy.shotTimer > 0.f) continue;
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            if (Distance(pos, playerPos) > def.range) continue;
            if (!g.map.SegmentClear(pos, playerPos, 0.5f, MapManager::Who::Projectile)) continue;   // 岩・外周が間にあれば撃たない

            const float life = def.range / def.shotSpeed * 1.3f;
            CreateProjectile(g, pos, (playerPos - pos).Normalized() * def.shotSpeed, def.shotDamage, life, 4.f,
                             /*pierce=*/false, /*kind=*/2, /*hostile=*/true);
            enemy.shotTimer = def.shotCooldown;
        }
    }
};

}  // namespace game
