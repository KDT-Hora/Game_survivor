#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "game/MapManager.h"

namespace app {

// 1ピクセル = 1ワールド単位のドット絵。画像ファイルを持たないものは、起動時にここで描いてテクスチャにする。
struct Pix {
    int w = 0, h = 0;
    std::vector<sf::Color> px;
    Pix() = default;
    Pix(int width, int height) : w(width), h(height), px(static_cast<size_t>(width) * height, sf::Color::Transparent) {}
    bool In(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    void Put(int x, int y, sf::Color c) { if (In(x, y)) px[static_cast<size_t>(y) * w + x] = c; }
    sf::Color At(int x, int y) const { return In(x, y) ? px[static_cast<size_t>(y) * w + x] : sf::Color::Transparent; }
    void Rect(int x, int y, int rw, int rh, sf::Color c);
    void Outline(sf::Color c);          // 不透明な部分の外側1ピクセルを縁取る
    Pix FlippedX() const;
    sf::Image ToImage() const;
    sf::Texture ToTexture() const;      // 補間なし
};

// 文字の並びからドット絵を作る。'.' は透明、ほかの文字は palette で色に対応させる。
Pix PixFromRows(const std::vector<std::string>& rows, const std::map<char, sf::Color>& palette);

// 決定的なハッシュ(地形の模様をランごとに同じにするため)
std::uint32_t Hash(std::uint32_t x, std::uint32_t y, std::uint32_t salt);

// ゲームで使うドット絵一式。
struct Art {
    Art();

    // 主人公(右向き。左向きは反転)。0 = 立ち、1,2 = 歩き
    sf::Texture player[3];
    sf::Texture playerLeft[3];
    sf::Texture rock[3];            // 岩(模様違い)
    sf::Texture wall[3];            // 壁(無傷 / 傷あり / 壊れかけ)
    sf::Texture gemSmall, gemLarge, gemGold;
    sf::Texture wood, stone, heart;
    sf::Texture bomb, bolt, shot;
    sf::Texture iconKill, iconBeam, iconBolt, iconClock;
    sf::Image playerIcon;           // ウィンドウアイコン用(拡大済み)

    // マップ全体の地形を1枚に描く(草・石畳・水・外周。隣接で縁取りが変わる)
    static sf::Image PaintTerrain(const game::MapManager& map, std::uint32_t seed);
};

}  // namespace app
