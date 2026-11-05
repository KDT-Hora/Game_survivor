#pragma once
#include "ecs/ECSManager.h"
#include "game/GameContext.h"

namespace game {

// ゲームの System の共通の親。ゲーム全体の状態(GameContext)への参照 g を持つ。
// 各 System は「Init() で必要な Component の組を返し、Update() で処理する」(rimrim と同じ形)。
class GameSystem : public ECS::System {
protected:
    explicit GameSystem(GameContext& context) : g(context) {}
    GameContext& g;
};

}  // namespace game
