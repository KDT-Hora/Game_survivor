#pragma once
#include <cmath>

#include "game/Systems/GameSystem.h"

namespace game {

// 敵(通常・大型・ボス)と素材の出現。
class EnemySpawnSystem : public GameSystem {
public:
    explicit EnemySpawnSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        if (!ecs.IsAlive(g.player)) return;
        const Vec2 playerPos = ecs.GetComponent<Transform>(g.player).pos;
        const float minutes = g.time / 60.f;

        if (g.spawnEnabled) SpawnEnemies(ecs, playerPos, minutes, dt);
        SpawnResources(playerPos, dt);
    }

private:
    // 敵の出現(ボスを含む)
    void SpawnEnemies(ECS::ECSManager&, Vec2 playerPos, float minutes, float dt) {
        // ボス(§13.1): 規定時間で1体だけ出現。プレイヤーから遠い側に置く。
        if (!g.bossSpawned && g.time >= g.bal.duration) {
            Vec2 pos;
            if (PickSpawnPos(playerPos, g.bal.spawnMax, g.bal.spawnMax * 1.6f, pos) ||
                PickSpawnPos(playerPos, g.bal.spawnMin, g.bal.spawnMax, pos)) {
                g.boss = CreateEnemy(g, g.bal.EnemyIndex("boss"), pos);
                g.bossSpawned = true;
                g.fx.push_back({FxEvent::Kind::BossAppeared, pos});
            }
        }

        // 通常の出現: 時間経過で増える(§13.2)。ボスが出た後も続ける。
        enemyAcc += (g.bal.rateBase + g.bal.ratePerMin * minutes) * dt;
        while (enemyAcc >= 1.f) {
            enemyAcc -= 1.f;
            int def = g.bal.EnemyIndex("normal");
            if (g.time >= g.bal.ghostStart) {
                const float ratio = g.bal.ghostRatioStart + g.bal.ghostRatioPerMin * ((g.time - g.bal.ghostStart) / 60.f);
                if (g.rng.Chance(ratio)) def = g.bal.EnemyIndex("ghost");
            }
            if (g.time >= g.bal.shooterStart) {
                const float ratio = g.bal.shooterRatioStart + g.bal.shooterRatioPerMin * ((g.time - g.bal.shooterStart) / 60.f);
                if (g.rng.Chance(ratio)) def = g.bal.EnemyIndex("shooter");
            }
            if (g.time >= g.bal.bigStart) {
                const float ratio = g.bal.bigRatioStart + g.bal.bigRatioPerMin * ((g.time - g.bal.bigStart) / 60.f);
                if (g.rng.Chance(ratio)) def = g.bal.EnemyIndex("big");
            }
            // 上限(§22-6)。通常敵で埋まっても、大型敵の枠は少し残す(でないと終盤に大型が出なくなる)。
            const int cap = def == g.bal.EnemyIndex("big") ? g.bal.maxAlive + g.bal.maxAlive / 8 : g.bal.maxAlive;
            if (g.enemyCount >= cap) continue;
            Vec2 pos;
            if (PickSpawnPos(playerPos, g.bal.spawnMin, g.bal.spawnMax, pos)) {
                CreateEnemy(g, def, pos);
                ++g.enemyCount;
            }
        }
    }

    // 素材の補充
    void SpawnResources(Vec2 playerPos, float dt) {
        if (g.bal.resourceRespawn > 0.f) {
            resourceTimer += dt;
            while (resourceTimer >= g.bal.resourceRespawn) {
                resourceTimer -= g.bal.resourceRespawn;
                if (g.pickupCount >= g.bal.resourceMax) continue;
                Vec2 pos;
                if (PickSpawnPos(playerPos, g.bal.spawnMin * 0.5f, g.bal.spawnMax * 1.4f, pos)) {
                    const bool wood = g.rng.Chance(0.75f);
                    CreatePickup(g, wood ? PickupKind::Wood : PickupKind::Stone, wood ? 2 : 1, pos);
                    ++g.pickupCount;
                }
            }
        }
    }

    // center から minD〜maxD 離れた、敵が立てる場所をランダムに探す。
    bool PickSpawnPos(Vec2 center, float minD, float maxD, Vec2& out) {
        const float kPi = 3.14159265358979f;
        for (int i = 0; i < 40; ++i) {
            const float angle = g.rng.Range(0.f, 2.f * kPi), dist = g.rng.Range(minD, maxD);
            const Vec2 p{center.x + std::cos(angle) * dist, center.y + std::sin(angle) * dist};
            const Vec2i tile = g.map.WorldToTile(p);
            if (!g.map.InBounds(tile) || g.map.BlocksEnemy(tile)) continue;
            out = g.map.TileCenter(tile);
            return true;
        }
        return false;
    }

    float enemyAcc = 0.f, resourceTimer = 0.f;
};

}  // namespace game
