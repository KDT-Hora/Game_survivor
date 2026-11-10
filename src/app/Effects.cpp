#include "app/Effects.h"

#include <algorithm>
#include <cmath>

namespace app {

namespace {
const float kPi = 3.14159265358979f;
const size_t kMaxParticles = 1500;
const size_t kMaxTexts = 80;
}  // namespace

void Effects::Clear() {
    particles.clear();
    shapes.clear();
    texts.clear();
    trauma = 0.f;
    shakeOffset = {};
}

void Effects::Update(float dt) {
    for (auto& p : particles) {
        p.life -= dt;
        p.vel.y += p.gravity * dt;
        p.vel *= std::max(0.f, 1.f - p.drag * dt);
        p.pos += p.vel * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) { return p.life <= 0.f; }), particles.end());
    for (auto& s : shapes) s.life -= dt;
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(), [](const Shape& s) { return s.life <= 0.f; }), shapes.end());
    for (auto& t : texts) {
        t.life -= dt;
        t.vel *= std::max(0.f, 1.f - 3.f * dt);
        t.pos += t.vel * dt;
    }
    texts.erase(std::remove_if(texts.begin(), texts.end(), [](const Text& t) { return t.life <= 0.f; }), texts.end());

    trauma = std::max(0.f, trauma - dt * 1.8f);
    const float k = trauma * trauma * 7.f;
    shakeOffset = {(Rand() * 2.f - 1.f) * k, (Rand() * 2.f - 1.f) * k};
}

void Effects::Burst(sf::Vector2f pos, int count, sf::Color col, float speed, float life, float size, float gravity, bool additive) {
    for (int i = 0; i < count && particles.size() < kMaxParticles; ++i) {
        Particle p;
        const float a = Rand() * 2.f * kPi, v = speed * (0.4f + 0.6f * Rand());
        p.pos = pos;
        p.vel = {std::cos(a) * v, std::sin(a) * v};
        p.total = p.life = life * (0.6f + 0.4f * Rand());
        p.size = size * (0.7f + 0.6f * Rand());
        p.gravity = gravity;
        p.drag = 2.5f;
        p.col = col;
        p.additive = additive;
        particles.push_back(p);
    }
}

void Effects::Dust(sf::Vector2f pos, int count, sf::Color col) {
    for (int i = 0; i < count && particles.size() < kMaxParticles; ++i) {
        Particle p;
        const float a = Rand() * 2.f * kPi, v = 20.f + 30.f * Rand();
        p.pos = {pos.x + (Rand() - 0.5f) * 14.f, pos.y + (Rand() - 0.5f) * 10.f};
        p.vel = {std::cos(a) * v, std::sin(a) * v * 0.5f - 10.f};
        p.total = p.life = 0.35f + 0.3f * Rand();
        p.size = 2.f + 2.f * Rand();
        p.drag = 4.f;
        p.col = col;
        particles.push_back(p);
    }
}

void Effects::Ring(sf::Vector2f pos, float radius, sf::Color col, float life, float thickness) {
    shapes.push_back({pos, radius, life, life, thickness, col, true});
}

void Effects::Flash(sf::Vector2f pos, float radius, sf::Color col, float life) {
    shapes.push_back({pos, radius, life, life, 0.f, col, false});
}

void Effects::Popup(sf::Vector2f pos, const std::string& text, sf::Color col, unsigned size, float life) {
    if (texts.size() >= kMaxTexts) texts.erase(texts.begin());
    Text t;
    t.pos = {pos.x + (Rand() - 0.5f) * 10.f, pos.y};
    t.vel = {(Rand() - 0.5f) * 30.f, -70.f - 30.f * Rand()};
    t.total = t.life = life;
    t.text = text;
    t.col = col;
    t.size = size;
    texts.push_back(t);
}

void Effects::Shake(float amount) { trauma = std::min(1.f, trauma + amount); }

void Effects::DrawWorld(sf::RenderTarget& t, const sf::Font* font) {
    // 粒(四角)。通常と加算の2回に分けて描く
    for (int pass = 0; pass < 2; ++pass) {
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        for (const auto& p : particles) {
            if (p.additive != (pass == 1)) continue;
            const float k = std::clamp(p.life / p.total, 0.f, 1.f);
            sf::Color c = p.col;
            c.a = static_cast<std::uint8_t>(c.a * std::min(1.f, k * 1.5f));
            const float s = p.size * (0.5f + 0.5f * k);
            const sf::Vector2f a{p.pos.x - s, p.pos.y - s}, b{p.pos.x + s, p.pos.y - s}, cc{p.pos.x + s, p.pos.y + s}, d{p.pos.x - s, p.pos.y + s};
            va.append({a, c}); va.append({b, c}); va.append({cc, c});
            va.append({a, c}); va.append({cc, c}); va.append({d, c});
        }
        if (va.getVertexCount() == 0) continue;
        sf::RenderStates st;
        if (pass == 1) st.blendMode = sf::BlendAdd;
        t.draw(va, st);
    }

    sf::CircleShape circle;
    circle.setPointCount(40);
    for (const auto& s : shapes) {
        const float k = std::clamp(s.life / s.total, 0.f, 1.f);   // 1 → 0
        sf::Color c = s.col;
        if (s.ring) {
            const float r = s.radius * (1.f - 0.6f * k);
            c.a = static_cast<std::uint8_t>(c.a * k);
            circle.setRadius(r);
            circle.setOrigin({r, r});
            circle.setFillColor(sf::Color::Transparent);
            circle.setOutlineColor(c);
            circle.setOutlineThickness(s.thickness);
        } else {
            const float r = s.radius * (0.4f + 0.6f * k);
            c.a = static_cast<std::uint8_t>(c.a * k);
            circle.setRadius(r);
            circle.setOrigin({r, r});
            circle.setFillColor(c);
            circle.setOutlineThickness(0.f);
        }
        circle.setPosition(s.pos);
        t.draw(circle, sf::RenderStates(sf::BlendAdd));
    }

    if (!font) return;
    for (const auto& x : texts) {
        const float k = std::clamp(x.life / x.total, 0.f, 1.f);
        sf::Text text(*font, sf::String::fromUtf8(x.text.begin(), x.text.end()), x.size);
        sf::Color c = x.col;
        c.a = static_cast<std::uint8_t>(255 * std::min(1.f, k * 2.f));
        text.setFillColor(c);
        text.setOutlineColor(sf::Color(20, 16, 28, c.a));
        text.setOutlineThickness(1.5f);
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, b.position.y + b.size.y / 2.f});
        text.setPosition({std::round(x.pos.x), std::round(x.pos.y)});
        t.draw(text);
    }
}

}  // namespace app
