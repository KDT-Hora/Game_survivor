#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 攻撃手段(武器)の共通の親。プレイヤーの Arsenal の中から、自分の種類のスロットだけを選び、
// クールダウンが終わったら Fire() を呼ぶ。攻撃手段を増やすときは、これを継承して Fire() を書く。
class WeaponSystem : public GameSystem {
public:
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player, Transform, Arsenal, PlayerStats>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            auto& arsenal = ecs.GetComponent<Arsenal>(e);
            const auto& stats = ecs.GetComponent<PlayerStats>(e);
            for (WeaponSlot& slot : arsenal.slots) {
                const WeaponDef& def = g.bal.weapons[slot.def];
                if (def.type != type) continue;
                slot.cooldown -= dt;
                if (slot.cooldown > 0.f) continue;
                if (Fire(ecs, e, def, slot, stats))   // 撃てたときだけクールダウンに入る(敵がいなければ次のフレームも試す)
                    slot.cooldown = Cooldown(def, slot, stats);
            }
        }
    }

protected:
    WeaponSystem(GameContext& c, WeaponType t) : GameSystem(c), type(t) {}

    // 武器を使う。使ったら true。
    virtual bool Fire(ECS::ECSManager& ecs, EntityID player, const WeaponDef& def, const WeaponSlot& slot,
                      const PlayerStats& stats) = 0;

    // 同じ武器をもう一度選ぶとレベルが上がる。レベルに応じた威力とクールダウン。
    static float Damage(const WeaponDef& d, const WeaponSlot& s, const PlayerStats& st) {
        return d.damage * (1.f + d.levelDamage * static_cast<float>(s.level - 1)) * st.damageMul;
    }
    static float Cooldown(const WeaponDef& d, const WeaponSlot& s, const PlayerStats& st) {
        const float mul = std::max(0.4f, 1.f - d.levelCooldown * static_cast<float>(s.level - 1));
        return d.cooldown * mul / st.attackSpeedMul;
    }

private:
    WeaponType type;
};

}  // namespace game
