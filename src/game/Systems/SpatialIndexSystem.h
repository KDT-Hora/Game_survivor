#pragma once
#include "game/Systems/GameSystem.h"

namespace game {

// 毎フレームの先頭で、敵・アイテムの空間ハッシュと、描画用の弾・爆弾の一覧を作り直す。
class SpatialIndexSystem : public GameSystem {
public:
    explicit SpatialIndexSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Transform>(); }

    void Update(ECS::ECSManager& ecs, float) override {
        const float W = g.map.Width() * g.map.TileSize(), H = g.map.Height() * g.map.TileSize();
        g.enemyGrid.Reset(64.f, W, H);
        g.pickupGrid.Reset(64.f, W, H);
        g.enemyCount = g.pickupCount = 0;
        g.projectiles.clear();
        g.bombs.clear();
        for (EntityID e : mEntities) {
            if (ecs.HasComponent<Projectile>(e)) {
                g.projectiles.push_back(e);
            } else if (ecs.HasComponent<Bomb>(e)) {
                g.bombs.push_back(e);
            } else if (ecs.HasComponent<Enemy>(e)) {
                g.enemyGrid.Insert(e, ecs.GetComponent<Transform>(e).pos);
                ++g.enemyCount;
            } else if (ecs.HasComponent<Pickup>(e)) {
                g.pickupGrid.Insert(e, ecs.GetComponent<Transform>(e).pos);
                ++g.pickupCount;
            }
        }
    }
};

}  // namespace game
