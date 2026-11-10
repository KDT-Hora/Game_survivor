#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 経験値がたまったらレベルを上げ、強化の選択待ちを増やす。
class ExperienceSystem : public GameSystem {
public:
    explicit ExperienceSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player, Experience, Transform>(); }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            auto& experience = ecs.GetComponent<Experience>(e);
            while (experience.xp >= experience.next) {
                experience.xp -= experience.next;
                ++experience.level;
                experience.next = g.bal.xpBase + g.bal.xpGrowth * (experience.level - 1);
                ++g.pendingLevelUps;
                g.fx.push_back({FxEvent::Kind::LevelUp, ecs.GetComponent<Transform>(e).pos, 0.f, experience.level});
            }
        }
    }
};

}  // namespace game
