#include "app/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace app {

using namespace game;

namespace {

sf::Color TerrainColor(Terrain t, int x, int y) {
    const int v = ((x + y) & 1) ? 6 : 0;  // 市松でタイルの境目を見やすくする
    switch (t) {
        case Terrain::Grass: return {static_cast<std::uint8_t>(64 + v), static_cast<std::uint8_t>(138 + v), static_cast<std::uint8_t>(62 + v)};
        case Terrain::Stone: return {static_cast<std::uint8_t>(128 + v), static_cast<std::uint8_t>(128 + v), static_cast<std::uint8_t>(134 + v)};
        case Terrain::Water: return {static_cast<std::uint8_t>(48 + v), static_cast<std::uint8_t>(94 + v), static_cast<std::uint8_t>(176 + v)};
        case Terrain::Border: return {34, 30, 38};
    }
    return sf::Color::Magenta;
}

std::string FailText(BuildFail f) {
    switch (f) {
        case BuildFail::None: return "";
        case BuildFail::OutOfBounds: return "マップ外";
        case BuildFail::Rock: return "岩がある";
        case BuildFail::ExistingWall: return "壁がある";
        case BuildFail::Terrain: return "建築できない地形";
        case BuildFail::NoMaterial: return "素材が足りない";
        case BuildFail::NoRoom: return "押し出し先がない";
        case BuildFail::NoDirection: return "方向が定まらない";
    }
    return "";
}

sf::Vector2f V(core::Vec2 v) { return {v.x, v.y}; }

}  // namespace

Renderer::Renderer(const std::string& dir) : dataDir(dir) {
    for (const char* p : {"C:/Windows/Fonts/meiryo.ttc", "C:/Windows/Fonts/YuGothM.ttc", "C:/Windows/Fonts/msgothic.ttc",
                          "C:/Windows/Fonts/arial.ttf"}) {
        if (font.openFromFile(p)) { hasFont = true; break; }
    }
    if (!hasFont) std::fprintf(stderr, "警告: フォントを読み込めませんでした(文字が表示されません)\n");
}

// 敵の画像(種類ごとのフォルダの 0.png, 1.png ...)を読む。読めなければ空のまま(円で描く)。
void Renderer::EnsureSprites(const Balance& bal) {
    if (spritesLoaded) return;
    spritesLoaded = true;
    namespace fs = std::filesystem;
    enemyFrames.assign(bal.enemies.size(), {});
    for (size_t i = 0; i < bal.enemies.size(); ++i) {
        const fs::path dir = fs::u8path(dataDir) / fs::u8path(bal.spriteDir) / fs::u8path(bal.enemies[i].sprite);
        for (int f = 0; f < 64; ++f) {
            const fs::path file = dir / (std::to_string(f) + ".png");
            if (!fs::exists(file)) break;
            sf::Texture tex;
            if (!tex.loadFromFile(file)) break;
            tex.setSmooth(false);   // ドット絵なので補間しない
            enemyFrames[i].push_back(std::move(tex));
        }
        if (enemyFrames[i].empty())
            std::fprintf(stderr, "警告: 敵の画像を読めませんでした: %s\n", dir.string().c_str());
    }
}

void Renderer::Text(sf::RenderTarget& t, const std::string& utf8, sf::Vector2f pos, unsigned size, sf::Color c, bool centered) {
    if (!hasFont || utf8.empty()) return;
    sf::Text text(font, sf::String::fromUtf8(utf8.begin(), utf8.end()), size);
    text.setFillColor(c);
    text.setOutlineColor(sf::Color(0, 0, 0, 200));
    text.setOutlineThickness(2.f);
    if (centered) {
        const sf::FloatRect b = text.getLocalBounds();
        text.setOrigin({b.position.x + b.size.x / 2.f, 0.f});
    }
    text.setPosition(pos);
    t.draw(text);
}

void Renderer::Bar(sf::RenderTarget& t, sf::FloatRect r, float ratio, sf::Color fill) {
    sf::RectangleShape bg(r.size);
    bg.setPosition(r.position);
    bg.setFillColor(sf::Color(0, 0, 0, 170));
    bg.setOutlineColor(sf::Color(255, 255, 255, 120));
    bg.setOutlineThickness(1.f);
    t.draw(bg);
    sf::RectangleShape f({r.size.x * std::clamp(ratio, 0.f, 1.f), r.size.y});
    f.setPosition(r.position);
    f.setFillColor(fill);
    t.draw(f);
}

