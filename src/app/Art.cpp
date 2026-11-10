#include "app/Art.h"

#include <algorithm>
#include <cmath>

namespace app {

using game::MapManager;
using game::Terrain;

// ------------------------------------------------------------------ Pix
void Pix::Rect(int x, int y, int rw, int rh, sf::Color c) {
    for (int j = y; j < y + rh; ++j)
        for (int i = x; i < x + rw; ++i) Put(i, j, c);
}

void Pix::Outline(sf::Color c) {
    Pix src = *this;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (src.At(x, y).a != 0) continue;
            const bool edge = src.At(x - 1, y).a || src.At(x + 1, y).a || src.At(x, y - 1).a || src.At(x, y + 1).a;
            if (edge) Put(x, y, c);
        }
}

Pix Pix::FlippedX() const {
    Pix out(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out.Put(w - 1 - x, y, At(x, y));
    return out;
}

sf::Image Pix::ToImage() const {
    std::vector<std::uint8_t> bytes(px.size() * 4);
    for (size_t i = 0; i < px.size(); ++i) {
        bytes[i * 4 + 0] = px[i].r;
        bytes[i * 4 + 1] = px[i].g;
        bytes[i * 4 + 2] = px[i].b;
        bytes[i * 4 + 3] = px[i].a;
    }
    return sf::Image(sf::Vector2u(static_cast<unsigned>(w), static_cast<unsigned>(h)), bytes.data());
}

sf::Texture Pix::ToTexture() const {
    sf::Texture t(ToImage());
    t.setSmooth(false);
    return t;
}

Pix PixFromRows(const std::vector<std::string>& rows, const std::map<char, sf::Color>& palette) {
    const int h = static_cast<int>(rows.size());
    int w = 0;
    for (const auto& r : rows) w = std::max(w, static_cast<int>(r.size()));
    Pix p(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < static_cast<int>(rows[y].size()); ++x) {
            const char c = rows[y][x];
            if (c == '.' || c == ' ') continue;
            auto it = palette.find(c);
            if (it != palette.end()) p.Put(x, y, it->second);
        }
    return p;
}

std::uint32_t Hash(std::uint32_t x, std::uint32_t y, std::uint32_t salt) {
    std::uint32_t h = x * 0x8da6b343u ^ y * 0xd8163841u ^ salt * 0xcb1ab31fu;
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return h;
}

namespace {

const sf::Color kInk(30, 26, 38);

sf::Color Shade(sf::Color c, int d) {
    auto f = [&](std::uint8_t v) { return static_cast<std::uint8_t>(std::clamp(static_cast<int>(v) + d, 0, 255)); };
    return {f(c.r), f(c.g), f(c.b), c.a};
}

// ---- 主人公。フード付きの外套を着た小柄な旅人。右向き(目が右に寄っている)。20x24。
Pix PlayerFrame(int frame) {
    const std::map<char, sf::Color> pal = {
        {'k', kInk},
        {'h', {208, 92, 52}},     // 外套
        {'H', {150, 58, 38}},     // 外套の陰
        {'s', {240, 200, 164}},   // 肌
        {'S', {206, 150, 116}},   // 肌の陰
        {'e', {30, 26, 38}},      // 目
        {'b', {232, 216, 184}},   // 上着
        {'B', {200, 182, 150}},   // 上着の陰
        {'l', {92, 60, 36}},      // ベルト
        {'y', {220, 170, 70}},    // ベルトの金具
        {'t', {70, 46, 32}},      // 靴
    };
    std::vector<std::string> rows = {
        ".......kkkkkk.......",
        "......khhhhhhk......",
        ".....khhhhhhhhk.....",
        "....khhhhhhhhhhk....",
        "....khhhhhhhhhhk....",
        "....khhhhhhhhhhk....",
        "....kHHssssssHHk....",
        "....kHsssssssssk....",
        "....kHsssssessesk...",
        "....kHsssssssssk....",
        "....kHSSssssSSHk....",
        ".....kHHHHHHHHk.....",
        "...kkkhhkbbbbkhhkkk.",
        "..khhhhkbbbbbbkhhhhk",
        "..khhhhkbbbbbbkhhhhk",
        "..kHhhhkbbbbbBkhhhHk",
        "..kHhhhkllyyllkhhhHk",
        "..kHhhhkbbbbbBkhhhHk",
        "..kHHhhkbbbbbBkhhHHk",
        "..kHHHHkbbbbbBkHHHHk",
        "...kkkkkbbbbBkkkkk..",
        "......kttkkttk......",
        "......kttk.kttk.....",
        "......kkkk.kkkk.....",
    };
    // 顔の行は幅が1つ多いので揃える(右目の分)
    rows[8] = "....kHsssssesesk....";
    if (frame == 1) {   // 左足を前に(短く見せる)
        rows[22] = "......kkkk.kttk.....";
        rows[23] = "...........kkkk.....";
    } else if (frame == 2) {
        rows[22] = "......kttk.kkkk.....";
        rows[23] = "......kkkk..........";
    }
    Pix p = PixFromRows(rows, pal);
    if (frame != 0) {   // 歩くときは体が1ドット上下する
        Pix moved(p.w, p.h);
        for (int y = 0; y < p.h; ++y)
            for (int x = 0; x < p.w; ++x)
                if (y < 21) moved.Put(x, y + 1, p.At(x, y));
                else moved.Put(x, y, p.At(x, y));
        return moved;
    }
    return p;
}

// ---- 岩。超楕円にゆらぎを足した形に、左上の光と右下の陰、ひびを入れる。
Pix RockPix(int variant) {
    const int S = 32;
    Pix p(S, S);
    const std::uint32_t salt = 77u + static_cast<std::uint32_t>(variant) * 31u;
    const float cx = 15.5f + (variant == 1 ? 0.6f : (variant == 2 ? -0.5f : 0.f));
    const float cy = 15.5f + 0.8f;
    const float rx = variant == 2 ? 12.5f : 13.3f, ry = variant == 1 ? 11.6f : 12.4f;
    const sf::Color fill(132, 128, 122), light(170, 166, 156), dark(98, 94, 90), darker(78, 74, 72);
    auto inside = [&](int x, int y) {
        const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
        const float ang = std::atan2(dy, dx);
        const float wob = 0.06f * std::sin(ang * 3.f + static_cast<float>(variant) * 1.7f) + 0.05f * std::sin(ang * 5.f + static_cast<float>(variant));
        const float d = std::pow(std::abs(dx), 2.6f) + std::pow(std::abs(dy), 2.6f);
        return d < 1.f + wob;
    };
    for (int y = 0; y < S; ++y)
        for (int x = 0; x < S; ++x) {
            if (!inside(x, y)) continue;
            const float lx = (x + 0.5f - cx) / rx, ly = (y + 0.5f - cy) / ry;
            const float t = lx * 0.6f + ly;   // 左上 → 右下
            sf::Color c = fill;
            if (t < -0.55f) c = light;
            else if (t > 0.45f) c = dark;
            if (t > 0.85f) c = darker;
            const std::uint32_t h = Hash(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), salt);
            if ((h % 23u) == 0u) c = Shade(c, -14);
            if ((h % 29u) == 1u) c = Shade(c, 12);
            p.Put(x, y, c);
        }
    // ひび: 短い折れ線
    int x = 10 + variant * 4, y = 9 + (variant == 1 ? 6 : 0);
    for (int i = 0; i < 7; ++i) {
        if (p.At(x, y).a) p.Put(x, y, darker);
        const std::uint32_t h = Hash(static_cast<std::uint32_t>(i), static_cast<std::uint32_t>(variant), 5u);
        x += (h & 1u) ? 1 : 0;
        y += (h & 2u) ? 1 : ((h & 4u) ? 0 : 1);
    }
    p.Outline(sf::Color(58, 54, 62));
    return p;
}

// ---- 壁。丸太を4本並べた柵。state 1 はひび、state 2 は1本欠けている。
Pix WallPix(int state) {
    const int S = 32;
    Pix p(S, S);
    const sf::Color woodC(168, 116, 64), lightC(198, 148, 90), darkC(122, 80, 46), rope(104, 78, 50), ropeL(140, 108, 70);
    const int tops[4] = {4, 2, 5, 3};
    for (int log = 0; log < 4; ++log) {
        if (state == 2 && log == 2) continue;   // 欠けた丸太
        const int x0 = 1 + log * 8;              // 幅 7、間隔 1
        const int top = tops[log];
        for (int y = top; y < S - 1; ++y)
            for (int x = x0; x < x0 + 7; ++x) {
                sf::Color c = woodC;
                if (x == x0 + 1 || x == x0 + 2) c = lightC;
                if (x == x0 + 5) c = darkC;
                if (y == top) c = (x == x0 || x == x0 + 6) ? sf::Color::Transparent : lightC;   // 先端は少し丸く
                if (y == top + 1 && (x == x0 || x == x0 + 6)) c = lightC;
                if (c.a) p.Put(x, y, c);
            }
        // 木目
        for (int y = top + 5; y < S - 3; y += 6) p.Put(x0 + 3, y + (log % 2), darkC);
    }
    // 縄
    for (int band : {11, 22})
        for (int x = 0; x < S; ++x) {
            if (!p.At(x, band).a) continue;
            p.Put(x, band, rope);
            p.Put(x, band + 1, ropeL);
        }
    if (state >= 1) {   // ひび
        const int cracks[][2] = {{3, 14}, {4, 15}, {4, 16}, {5, 17}, {19, 7}, {20, 8}, {20, 9}, {27, 18}, {26, 19}, {26, 20}, {11, 25}, {12, 26}};
        for (auto& c : cracks)
            if (p.At(c[0], c[1]).a) p.Put(c[0], c[1], sf::Color(70, 44, 28));
    }
    if (state == 2) {
        for (int x = 1; x < 8; ++x) p.Put(x, 6 + (x % 3), sf::Color::Transparent);   // 先端が欠ける
        const int more[][2] = {{9, 12}, {10, 13}, {10, 14}, {11, 15}, {28, 26}, {29, 27}, {25, 10}, {26, 11}};
        for (auto& c : more)
            if (p.At(c[0], c[1]).a) p.Put(c[0], c[1], sf::Color(70, 44, 28));
    }
    p.Outline(sf::Color(62, 40, 26));
    return p;
}

Pix GemPix(int size, sf::Color fill, sf::Color light, sf::Color dark) {
    // size = 半幅。縦は横より少し長いひし形
    const int w = size * 2 + 1, h = size * 2 + 3;
    Pix p(w, h);
    const float cx = static_cast<float>(size), cy = static_cast<float>(size + 1);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float d = std::abs(x - cx) / (size + 0.5f) + std::abs(y - cy) / (size + 1.5f);
            if (d > 1.f) continue;
            sf::Color c = fill;
            if (x < cx && y < cy) c = light;
            if (x > cx && y > cy) c = dark;
            if (x == static_cast<int>(cx) && y <= cy) c = light;
            p.Put(x, y, c);
        }
    p.Put(static_cast<int>(cx) - 1, static_cast<int>(cy) - 1, sf::Color(255, 255, 255));
    p.Outline(kInk);
    return p;
}

