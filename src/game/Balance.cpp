#include "game/Balance.h"

#include <stdexcept>

namespace game {

int Balance::EnemyIndex(const std::string& name) const {
    for (size_t i = 0; i < enemies.size(); ++i)
        if (enemies[i].name == name) return static_cast<int>(i);
    throw std::runtime_error("balance: 敵定義がありません: " + name);
}

static UpgradeStat ParseStat(const std::string& s) {
    if (s == "damage") return UpgradeStat::Damage;
    if (s == "attack_speed") return UpgradeStat::AttackSpeed;
    if (s == "max_hp") return UpgradeStat::MaxHp;
    if (s == "damage_reduction") return UpgradeStat::DamageReduction;
    if (s == "move_speed") return UpgradeStat::MoveSpeed;
    if (s == "wall_hp") return UpgradeStat::WallHp;
    if (s == "build_cost") return UpgradeStat::BuildCost;
    if (s == "push_speed") return UpgradeStat::PushSpeed;
    if (s == "pickup_range") return UpgradeStat::PickupRange;
    if (s == "grant_weapon") return UpgradeStat::GrantWeapon;
    if (s == "extra_projectiles") return UpgradeStat::ExtraProjectiles;
    throw std::runtime_error("balance: 不明な強化 stat: " + s);
}

Balance Balance::Load(const core::Config& c) {
    Balance b;
    b.tile = c.F("tile.size");
    b.duration = c.F("game.duration");
    b.step = c.F("game.step");
    b.mapW = c.I("map.width");
    b.mapH = c.I("map.height");
    b.safeRadius = c.I("map.safe_radius");
    b.rockCount = c.I("map.rock_count");
    b.waterClusters = c.I("map.water_clusters");
    b.stoneClusters = c.I("map.stone_clusters");
    b.clusterSize = c.I("map.cluster_size");
    b.resourceCount = c.I("map.resource_count");
    b.resourceMax = c.I("map.resource_max");
    b.resourceRespawn = c.F("map.resource_respawn");

    b.playerHp = c.F("player.hp");
    b.playerSpeed = c.F("player.speed");
    b.playerRadius = c.F("player.radius");
    b.playerInvuln = c.F("player.invuln");
    b.pickupRange = c.F("player.pickup_range");
    b.startWood = c.I("player.start_wood");
    b.startStone = c.I("player.start_stone");
    b.pushSpeedMul = c.F("push.speed_mul");
    b.pushHold = c.F("push.hold_time");
    b.pushMargin = c.F("push.contact_margin");

    b.wallPathCost = c.F("enemy.wall_path_cost");
    b.rockPathCost = c.F("enemy.rock_path_cost");
    b.rockPushHold = c.F("enemy.rock_push_hold");
    b.rockBreakAfter = c.F("enemy.rock_break_after");
    b.separation = c.F("enemy.separation");
    b.waypointReach = c.F("enemy.waypoint_reach");
    b.repathInterval = c.F("path.repath_interval");
    b.pathBudget = c.I("path.budget_per_frame");
    b.pathMaxExpansions = c.I("path.max_expansions");

    b.spriteDir = c.S("sprite.dir");
    b.animFps = c.F("sprite.anim_fps");
    b.spawnMin = c.F("spawn.min_dist");
    b.spawnMax = c.F("spawn.max_dist");
    b.rateBase = c.F("spawn.rate_base");
    b.ratePerMin = c.F("spawn.rate_per_min");
    b.maxAlive = c.I("spawn.max_alive");
    b.shooterStart = c.F("spawn.shooter_start");
    b.shooterRatioStart = c.F("spawn.shooter_ratio_start");
    b.shooterRatioPerMin = c.F("spawn.shooter_ratio_per_min");
    b.bigStart = c.F("spawn.big_start");
    b.bigRatioStart = c.F("spawn.big_ratio_start");
    b.bigRatioPerMin = c.F("spawn.big_ratio_per_min");
    b.hpGrowthPerMin = c.F("spawn.hp_growth_per_min");

    b.xpBase = c.I("level.xp_base");
    b.xpGrowth = c.I("level.xp_growth");
    b.levelChoices = c.I("level.choices");
    b.pickupRadius = c.F("pickup.radius");

    for (const auto& n : c.List("enemies")) {
        const std::string p = "enemy." + n + ".";
        EnemyDef e;
        e.name = n;
        e.hp = c.F(p + "hp");
        e.speed = c.F(p + "speed");
        e.radius = c.F(p + "radius");
        e.contactDamage = c.F(p + "contact_damage");
        e.xp = c.I(p + "xp");
        e.breaksWalls = c.B(p + "breaks_walls");
        e.pushesRocks = c.B(p + "pushes_rocks");
        e.wallDps = c.F(p + "wall_dps");
        e.range = c.F(p + "range");
        e.shotDamage = c.F(p + "shot_damage");
        e.shotSpeed = c.F(p + "shot_speed");
        e.shotCooldown = c.F(p + "shot_cooldown");
        e.dropWoodChance = c.F(p + "drop_wood_chance");
        e.dropWoodAmount = c.I(p + "drop_wood_amount");
        e.dropStoneChance = c.F(p + "drop_stone_chance");
        e.dropStoneAmount = c.I(p + "drop_stone_amount");
        e.sprite = c.S(p + "sprite");
        e.drawSize = c.F(p + "draw_size");
        b.enemies.push_back(e);
    }
    for (const auto& n : c.List("walls")) {
        const std::string p = "wall." + n + ".";
        b.walls.push_back({c.S(p + "name"), c.F(p + "hp"), c.I(p + "cost_wood"), c.I(p + "cost_stone")});
    }
    for (const auto& n : c.List("weapons")) {
        const std::string p = "weapon." + n + ".";
        auto opt = [&](const char* k) { return c.Has(p + k) ? c.F(p + k) : 0.f; };   // 種類により不要な項目は省略可
        WeaponDef w;
        w.id = n;
        w.name = c.S(p + "name");
        const std::string type = c.S(p + "type");
        if (type == "bolt") w.type = WeaponType::Bolt;
        else if (type == "bomb") w.type = WeaponType::Bomb;
        else if (type == "beam") w.type = WeaponType::Beam;
        else throw std::runtime_error("balance: 不明な武器 type: " + type);
        w.damage = c.F(p + "damage");
        w.cooldown = c.F(p + "cooldown");
        w.range = opt("range");
        w.speed = opt("speed");
        w.radius = opt("radius");
        w.lifetime = opt("lifetime");
        w.fuse = opt("fuse");
        w.blastRadius = opt("blast_radius");
        w.flash = opt("flash");
        w.levelDamage = opt("level_damage");
        w.levelCooldown = opt("level_cooldown");
        b.weapons.push_back(w);
    }
    for (const auto& n : c.List("upgrades")) {
        const std::string p = "upgrade." + n + ".";
        UpgradeDef u;
        u.id = n;
        u.name = c.S(p + "name");
        u.category = c.S(p + "category");
        u.desc = c.Has(p + "desc") ? c.S(p + "desc") : "";
        u.stat = ParseStat(c.S(p + "stat"));
        u.amount = c.F(p + "amount");
        if (u.stat == UpgradeStat::GrantWeapon) {
            const std::string wn = c.S(p + "weapon");
            for (size_t i = 0; i < b.weapons.size(); ++i)
                if (b.weapons[i].id == wn) u.weapon = static_cast<int>(i);
            if (u.weapon < 0) throw std::runtime_error("balance: 強化が参照する武器がありません: " + wn);
        }
        b.upgrades.push_back(u);
    }
    return b;
}

}  // namespace game
