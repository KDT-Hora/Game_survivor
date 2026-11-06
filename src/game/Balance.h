#pragma once
#include <string>
#include <vector>

#include "core/Config.h"

namespace game {

// balance.cfg から読み込んだ数値(ホットパス用にキャッシュ)。ソースに数値を直書きしない(§23-5)。
struct EnemyDef {
    std::string name;
    float hp, speed, radius, contactDamage;
    int xp;
    bool breaksWalls, pushesRocks;
    bool ignoresWalls = false;   // true なら、プレイヤーの壁をすり抜ける(地形と岩には止められる)
    float wallDps;
    float range = 0, shotDamage = 0, shotSpeed = 0, shotCooldown = 1;   // range > 0 なら遠距離攻撃をする敵
    float dropWoodChance; int dropWoodAmount;
    float dropStoneChance; int dropStoneAmount;
    std::string sprite;   // 画像フォルダ名(sprite.dir 配下。0.png, 1.png ... をアニメーションに使う)
    float drawSize = 32;  // 描画サイズ(ワールド単位)。当たり判定の半径とは別
};

struct WallDef {
    std::string name;
    float hp;
    int costWood, costStone;
};

enum class WeaponType { Bolt, Bomb, Beam };

struct WeaponDef {
    std::string id, name;
    WeaponType type = WeaponType::Bolt;
    float damage = 0, cooldown = 1, range = 0, speed = 0, radius = 0, lifetime = 0;
    float fuse = 0, blastRadius = 0, flash = 0;   // 爆弾
    float levelDamage = 0, levelCooldown = 0;     // レベル1つあたりの威力増加率・クールダウン短縮率
};

enum class UpgradeStat { Damage, AttackSpeed, MaxHp, DamageReduction, MoveSpeed, WallHp, BuildCost, PushSpeed, PickupRange,
                         GrantWeapon, ExtraProjectiles };

struct UpgradeDef {
    std::string id, name, category, desc;
    UpgradeStat stat;
    float amount;
    int weapon = -1;   // GrantWeapon のとき bal.weapons の添字
};

struct Balance {
    float tile, duration, step;
    int mapW, mapH, safeRadius, rockCount, waterClusters, stoneClusters, clusterSize, resourceCount, resourceMax;
    float resourceRespawn;

    float playerHp, playerSpeed, playerRadius, playerInvuln, pickupRange;
    int startWood, startStone;
    float pushSpeedMul, pushHold, pushMargin;

    float wallPathCost, rockPathCost, rockPushHold, rockBreakAfter, separation, waypointReach;
    float repathInterval;
    int pathBudget, pathMaxExpansions;

    std::string spriteDir;   // data フォルダからの相対パス
    float animFps = 6.f;

    float shooterStart, shooterRatioStart, shooterRatioPerMin;   // 遠距離敵の出現
    float ghostStart, ghostRatioStart, ghostRatioPerMin;         // 壁をすり抜ける敵の出現
    float spawnMin, spawnMax, rateBase, ratePerMin;
    int maxAlive;
    float bigStart, bigRatioStart, bigRatioPerMin, hpGrowthPerMin;

    int xpBase, xpGrowth, levelChoices;
    float pickupRadius;

    std::vector<EnemyDef> enemies;
    std::vector<WallDef> walls;
    std::vector<WeaponDef> weapons;
    std::vector<UpgradeDef> upgrades;

    int EnemyIndex(const std::string& name) const;
    static Balance Load(const core::Config& c);
};

}  // namespace game
