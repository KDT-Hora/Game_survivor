#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// ダメージの適用。
//  1. この フレームに積まれたダメージ(弾・爆弾・敵の壁攻撃)を HP から引く。敵は押し戻され、短く光る。
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
            if (ev.target == g.player) {
                amount *= 1.f - ecs.GetComponent<PlayerStats>(g.player).damageReduction;
                health->hp -= amount;
                g.fx.push_back({FxEvent::Kind::PlayerHurt, ecs.GetComponent<Transform>(g.player).pos, amount});
                continue;
            }
            health->hp -= amount;
            if (auto* enemy = ecs.TryGet<Enemy>(ev.target)) {
                auto& transform = ecs.GetComponent<Transform>(ev.target);
                const EnemyDef& def = g.bal.enemies[enemy->def];
                enemy->hitFlash = 0.12f;
                if (ev.knock.LengthSq() > 0.f && def.knockbackMul > 0.f) {
                    const float radius = ecs.GetComponent<Collider>(ev.target).radius;
                    // 壁際では行けるところまで寄せる(MoveAndSlide は一度に動かすので、数回に分けて進める)
                    const Vec2 knock = ev.knock * def.knockbackMul;
                    const int steps = std::max(1, static_cast<int>(knock.Length() / 3.f));
                    for (int i = 0; i < steps; ++i)
                        transform.pos = MoveAndSlide(g.map, transform.pos, knock * (1.f / static_cast<float>(steps)), radius, WhoOf(def));
                    transform.pos = Depenetrate(g.map, transform.pos, radius, WhoOf(def));
                }
                g.fx.push_back({FxEvent::Kind::EnemyHit, transform.pos, amount, enemy->def});
            }
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
                const float taken = damage * (1.f - stats.damageReduction);
                health.hp -= taken;
                health.invuln = g.bal.playerInvuln;
                g.fx.push_back({FxEvent::Kind::PlayerHurt, pos, taken});
            }
        }
    }
};

}  // namespace game
