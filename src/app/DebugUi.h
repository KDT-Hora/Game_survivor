#pragma once
#include <SFML/Graphics.hpp>

#include "app/Renderer.h"
#include "game/GameSession.h"

namespace app {

// デバッグ用の GUI(Dear ImGui)。F1 でデバッグモードに入り、3つのウィンドウを出す。
//   システム   … 当たり判定の表示・ゲーム速度(早送り)・ゲーム時間
//   プレイヤー … 移動速度・攻撃力・攻撃速度などのステータス、HP、素材、武器
//   敵の出現   … 出現の ON/OFF・出現ペース・種類ごとの割合・手動の出現・全滅
// ゲームの数値は「いまのラン」にだけ効く(新しいランを始めると balance.cfg の値に戻る)。
// ゲーム本体(src/game)は ImGui を知らない。ここから GameSession を直接書き換える。
class DebugUi {
public:
    // 失敗したら false(その場合はデバッグモードを使えない)
    bool Init(sf::RenderWindow& window);
    void Shutdown(sf::RenderWindow& window);

    void ProcessEvent(sf::RenderWindow& window, const sf::Event& ev);
    // 1フレームの先頭で呼ぶ。デバッグモード中はウィンドウを組み立てる。
    void BeginFrame(sf::RenderWindow& window, sf::Time dt, game::GameSession& sim, Renderer& renderer);
    void Render(sf::RenderWindow& window);

    bool Enabled() const { return enabled; }
    void Toggle() { enabled = !enabled; }
    // ゲームの進む速さ(1 = 等速)。デバッグモード以外では常に 1。
    float TimeScale() const { return enabled ? timeScale : 1.f; }
    // マウス・キーボードを ImGui が使っているか(ゲームの入力を止めるため)
    bool WantsMouse() const;
    bool WantsKeyboard() const;

private:
    void SystemWindow(game::GameSession& sim, Renderer& renderer, sf::Time dt);
    void PlayerWindow(game::GameSession& sim);
    void SpawnWindow(game::GameSession& sim);
    void ApplyEveryFrame(game::GameSession& sim);

    bool ready = false;
    bool enabled = false;

    // デバッグの設定(新しいランを始めても残る)
    float timeScale = 1.f;
    bool godMode = false;
    bool spawnEnabled = true;
    int spawnCount = 5;
};

}  // namespace app