Pix WoodPix() {
    const std::map<char, sf::Color> pal = {
        {'k', kInk}, {'w', {150, 100, 56}}, {'l', {190, 138, 80}}, {'d', {112, 72, 40}}, {'e', {214, 176, 120}}, {'r', {150, 100, 56}},
    };
    return PixFromRows({
        ".kkkkkkkkkk.",
        "klllllllllek",
        "kwwwwwwwwerk",
        "kwwwwwwwwerk",
        "kddwwddwwekk",
        ".kkkkkkkkk..",
    }, pal);
}

Pix StonePix() {
    const std::map<char, sf::Color> pal = {{'k', kInk}, {'s', {176, 176, 184}}, {'l', {216, 216, 222}}, {'d', {122, 122, 130}}};
    return PixFromRows({
        "...kkkk..",
        "..klllsk.",
        ".klsssssk",
        "kssssssdk",
        "kssssdddk",
        ".kdddddk.",
        "..kkkkk..",
    }, pal);
}

Pix HeartPix() {
    const std::map<char, sf::Color> pal = {{'k', kInk}, {'r', {226, 64, 80}}, {'l', {255, 150, 160}}, {'d', {170, 40, 60}}};
    return PixFromRows({
        ".kk..kk.",
        "klrkkrrk",
        "klrrrrrk",
        "krrrrrdk",
        ".krrrdk.",
        "..krdk..",
        "...kk...",
    }, pal);
}