sf::View Renderer::GameView(GameSession& sim, sf::Vector2f size) const {
    const float T = sim.Map().TileSize();
    const float mw = sim.Map().Width() * T, mh = sim.Map().Height() * T;
    core::Vec2 c = sim.Ecs().IsAlive(sim.PlayerEntity()) ? sim.Ecs().GetComponent<Transform>(sim.PlayerEntity()).pos : core::Vec2{mw / 2, mh / 2};
    c.x = size.x >= mw ? mw / 2 : std::clamp(c.x, size.x / 2, mw - size.x / 2);
    c.y = size.y >= mh ? mh / 2 : std::clamp(c.y, size.y / 2, mh - size.y / 2);
    return sf::View({c.x, c.y}, size);
}

void Renderer::Draw(sf::RenderTarget& target, GameSession& sim, bool showInventory) {
    const sf::Vector2f size(target.getSize());
    target.clear(sf::Color(20, 18, 24));
    target.setView(GameView(sim, size));
    DrawWorld(target, sim);
    if (debug.colliders) DrawColliders(target, sim);
    if (sim.State() == GameState::InGame || sim.State() == GameState::Paused) DrawBuildPreview(target, sim);
    target.setView(sf::View(sf::FloatRect({0.f, 0.f}, size)));
    DrawHud(target, sim, showInventory);
    DrawOverlay(target, sim);
}

