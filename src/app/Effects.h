#pragma once
#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace app {

// 見た目だけの演出(粒・広がる輪・浮かぶ数字・画面揺れ)。ゲームの状態には触らない。
// 実時間で進めるので、ポーズ中やレベルアップ画面でも止まらず消えていく。
class Effects {
public:
    void Clear();
    void Update(float dt);

    // 粒を散らす。speed は px/秒、gravity は下向きの加速。
    void Burst(sf::Vector2f pos, int count, sf::Color col, float speed, float life, float size, float gravity = 0.f, bool additive = false);
    void Dust(sf::Vector2f pos, int count, sf::Color col);          // 地面の埃(ゆっくり広がって薄くなる)
    void Ring(sf::Vector2f pos, float radius, sf::Color col, float life, float thickness = 2.f);   // 広がって消える輪
    void Flash(sf::Vector2f pos, float radius, sf::Color col, float life);                        // 縮んで消える円
    void Popup(sf::Vector2f pos, const std::string& text, sf::Color col, unsigned size, float life = 0.7f);

    void Shake(float amount);                 // 0..1 を足す。大きいほど強く、二乗で減衰する
    sf::Vector2f ShakeOffset() const { return shakeOffset; }

    void DrawWorld(sf::RenderTarget& t, const sf::Font* font);

    size_t ParticleCount() const { return particles.size(); }

private:
    struct Particle {
        sf::Vector2f pos, vel;
        float life = 0.f, total = 1.f, size = 2.f, gravity = 0.f, drag = 0.f;
        sf::Color col;
        bool additive = false;
    };
    struct Shape {   // 輪・円
        sf::Vector2f pos;
        float radius = 10.f, life = 0.f, total = 0.3f, thickness = 2.f;
        sf::Color col;
        bool ring = true;
    };
    struct Text {
        sf::Vector2f pos, vel;
        float life = 0.f, total = 0.7f;
        std::string text;
        sf::Color col;
        unsigned size = 13;
    };

    float Rand() { rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5; return (rngState >> 8) * (1.f / 16777216.f); }

    std::vector<Particle> particles;
    std::vector<Shape> shapes;
    std::vector<Text> texts;
    float trauma = 0.f;
    sf::Vector2f shakeOffset;
    std::uint32_t rngState = 2463534242u;
};

}  // namespace app
