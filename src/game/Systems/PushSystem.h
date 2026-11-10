#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 岩押し(§7)。岩に触れたまま岩の方向へ入力し続けると、一定時間後に1マス動く。
class PushSystem : public GameSystem {
public:
    explicit PushSystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Player, Transform, Collider, PlayerStats>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        for (EntityID e : mEntities) {
            auto& player = ecs.GetComponent<Player>(e);
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const float radius = ecs.GetComponent<Collider>(e).radius;
            const auto& stats = ecs.GetComponent<PlayerStats>(e);
            const Vec2i playerTile = g.map.WorldToTile(pos);

            // 接触していて、移動入力がその岩の方向を向いている岩を探す
            Vec2i rockTile{-1, -1}, dir{0, 0};
            if (player.wish.LengthSq() > 0.f) {
                for (int dy = -1; dy <= 1 && rockTile.x < 0; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const Vec2i t{playerTile.x + dx, playerTile.y + dy};
                        if (g.map.RockAt(t) == ECS::NULL_ENTITY) continue;
                        if (!CircleOverlapsTile(g.map, pos, radius + g.bal.pushMargin, t)) continue;
                        const Vec2 d{static_cast<float>(dx), static_cast<float>(dy)};
                        const bool diagonal = dx != 0 && dy != 0;
                        const float need = diagonal ? 0.92f : 0.5f;   // 斜めは入力が斜めのときだけ押す
                        if (Dot(player.wish, d.Normalized()) < need) continue;
                        rockTile = t;
                        dir = {dx, dy};
                        break;
                    }
                }
            }

            bool canPush = false;
            if (rockTile.x >= 0) {
                const Vec2i dest = rockTile + dir;
                canPush = CanPushRock(g.map, rockTile, dir) && !EnemyOverlapsTile(g, dest);
            }
            if (canPush) {
                if (player.pushRock != rockTile) { player.pushRock = rockTile; player.pushTimer = 0.f; }
                player.pushDir = dir;
                player.pushGrace = 0.25f;
                player.pushTimer += dt * stats.pushSpeedMul;
                if (player.pushTimer >= g.bal.pushHold) {
                    const EntityID rock = g.map.RockAt(rockTile);
                    const Vec2i dest = rockTile + dir;
                    g.map.MoveRock(rockTile, dest);   // 占有の更新は MapManager に任せる
                    ecs.GetComponent<GridPosition>(rock).tile = dest;
                    player.pushRock = dest;
                    player.pushTimer = 0.f;
                    ++g.rocksPushed;
                    g.fx.push_back({FxEvent::Kind::RockMoved, g.map.TileCenter(dest), 0.f, 0});
                }
            } else {
                player.pushTimer = 0.f;
                player.pushRock = {-1, -1};
                player.pushDir = {0, 0};
                player.pushGrace = std::max(0.f, player.pushGrace - dt);
            }
            player.pushing = player.pushGrace > 0.f;
        }
    }
};

}  // namespace game