void Renderer::DrawWorld(sf::RenderTarget& t, GameSession& sim) {
    EnsureSprites(sim.Ctx().bal);
    auto& ecs = sim.Ecs();
    auto& map = sim.Map();
    auto& g = sim.Ctx();
    const float T = map.TileSize();

    // 可視範囲のタイルだけ描く
    const sf::View v = t.getView();
    const sf::Vector2f c = v.getCenter(), s = v.getSize();
    const int x0 = std::max(0, static_cast<int>((c.x - s.x / 2) / T)), x1 = std::min(map.Width() - 1, static_cast<int>((c.x + s.x / 2) / T));
    const int y0 = std::max(0, static_cast<int>((c.y - s.y / 2) / T)), y1 = std::min(map.Height() - 1, static_cast<int>((c.y + s.y / 2) / T));

    sf::VertexArray va(sf::PrimitiveType::Triangles);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const sf::Color col = TerrainColor(map.GetTerrain({x, y}), x, y);
            const float px = x * T, py = y * T;
            const sf::Vector2f a{px, py}, b{px + T, py}, cc{px + T, py + T}, d{px, py + T};
            va.append({a, col}); va.append({b, col}); va.append({cc, col});
            va.append({a, col}); va.append({cc, col}); va.append({d, col});
        }
    t.draw(va);

    sf::RectangleShape rect;
    // 岩・壁(タイル座標の物体)
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const core::Vec2i tile{x, y};
            if (map.RockAt(tile) != ECS::NULL_ENTITY) {
                rect.setSize({T - 4.f, T - 4.f});
                rect.setPosition({x * T + 2.f, y * T + 2.f});
                rect.setFillColor(sf::Color(112, 106, 98));
                rect.setOutlineColor(sf::Color(60, 56, 52));
                rect.setOutlineThickness(2.f);
                t.draw(rect);
                rect.setSize({T - 14.f, T - 14.f});
                rect.setPosition({x * T + 7.f, y * T + 7.f});
                rect.setFillColor(sf::Color(140, 134, 124));
                rect.setOutlineThickness(0.f);
                t.draw(rect);
            }
            const EntityID w = map.WallAt(tile);
            if (w != ECS::NULL_ENTITY) {
                const auto* hp = ecs.TryGet<Health>(w);
                const float ratio = hp ? std::clamp(hp->hp / hp->maxHp, 0.f, 1.f) : 1.f;
                rect.setSize({T - 2.f, T - 2.f});
                rect.setPosition({x * T + 1.f, y * T + 1.f});
                rect.setFillColor(sf::Color(static_cast<std::uint8_t>(110 + 60 * ratio), static_cast<std::uint8_t>(70 + 40 * ratio), 38));
                rect.setOutlineColor(sf::Color(70, 42, 20));
                rect.setOutlineThickness(2.f);
                t.draw(rect);
                rect.setOutlineThickness(0.f);
                rect.setSize({T - 2.f, 3.f});
                rect.setFillColor(sf::Color(70, 42, 20));
                for (int i = 1; i <= 3; ++i) {
                    rect.setPosition({x * T + 1.f, y * T + i * T / 4.f});
                    t.draw(rect);
                }
                if (ratio < 0.999f) Bar(t, {{x * T + 3.f, y * T + T - 7.f}, {T - 6.f, 4.f}}, ratio, sf::Color(220, 80, 60));
            }
        }

    // 押している岩の進捗
    if (ecs.IsAlive(sim.PlayerEntity())) {
        const auto& p = ecs.GetComponent<Player>(sim.PlayerEntity());
        if (p.pushRock.x >= 0 && p.pushTimer > 0.f)
            Bar(t, {{p.pushRock.x * T + 3.f, p.pushRock.y * T - 7.f}, {T - 6.f, 4.f}}, p.pushTimer / g.bal.pushHold, sf::Color(240, 220, 90));
    }

    // アイテム・弾・敵・プレイヤー(連続座標)。画面外は描かない。
    auto onScreen = [&](core::Vec2 p, float m) {
        return p.x > c.x - s.x / 2 - m && p.x < c.x + s.x / 2 + m && p.y > c.y - s.y / 2 - m && p.y < c.y + s.y / 2 + m;
    };
    sf::CircleShape circle;
    g.pickupGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* pk = ecs.TryGet<Pickup>(e);
        if (!tr || !pk || !onScreen(tr->pos, 16.f)) return;
        sf::Color col;
        float r = 5.f;
        switch (pk->kind) {
            case PickupKind::Xp: col = {90, 220, 240}; r = 4.f; break;
            case PickupKind::Wood: col = {170, 110, 50}; r = 6.f; break;
            case PickupKind::Stone: col = {190, 190, 200}; r = 6.f; break;
            case PickupKind::Scrap: col = {200, 150, 90}; r = 5.f; break;
        }
        circle.setRadius(r);
        circle.setOrigin({r, r});
        circle.setPosition(V(tr->pos));
        circle.setFillColor(col);
        circle.setOutlineColor(sf::Color(0, 0, 0, 180));
        circle.setOutlineThickness(1.5f);
        t.draw(circle);
    });

    g.enemyGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        const auto* en = ecs.TryGet<Enemy>(e);
        const auto* hp = ecs.TryGet<Health>(e);
        if (!tr || !col || !en || !hp || !onScreen(tr->pos, 40.f)) return;
        const EnemyDef& def = g.bal.enemies[en->def];
        const auto& frames = enemyFrames[en->def];
        float topY = tr->pos.y - col->radius;
        if (!frames.empty()) {
            // スライムのアニメーション: 時間でコマを進める。個体ごとに位相をずらして、全員が同時に跳ねないようにする。
            const int n = static_cast<int>(frames.size());
            const int idx = static_cast<int>((g.time + static_cast<float>(e % 11) * 0.137f) * g.bal.animFps) % n;
            const sf::Texture& tex = frames[idx];
            const float size = def.drawSize;
            // 足元の影
            sf::CircleShape shadow(size * 0.36f);
            shadow.setScale({1.f, 0.4f});
            shadow.setOrigin({size * 0.36f, size * 0.36f});
            shadow.setPosition({tr->pos.x, tr->pos.y + size * 0.28f});
            shadow.setFillColor(sf::Color(0, 0, 0, 70));
            t.draw(shadow);
            if (en->state != EnemyState::Chase) {   // 壁を攻撃中・岩を押し中
                sf::CircleShape ring(col->radius + 3.f);
                ring.setOrigin({col->radius + 3.f, col->radius + 3.f});
                ring.setPosition(V(tr->pos));
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(sf::Color(255, 230, 120, 220));
                ring.setOutlineThickness(2.f);
                t.draw(ring);
            }
            sf::Sprite spr(tex);
            const sf::Vector2f ts(tex.getSize());
            spr.setOrigin({ts.x / 2.f, ts.y / 2.f});
            spr.setScale({size / ts.x, size / ts.y});
            spr.setPosition({tr->pos.x, tr->pos.y - size * 0.06f});
            t.draw(spr);
            topY = tr->pos.y - size * 0.5f;
        } else {   // 画像がないときの代替表示
            circle.setRadius(col->radius);
            circle.setOrigin({col->radius, col->radius});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(sf::Color(206, 64, 64));
            circle.setOutlineColor(sf::Color(60, 10, 10));
            circle.setOutlineThickness(2.f);
            t.draw(circle);
        }
        if (hp->hp < hp->maxHp) {
            const float w = std::max(col->radius * 2.f, def.drawSize * 0.6f);
            Bar(t, {{tr->pos.x - w / 2, topY - 6.f}, {w, 3.f}}, hp->hp / hp->maxHp, sf::Color(230, 70, 70));
        }
    });

    // 爆弾(SpatialIndexSystem が毎フレーム列挙する): 導火線の間は点滅、爆発後は広がる輪
    for (EntityID e : g.bombs) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* b = ecs.TryGet<Bomb>(e);
        if (!tr || !b) continue;
        if (b->exploded) {
            const float k = std::clamp(b->flash / std::max(0.001f, b->flashTotal), 0.f, 1.f);   // 1→0
            const float rad = b->blastRadius * (1.f - 0.35f * k);
            circle.setRadius(rad);
            circle.setOrigin({rad, rad});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(sf::Color(255, 150, 40, static_cast<std::uint8_t>(40 + 130 * k)));
            circle.setOutlineColor(sf::Color(255, 220, 120, static_cast<std::uint8_t>(80 + 175 * k)));
            circle.setOutlineThickness(3.f);
        } else {
            const bool blink = static_cast<int>(b->fuse * 6.f) % 2 == 0;
            circle.setRadius(7.f);
            circle.setOrigin({7.f, 7.f});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(blink ? sf::Color(230, 50, 40) : sf::Color(40, 40, 46));
            circle.setOutlineColor(sf::Color(250, 220, 120));
            circle.setOutlineThickness(2.f);
        }
        t.draw(circle);
    }

    // 弾(SpatialIndexSystem が毎フレーム列挙する)。ビームは光の筋で描く。
    for (EntityID e : g.projectiles) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        const auto* pj = ecs.TryGet<Projectile>(e);
        if (!tr || !col || !pj) continue;
        if (pj->kind == 1) {
            const core::Vec2 d = pj->vel.Normalized();
            const core::Vec2 n{-d.y, d.x};
            const core::Vec2 tail = tr->pos - d * 70.f;
            const float w = col->radius;
            sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
            quad[0] = {V(tail + n * (w * 0.25f)), sf::Color(120, 230, 255, 0)};
            quad[1] = {V(tail - n * (w * 0.25f)), sf::Color(120, 230, 255, 0)};
            quad[2] = {V(tr->pos + n * w), sf::Color(190, 250, 255, 230)};
            quad[3] = {V(tr->pos - n * w), sf::Color(190, 250, 255, 230)};
            t.draw(quad);
            continue;
        }
        circle.setRadius(col->radius);
        circle.setOrigin({col->radius, col->radius});
        circle.setPosition(V(tr->pos));
        circle.setFillColor(pj->hostile ? sf::Color(220, 90, 240) : sf::Color(255, 240, 120));   // 敵の弾は紫
        circle.setOutlineColor(pj->hostile ? sf::Color(70, 20, 90) : sf::Color(160, 120, 20));
        circle.setOutlineThickness(1.f);
        t.draw(circle);
    }

    if (ecs.IsAlive(sim.PlayerEntity())) {
        const auto& tr = ecs.GetComponent<Transform>(sim.PlayerEntity());
        const auto& col = ecs.GetComponent<Collider>(sim.PlayerEntity());
        const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
        const auto& p = ecs.GetComponent<Player>(sim.PlayerEntity());
        // 照準方向(建築の向き)
        sf::VertexArray line(sf::PrimitiveType::Lines, 2);
        line[0] = {V(tr.pos), sf::Color(255, 255, 255, 90)};
        line[1] = {V(tr.pos + p.aim * 28.f), sf::Color(255, 255, 255, 220)};
        t.draw(line);
        circle.setRadius(col.radius);
        circle.setOrigin({col.radius, col.radius});
        circle.setPosition(V(tr.pos));
        const bool blink = hp.invuln > 0.f && (static_cast<int>(hp.invuln * 20.f) % 2 == 0);
        circle.setFillColor(blink ? sf::Color(255, 255, 255, 120) : sf::Color(70, 150, 245));
        circle.setOutlineColor(sf::Color(15, 40, 90));
        circle.setOutlineThickness(2.f);
        t.draw(circle);
    }
}

