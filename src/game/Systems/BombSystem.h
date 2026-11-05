#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 設置された爆弾の導火線を進め、尽きたら爆発させる。爆発の範囲内の敵に1回ダメージ(プレイヤーには当たらない)。
class BombSystem : public GameSystem {
public:
    explicit BombSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Bomb, Transform>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            auto& bomb = ecs.GetComponent<Bomb>(e);
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;

            if (bomb.exploded) {   // 爆発の演出が終わったら消す
                bomb.flash -= dt;
                if (bomb.flash <= 0.f) ecs.DestroyLater(e);
                continue;
            }
            bomb.fuse -= dt;
            if (bomb.fuse > 0.f) continue;

            bomb.exploded = true;
            g.enemyGrid.Query(pos, bomb.blastRadius + 40.f, [&](EntityID other) {
                if (!ecs.IsAlive(other)) return;
                auto* t = ecs.TryGet<Transform>(other);
                auto* c = ecs.TryGet<Collider>(other);
                if (t && c && Distance(t->pos, pos) < bomb.blastRadius + c->radius) g.damage.push_back({other, bomb.damage});
            });
        }
    }
};

}  // namespace game
