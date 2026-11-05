#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// ダメージの適用。
//  1. この フレームに積まれたダメージ(弾・爆弾・敵の壁攻撃)を HP から引く。
//  2. 敵との接触ダメージをプレイヤーに与える(受けたあとは短い無敵時間)。
class DamageSystem : public GameSystem {
public:
    explicit DamageSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() {
        return ECS::ECSManager::MakeSignature<Player, Transform, Collider, Health, PlayerStats>();
    }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (const DamageEvent& ev : g.damage) {
            auto* health = ecs.TryGet<Health>(ev.target);
            if (!health) continue;
            float amount = ev.amount;
            if (ev.target == g.player) amount *= 1.f - ecs.GetComponent<PlayerStats>(g.player).damageReduction;
            health->hp -= amount;
        }
        g.damage.clear();

        for (EntityID e : mEntities) {
            auto& health = ecs.GetComponent<Health>(e);
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const float radius = ecs.GetComponent<Collider>(e).radius;
            const auto& stats = ecs.GetComponent<PlayerStats>(e);

            health.invuln = std::max(0.f, health.invuln - dt);
            if (health.invuln > 0.f) continue;   // 無敵中は受けない(連続接触で即死しない: §12)

            float damage = 0.f;
            g.enemyGrid.Query(pos, radius + 40.f, [&](EntityID other) {
                if (!ecs.IsAlive(other)) return;
                auto* t = ecs.TryGet<Transform>(other);
                auto* c = ecs.TryGet<Collider>(other);
                auto* enemy = ecs.TryGet<Enemy>(other);
                if (!t || !c || !enemy) return;
                if (Distance(t->pos, pos) < radius + c->radius)
                    damage = std::max(damage, g.bal.enemies[enemy->def].contactDamage);
            });
            if (damage > 0.f) {
                health.hp -= damage * (1.f - stats.damageReduction);
                health.invuln = g.bal.playerInvuln;
            }
        }
    }
};

}  // namespace game