// 当たり判定の表示(デバッグ)。見た目の大きさと実際の判定のずれを確かめるためのもの。
//   タイル: 赤 = プレイヤーが通れない(岩・壁・水・外周)、橙 = 敵が通れない
//   円: 水色 = プレイヤー、緑 = 敵、黄 = 弾、白 = アイテム(回収範囲は薄く)、爆弾は爆発の半径
void Renderer::DrawColliders(sf::RenderTarget& t, GameSession& sim) {
    auto& ecs = sim.Ecs();
    auto& map = sim.Map();
    auto& g = sim.Ctx();
    const float T = map.TileSize();
    const sf::View v = t.getView();
    const sf::Vector2f c = v.getCenter(), s = v.getSize();
    const int x0 = std::max(0, static_cast<int>((c.x - s.x / 2) / T)), x1 = std::min(map.Width() - 1, static_cast<int>((c.x + s.x / 2) / T));
    const int y0 = std::max(0, static_cast<int>((c.y - s.y / 2) / T)), y1 = std::min(map.Height() - 1, static_cast<int>((c.y + s.y / 2) / T));

    sf::RectangleShape box({T - 1.f, T - 1.f});
    box.setFillColor(sf::Color::Transparent);
    box.setOutlineThickness(-1.5f);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const core::Vec2i tile{x, y};
            if (map.BlocksPlayer(tile)) box.setOutlineColor(sf::Color(255, 60, 60, 200));
            else if (map.BlocksEnemy(tile)) box.setOutlineColor(sf::Color(255, 160, 40, 200));
            else continue;
            box.setPosition({x * T + 0.5f, y * T + 0.5f});
            t.draw(box);
        }

    sf::CircleShape ring;
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineThickness(1.5f);
    auto drawRing = [&](core::Vec2 pos, float r, sf::Color col) {
        ring.setRadius(r);
        ring.setOrigin({r, r});
        ring.setPosition({pos.x, pos.y});
        ring.setOutlineColor(col);
        t.draw(ring);
    };

    g.enemyGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        const auto* en = ecs.TryGet<Enemy>(e);
        if (!tr || !col) return;
        drawRing(tr->pos, col->radius, sf::Color(80, 255, 80, 230));
        if (debug.paths && en && en->usingPath && en->pathIdx < en->path.size()) {   // 経路: 敵 → 残りの通過点
            sf::VertexArray line(sf::PrimitiveType::LineStrip);
            line.append({{tr->pos.x, tr->pos.y}, sf::Color(80, 255, 80, 160)});
            for (size_t i = en->pathIdx; i < en->path.size(); ++i) {
                const core::Vec2 p = map.TileCenter(en->path[i]);
                line.append({{p.x, p.y}, sf::Color(80, 255, 80, 160)});
            }
            t.draw(line);
        }
    });
    g.pickupGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        if (const auto* tr = ecs.TryGet<Transform>(e)) drawRing(tr->pos, 4.f, sf::Color(255, 255, 255, 120));
    });
    for (EntityID e : g.projectiles) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        if (tr && col) drawRing(tr->pos, col->radius, sf::Color(255, 230, 60, 230));
    }
    for (EntityID e : g.bombs) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* b = ecs.TryGet<Bomb>(e);
        if (tr && b) drawRing(tr->pos, b->blastRadius, sf::Color(255, 120, 40, 160));
    }
    if (ecs.IsAlive(sim.PlayerEntity())) {
        const auto& tr = ecs.GetComponent<Transform>(sim.PlayerEntity());
        drawRing(tr.pos, ecs.GetComponent<Collider>(sim.PlayerEntity()).radius, sf::Color(80, 220, 255, 255));
        const float range = g.bal.pickupRadius + g.bal.pickupRange * ecs.GetComponent<PlayerStats>(sim.PlayerEntity()).pickupRangeMul +
                            ecs.GetComponent<Collider>(sim.PlayerEntity()).radius;
        drawRing(tr.pos, range, sf::Color(255, 255, 255, 50));   // アイテムを拾える範囲
    }
}

