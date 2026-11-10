#pragma once
#include <cmath>

#include "game/Systems/WeaponSystem.h"

namespace game {

// 通常攻撃: 近い敵へ玉を撃つ。強化で玉が増えると、近い順に別々の敵へ飛ぶ。
class BoltWeaponSystem : public WeaponSystem {
public:
    explicit BoltWeaponSystem(GameContext& c) : WeaponSystem(c, WeaponType::Bolt) {}

protected:
    bool Fire(ECS::ECSManager& ecs, EntityID player, const WeaponDef& def, const WeaponSlot& slot,
              const PlayerStats& stats) override {
        const Vec2 pos = ecs.GetComponent<Transform>(player).pos;

        // 射程内で、弾が地形に遮られない敵を、近い順に並べる(§10.1)
        struct Target { float dist2; EntityID e; };
        std::vector<Target> targets;
        g.enemyGrid.Query(pos, def.range, [&](EntityID o) {
            if (!ecs.IsAlive(o)) return;
            auto* t = ecs.TryGet<Transform>(o);
            if (!t) return;
            const float dist2 = (t->pos - pos).LengthSq();
            if (dist2 <= def.range * def.range && g.map.SegmentClear(pos, t->pos, 0.5f, MapManager::Who::Projectile))
                targets.push_back({dist2, o});
        });
        if (targets.empty()) return false;
        std::sort(targets.begin(), targets.end(), [](const Target& a, const Target& b) {
            return a.dist2 != b.dist2 ? a.dist2 < b.dist2 : a.e < b.e;
        });

        // 玉の数だけ、近い敵から順に撃つ。敵が玉より少ないときは、同じ敵へ少し角度をずらして撃つ。
        const int count = 1 + stats.extraProjectiles;
        for (int i = 0; i < count; ++i) {
            const Target& target = targets[static_cast<size_t>(i) % targets.size()];
            Vec2 dir = (ecs.GetComponent<Transform>(target.e).pos - pos).Normalized();
            const int round = i / static_cast<int>(targets.size());
            if (round > 0) {
                const float angle = 0.12f * static_cast<float>((round + 1) / 2) * (round % 2 ? 1.f : -1.f);
                dir = {dir.x * std::cos(angle) - dir.y * std::sin(angle), dir.x * std::sin(angle) + dir.y * std::cos(angle)};
            }
            CreateProjectile(g, pos, dir * def.speed, Damage(def, slot, stats), def.lifetime, def.radius,
                             false, 0, false, def.knockback);
        }
        return true;
    }
};

}  // namespace game
