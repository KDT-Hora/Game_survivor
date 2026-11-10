#pragma once
#include <algorithm>

#include "game/Systems/GameSystem.h"

namespace game {

// 敵の行動決定。見通しがあれば直進、なければ経路に沿う。壁・岩にぶつかったら能力に応じて対処する(§8)。
// ここでは「どこへ向かうか(target)」と「止まるか(hold)」を決めるだけで、実際の移動は EnemyMovementSystem が行う。
class EnemyAISystem : public GameSystem {
public:
    explicit EnemyAISystem(GameContext& c) : GameSystem(c) {}
    static ECS::Signature Init() { return ECS::ECSManager::MakeSignature<Enemy, Transform, Collider, Health>(); }

    void Update(ECS::ECSManager& ecs, float dt) override {
        if (!ecs.IsAlive(g.player)) return;
        const Vec2 playerPos = ecs.GetComponent<Transform>(g.player).pos;
        for (EntityID e : mEntities) {
            auto& enemy = ecs.GetComponent<Enemy>(e);
            const Vec2 pos = ecs.GetComponent<Transform>(e).pos;
            const float radius = ecs.GetComponent<Collider>(e).radius;
            const EnemyDef& def = g.bal.enemies[enemy.def];
            enemy.hold = false;
            enemy.hitFlash = std::max(0.f, enemy.hitFlash - dt);

            // 1. 見通しがあれば直進(経路探索を節約し、動きも滑らかになる)
            if (Distance(pos, playerPos) < 420.f && g.map.SegmentClear(pos, playerPos, radius * 0.8f, WhoOf(def))) {
                enemy.usingPath = false;
                // 全員が同じ点を目指すと一列の団子になるので、個体ごとに少し横へずらした点を狙う。近づいたらずれは消える。
                const Vec2 to = playerPos - pos;
                const Vec2 side{-to.y, to.x};
                const float spread = std::min(1.f, Distance(pos, playerPos) / 160.f) * radius * 1.5f;
                enemy.target = playerPos + side.Normalized() * (enemy.lateral * spread);
                enemy.state = EnemyState::Chase;
                enemy.obstacleTimer = 0.f;
                if (def.range > 0.f && Distance(pos, playerPos) < def.range * 0.85f) enemy.hold = true;   // 遠距離敵は射程に入ったら止まって撃つ
                continue;
            }
            if (!enemy.usingPath) {  // 見通しを失った: 古い経路は捨てて新しく探索する
                enemy.path.clear();
                enemy.pathIdx = 0;
                enemy.needsPath = true;
            }
            enemy.usingPath = true;

            // 2. 経路に沿って進む(着いた通過点は飛ばす)
            while (enemy.pathIdx < enemy.path.size() &&
                   (Distance(pos, g.map.TileCenter(enemy.path[enemy.pathIdx])) < g.bal.waypointReach ||
                    g.map.WorldToTile(pos) == enemy.path[enemy.pathIdx]))
                ++enemy.pathIdx;
            if (enemy.pathIdx >= enemy.path.size()) {
                // 経路がない/終点(到達できず、いちばん近い点に着いた場合も含む): その場で待って次の探索を待つ。
                enemy.hold = true;
                enemy.target = pos;
                enemy.state = EnemyState::Chase;
                continue;
            }
            const Vec2i next = enemy.path[enemy.pathIdx];
            enemy.target = g.map.TileCenter(next);

            // 3. 次のタイルに障害物があるとき(できることは敵のデータで決まる §8.4)
            const EntityID wall = g.map.WallAt(next), rock = g.map.RockAt(next);
            if (wall != ECS::NULL_ENTITY && def.breaksWalls) {
                AttackWall(enemy, def, pos, radius, next, wall, dt);
            } else if (rock != ECS::NULL_ENTITY && def.pushesRocks) {
                PushOrBreakRock(ecs, enemy, pos, radius, next, rock, dt);
            } else if (g.map.Blocks(next, WhoOf(def))) {
                // 通常の敵: 経路の先に壁や岩ができた。通り抜けず、すぐ探索し直す。
                enemy.needsPath = true;
                enemy.hold = true;
            } else {
                enemy.state = EnemyState::Chase;
                enemy.obstacleTimer = 0.f;
            }
        }
    }

private:
    static int Sign(int v) { return v > 0 ? 1 : (v < 0 ? -1 : 0); }

    // 壁に接したら止まって攻撃する。
    void AttackWall(Enemy& enemy, const EnemyDef& def, Vec2 pos, float radius, Vec2i wallTile, EntityID wall, float dt) {
        if (!CircleOverlapsTile(g.map, pos, radius + 4.f, wallTile)) return;
        enemy.state = EnemyState::AttackWall;
        enemy.hold = true;
        g.damage.push_back({wall, def.wallDps * dt});
    }

    // 岩に接したら押す。押せない岩は、一定時間後に壊す(§7.3)。
    void PushOrBreakRock(ECS::ECSManager& ecs, Enemy& enemy, Vec2 pos, float radius, Vec2i rockTile, EntityID rock, float dt) {
        if (!CircleOverlapsTile(g.map, pos, radius + 4.f, rockTile)) return;
        enemy.state = EnemyState::PushRock;
        enemy.hold = true;
        if (enemy.obstacleTile != rockTile) { enemy.obstacleTile = rockTile; enemy.obstacleTimer = 0.f; }
        enemy.obstacleTimer += dt;

        const Vec2i enemyTile = g.map.WorldToTile(pos);
        const Vec2i dir{Sign(rockTile.x - enemyTile.x), Sign(rockTile.y - enemyTile.y)};
        // 押し先に敵がいない・岩の陰のプレイヤーを押し潰さない(§5.1)
        const bool canPush = (dir.x != 0 || dir.y != 0) && CanPushRock(g.map, rockTile, dir) &&
                             !EnemyOverlapsTile(g, rockTile + dir) && !PlayerOverlapsTile(g, rockTile + dir);
        if (canPush && enemy.obstacleTimer >= g.bal.rockPushHold) {
            g.map.MoveRock(rockTile, rockTile + dir);
            ecs.GetComponent<GridPosition>(rock).tile = rockTile + dir;
            g.fx.push_back({FxEvent::Kind::RockMoved, g.map.TileCenter(rockTile + dir), 0.f, 1});
            enemy.obstacleTimer = 0.f;
        } else if (!canPush && enemy.obstacleTimer >= g.bal.rockBreakAfter) {
            g.map.RemoveRock(rockTile);
            ecs.DestroyLater(rock);
            g.fx.push_back({FxEvent::Kind::RockMoved, g.map.TileCenter(rockTile), 0.f, 2});
            enemy.obstacleTimer = 0.f;
        }
    }
};

}  // namespace game