void Renderer::DrawBuildPreview(sf::RenderTarget& t, GameSession& sim) {
    auto& ecs = sim.Ecs();
    if (!ecs.IsAlive(sim.PlayerEntity())) return;
    auto& map = sim.Map();
    const float T = map.TileSize();
    const BuildPlan plan = sim.PreviewBuild();   // 実行と同じ判定(§14.3)
    if (!map.InBounds(plan.tile)) return;
    sf::RectangleShape r({T - 2.f, T - 2.f});
    r.setPosition({plan.tile.x * T + 1.f, plan.tile.y * T + 1.f});
    const bool needsPush = plan.ok && !plan.pushes.empty();
    r.setFillColor(plan.ok ? (needsPush ? sf::Color(250, 220, 80, 90) : sf::Color(120, 255, 120, 90)) : sf::Color(255, 60, 60, 100));
    r.setOutlineColor(plan.ok ? sf::Color(200, 255, 200, 220) : sf::Color(255, 120, 120, 230));
    r.setOutlineThickness(2.f);
    t.draw(r);
    if (!plan.ok) {   // ×印
        sf::VertexArray x(sf::PrimitiveType::Lines, 4);
        const float a = plan.tile.x * T + 6.f, b = plan.tile.x * T + T - 6.f, c = plan.tile.y * T + 6.f, d = plan.tile.y * T + T - 6.f;
        const sf::Color col(255, 90, 90);
        x[0] = {{a, c}, col}; x[1] = {{b, d}, col}; x[2] = {{a, d}, col}; x[3] = {{b, c}, col};
        t.draw(x);
    }
    // 押し出し先
    for (const PushMove& m : plan.pushes) {
        const auto* tr = ecs.TryGet<Transform>(m.id);
        sf::VertexArray ln(sf::PrimitiveType::Lines, 2);
        if (tr) ln[0] = {V(tr->pos), sf::Color(255, 230, 90)};
        ln[1] = {V(m.to), sf::Color(255, 230, 90)};
        if (tr) t.draw(ln);
        sf::CircleShape dot(5.f);
        dot.setOrigin({5.f, 5.f});
        dot.setPosition(V(m.to));
        dot.setFillColor(sf::Color(255, 230, 90, 140));
        dot.setOutlineColor(sf::Color(255, 230, 90));
        dot.setOutlineThickness(1.5f);
        t.draw(dot);
    }
}