Pix BombPix() {
    const std::map<char, sf::Color> pal = {
        {'k', kInk}, {'b', {52, 48, 62}}, {'l', {98, 94, 110}}, {'f', {160, 120, 70}}, {'s', {255, 220, 120}}, {'m', {170, 160, 180}},
    };
    return PixFromRows({
        "..........s.",
        ".........fk.",
        "....kkkkf...",
        "...kmmmkk...",
        "..kbbbbbbk..",
        ".kbllbbbbbk.",
        ".kblbbbbbbk.",
        ".kbbbbbbbbk.",
        ".kbbbbbbbbk.",
        "..kbbbbbbk..",
        "...kbbbbk...",
        "....kkkk....",
    }, pal);
}

Pix BoltPix() {
    const std::map<char, sf::Color> pal = {{'k', {160, 110, 20}}, {'y', {255, 236, 120}}, {'w', {255, 255, 225}}};
    return PixFromRows({
        "..kkkkkk..",
        ".kyyywwwk.",
        ".kyyywwwk.",
        "..kkkkkk..",
    }, pal);
}

Pix ShotPix() {
    const std::map<char, sf::Color> pal = {{'k', {70, 20, 90}}, {'p', {210, 90, 235}}, {'l', {240, 180, 250}}};
    return PixFromRows({
        "..kkkk..",
        ".kllppk.",
        "klpppppk",
        "klpppppk",
        "kpppppppk",
        "kpppppppk",
        ".kppppk.",
        "..kkkk..",
    }, pal);
}

