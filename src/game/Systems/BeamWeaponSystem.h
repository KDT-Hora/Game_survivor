#pragma once
#include "game/Systems/WeaponSystem.h"

namespace game {

// 貫通ビーム: カーソルの方向へ一直線に飛ぶ。敵は貫通し、岩・外周で止まる。
class BeamWeaponSystem : public WeaponSystem {
public:
    explicit BeamWeaponSystem(GameContext& c) : WeaponSystem(c, WeaponType::Beam) {}

protected:
    bool Fire(ECS::ECSManager& ecs, EntityID player, const WeaponDef& def, const WeaponSlot& slot,
              const PlayerStats& stats) override {
        const Vec2 pos = ecs.GetComponent<Transform>(player).pos;
        const Vec2 aim = ecs.GetComponent<Player>(player).aim;
        CreateProjectile(g, pos, aim * def.speed, Damage(def, slot, stats), def.lifetime, def.radius,
                         /*pierce=*/true, /*kind=*/1);
        return true;
    }
};

}  // namespace game
