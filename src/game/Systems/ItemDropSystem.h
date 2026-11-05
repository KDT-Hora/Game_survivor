#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 敵が死んだ位置に、経験値と素材のアイテムを落とす。
class ItemDropSystem : public GameSystem {
public:
    explicit ItemDropSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player>(); }

    void Update(ECS::ECSManager&, float) override {
        for (const DeathRecord& death : g.deaths) {
            const EnemyDef& def = g.bal.enemies[death.enemyDef];
            CreatePickup(g, PickupKind::Xp, def.xp, Jitter(death.pos));
            if (g.rng.Chance(def.dropWoodChance)) CreatePickup(g, PickupKind::Wood, def.dropWoodAmount, Jitter(death.pos));
            if (g.rng.Chance(def.dropStoneChance)) CreatePickup(g, PickupKind::Stone, def.dropStoneAmount, Jitter(death.pos));
        }
        g.deaths.clear();
    }

private:
    // 重ならないよう少しずらす。ずらした先が壁の中なら、ずらさない。
    Vec2 Jitter(Vec2 p) {
        const Vec2 q{p.x + g.rng.Range(-8.f, 8.f), p.y + g.rng.Range(-8.f, 8.f)};
        const Vec2i tile = g.map.WorldToTile(q);
        return (g.map.InBounds(tile) && !g.map.BlocksPlayer(tile)) ? q : p;
    }
};

}  // namespace game