Pix KillIconPix() {
    const std::map<char, sf::Color> pal = {{'k', kInk}, {'s', {220, 220, 230}}, {'h', {150, 110, 60}}};
    return PixFromRows({
        "s.......s",
        "ks.....sk",
        ".ks...sk.",
        "..ks.sk..",
        "...ksk...",
        "..ks.sk..",
        ".kh...hk.",
        "kh.....hk",
        "h.......h",
    }, pal);
}

Pix BeamIconPix() {
    const std::map<char, sf::Color> pal = {{'c', {120, 230, 255}}, {'w', {230, 250, 255}}, {'d', {60, 140, 190}}};
    return PixFromRows({
        "......cccccccccc",
        "..ccccwwwwwwwwww",
        "ccccwwwwwwwwwwww",
        "..ccccwwwwwwwwww",
        "......cccccccccc",
        "..........dddd..",
    }, pal);
}

Pix ClockIconPix() {
    const std::map<char, sf::Color> pal = {{'k', kInk}, {'w', {236, 236, 240}}, {'d', {120, 120, 130}}};
    return PixFromRows({
        "..kkkkk..",
        ".kwwwwwk.",
        "kwwwkwwwk",
        "kwwwkwwwk",
        "kwwwkkkwk",
        "kwwwwwwwk",
        "kdwwwwwdk",
        ".kdwwwdk.",
        "..kkkkk..",
    }, pal);
}

Pix Scaled(const Pix& src, int k) {
    Pix out(src.w * k, src.h * k);
    for (int y = 0; y < out.h; ++y)
        for (int x = 0; x < out.w; ++x) out.Put(x, y, src.At(x / k, y / k));
    return out;
}

}  // namespace

