#pragma once
#include <memory>
#include <string>
#include <vector>

#include "game/GameContext.h"

namespace game {

// ゲーム本体(SFML 非依存)。app 層は Step() に入力を渡し、状態を読んで描画するだけ。
// テストはこのクラスを直接動かす(ヘッドレス)。
class GameSession {
public:
    GameSession(const core::Config& cfg, std::uint32_t seed);

    // 新しいランを開始する(マップ生成・プレイヤー配置・状態を InGame へ)。
    void StartRun(std::uint32_t seed);
    // 入力を渡して1ステップ進める(固定 dt)。Title/Paused/LevelUp/GameOver/Result の遷移もここ。
    void Step(const InputState& input, float dt);

    // ゲームモード(balance.cfg の modes)。選んだモードの `mode.<id>.<キー>` が、次のランの設定を上書きする。
    struct ModeInfo { std::string id, label, desc; };
    const std::vector<ModeInfo>& Modes() const { return modes; }
    int Mode() const { return mode; }
    void SetMode(int index) { if (index >= 0 && index < static_cast<int>(modes.size())) mode = index; }
    bool SetModeById(const std::string& id);

    GameState State() const { return world->ctx.state; }
    bool Won() const { return won; }

    GameContext& Ctx() { return world->ctx; }
    ECS::ECSManager& Ecs() { return world->ecs; }
    MapManager& Map() { return world->map; }
    EntityID PlayerEntity() const { return world->ctx.player; }

    // 建築プレビュー(実行と同じ判定: §14.3)
    BuildPlan PreviewBuild();
    void ApplyUpgrade(int upgradeIndex);

private:
    struct World {
        ECS::ECSManager ecs;
        MapManager map;
        GameContext ctx;
        World(const core::Config& cfg, std::uint32_t seed);
    };
    void Build(std::uint32_t seed);
    void RunFrame(float dt);
    void MakeOffer();

    const core::Config& cfg;
    core::Config activeCfg;   // cfg にモードの上書きを適用したもの(World はこれを参照する)
    std::vector<ModeInfo> modes;
    int mode = 0;
    std::unique_ptr<World> world;
    bool won = false;
};

// マップ生成(将来のランダム生成に対応できるよう分離)。
void GenerateMap(GameContext& g);

}  // namespace game
