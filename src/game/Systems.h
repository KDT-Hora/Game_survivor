#pragma once
// すべての System をまとめて取り込む入口。System 1つにつき Systems/ の下に1ファイル。
#include "game/Systems/BeamWeaponSystem.h"
#include "game/Systems/BoltWeaponSystem.h"
#include "game/Systems/BombSystem.h"
#include "game/Systems/BombWeaponSystem.h"
#include "game/Systems/BuildingSystem.h"
#include "game/Systems/DamageSystem.h"
#include "game/Systems/DeathSystem.h"
#include "game/Systems/DestructionSystem.h"
#include "game/Systems/EnemyAISystem.h"
#include "game/Systems/EnemyMovementSystem.h"
#include "game/Systems/EnemyShootSystem.h"
#include "game/Systems/EnemySpawnSystem.h"
#include "game/Systems/ExperienceSystem.h"
#include "game/Systems/ItemDropSystem.h"
#include "game/Systems/ItemPickupSystem.h"
#include "game/Systems/PathfindingSystem.h"
#include "game/Systems/Phase.h"
#include "game/Systems/PlayerAimSystem.h"
#include "game/Systems/PlayerMovementSystem.h"
#include "game/Systems/ProjectileSystem.h"
#include "game/Systems/PushSystem.h"
#include "game/Systems/SpatialIndexSystem.h"

namespace game {

// 全 System を、所属するフェーズつきで登録する(ECSRegister.cpp)。
void RegisterSystems(GameContext& g);

}  // namespace game
