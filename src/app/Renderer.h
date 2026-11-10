#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "app/Art.h"
#include "app/Effects.h"
#include "game/GameSession.h"

namespace app {

// SFML 描画(地形・岩・壁・敵・アイテム・弾・演出・HUD・各画面)。ゲームロジックは持たない。
// 使い方: 毎フレーム SetMouse → (Step のたびに OnStep) → Update → Draw。
class Renderer {
public:
    explicit Renderer(const std::string& dataDir);

    void SetMouse(sf::Vector2f screenPos) { mouse = screenPos; }
    // Step 直後に呼ぶ。そのステップの出来事(被弾・撃破・爆発など)を演出に変える。
    void OnStep(game::GameSession& sim);
    // 実時間で進める演出(カメラの追従・揺れ・粒・画面の出入り)。
    void Update(game::GameSession& sim, float dt, sf::Vector2f viewSize);

    // プレイヤー追従カメラ。マップ外周ではカメラ移動を制限する(§14.1)。
    sf::View GameView(game::GameSession& sim, sf::Vector2f size) const;
    void Draw(sf::RenderTarget& target, game::GameSession& sim, bool showStatus);

    // デバッグ表示(F1 のデバッグモードから切り替える)
    struct DebugView {
        bool colliders = false;   // 当たり判定(円・タイル)を重ねて描く
        bool paths = false;       // 敵の経路を線で描く
    } debug;

    // 画面上の矩形(描画と入力判定で共有)
    static sf::FloatRect CardRect(int index, int count, sf::Vector2f size);   // レベルアップの選択肢
    static sf::FloatRect ModeRect(int index, int count, sf::Vector2f size);   // タイトルのモード

    const sf::Image& WindowIcon() const { return art.playerIcon; }
    float zoom = 1.f;   // 確認用の拡大率(--zoom)。HUD は拡大しない

private:
    enum class Align { Left, Center, Right };

    void EnsureSprites(const game::Balance& bal);
    void EnsureTerrain(game::GameSession& sim);
    sf::Vector2f CameraTarget(game::GameSession& sim, sf::Vector2f size) const;

    void DrawWorld(sf::RenderTarget& t, game::GameSession& sim);
    void DrawColliders(sf::RenderTarget& t, game::GameSession& sim);
    void DrawBuildPreview(sf::RenderTarget& t, game::GameSession& sim);
    void DrawHud(sf::RenderTarget& t, game::GameSession& sim, bool showStatus);
    void DrawStatusPanel(sf::RenderTarget& t, game::GameSession& sim, sf::Vector2f center);
    void DrawVignette(sf::RenderTarget& t, game::GameSession& sim);
    void DrawOverlay(sf::RenderTarget& t, game::GameSession& sim);
    void DrawTitle(sf::RenderTarget& t, game::GameSession& sim);
    void DrawLevelUp(sf::RenderTarget& t, game::GameSession& sim);
    void DrawResult(sf::RenderTarget& t, game::GameSession& sim);

    // 部品
    sf::Vector2f Text(sf::RenderTarget& t, const std::string& utf8, sf::Vector2f pos, unsigned size, sf::Color c,
                      Align align = Align::Left, float outline = -1.f);
    void Panel(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill, sf::Color edge, float thickness = 2.f);
    void Bar(sf::RenderTarget& t, sf::FloatRect r, float ratio, sf::Color fill, sf::Color back = sf::Color(16, 14, 22, 220));
    void Icon(sf::RenderTarget& t, const sf::Texture& tex, sf::Vector2f pos, float scale, sf::Color tint = sf::Color::White, bool centered = true);
    void Shadow(sf::RenderTarget& t, sf::Vector2f pos, float rx, float ry, std::uint8_t alpha = 70);

    sf::Font font;
    bool hasFont = false;
    std::string dataDir;
    Art art;
    Effects fx;

    bool spritesLoaded = false;
    std::vector<std::vector<sf::Texture>> enemyFrames;   // 敵の種類ごとのアニメーションコマ(空ならフォールバックの円)
    std::vector<sf::Color> enemyColors;                  // 撃破の粒の色(画像の平均色)

    sf::Texture terrain;
    std::uint32_t terrainRun = 0xffffffffu;

    // カメラ・演出の状態(実時間)
    sf::Vector2f camera;
    bool cameraValid = false;
    sf::Vector2f mouse;
    float clock = 0.f;            // 実時間の経過(UI のアニメーション用)
    float stateTime = 0.f;        // いまの画面になってからの時間
    game::GameState lastState = game::GameState::Title;
    std::uint32_t lastRun = 0xffffffffu;
    float hurt = 0.f;             // 被弾の赤み(0..1)
    float bossBanner = 0.f;       // ボス出現の表示の残り時間
    float buildFailTime = 0.f;    // 建築失敗の表示の残り時間
    std::string buildFailText;
    bool facingLeft = false;
};

}  // namespace app
