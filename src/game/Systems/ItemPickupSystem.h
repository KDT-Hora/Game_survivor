#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 近くに落ちているアイテムを拾う。少し離れたアイテムはプレイヤーへ吸い寄せられる。
class ItemPickupSystem : public GameSystem {
public:
    explicit ItemPickupSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() {
        return ECS::ECSManager::MakeSignature<Player, Transform, Collider, Inventory, Experience, PlayerStats, Health>();
    }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const float radius = ecs.GetComponent<Collider>(e).radius;
            auto& inventory = ecs.GetComponent<Inventory>(e);
            auto& experience = ecs.GetComponent<Experience>(e);
            auto& health = ecs.GetComponent<Health>(e);
            const float rangeMul = ecs.GetComponent<PlayerStats>(e).pickupRangeMul;
            const float range = radius + g.bal.pickupRadius + g.bal.pickupRange * rangeMul;
            const float magnet = g.bal.magnetRadius * rangeMul;

            g.pickupGrid.Query(pos, std::max(range, magnet), [&](EntityID other) {
                if (!ecs.IsAlive(other)) return;
                auto* t = ecs.TryGet<Transform>(other);
                auto* pickup = ecs.TryGet<Pickup>(other);
                if (!t || !pickup) return;
                const float dist = Distance(t->pos, pos);
                if (dist > range) {
                    // 吸い寄せ。回復アイテムは HP が満タンなら置いておく(あとで取りに来られる)
                    if (dist > magnet || (pickup->kind == PickupKind::Heal && health.hp >= health.maxHp)) return;
                    const float speed = g.bal.magnetSpeed * (1.5f - dist / magnet);   // 近いほど速く
                    t->pos += (pos - t->pos).Normalized() * std::min(speed * dt, dist);
                    return;
                }
                switch (pickup->kind) {
                    case PickupKind::Xp: experience.xp += pickup->amount; break;
                    case PickupKind::Wood: inventory.wood += pickup->amount; break;
                    case PickupKind::Stone: inventory.stone += pickup->amount; break;
                    case PickupKind::Scrap: inventory.scrap += pickup->amount; break;
                    case PickupKind::Heal: health.hp = std::min(health.maxHp, health.hp + g.bal.healAmount * pickup->amount); break;
                }
                g.fx.push_back({FxEvent::Kind::Pickup, t->pos, static_cast<float>(pickup->amount), static_cast<int>(pickup->kind)});
                ecs.DestroyLater(other);
            });
        }
    }
};

}  // namespace game