// ------------------------------------------------------------------ Art
Art::Art() {
    for (int i = 0; i < 3; ++i) {
        const Pix p = PlayerFrame(i);
        player[i] = p.ToTexture();
        playerLeft[i] = p.FlippedX().ToTexture();
        rock[i] = RockPix(i).ToTexture();
        wall[i] = WallPix(i).ToTexture();
    }
    gemSmall = GemPix(3, {72, 196, 230}, {190, 240, 255}, {40, 130, 180}).ToTexture();
    gemLarge = GemPix(4, {72, 196, 230}, {190, 240, 255}, {40, 130, 180}).ToTexture();
    gemGold = GemPix(5, {240, 196, 70}, {255, 240, 170}, {180, 130, 30}).ToTexture();
    wood = WoodPix().ToTexture();
    stone = StonePix().ToTexture();
    heart = HeartPix().ToTexture();
    bomb = BombPix().ToTexture();
    bolt = BoltPix().ToTexture();
    shot = ShotPix().ToTexture();
    iconKill = KillIconPix().ToTexture();
    iconBeam = BeamIconPix().ToTexture();
    iconBolt = BoltPix().ToTexture();
    iconClock = ClockIconPix().ToTexture();

    // ウィンドウのアイコン: 草色の地に主人公
    {
        const Pix p = PlayerFrame(0);
        Pix icon(32, 32);
        icon.Rect(0, 0, 32, 32, sf::Color(88, 140, 70));
        for (int y = 0; y < p.h; ++y)
            for (int x = 0; x < p.w; ++x) {
                const sf::Color c = p.At(x, y);
                if (c.a) icon.Put(6 + x, 4 + y, c);
            }
        playerIcon = Scaled(icon, 2).ToImage();
    }
}

// ------------------------------------------------------------------ 地形
namespace {

// 4タイル周期の滑らかなノイズ(-1..1)。タイルごとの明るさのムラに使う。
float ValueNoise(float x, float y, std::uint32_t salt) {
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const float fx = x - x0, fy = y - y0;
    auto v = [&](int i, int j) {
        return (Hash(static_cast<std::uint32_t>(i + 1000), static_cast<std::uint32_t>(j + 1000), salt) % 2000u) / 1000.f - 1.f;
    };
    auto s = [](float t) { return t * t * (3.f - 2.f * t); };
    const float a = v(x0, y0), b = v(x0 + 1, y0), c = v(x0, y0 + 1), d = v(x0 + 1, y0 + 1);
    const float u = s(fx), w = s(fy);
    return (a * (1 - u) + b * u) * (1 - w) + (c * (1 - u) + d * u) * w;
}

bool IsLand(Terrain t) { return t == Terrain::Grass || t == Terrain::Stone; }

}  // namespace

