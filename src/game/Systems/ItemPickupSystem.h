#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 近くに落ちているアイテムを拾う。
class ItemPickupSystem : public GameSystem {
public:
    explicit ItemPickupSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() {
        return ECS::ECSManager::MakeSignature<Player, Transform, Collider, Inventory, Experience, PlayerStats>();
    }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const float radius = ecs.GetComponent<Collider>(e).radius;
            auto& inventory = ecs.GetComponent<Inventory>(e);
            auto& experience = ecs.GetComponent<Experience>(e);
            const float range = radius + g.bal.pickupRadius + g.bal.pickupRange * ecs.GetComponent<PlayerStats>(e).pickupRangeMul;

            g.pickupGrid.Query(pos, range, [&](EntityID other) {
                if (!ecs.IsAlive(other)) return;
                auto* t = ecs.TryGet<Transform>(other);
                auto* pickup = ecs.TryGet<Pickup>(other);
                if (!t || !pickup || Distance(t->pos, pos) > range) return;
                switch (pickup->kind) {
                    case PickupKind::Xp: experience.xp += pickup->amount; break;
                    case PickupKind::Wood: inventory.wood += pickup->amount; break;
                    case PickupKind::Stone: inventory.stone += pickup->amount; break;
                    case PickupKind::Scrap: inventory.scrap += pickup->amount; break;
                }
                ecs.DestroyLater(other);
            });
        }
    }
};

}  // namespace game
