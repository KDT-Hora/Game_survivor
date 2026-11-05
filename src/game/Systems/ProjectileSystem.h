#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 弾の移動と命中。通常弾は1体に当たると消える。貫通弾(ビーム)は敵を通り抜け、同じ敵には1回だけ当たる。
// 敵の弾(hostile)はプレイヤーと壁に当たる。どれも岩・外周で止まる(プレイヤーの壁は通過する)。
class ProjectileSystem : public GameSystem {
public:
    explicit ProjectileSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Projectile, Transform, Collider>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            auto& projectile = ecs.GetComponent<Projectile>(e);
            auto& transform = ecs.GetComponent<Transform>(e);
            const float radius = ecs.GetComponent<Collider>(e).radius;

            projectile.life -= dt;
            const Vec2 from = transform.pos;
            transform.pos += projectile.vel * dt;
            // 寿命切れ、または進んだ線上に岩・外周があれば消す。
            if (projectile.life <= 0.f || !g.map.SegmentClear(from, transform.pos, 0.5f, MapManager::Who::Projectile)) {
                ecs.DestroyLater(e);
                continue;
            }

            if (projectile.hostile) HitPlayerOrWall(ecs, e, projectile, transform.pos, radius);
            else if (projectile.pierce) HitAll(ecs, projectile, transform.pos, radius);
            else HitNearest(ecs, e, projectile, transform.pos, radius);
        }
    }

private:
    // 敵の弾: 壁に当たったら壁にダメージを与えて消える。プレイヤーに当たったらダメージを与えて消える。
    void HitPlayerOrWall(ECS::ECSManager& ecs, EntityID self, const Projectile& projectile, Vec2 pos, float radius) {
        const EntityID wall = g.map.WallAt(g.map.WorldToTile(pos));
        if (wall != ECS::NULL_ENTITY) {
            g.damage.push_back({wall, projectile.damage});
            ecs.DestroyLater(self);
            return;
        }
        if (!ecs.IsAlive(g.player)) return;
        const float playerRadius = ecs.GetComponent<Collider>(g.player).radius;
        if (Distance(ecs.GetComponent<Transform>(g.player).pos, pos) < playerRadius + radius) {
            g.damage.push_back({g.player, projectile.damage});
            ecs.DestroyLater(self);
        }
    }

    // 貫通弾: 重なっている敵すべてに、1体につき1回ダメージ。
    void HitAll(ECS::ECSManager& ecs, Projectile& projectile, Vec2 pos, float radius) {
        g.enemyGrid.Query(pos, 40.f, [&](EntityID other) {
            if (!ecs.IsAlive(other)) return;
            auto* t = ecs.TryGet<Transform>(other);
            auto* c = ecs.TryGet<Collider>(other);
            if (!t || !c || Distance(t->pos, pos) >= c->radius + radius) return;
            if (std::find(projectile.hit.begin(), projectile.hit.end(), other) != projectile.hit.end()) return;
            projectile.hit.push_back(other);
            g.damage.push_back({other, projectile.damage});
        });
    }

    // 通常弾: 重なっている中でいちばん近い敵に当たって消える。
    void HitNearest(ECS::ECSManager& ecs, EntityID self, const Projectile& projectile, Vec2 pos, float radius) {
        EntityID hit = ECS::NULL_ENTITY;
        float best = 1e30f;
        g.enemyGrid.Query(pos, 32.f, [&](EntityID other) {
            if (!ecs.IsAlive(other)) return;
            auto* t = ecs.TryGet<Transform>(other);
            auto* c = ecs.TryGet<Collider>(other);
            if (!t || !c) return;
            const float dist = Distance(t->pos, pos);
            if (dist < c->radius + radius && dist < best) { best = dist; hit = other; }
        });
        if (hit == ECS::NULL_ENTITY) return;
        g.damage.push_back({hit, projectile.damage});
        ecs.DestroyLater(self);
    }
};

}  // namespace game
