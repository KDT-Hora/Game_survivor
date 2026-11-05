#pragma once
#include <cmath>
#include <cstdint>
#include <functional>

namespace core {

// 連続ワールド座標(プレイヤー・敵・アイテム・弾)。タイル座標 Vec2i と型で区別する。
struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
    float LengthSq() const { return x * x + y * y; }
    float Length() const { return std::sqrt(LengthSq()); }
    Vec2 Normalized() const {
        float l = Length();
        return l > 1e-6f ? Vec2{x / l, y / l} : Vec2{0.f, 0.f};
    }
};
inline float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float Distance(Vec2 a, Vec2 b) { return (a - b).Length(); }

// タイル座標(岩・壁・地形)。
struct Vec2i {
    int x = 0, y = 0;
    Vec2i() = default;
    Vec2i(int x_, int y_) : x(x_), y(y_) {}
    bool operator==(Vec2i o) const { return x == o.x && y == o.y; }
    bool operator!=(Vec2i o) const { return !(*this == o); }
    Vec2i operator+(Vec2i o) const { return {x + o.x, y + o.y}; }
    Vec2i operator-(Vec2i o) const { return {x - o.x, y - o.y}; }
};

// 8方向の固定優先順位(同距離のタイブレークに使う): 上, 右, 下, 左, 右上, 右下, 左下, 左上。
inline const Vec2i* Dir8() {
    static const Vec2i d[8] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};
    return d;
}

// 方向ベクトルを最も近い8方向へ量子化する。ゼロベクトルなら (0,0)。
inline Vec2i QuantizeDir8(Vec2 v) {
    if (v.LengthSq() < 1e-8f) return {0, 0};
    const float a = std::atan2(v.y, v.x);  // -pi..pi, y は下向き正
    int k = static_cast<int>(std::lround(a / (3.14159265358979f / 4.f)));  // -4..4
    k = (k + 8) % 8;                                                       // 0..7: 0=右, 1=右下, 2=下 ...
    static const Vec2i table[8] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    return table[k];
}

// 決定的な乱数(xorshift32)。ラン単位の再現性のため標準の乱数は使わない。
class Rng {
public:
    explicit Rng(std::uint32_t seed = 1) : s(seed ? seed : 1) {}
    std::uint32_t Next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float Float() { return (Next() >> 8) * (1.0f / 16777216.0f); }       // [0,1)
    float Range(float a, float b) { return a + (b - a) * Float(); }
    int Int(int a, int b) { return a + static_cast<int>(Next() % static_cast<std::uint32_t>(b - a + 1)); }  // [a,b]
    bool Chance(float p) { return Float() < p; }

private:
    std::uint32_t s;
};

}  // namespace core

namespace std {
template <>
struct hash<core::Vec2i> {
    size_t operator()(core::Vec2i v) const noexcept {
        return static_cast<size_t>(static_cast<std::uint32_t>(v.x)) * 73856093u ^
               static_cast<size_t>(static_cast<std::uint32_t>(v.y)) * 19349663u;
    }
};
}  // namespace std