sf::Image Art::PaintTerrain(const MapManager& map, std::uint32_t seed) {
    const int T = static_cast<int>(map.TileSize());
    const int W = map.Width(), H = map.Height();
    Pix img(W * T, H * T);

    const sf::Color grass(88, 140, 70), grassDark(70, 118, 58), grassLight(110, 162, 84), tuft(60, 102, 50);
    const sf::Color stoneC(140, 141, 146), stoneEdge(104, 105, 112), stoneCrack(96, 97, 104), stoneChip(170, 170, 176);
    const sf::Color water(56, 104, 178), waterRipple(96, 146, 210), foam(160, 205, 235), foamInner(110, 160, 215);
    const sf::Color border(46, 42, 54), borderSpeck(62, 58, 72), borderTop(86, 82, 98), borderCrack(32, 28, 40);

    auto at = [&](int tx, int ty) { return map.GetTerrain({tx, ty}); };

    for (int ty = 0; ty < H; ++ty) {
        for (int tx = 0; tx < W; ++tx) {
            const Terrain t = at(tx, ty);
            const std::uint32_t h = Hash(static_cast<std::uint32_t>(tx), static_cast<std::uint32_t>(ty), seed);
            const int ox = tx * T, oy = ty * T;
            // 明るさのムラ: 大きなうねりと細かいざらつき(ピクセル単位。タイルの境目が出ないように)
            auto toneAt = [&](int px, int py) {
                const float gx = (ox + px) / (6.f * T), gy = (oy + py) / (6.f * T);
                const float n = ValueNoise(gx, gy, seed + 9u) * 6.f + ValueNoise(gx * 5.f, gy * 5.f, seed + 21u) * 2.5f;
                return static_cast<int>(std::round(n));
            };
            const int tone = toneAt(T / 2, T / 2);
            auto fillTone = [&](sf::Color base) {
                for (int py = 0; py < T; ++py)
                    for (int px = 0; px < T; ++px) img.Put(ox + px, oy + py, Shade(base, toneAt(px, py)));
            };

            if (t == Terrain::Grass) {
                fillTone(grass);
                // まばらな濃淡
                for (int i = 0; i < 9; ++i) {
                    const std::uint32_t r = Hash(h, static_cast<std::uint32_t>(i), 3u);
                    const int px = static_cast<int>(r % 32u), py = static_cast<int>((r >> 5) % 32u);
                    img.Put(ox + px, oy + py, Shade((r >> 10) % 3u ? grassDark : grassLight, tone));
                }
                // 草の房(たまに)
                if (h % 5u == 0u) {
                    const int px = 4 + static_cast<int>((h >> 8) % 22u), py = 6 + static_cast<int>((h >> 13) % 20u);
                    const sf::Color c = Shade(tuft, tone);
                    img.Put(ox + px, oy + py, c); img.Put(ox + px + 2, oy + py, c);
                    img.Put(ox + px, oy + py - 1, c); img.Put(ox + px + 1, oy + py + 1, c); img.Put(ox + px + 2, oy + py - 1, c);
                }
                // 花(ごくたまに)
                if (h % 41u == 7u) {
                    const int px = 6 + static_cast<int>((h >> 9) % 20u), py = 6 + static_cast<int>((h >> 14) % 20u);
                    const sf::Color petals[3] = {{236, 226, 120}, {240, 240, 240}, {232, 130, 150}};
                    const sf::Color c = petals[(h >> 20) % 3u];
                    img.Put(ox + px, oy + py, c); img.Put(ox + px + 1, oy + py, c); img.Put(ox + px, oy + py + 1, c); img.Put(ox + px + 1, oy + py + 1, c);
                    img.Put(ox + px, oy + py + 2, Shade(tuft, tone));
                }
            } else if (t == Terrain::Stone) {
                fillTone(stoneC);
                for (int i = 0; i < 6; ++i) {
                    const std::uint32_t r = Hash(h, static_cast<std::uint32_t>(i), 4u);
                    img.Put(ox + static_cast<int>(r % 32u), oy + static_cast<int>((r >> 5) % 32u), Shade((r >> 10) & 1u ? stoneChip : stoneEdge, tone));
                }
                if (h % 3u == 0u) {   // ひび
                    int px = 4 + static_cast<int>((h >> 6) % 20u), py = 4 + static_cast<int>((h >> 11) % 20u);
                    for (int i = 0; i < 6; ++i) {
                        img.Put(ox + px, oy + py, Shade(stoneCrack, tone));
                        const std::uint32_t r = Hash(h, static_cast<std::uint32_t>(i), 6u);
                        px += (r & 1u) ? 1 : 0;
                        py += (r & 2u) ? 1 : 0;
                        if (!(r & 3u)) ++px;
                    }
                }
                // 石畳の目地: 石でない隣に面した辺を濃く
                if (at(tx, ty - 1) != Terrain::Stone) img.Rect(ox, oy, T, 1, stoneEdge);
                if (at(tx, ty + 1) != Terrain::Stone) img.Rect(ox, oy + T - 1, T, 1, stoneEdge);
                if (at(tx - 1, ty) != Terrain::Stone) img.Rect(ox, oy, 1, T, stoneEdge);
                if (at(tx + 1, ty) != Terrain::Stone) img.Rect(ox + T - 1, oy, 1, T, stoneEdge);
            } else if (t == Terrain::Water) {
                fillTone(water);
                for (int i = 0; i < 3; ++i) {   // さざ波
                    const std::uint32_t r = Hash(h, static_cast<std::uint32_t>(i), 8u);
                    const int px = static_cast<int>(r % 28u), py = static_cast<int>((r >> 5) % 32u);
                    const sf::Color c = Shade(waterRipple, tone);
                    img.Put(ox + px, oy + py, c); img.Put(ox + px + 1, oy + py, c); img.Put(ox + px + 2, oy + py, c);
                }
                // 岸: 陸に面した辺に泡と明るい帯
                const bool up = IsLand(at(tx, ty - 1)), down = IsLand(at(tx, ty + 1)), left = IsLand(at(tx - 1, ty)), right = IsLand(at(tx + 1, ty));
                if (up) { img.Rect(ox, oy, T, 1, foam); img.Rect(ox, oy + 1, T, 2, foamInner); }
                if (down) { img.Rect(ox, oy + T - 1, T, 1, foam); img.Rect(ox, oy + T - 3, T, 2, foamInner); }
                if (left) { img.Rect(ox, oy, 1, T, foam); img.Rect(ox + 1, oy, 2, T, foamInner); }
                if (right) { img.Rect(ox + T - 1, oy, 1, T, foam); img.Rect(ox + T - 3, oy, 2, T, foamInner); }
                if (!up && !left && IsLand(at(tx - 1, ty - 1))) img.Put(ox, oy, foam);
                if (!up && !right && IsLand(at(tx + 1, ty - 1))) img.Put(ox + T - 1, oy, foam);
                if (!down && !left && IsLand(at(tx - 1, ty + 1))) img.Put(ox, oy + T - 1, foam);
                if (!down && !right && IsLand(at(tx + 1, ty + 1))) img.Put(ox + T - 1, oy + T - 1, foam);
            } else {   // 外周の崖
                img.Rect(ox, oy, T, T, border);
                for (int i = 0; i < 10; ++i) {
                    const std::uint32_t r = Hash(h, static_cast<std::uint32_t>(i), 12u);
                    img.Put(ox + static_cast<int>(r % 32u), oy + static_cast<int>((r >> 5) % 32u), (r >> 10) & 1u ? borderSpeck : borderCrack);
                }
                // 内側に面した辺は少し明るく(段差に見せる)
                if (at(tx, ty + 1) != Terrain::Border && ty + 1 < H) img.Rect(ox, oy + T - 3, T, 3, borderTop);
                if (at(tx, ty - 1) != Terrain::Border && ty > 0) img.Rect(ox, oy, T, 2, borderTop);
                if (at(tx + 1, ty) != Terrain::Border && tx + 1 < W) img.Rect(ox + T - 2, oy, 2, T, borderTop);
                if (at(tx - 1, ty) != Terrain::Border && tx > 0) img.Rect(ox, oy, 2, T, borderTop);
                if (h % 4u == 0u) {   // 縦のひび
                    const int px = 3 + static_cast<int>((h >> 7) % 26u);
                    for (int py = 4 + static_cast<int>((h >> 12) % 8u); py < T - 6; ++py) img.Put(ox + px + ((py / 5) % 2), oy + py, borderCrack);
                }
            }
        }
    }
    // 陸側の水際を1ドット暗くする(水の泡と合わせて岸に見せる)
    for (int ty = 0; ty < H; ++ty)
        for (int tx = 0; tx < W; ++tx) {
            const Terrain t = at(tx, ty);
            if (!IsLand(t)) continue;
            const sf::Color edge = t == Terrain::Grass ? sf::Color(58, 94, 50) : sf::Color(104, 105, 112);
            const int ox = tx * T, oy = ty * T;
            if (at(tx, ty - 1) == Terrain::Water) img.Rect(ox, oy, T, 1, edge);
            if (at(tx, ty + 1) == Terrain::Water) img.Rect(ox, oy + T - 1, T, 1, edge);
            if (at(tx - 1, ty) == Terrain::Water) img.Rect(ox, oy, 1, T, edge);
            if (at(tx + 1, ty) == Terrain::Water) img.Rect(ox + T - 1, oy, 1, T, edge);
        }
    return img.ToImage();
}

}  // namespace app
