#pragma once
#include "game/Systems/WeaponSystem.h"

namespace game {

// 時限爆弾: 自分のいる場所に爆弾を置く。爆発は BombSystem が行う。
class BombWeaponSystem : public WeaponSystem {
public:
    explicit BombWeaponSystem(GameContext& c) : WeaponSystem(c, WeaponType::Bomb) {}

protected:
    bool Fire(ECS::ECSManager& ecs, EntityID player, const WeaponDef& def, const WeaponSlot& slot,
              const PlayerStats& stats) override {
        const Vec2 pos = ecs.GetComponent<Transform>(player).pos;
        CreateBomb(g, pos, def.fuse, def.blastRadius, Damage(def, slot, stats), def.flash);
        return true;
    }
};

}  // namespace game