void Renderer::DrawHud(sf::RenderTarget& t, GameSession& sim, bool showInventory) {
    if (sim.State() == GameState::Title) return;
    auto& ecs = sim.Ecs();
    auto& g = sim.Ctx();
    if (!ecs.IsAlive(sim.PlayerEntity())) return;
    const sf::Vector2f size(t.getSize());
    const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
    const auto& ex = ecs.GetComponent<Experience>(sim.PlayerEntity());
    const auto& inv = ecs.GetComponent<Inventory>(sim.PlayerEntity());
    const auto& pl = ecs.GetComponent<Player>(sim.PlayerEntity());
    char buf[160];

    // 左上: HP / 経験値 / レベル
    Bar(t, {{16.f, 14.f}, {220.f, 18.f}}, hp.hp / hp.maxHp, sf::Color(210, 60, 60));
    std::snprintf(buf, sizeof buf, "HP %d / %d", static_cast<int>(std::ceil(std::max(0.f, hp.hp))), static_cast<int>(hp.maxHp));
    Text(t, buf, {22.f, 11.f}, 14, sf::Color::White);
    Bar(t, {{16.f, 38.f}, {220.f, 10.f}}, static_cast<float>(ex.xp) / ex.next, sf::Color(80, 200, 230));
    std::snprintf(buf, sizeof buf, "Lv %d    EXP %d / %d", ex.level, ex.xp, ex.next);
    Text(t, buf, {16.f, 50.f}, 14, sf::Color(220, 240, 255));

    // 上中央: 経過時間 / ボス
    const int sec = static_cast<int>(g.time);
    std::snprintf(buf, sizeof buf, "%02d:%02d", sec / 60, sec % 60);
    Text(t, buf, {size.x / 2, 8.f}, 26, sf::Color::White, true);
    const int goal = static_cast<int>(g.bal.duration);
    if (!g.bossSpawned) {
        std::snprintf(buf, sizeof buf, "ボス出現まで %02d:%02d", std::max(0, goal - sec) / 60, std::max(0, goal - sec) % 60);
        Text(t, buf, {size.x / 2, 40.f}, 13, sf::Color(255, 210, 160), true);
    } else if (ecs.IsAlive(g.boss)) {
        const auto& bh = ecs.GetComponent<Health>(g.boss);
        Bar(t, {{size.x / 2 - 200.f, 42.f}, {400.f, 14.f}}, bh.hp / bh.maxHp, sf::Color(230, 120, 30));
        Text(t, "BOSS", {size.x / 2, 56.f}, 13, sf::Color(255, 200, 140), true);
    }

    // 右上: 素材 / 撃破数
    std::snprintf(buf, sizeof buf, "木材 %d    石材 %d    鉄くず %d", inv.wood, inv.stone, inv.scrap);
    Text(t, buf, {size.x - 330.f, 12.f}, 16, sf::Color(255, 235, 200));
    std::snprintf(buf, sizeof buf, "撃破 %d    敵 %d", g.kills, g.enemyCount);
    Text(t, buf, {size.x - 330.f, 36.f}, 14, sf::Color(220, 220, 220));

    // 左下: 建築
    const WallDef& wd = g.bal.walls[pl.selectedWall];
    const BuildPlan plan = sim.PreviewBuild();
    std::snprintf(buf, sizeof buf, "[%d] %s  木材 %d", pl.selectedWall + 1, wd.name.c_str(), WallWoodCost(g, pl.selectedWall));
    Text(t, buf, {16.f, size.y - 72.f}, 16, sf::Color::White);
    if (plan.ok)
        Text(t, plan.pushes.empty() ? "右クリックで設置できます" : "右クリックで設置(敵・アイテムを押し出します)", {16.f, size.y - 48.f}, 14,
             sf::Color(170, 255, 170));
    else
        Text(t, "設置できません: " + FailText(plan.reason), {16.f, size.y - 48.f}, 14, sf::Color(255, 150, 150));
    Text(t, "WASD 移動 / マウス 照準 / 右クリック 壁 / Esc 一時停止 / Tab 所持品", {16.f, size.y - 26.f}, 12, sf::Color(200, 200, 200, 200));

    // 右下: 所持武器と強化状態(§14.2)
    const auto& st = ecs.GetComponent<PlayerStats>(sim.PlayerEntity());
    const auto& ars = ecs.GetComponent<Arsenal>(sim.PlayerEntity());
    std::string weapons = "武器";
    for (const WeaponSlot& w : ars.slots) {
        std::snprintf(buf, sizeof buf, "  %s Lv%d", g.bal.weapons[w.def].name.c_str(), w.level);
        weapons += buf;
    }
    if (st.extraProjectiles > 0) {
        std::snprintf(buf, sizeof buf, "  (玉 +%d)", st.extraProjectiles);
        weapons += buf;
    }
    Text(t, weapons, {size.x - 520.f, size.y - 94.f}, 14, sf::Color(255, 240, 190));
    std::snprintf(buf, sizeof buf, "攻撃力 x%.2f   攻撃速度 x%.2f", st.damageMul, st.attackSpeedMul);
    Text(t, buf, {size.x - 520.f, size.y - 72.f}, 14, sf::Color(255, 240, 190));
    std::snprintf(buf, sizeof buf, "移動 x%.2f   壁耐久 x%.2f   軽減 %d%%   建築コスト -%d", st.moveSpeedMul, st.wallHpMul,
                  static_cast<int>(st.damageReduction * 100.f + 0.5f), st.buildCostReduction);
    Text(t, buf, {size.x - 520.f, size.y - 50.f}, 13, sf::Color(210, 210, 210));

    if (showInventory) {   // Tab: インベントリ確認
        sf::RectangleShape bg({260.f, 120.f});
        bg.setPosition({size.x / 2 - 130.f, size.y / 2 - 60.f});
        bg.setFillColor(sf::Color(0, 0, 0, 200));
        bg.setOutlineColor(sf::Color(255, 255, 255, 160));
        bg.setOutlineThickness(2.f);
        t.draw(bg);
        Text(t, "所持品", {size.x / 2, size.y / 2 - 52.f}, 18, sf::Color::White, true);
        std::snprintf(buf, sizeof buf, "木材  %d", inv.wood);
        Text(t, buf, {size.x / 2 - 90.f, size.y / 2 - 22.f}, 16, sf::Color(255, 220, 170));
        std::snprintf(buf, sizeof buf, "石材  %d", inv.stone);
        Text(t, buf, {size.x / 2 - 90.f, size.y / 2 + 4.f}, 16, sf::Color(220, 220, 230));
        std::snprintf(buf, sizeof buf, "鉄くず  %d", inv.scrap);
        Text(t, buf, {size.x / 2 - 90.f, size.y / 2 + 30.f}, 16, sf::Color(230, 190, 140));
    }
}

