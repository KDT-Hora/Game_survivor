#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 敵の死亡。HP が 0 以下の敵を消し、ドロップ用に死んだ位置を記録する。
class DeathSystem : public GameSystem {
public:
    explicit DeathSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Enemy, Health, Transform>(); }

    void Update(ECS::ECSManager& ecs, float) override {
        for (EntityID e : mEntities) {
            if (ecs.GetComponent<Health>(e).hp > 0.f) continue;
            g.deaths.push_back({ecs.GetComponent<Transform>(e).pos, ecs.GetComponent<Enemy>(e).def});
            ++g.kills;
            if (e == g.boss) g.bossKilled = true;
            ecs.DestroyLater(e);   // ループの途中で消さず、予約だけする(フレームの最後に Flush で消える)
        }
    }
};

}  // namespace game
