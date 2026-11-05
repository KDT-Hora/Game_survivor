#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

#include "game/GameSession.h"

namespace app {

// SFML 描画(地形・岩・壁・敵・アイテム・弾・プレビュー・HUD・各画面)。ゲームロジックは持たない。
class Renderer {
public:
    explicit Renderer(const std::string& dataDir);

    // プレイヤー追従カメラ。マップ外周ではカメラ移動を制限する(§14.1)。
    sf::View GameView(game::GameSession& sim, sf::Vector2f size) const;
    void Draw(sf::RenderTarget& target, game::GameSession& sim, bool showInventory);

    // デバッグ表示(F1 のデバッグモードから切り替える)
    struct DebugView {
        bool colliders = false;   // 当たり判定(円・タイル)を重ねて描く
        bool paths = false;       // 敵の経路を線で描く
    } debug;

    // レベルアップ画面のカード矩形(描画と入力判定で共有)
    static sf::FloatRect CardRect(int index, int count, sf::Vector2f size);

private:
    void EnsureSprites(const game::Balance& bal);
    void DrawWorld(sf::RenderTarget& t, game::GameSession& sim);
    void DrawColliders(sf::RenderTarget& t, game::GameSession& sim);
    void DrawBuildPreview(sf::RenderTarget& t, game::GameSession& sim);
    void DrawHud(sf::RenderTarget& t, game::GameSession& sim, bool showInventory);
    void DrawOverlay(sf::RenderTarget& t, game::GameSession& sim);
    void Text(sf::RenderTarget& t, const std::string& utf8, sf::Vector2f pos, unsigned size, sf::Color c,
              bool centered = false);
    void Bar(sf::RenderTarget& t, sf::FloatRect r, float ratio, sf::Color fill);

    sf::Font font;
    bool hasFont = false;
    std::string dataDir;
    bool spritesLoaded = false;
    std::vector<std::vector<sf::Texture>> enemyFrames;   // 敵の種類ごとのアニメーションコマ(空ならフォールバックの円)
};

}  // namespace app