sf::FloatRect Renderer::CardRect(int i, int n, sf::Vector2f size) {
    const float w = 250.f, h = 180.f, gap = 24.f;
    const float total = n * w + (n - 1) * gap;
    return {{size.x / 2 - total / 2 + i * (w + gap), size.y / 2 - h / 2}, {w, h}};
}

void Renderer::DrawOverlay(sf::RenderTarget& t, GameSession& sim) {
    const sf::Vector2f size(t.getSize());
    auto& g = sim.Ctx();
    auto dim = [&](std::uint8_t a) {
        sf::RectangleShape r(size);
        r.setFillColor(sf::Color(0, 0, 0, a));
        t.draw(r);
    };
    char buf[160];
    switch (sim.State()) {
        case GameState::Title:
            dim(170);
            Text(t, "Terrain Survivor", {size.x / 2, size.y / 2 - 130.f}, 54, sf::Color(255, 230, 160), true);
            Text(t, "地形を操作して生き残れ", {size.x / 2, size.y / 2 - 56.f}, 22, sf::Color::White, true);
            Text(t, "Enter / クリックで開始", {size.x / 2, size.y / 2 + 6.f}, 24, sf::Color(170, 255, 170), true);
            Text(t, "WASD 移動   マウス 照準   右クリック 壁を建てる   岩は体当たりで押せる", {size.x / 2, size.y / 2 + 70.f}, 15,
                 sf::Color(220, 220, 220), true);
            Text(t, "敵を岩や壁で誘導・封鎖し、10分後に現れるボスを倒せ", {size.x / 2, size.y / 2 + 96.f}, 15, sf::Color(220, 220, 220), true);
            break;
        case GameState::Paused:
            dim(150);
            Text(t, "一時停止", {size.x / 2, size.y / 2 - 30.f}, 40, sf::Color::White, true);
            Text(t, "Esc で再開", {size.x / 2, size.y / 2 + 24.f}, 18, sf::Color(220, 220, 220), true);
            break;
        case GameState::LevelUp: {
            dim(160);
            Text(t, "レベルアップ!  強化を1つ選んでください", {size.x / 2, size.y / 2 - 150.f}, 26, sf::Color(255, 240, 150), true);
            const int n = static_cast<int>(g.offer.size());
            for (int i = 0; i < n; ++i) {
                const sf::FloatRect cr = CardRect(i, n, size);
                const UpgradeDef& u = g.bal.upgrades[g.offer[i]];
                sf::RectangleShape card(cr.size);
                card.setPosition(cr.position);
                card.setFillColor(sf::Color(40, 44, 70, 235));
                card.setOutlineColor(sf::Color(180, 190, 255));
                card.setOutlineThickness(2.f);
                t.draw(card);
                std::snprintf(buf, sizeof buf, "[%d]", i + 1);
                Text(t, buf, {cr.position.x + 12.f, cr.position.y + 8.f}, 16, sf::Color(200, 200, 255));
                Text(t, u.category, {cr.position.x + cr.size.x / 2, cr.position.y + 38.f}, 15, sf::Color(150, 220, 255), true);
                Text(t, u.name, {cr.position.x + cr.size.x / 2, cr.position.y + 70.f}, 22, sf::Color::White, true);
                if (!u.desc.empty()) Text(t, u.desc, {cr.position.x + cr.size.x / 2, cr.position.y + 106.f}, 12, sf::Color(210, 220, 255), true);
                if (u.stat == UpgradeStat::GrantWeapon) {   // 新しい攻撃手段か、持っている手段の強化か
                    bool owned = false;
                    for (const WeaponSlot& w : sim.Ecs().GetComponent<Arsenal>(sim.PlayerEntity()).slots) owned |= w.def == u.weapon;
                    Text(t, owned ? "強化(Lv+1)" : "NEW 新しい攻撃手段!", {cr.position.x + cr.size.x - 12.f - (owned ? 70.f : 130.f), cr.position.y + 8.f}, 13,
                         owned ? sf::Color(170, 255, 170) : sf::Color(255, 220, 90));
                }
                const int taken = g.taken.size() > static_cast<size_t>(g.offer[i]) ? g.taken[g.offer[i]] : 0;
                std::snprintf(buf, sizeof buf, "取得済み x%d", taken);
                Text(t, buf, {cr.position.x + cr.size.x / 2, cr.position.y + 138.f}, 13, sf::Color(190, 190, 190), true);
            }
            break;
        }
        case GameState::GameOver:
        case GameState::Result: {
            dim(190);
            const bool won = sim.Won();
            Text(t, won ? "ボス撃破 — クリア!" : "ゲームオーバー", {size.x / 2, size.y / 2 - 130.f}, 48,
                 won ? sf::Color(255, 230, 120) : sf::Color(255, 120, 120), true);
            const int sec = static_cast<int>(g.time);
            const auto& ex = sim.Ecs().GetComponent<Experience>(sim.PlayerEntity());
            std::snprintf(buf, sizeof buf, "生存時間 %02d:%02d    到達レベル %d    撃破数 %d", sec / 60, sec % 60, ex.level, g.kills);
            Text(t, buf, {size.x / 2, size.y / 2 - 30.f}, 22, sf::Color::White, true);
            Text(t, "Enter / クリックでタイトルへ", {size.x / 2, size.y / 2 + 40.f}, 20, sf::Color(170, 255, 170), true);
            break;
        }
        case GameState::InGame: break;
    }
}

}  // namespace app
