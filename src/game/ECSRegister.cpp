#include "game/Systems.h"

namespace game {

// System の登録。どのフェーズに入れるかだけがここに集まっている(同じフェーズの中の順番は決めない)。
void RegisterSystems(GameContext& g) {
    auto& ecs = g.ecs;
    auto phase = [](Phase p) { return static_cast<int>(p); };

    // 準備
    ecs.RegisterSystem<SpatialIndexSystem>(phase(Phase::Prepare), SpatialIndexSystem::Init(), g);

    // プレイヤーの操作
    ecs.RegisterSystem<PlayerMovementSystem>(phase(Phase::Control), PlayerMovementSystem::Init(), g);

    // 移動後の位置を使うもの
    ecs.RegisterSystem<PlayerAimSystem>(phase(Phase::Interact), PlayerAimSystem::Init(), g);
    ecs.RegisterSystem<PushSystem>(phase(Phase::Interact), PushSystem::Init(), g);
    ecs.RegisterSystem<EnemySpawnSystem>(phase(Phase::Interact), EnemySpawnSystem::Init(), g);

    // 敵: 考える → 経路を探す → 動く
    ecs.RegisterSystem<EnemyAISystem>(phase(Phase::EnemyThink), EnemyAISystem::Init(), g);
    ecs.RegisterSystem<PathfindingSystem>(phase(Phase::EnemyPath), PathfindingSystem::Init(), g);
    ecs.RegisterSystem<EnemyMovementSystem>(phase(Phase::EnemyMove), EnemyMovementSystem::Init(), g);

    // 建築
    ecs.RegisterSystem<BuildingSystem>(phase(Phase::Build), BuildingSystem::Init(), g);

    // 攻撃手段(増やすときはここに足す)
    ecs.RegisterSystem<BoltWeaponSystem>(phase(Phase::Attack), BoltWeaponSystem::Init(), g);
    ecs.RegisterSystem<BombWeaponSystem>(phase(Phase::Attack), BombWeaponSystem::Init(), g);
    ecs.RegisterSystem<BeamWeaponSystem>(phase(Phase::Attack), BeamWeaponSystem::Init(), g);

    ecs.RegisterSystem<EnemyShootSystem>(phase(Phase::Attack), EnemyShootSystem::Init(), g);   // 遠距離の敵が撃つ

    // 弾・爆弾
    ecs.RegisterSystem<ProjectileSystem>(phase(Phase::Hazard), ProjectileSystem::Init(), g);
    ecs.RegisterSystem<BombSystem>(phase(Phase::Hazard), BombSystem::Init(), g);

    // ダメージ → 死亡・破壊 → ドロップ・回収 → 経験値
    ecs.RegisterSystem<DamageSystem>(phase(Phase::Damage), DamageSystem::Init(), g);
    ecs.RegisterSystem<DeathSystem>(phase(Phase::Resolve), DeathSystem::Init(), g);
    ecs.RegisterSystem<DestructionSystem>(phase(Phase::Resolve), DestructionSystem::Init(), g);
    ecs.RegisterSystem<ItemDropSystem>(phase(Phase::Reward), ItemDropSystem::Init(), g);
    ecs.RegisterSystem<ItemPickupSystem>(phase(Phase::Reward), ItemPickupSystem::Init(), g);
    ecs.RegisterSystem<ExperienceSystem>(phase(Phase::Progress), ExperienceSystem::Init(), g);
}

}  // namespace game
