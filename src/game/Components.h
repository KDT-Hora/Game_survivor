#pragma once
#include <vector>

#include "game/MapManager.h"

namespace game {

// 連続座標(プレイヤー・敵・アイテム・弾)。岩・壁は GridPosition(タイル座標)で持ち、混同しない。
struct Transform { Vec2 pos; };
struct Collider { float radius = 10.f; };
struct Health {
    float hp = 1.f, maxHp = 1.f;
    float invuln = 0.f;  // 残り無敵時間(プレイヤーのみ使用)
};
struct GridPosition { Vec2i tile; };
struct RockTag {};
struct Building { int def = 0; };

struct PlayerStats {
    float damageMul = 1.f, attackSpeedMul = 1.f, moveSpeedMul = 1.f;
    float wallHpMul = 1.f, pushSpeedMul = 1.f, pickupRangeMul = 1.f;
    float damageReduction = 0.f;
    int buildCostReduction = 0;
    int extraProjectiles = 0;   // 通常攻撃(ボルト)の追加弾数。複数の敵へ飛ぶ
};

struct Player {
    Vec2 aim{1.f, 0.f};
    Vec2 wish;                 // 今フレームの移動入力(正規化済み)
    bool buildRequested = false;
    int selectedWall = 0;
    // 岩押し
    bool pushing = false;
    float pushGrace = 0.f;
    float pushTimer = 0.f;
    Vec2i pushRock{-1, -1};
    Vec2i pushDir{0, 0};       // 押している向き(描画で岩を揺らすのに使う)
};

struct Inventory { int wood = 0, stone = 0, scrap = 0; };
struct Experience { int level = 1, xp = 0, next = 5; };
// 攻撃手段(武器)の所持。強化で新しい手段を獲得し、同じ手段をもう一度選ぶとレベルが上がる。
struct WeaponSlot {
    int def = 0;
    int level = 1;
    float cooldown = 0.f;
};
struct Arsenal { std::vector<WeaponSlot> slots; };

enum class EnemyState { Chase, AttackWall, PushRock };

struct Enemy {
    int def = 0;
    EnemyState state = EnemyState::Chase;
    // 経路(AI と探索の責務分離: ここは結果のみ保持)
    std::vector<Vec2i> path;
    size_t pathIdx = 0;
    bool needsPath = true;     // 探索要求
    bool usingPath = false;
    float repathTimer = 0.f;
    // AI の出力 → 移動 System が読む
    Vec2 target;
    bool hold = false;
    // 障害物処理
    float shotTimer = 0.f;     // 遠距離攻撃までの時間
    float obstacleTimer = 0.f;
    Vec2i obstacleTile{-1, -1};
    float hitFlash = 0.f;      // 被弾してからの残り時間(描画が白く光らせる)
    float lateral = 0.f;       // 追跡時に狙う位置の横ずれ(-1..1)。全員が同じ点に集まらないようにする
};

enum class PickupKind { Xp, Wood, Stone, Scrap, Heal };
struct Pickup {
    PickupKind kind = PickupKind::Xp;
    int amount = 1;
    float born = 0.f;          // 出現した時刻(描画の出現演出用)
};

struct Projectile {
    Vec2 vel;
    float damage = 0.f;
    float life = 1.f;
    bool pierce = false;          // 貫通弾(ビーム): 当たっても消えず、同じ敵には1回だけ当たる
    int kind = 0;                 // 0 = 通常弾, 1 = ビーム(描画用), 2 = 敵の弾
    bool hostile = false;         // true = 敵が撃った弾(プレイヤーに当たる。敵には当たらない)
    float knock = 0.f;            // 命中した敵を進行方向へ押す距離
    std::vector<EntityID> hit;    // 貫通弾が当てた敵
};

// 設置型の爆弾。導火線が尽きると爆発し、爆発範囲の敵にダメージ。爆発後は演出のためしばらく残る。
struct Bomb {
    float fuse = 2.f;
    float blastRadius = 80.f;
    float damage = 0.f;
    float knock = 0.f;            // 爆心から外へ敵を押す距離
    float flash = 0.25f;          // 爆発演出の残り時間
    float flashTotal = 0.25f;     // 爆発演出の長さ(描画の比率用)
    bool exploded = false;
};

}  // namespace game
