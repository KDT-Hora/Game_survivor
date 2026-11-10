#include "app/Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#include "game/Systems/WeaponSystem.h"

namespace app {

using namespace game;

namespace {

const float kPi = 3.14159265358979f;

std::string FailText(BuildFail f) {
    switch (f) {
        case BuildFail::None: return "";
        case BuildFail::OutOfBounds: return "マップの外";
        case BuildFail::Rock: return "岩がある";
        case BuildFail::ExistingWall: return "壁がある";
        case BuildFail::Terrain: return "ここには建てられない";
        case BuildFail::NoMaterial: return "木材が足りない";
        case BuildFail::NoRoom: return "押し出す場所がない";
        case BuildFail::NoDirection: return "向きが決まらない";
    }
    return "";
}

// 強化の効果を短く言う(desc が無いもの用)
std::string EffectText(const UpgradeDef& u) {
    if (!u.desc.empty()) return u.desc;
    char buf[64];
    const int pct = static_cast<int>(u.amount * 100.f + 0.5f);
    switch (u.stat) {
        case UpgradeStat::Damage: std::snprintf(buf, sizeof buf, "攻撃力 +%d%%", pct); break;
        case UpgradeStat::AttackSpeed: std::snprintf(buf, sizeof buf, "攻撃の間隔 -%d%%", pct); break;
        case UpgradeStat::MaxHp: std::snprintf(buf, sizeof buf, "最大HP +%d(いま回復もする)", static_cast<int>(u.amount)); break;
        case UpgradeStat::DamageReduction: std::snprintf(buf, sizeof buf, "受けるダメージ -%d%%", pct); break;
        case UpgradeStat::MoveSpeed: std::snprintf(buf, sizeof buf, "移動速度 +%d%%", pct); break;
        case UpgradeStat::WallHp: std::snprintf(buf, sizeof buf, "新しく建てる壁の耐久 +%d%%", pct); break;
        case UpgradeStat::BuildCost: std::snprintf(buf, sizeof buf, "壁の木材コスト -%d", static_cast<int>(u.amount)); break;
        case UpgradeStat::PushSpeed: std::snprintf(buf, sizeof buf, "岩を押す速さ +%d%%", pct); break;
        case UpgradeStat::PickupRange: std::snprintf(buf, sizeof buf, "アイテムを拾う範囲 +%d%%", pct); break;
        case UpgradeStat::ExtraProjectiles: std::snprintf(buf, sizeof buf, "通常攻撃の玉 +%d", static_cast<int>(u.amount)); break;
        case UpgradeStat::GrantWeapon: return "";
    }
    return buf;
}

sf::Color CategoryColor(const std::string& c) {
    if (c == "攻撃") return {214, 92, 70};
    if (c == "防御") return {76, 128, 210};
    if (c == "移動") return {90, 170, 100};
    if (c == "建築") return {168, 116, 64};
    if (c == "岩操作") return {130, 126, 140};
    if (c == "収集") return {60, 170, 170};
    if (c == "攻撃手段") return {160, 90, 200};
    return {120, 120, 130};
}

sf::Vector2f V(core::Vec2 v) { return {v.x, v.y}; }
sf::Vector2f R(sf::Vector2f v) { return {std::round(v.x), std::round(v.y)}; }

std::string Clock(float seconds) {
    char buf[16];
    const int s = std::max(0, static_cast<int>(seconds));
    std::snprintf(buf, sizeof buf, "%02d:%02d", s / 60, s % 60);
    return buf;
}

}  // namespace

// ============================================================================
Renderer::Renderer(const std::string& dir) : dataDir(dir) {
    for (const char* p : {"C:/Windows/Fonts/meiryob.ttc", "C:/Windows/Fonts/BIZ-UDGothicB.ttc", "C:/Windows/Fonts/YuGothB.ttc",
                          "C:/Windows/Fonts/meiryo.ttc", "C:/Windows/Fonts/msgothic.ttc", "C:/Windows/Fonts/arial.ttf"}) {
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
    enemyColors.assign(bal.enemies.size(), sf::Color(200, 80, 80));
    for (size_t i = 0; i < bal.enemies.size(); ++i) {
        const fs::path dir = fs::u8path(dataDir) / fs::u8path(bal.spriteDir) / fs::u8path(bal.enemies[i].sprite);
        for (int f = 0; f < 64; ++f) {
            const fs::path file = dir / (std::to_string(f) + ".png");
            if (!fs::exists(file)) break;
            sf::Image img;
            if (!img.loadFromFile(file)) break;
            if (f == 0) {   // 撃破の粒の色: 不透明な部分の平均
                long r = 0, g = 0, b = 0, n = 0;
                for (unsigned y = 0; y < img.getSize().y; ++y)
                    for (unsigned x = 0; x < img.getSize().x; ++x) {
                        const sf::Color c = img.getPixel({x, y});
                        if (c.a < 128 || (c.r + c.g + c.b) < 90) continue;   // 輪郭の黒は除く
                        r += c.r; g += c.g; b += c.b; ++n;
                    }
                if (n > 0) enemyColors[i] = sf::Color(static_cast<std::uint8_t>(std::min(255L, r / n + 30)), static_cast<std::uint8_t>(std::min(255L, g / n + 30)),
                                                      static_cast<std::uint8_t>(std::min(255L, b / n + 30)));
            }
            sf::Texture tex(img);
            tex.setSmooth(false);   // ドット絵なので補間しない
            enemyFrames[i].push_back(std::move(tex));
        }
        if (enemyFrames[i].empty())
            std::fprintf(stderr, "警告: 敵の画像を読めませんでした: %s\n", dir.string().c_str());
    }
}

void Renderer::EnsureTerrain(GameSession& sim) {
    if (terrainRun == sim.RunId()) return;
    terrainRun = sim.RunId();
    terrain = sf::Texture(Art::PaintTerrain(sim.Map(), sim.RunId() * 7919u + 13u));
    terrain.setSmooth(false);
}

// ------------------------------------------------------------------ 部品
sf::Vector2f Renderer::Text(sf::RenderTarget& t, const std::string& utf8, sf::Vector2f pos, unsigned size, sf::Color c, Align align, float outline) {
    if (!hasFont || utf8.empty()) return {};
    sf::Text text(font, sf::String::fromUtf8(utf8.begin(), utf8.end()), size);
    text.setFillColor(c);
    text.setOutlineColor(sf::Color(18, 14, 24, static_cast<std::uint8_t>(std::min<int>(230, c.a))));
    text.setOutlineThickness(outline >= 0.f ? outline : (size >= 24 ? 3.f : (size >= 16 ? 2.f : 1.5f)));
    const sf::FloatRect b = text.getLocalBounds();
    float ox = 0.f;
    if (align == Align::Center) ox = b.position.x + b.size.x / 2.f;
    else if (align == Align::Right) ox = b.position.x + b.size.x;
    text.setOrigin({ox, 0.f});
    text.setPosition(R(pos));
    t.draw(text);
    return b.size;
}

void Renderer::Panel(sf::RenderTarget& t, sf::FloatRect r, sf::Color fill, sf::Color edge, float thickness) {
    sf::RectangleShape bg(r.size);
    bg.setPosition(R(r.position));
    bg.setFillColor(fill);
    bg.setOutlineColor(edge);
    bg.setOutlineThickness(thickness);
    t.draw(bg);
}

void Renderer::Bar(sf::RenderTarget& t, sf::FloatRect r, float ratio, sf::Color fill, sf::Color back) {
    sf::RectangleShape bg(r.size);
    bg.setPosition(R(r.position));
    bg.setFillColor(back);
    bg.setOutlineColor(sf::Color(18, 14, 24, 240));
    bg.setOutlineThickness(2.f);
    t.draw(bg);
    const float w = std::round(r.size.x * std::clamp(ratio, 0.f, 1.f));
    if (w <= 0.f) return;
    sf::RectangleShape f({w, r.size.y});
    f.setPosition(R(r.position));
    f.setFillColor(fill);
    t.draw(f);
    // 上に細い光
    sf::RectangleShape hi({w, std::max(1.f, std::floor(r.size.y * 0.25f))});
    hi.setPosition(R(r.position));
    hi.setFillColor(sf::Color(255, 255, 255, 60));
    t.draw(hi);
}

void Renderer::Icon(sf::RenderTarget& t, const sf::Texture& tex, sf::Vector2f pos, float scale, sf::Color tint, bool centered) {
    sf::Sprite s(tex);
    const sf::Vector2f ts(tex.getSize());
    if (centered) s.setOrigin({ts.x / 2.f, ts.y / 2.f});
    s.setScale({scale, scale});
    s.setColor(tint);
    s.setPosition(R(pos));
    t.draw(s);
}

void Renderer::Shadow(sf::RenderTarget& t, sf::Vector2f pos, float rx, float ry, std::uint8_t alpha) {
    sf::CircleShape sh(rx);
    sh.setPointCount(20);
    sh.setScale({1.f, ry / rx});
    sh.setOrigin({rx, rx});
    sh.setPosition(pos);
    sh.setFillColor(sf::Color(0, 0, 0, alpha));
    t.draw(sh);
}

// ------------------------------------------------------------------ カメラ・演出の進行
sf::Vector2f Renderer::CameraTarget(GameSession& sim, sf::Vector2f viewSize) const {
    const sf::Vector2f size = viewSize / zoom;
    const float T = sim.Map().TileSize();
    const float mw = sim.Map().Width() * T, mh = sim.Map().Height() * T;
    sf::Vector2f c{mw / 2, mh / 2};
    if (sim.State() == GameState::Title) {   // タイトルではゆっくり流す
        c += {std::cos(clock * 0.13f) * 160.f, std::sin(clock * 0.09f) * 120.f};
    } else if (sim.Ecs().IsAlive(sim.PlayerEntity())) {
        c = V(sim.Ecs().GetComponent<Transform>(sim.PlayerEntity()).pos);
    }
    c.x = size.x >= mw ? mw / 2 : std::clamp(c.x, size.x / 2, mw - size.x / 2);
    c.y = size.y >= mh ? mh / 2 : std::clamp(c.y, size.y / 2, mh - size.y / 2);
    return c;
}

sf::View Renderer::GameView(GameSession& sim, sf::Vector2f size) const {
    sf::Vector2f c = cameraValid ? camera : CameraTarget(sim, size);
    c += fx.ShakeOffset();
    return sf::View(R(c), size / zoom);   // 整数に丸めてドットをにじませない
}

void Renderer::Update(GameSession& sim, float dt, sf::Vector2f size) {
    clock += dt;
    const sf::Vector2f target = CameraTarget(sim, size);
    const bool newRun = sim.RunId() != lastRun;
    if (!cameraValid || newRun) {
        camera = target;
        cameraValid = true;
    } else {
        const float k = 1.f - std::exp(-dt * (sim.State() == GameState::Title ? 1.5f : 9.f));
        camera += (target - camera) * k;
    }
    if (newRun) {
        fx.Clear();
        hurt = bossBanner = buildFailTime = 0.f;
        lastRun = sim.RunId();
    }
    if (sim.State() != lastState) {
        stateTime = 0.f;
        lastState = sim.State();
    }
    stateTime += dt;
    fx.Update(dt);
    hurt = std::max(0.f, hurt - dt * 2.2f);
    bossBanner = std::max(0.f, bossBanner - dt);
    buildFailTime = std::max(0.f, buildFailTime - dt);
}

void Renderer::OnStep(GameSession& sim) {
    auto& g = sim.Ctx();
    EnsureSprites(g.bal);
    for (const FxEvent& ev : g.fx) {
        const sf::Vector2f p = V(ev.pos);
        switch (ev.kind) {
            case FxEvent::Kind::EnemyHit: {
                fx.Burst(p, 3, sf::Color(255, 240, 200), 90.f, 0.25f, 1.6f, 0.f, true);
                const int dmg = static_cast<int>(ev.amount + 0.5f);
                if (dmg > 0) {
                    const bool big = ev.amount >= 30.f;
                    fx.Popup({p.x, p.y - 14.f}, std::to_string(dmg), big ? sf::Color(255, 214, 90) : sf::Color(255, 250, 240), big ? 17u : 13u);
                }
                break;
            }
            case FxEvent::Kind::EnemyDied: {
                const sf::Color c = ev.def >= 0 && ev.def < static_cast<int>(enemyColors.size()) ? enemyColors[ev.def] : sf::Color(200, 200, 200);
                const bool boss = ev.def == g.bal.EnemyIndex("boss");
                fx.Burst(p, boss ? 60 : 9, c, boss ? 260.f : 110.f, boss ? 1.2f : 0.45f, boss ? 5.f : 2.5f, 160.f);
                fx.Ring(p, ev.amount * 2.2f + (boss ? 120.f : 0.f), sf::Color(255, 255, 255, 160), boss ? 0.8f : 0.25f);
                if (boss) fx.Shake(0.9f);
                break;
            }
            case FxEvent::Kind::PlayerHurt:
                hurt = std::min(1.f, hurt + 0.6f);
                fx.Shake(0.35f);
                fx.Burst(p, 6, sf::Color(230, 70, 80), 120.f, 0.35f, 2.f, 200.f);
                break;
            case FxEvent::Kind::Explosion:
                fx.Flash(p, ev.amount, sf::Color(255, 230, 160, 200), 0.18f);
                fx.Ring(p, ev.amount, sf::Color(255, 180, 80, 230), 0.35f, 4.f);
                fx.Burst(p, 26, sf::Color(255, 160, 50), 260.f, 0.6f, 3.f, 120.f, true);
                fx.Burst(p, 14, sf::Color(90, 70, 60), 140.f, 0.7f, 3.f, 100.f);
                fx.Shake(0.55f);
                break;
            case FxEvent::Kind::WallBuilt:
                fx.Dust({p.x, p.y + 10.f}, 10, sf::Color(170, 150, 120, 200));
                break;
            case FxEvent::Kind::WallBroken:
                fx.Burst(p, 14, sf::Color(150, 100, 56), 150.f, 0.6f, 3.f, 260.f);
                fx.Dust(p, 6, sf::Color(170, 150, 120, 180));
                fx.Shake(0.15f);
                break;
            case FxEvent::Kind::BuildFailed:
                buildFailTime = 0.9f;
                buildFailText = FailText(static_cast<BuildFail>(ev.def));
                break;
            case FxEvent::Kind::RockMoved:
                fx.Dust({p.x, p.y + 12.f}, ev.def == 2 ? 22 : 10, sf::Color(140, 130, 120, 200));
                if (ev.def == 2) fx.Burst(p, 12, sf::Color(132, 128, 122), 160.f, 0.6f, 3.f, 240.f);
                fx.Shake(ev.def == 2 ? 0.3f : 0.12f);
                break;
            case FxEvent::Kind::Pickup: {
                const auto kind = static_cast<PickupKind>(ev.def);
                const int n = static_cast<int>(ev.amount);
                if (kind == PickupKind::Wood) fx.Popup({p.x, p.y - 10.f}, "木材 +" + std::to_string(n), sf::Color(230, 190, 130), 13u, 0.8f);
                else if (kind == PickupKind::Stone) fx.Popup({p.x, p.y - 10.f}, "石材 +" + std::to_string(n), sf::Color(220, 220, 230), 13u, 0.8f);
                else if (kind == PickupKind::Heal) {
                    fx.Popup({p.x, p.y - 10.f}, "HP +" + std::to_string(static_cast<int>(g.bal.healAmount) * n), sf::Color(255, 140, 150), 15u, 0.9f);
                    fx.Burst(p, 10, sf::Color(255, 120, 140), 70.f, 0.6f, 2.f, -60.f, true);
                } else if (kind == PickupKind::Xp && n >= 3) {
                    fx.Burst(p, 6, sf::Color(120, 230, 255), 60.f, 0.4f, 2.f, 0.f, true);
                }
                break;
            }
            case FxEvent::Kind::LevelUp:
                fx.Ring(p, 90.f, sf::Color(255, 240, 150, 220), 0.6f, 3.f);
                fx.Burst(p, 24, sf::Color(255, 230, 120), 160.f, 0.8f, 2.5f, -40.f, true);
                break;
            case FxEvent::Kind::BossAppeared:
                bossBanner = 3.f;
                fx.Shake(0.7f);
                break;
        }
    }
}

// ============================================================================
void Renderer::Draw(sf::RenderTarget& target, GameSession& sim, bool showStatus) {
    const sf::Vector2f size(target.getSize());
    target.clear(sf::Color(24, 22, 30));
    target.setView(GameView(sim, size));
    DrawWorld(target, sim);
    if (debug.colliders) DrawColliders(target, sim);
    if (sim.State() == GameState::InGame || sim.State() == GameState::Paused) DrawBuildPreview(target, sim);
    fx.DrawWorld(target, hasFont ? &font : nullptr);
    target.setView(sf::View(sf::FloatRect({0.f, 0.f}, size)));
    DrawVignette(target, sim);
    DrawHud(target, sim, showStatus);
    DrawOverlay(target, sim);
}

// ------------------------------------------------------------------ ワールド
void Renderer::DrawWorld(sf::RenderTarget& t, GameSession& sim) {
    EnsureSprites(sim.Ctx().bal);
    EnsureTerrain(sim);
    auto& ecs = sim.Ecs();
    auto& map = sim.Map();
    auto& g = sim.Ctx();
    const float T = map.TileSize();

    const sf::View v = t.getView();
    const sf::Vector2f c = v.getCenter(), s = v.getSize();
    const int x0 = std::max(0, static_cast<int>((c.x - s.x / 2) / T)), x1 = std::min(map.Width() - 1, static_cast<int>((c.x + s.x / 2) / T));
    const int y0 = std::max(0, static_cast<int>((c.y - s.y / 2) / T)), y1 = std::min(map.Height() - 1, static_cast<int>((c.y + s.y / 2) / T));

    // 地形(1枚絵)。見えている範囲だけ
    {
        sf::Sprite ground(terrain);
        const sf::IntRect part({x0 * static_cast<int>(T), y0 * static_cast<int>(T)}, {(x1 - x0 + 1) * static_cast<int>(T), (y1 - y0 + 1) * static_cast<int>(T)});
        ground.setTextureRect(part);
        ground.setPosition({x0 * T, y0 * T});
        t.draw(ground);
    }
    // 水のきらめき(動く)
    {
        sf::VertexArray va(sf::PrimitiveType::Triangles);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                if (map.GetTerrain({x, y}) != Terrain::Water) continue;
                const std::uint32_t h = Hash(static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), 99u);
                if (h % 3u) continue;
                const float phase = (h % 1000u) / 1000.f;
                const float k = 0.5f + 0.5f * std::sin(clock * 1.6f + phase * 6.28f);
                const float px = x * T + 3.f + std::fmod(static_cast<float>(h % 23u) + clock * 5.f, 24.f), py = y * T + 4.f + static_cast<float>((h >> 6) % 24u);
                const sf::Color col(190, 225, 245, static_cast<std::uint8_t>(40 + 110 * k));
                const sf::Vector2f a{px, py}, b{px + 4.f, py}, cc{px + 4.f, py + 1.f}, d{px, py + 1.f};
                va.append({a, col}); va.append({b, col}); va.append({cc, col});
                va.append({a, col}); va.append({cc, col}); va.append({d, col});
            }
        if (va.getVertexCount()) t.draw(va);
    }

    // 岩・壁(タイル座標の物体)
    const Player* pl = ecs.IsAlive(sim.PlayerEntity()) ? ecs.TryGet<Player>(sim.PlayerEntity()) : nullptr;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            const core::Vec2i tile{x, y};
            const EntityID rockId = map.RockAt(tile);
            if (rockId != ECS::NULL_ENTITY) {
                sf::Vector2f off;
                if (pl && pl->pushRock == tile && pl->pushTimer > 0.f) {   // 押されている岩は震える
                    const float k = pl->pushTimer / g.bal.pushHold;
                    off = {pl->pushDir.x * std::sin(clock * 50.f) * 1.5f * k, pl->pushDir.y * std::sin(clock * 50.f) * 1.5f * k};
                }
                Shadow(t, {x * T + T / 2 + off.x, y * T + T - 4.f + off.y}, 13.f, 4.f, 80);
                sf::Sprite spr(art.rock[rockId % 3]);
                spr.setPosition({x * T + std::round(off.x), y * T + std::round(off.y)});
                t.draw(spr);
            }
            const EntityID w = map.WallAt(tile);
            if (w != ECS::NULL_ENTITY) {
                const auto* hp = ecs.TryGet<Health>(w);
                const float ratio = hp ? std::clamp(hp->hp / hp->maxHp, 0.f, 1.f) : 1.f;
                const int state = ratio > 0.66f ? 0 : (ratio > 0.33f ? 1 : 2);
                Shadow(t, {x * T + T / 2, y * T + T - 3.f}, 15.f, 3.f, 70);
                sf::Sprite spr(art.wall[state]);
                spr.setPosition({x * T, y * T});
                t.draw(spr);
                if (ratio < 0.999f) Bar(t, {{x * T + 4.f, y * T + T - 6.f}, {T - 8.f, 3.f}}, ratio, sf::Color(220, 80, 60));
            }
        }

    // 押している岩の進捗
    if (pl && pl->pushRock.x >= 0 && pl->pushTimer > 0.f)
        Bar(t, {{pl->pushRock.x * T + 4.f, pl->pushRock.y * T - 8.f}, {T - 8.f, 4.f}}, pl->pushTimer / g.bal.pushHold, sf::Color(240, 220, 90));

    auto onScreen = [&](core::Vec2 p, float m) {
        return p.x > c.x - s.x / 2 - m && p.x < c.x + s.x / 2 + m && p.y > c.y - s.y / 2 - m && p.y < c.y + s.y / 2 + m;
    };

    // アイテム
    g.pickupGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* pk = ecs.TryGet<Pickup>(e);
        if (!tr || !pk || !onScreen(tr->pos, 16.f)) return;
        const sf::Texture* tex = &art.gemSmall;
        switch (pk->kind) {
            case PickupKind::Xp: tex = pk->amount >= 20 ? &art.gemGold : (pk->amount >= 3 ? &art.gemLarge : &art.gemSmall); break;
            case PickupKind::Wood: tex = &art.wood; break;
            case PickupKind::Stone: tex = &art.stone; break;
            case PickupKind::Scrap: tex = &art.stone; break;
            case PickupKind::Heal: tex = &art.heart; break;
        }
        const float age = g.time - pk->born;
        const float pop = std::clamp(age / 0.25f, 0.f, 1.f);
        const float scale = pop < 1.f ? 0.3f + 0.7f * (1.f - (1.f - pop) * (1.f - pop)) : 1.f;
        const float bob = pk->kind == PickupKind::Xp || pk->kind == PickupKind::Heal ? std::sin(clock * 4.f + static_cast<float>(e % 13)) * 1.5f : 0.f;
        Shadow(t, {tr->pos.x, tr->pos.y + 5.f}, 4.f, 1.5f, 50);
        sf::Sprite spr(*tex);
        const sf::Vector2f ts(tex->getSize());
        spr.setOrigin({ts.x / 2.f, ts.y / 2.f});
        float sc = scale;
        if (pk->kind == PickupKind::Heal) sc *= 1.f + 0.12f * std::sin(clock * 6.f);
        spr.setScale({sc, sc});
        spr.setPosition({std::round(tr->pos.x), std::round(tr->pos.y + bob)});
        t.draw(spr);
    });

    // 敵
    sf::CircleShape circle;
    g.enemyGrid.Query({c.x, c.y}, std::max(s.x, s.y), [&](EntityID e) {
        if (!ecs.IsAlive(e)) return;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        const auto* en = ecs.TryGet<Enemy>(e);
        const auto* hp = ecs.TryGet<Health>(e);
        if (!tr || !col || !en || !hp || !onScreen(tr->pos, 48.f)) return;
        const EnemyDef& def = g.bal.enemies[en->def];
        const auto& frames = enemyFrames[en->def];
        const bool boss = e == g.boss;
        float topY = tr->pos.y - col->radius;
        if (boss) {   // ボスの足元の輪
            const float r = def.drawSize * 0.55f + 3.f * std::sin(clock * 3.f);
            circle.setRadius(r);
            circle.setPointCount(40);
            circle.setOrigin({r, r});
            circle.setPosition(V(tr->pos) + sf::Vector2f{0.f, def.drawSize * 0.3f});
            circle.setScale({1.f, 0.45f});
            circle.setFillColor(sf::Color(220, 40, 40, 50));
            circle.setOutlineColor(sf::Color(255, 90, 70, 180));
            circle.setOutlineThickness(2.f);
            t.draw(circle);
            circle.setScale({1.f, 1.f});
        }
        if (!frames.empty()) {
            // スライムのアニメーション: 時間でコマを進める。個体ごとに位相をずらして、全員が同時に跳ねないようにする。
            const int n = static_cast<int>(frames.size());
            const int idx = static_cast<int>((g.time + static_cast<float>(e % 11) * 0.137f) * g.bal.animFps) % n;
            const sf::Texture& tex = frames[idx];
            const float size = def.drawSize;
            Shadow(t, {tr->pos.x, tr->pos.y + size * 0.28f}, size * 0.36f, size * 0.14f, 70);
            if (en->state != EnemyState::Chase) {   // 壁を攻撃中・岩を押し中
                sf::CircleShape ring(col->radius + 3.f);
                ring.setOrigin({col->radius + 3.f, col->radius + 3.f});
                ring.setPosition(V(tr->pos));
                ring.setFillColor(sf::Color::Transparent);
                ring.setOutlineColor(sf::Color(255, 230, 120, static_cast<std::uint8_t>(140 + 80 * std::sin(clock * 10.f))));
                ring.setOutlineThickness(2.f);
                t.draw(ring);
            }
            sf::Sprite spr(tex);
            const sf::Vector2f ts(tex.getSize());
            spr.setOrigin({ts.x / 2.f, ts.y / 2.f});
            const float k = std::clamp(en->hitFlash / 0.12f, 0.f, 1.f);   // 被弾でつぶれる
            spr.setScale({size / ts.x * (1.f + 0.22f * k), size / ts.y * (1.f - 0.22f * k)});
            spr.setPosition({std::round(tr->pos.x), std::round(tr->pos.y - size * 0.06f + size * 0.11f * k)});
            if (def.ignoresWalls) spr.setColor(sf::Color(255, 255, 255, 150));   // 壁をすり抜ける敵は半透明
            t.draw(spr);
            if (k > 0.f) {   // 白く光る
                spr.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(200 * k)));
                t.draw(spr, sf::RenderStates(sf::BlendAdd));
            }
            topY = tr->pos.y - size * 0.5f;
        } else {   // 画像がないときの代替表示
            circle.setRadius(col->radius);
            circle.setPointCount(30);
            circle.setOrigin({col->radius, col->radius});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(sf::Color(206, 64, 64));
            circle.setOutlineColor(sf::Color(60, 10, 10));
            circle.setOutlineThickness(2.f);
            t.draw(circle);
        }
        if (hp->hp < hp->maxHp && !boss) {
            const float w = std::max(col->radius * 2.f, def.drawSize * 0.6f);
            Bar(t, {{std::round(tr->pos.x - w / 2), std::round(topY - 7.f)}, {w, 3.f}}, hp->hp / hp->maxHp, sf::Color(230, 70, 70));
        }
    });

    // 爆弾: 導火線の間は点滅(残りが少ないほど速く)、爆発後は広がる輪
    for (EntityID e : g.bombs) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* b = ecs.TryGet<Bomb>(e);
        if (!tr || !b) continue;
        if (b->exploded) {
            const float k = std::clamp(b->flash / std::max(0.001f, b->flashTotal), 0.f, 1.f);   // 1→0
            const float rad = b->blastRadius * (1.f - 0.35f * k);
            circle.setRadius(rad);
            circle.setPointCount(48);
            circle.setOrigin({rad, rad});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(sf::Color(255, 150, 40, static_cast<std::uint8_t>(30 + 110 * k)));
            circle.setOutlineColor(sf::Color(255, 220, 120, static_cast<std::uint8_t>(60 + 170 * k)));
            circle.setOutlineThickness(3.f);
            t.draw(circle, sf::RenderStates(sf::BlendAdd));
            continue;
        }
        Shadow(t, {tr->pos.x, tr->pos.y + 6.f}, 6.f, 2.f, 70);
        const float rate = b->fuse < 0.7f ? 18.f : 7.f;
        const bool blink = static_cast<int>(b->fuse * rate) % 2 == 0;
        sf::Sprite spr(art.bomb);
        spr.setOrigin({6.f, 8.f});
        spr.setPosition({std::round(tr->pos.x), std::round(tr->pos.y)});
        if (blink) spr.setColor(sf::Color(255, 150, 150));
        t.draw(spr);
        // 火花
        const float sx = tr->pos.x + 5.f + (std::sin(clock * 40.f) > 0 ? 1.f : 0.f), sy = tr->pos.y - 9.f;
        circle.setRadius(2.f);
        circle.setPointCount(8);
        circle.setOrigin({2.f, 2.f});
        circle.setPosition({sx, sy});
        circle.setFillColor(sf::Color(255, 230, 140, 220));
        circle.setOutlineThickness(0.f);
        t.draw(circle, sf::RenderStates(sf::BlendAdd));
    }

    // 弾
    for (EntityID e : g.projectiles) {
        if (!ecs.IsAlive(e)) continue;
        const auto* tr = ecs.TryGet<Transform>(e);
        const auto* col = ecs.TryGet<Collider>(e);
        const auto* pj = ecs.TryGet<Projectile>(e);
        if (!tr || !col || !pj) continue;
        const core::Vec2 d = pj->vel.Normalized();
        if (pj->kind == 1) {   // ビーム: 光の筋
            const core::Vec2 n{-d.y, d.x};
            const core::Vec2 tail = tr->pos - d * 80.f;
            const float w = col->radius;
            sf::VertexArray quad(sf::PrimitiveType::TriangleStrip, 4);
            quad[0] = {V(tail + n * (w * 0.2f)), sf::Color(120, 230, 255, 0)};
            quad[1] = {V(tail - n * (w * 0.2f)), sf::Color(120, 230, 255, 0)};
            quad[2] = {V(tr->pos + n * w), sf::Color(190, 250, 255, 220)};
            quad[3] = {V(tr->pos - n * w), sf::Color(190, 250, 255, 220)};
            t.draw(quad, sf::RenderStates(sf::BlendAdd));
            sf::VertexArray core(sf::PrimitiveType::TriangleStrip, 4);
            core[0] = {V(tail + d * 30.f + n * 2.f), sf::Color(255, 255, 255, 0)};
            core[1] = {V(tail + d * 30.f - n * 2.f), sf::Color(255, 255, 255, 0)};
            core[2] = {V(tr->pos + n * 2.5f), sf::Color(255, 255, 255, 240)};
            core[3] = {V(tr->pos - n * 2.5f), sf::Color(255, 255, 255, 240)};
            t.draw(core);
            continue;
        }
        const float ang = std::atan2(d.y, d.x) * 180.f / kPi;
        if (pj->hostile) {
            circle.setRadius(9.f);
            circle.setPointCount(16);
            circle.setOrigin({9.f, 9.f});
            circle.setPosition(V(tr->pos));
            circle.setFillColor(sf::Color(200, 90, 240, 70));
            circle.setOutlineThickness(0.f);
            t.draw(circle, sf::RenderStates(sf::BlendAdd));
            sf::Sprite spr(art.shot);
            spr.setOrigin({4.f, 4.f});
            spr.setPosition({std::round(tr->pos.x), std::round(tr->pos.y)});
            t.draw(spr);
            continue;
        }
        // 通常の玉: 尾を引く
        for (int i = 1; i <= 3; ++i) {
            const core::Vec2 p = tr->pos - d * (static_cast<float>(i) * 5.f);
            const float r = 4.f - i * 0.8f;
            circle.setRadius(r);
            circle.setPointCount(12);
            circle.setOrigin({r, r});
            circle.setPosition(V(p));
            circle.setFillColor(sf::Color(255, 220, 120, static_cast<std::uint8_t>(120 - i * 30)));
            circle.setOutlineThickness(0.f);
            t.draw(circle, sf::RenderStates(sf::BlendAdd));
        }
        sf::Sprite spr(art.bolt);
        spr.setOrigin({5.f, 2.f});
        spr.setRotation(sf::degrees(ang));
        spr.setPosition(V(tr->pos));
        t.draw(spr);
    }

    // プレイヤー
    if (ecs.IsAlive(sim.PlayerEntity())) {
        const auto& tr = ecs.GetComponent<Transform>(sim.PlayerEntity());
        const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
        const auto& p = ecs.GetComponent<Player>(sim.PlayerEntity());
        if (p.aim.x > 0.25f) facingLeft = false;
        else if (p.aim.x < -0.25f) facingLeft = true;
        const bool walking = p.wish.LengthSq() > 0.f && sim.State() == GameState::InGame;
        const int frame = walking ? 1 + static_cast<int>(clock * 9.f) % 2 : 0;
        const sf::Texture& tex = facingLeft ? art.playerLeft[frame] : art.player[frame];
        Shadow(t, {tr.pos.x, tr.pos.y + 10.f}, 10.f, 4.f, 80);
        sf::Sprite spr(tex);
        spr.setOrigin({10.f, 14.f});
        spr.setPosition({std::round(tr.pos.x), std::round(tr.pos.y)});
        const bool blink = hp.invuln > 0.f && (static_cast<int>(hp.invuln * 20.f) % 2 == 0);
        if (blink) spr.setColor(sf::Color(255, 255, 255, 110));
        t.draw(spr);
        // 照準(建築の向き): 小さな三角
        const core::Vec2 a = p.aim, n{-a.y, a.x};
        const core::Vec2 tip = tr.pos + a * 24.f;
        sf::VertexArray tri(sf::PrimitiveType::Triangles, 3);
        const sf::Color ac(255, 255, 255, 170);
        tri[0] = {V(tip), ac};
        tri[1] = {V(tip - a * 6.f + n * 3.5f), ac};
        tri[2] = {V(tip - a * 6.f - n * 3.5f), ac};
        t.draw(tri);
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
        const float radius = ecs.GetComponent<Collider>(sim.PlayerEntity()).radius;
        const float mul = ecs.GetComponent<PlayerStats>(sim.PlayerEntity()).pickupRangeMul;
        drawRing(tr.pos, radius, sf::Color(80, 220, 255, 255));
        drawRing(tr.pos, g.bal.pickupRadius + g.bal.pickupRange * mul + radius, sf::Color(255, 255, 255, 50));   // 拾える範囲
        drawRing(tr.pos, g.bal.magnetRadius * mul, sf::Color(255, 255, 255, 30));                              // 吸い寄せの範囲
    }
}

void Renderer::DrawBuildPreview(sf::RenderTarget& t, GameSession& sim) {
    auto& ecs = sim.Ecs();
    if (!ecs.IsAlive(sim.PlayerEntity())) return;
    auto& map = sim.Map();
    const float T = map.TileSize();
    const BuildPlan plan = sim.PreviewBuild();   // 実行と同じ判定(§14.3)
    if (!map.InBounds(plan.tile)) return;
    const bool needsPush = plan.ok && !plan.pushes.empty();
    const float pulse = 0.5f + 0.5f * std::sin(clock * 5.f);
    sf::Vector2f shake;
    if (buildFailTime > 0.6f) shake.x = std::sin(clock * 60.f) * 2.f;   // 失敗直後は横に震える

    sf::Sprite ghost(art.wall[0]);
    ghost.setPosition({plan.tile.x * T + shake.x, plan.tile.y * T});
    ghost.setColor(plan.ok ? (needsPush ? sf::Color(255, 235, 150, 150) : sf::Color(190, 255, 190, 150)) : sf::Color(255, 110, 110, 130));
    t.draw(ghost);

    sf::RectangleShape r({T - 2.f, T - 2.f});
    r.setPosition({plan.tile.x * T + 1.f + shake.x, plan.tile.y * T + 1.f});
    r.setFillColor(sf::Color::Transparent);
    r.setOutlineColor(plan.ok ? sf::Color(220, 255, 220, static_cast<std::uint8_t>(140 + 100 * pulse)) : sf::Color(255, 120, 120, 230));
    r.setOutlineThickness(2.f);
    t.draw(r);
    if (!plan.ok) {   // ×印
        sf::VertexArray x(sf::PrimitiveType::Lines, 4);
        const float a = plan.tile.x * T + 8.f + shake.x, b = plan.tile.x * T + T - 8.f + shake.x, c = plan.tile.y * T + 8.f, d = plan.tile.y * T + T - 8.f;
        const sf::Color col(255, 90, 90);
        x[0] = {{a, c}, col}; x[1] = {{b, d}, col}; x[2] = {{a, d}, col}; x[3] = {{b, c}, col};
        t.draw(x);
    }
    if (buildFailTime > 0.f && !buildFailText.empty()) {
        sf::Color col(255, 150, 150, static_cast<std::uint8_t>(255 * std::min(1.f, buildFailTime / 0.3f)));
        Text(t, buildFailText, {plan.tile.x * T + T / 2, plan.tile.y * T - 18.f - (0.9f - buildFailTime) * 10.f}, 13, col, Align::Center);
    }
    // 押し出し先
    for (const PushMove& m : plan.pushes) {
        const auto* tr = ecs.TryGet<Transform>(m.id);
        if (!tr) continue;
        sf::VertexArray ln(sf::PrimitiveType::Lines, 2);
        ln[0] = {V(tr->pos), sf::Color(255, 230, 90, 120)};
        ln[1] = {V(m.to), sf::Color(255, 230, 90)};
        t.draw(ln);
        sf::CircleShape dot(4.f);
        dot.setOrigin({4.f, 4.f});
        dot.setPosition(V(m.to));
        dot.setFillColor(sf::Color(255, 230, 90, 120));
        dot.setOutlineColor(sf::Color(255, 230, 90));
        dot.setOutlineThickness(1.5f);
        t.draw(dot);
    }
}

// ------------------------------------------------------------------ 画面の縁の赤み
void Renderer::DrawVignette(sf::RenderTarget& t, GameSession& sim) {
    if (sim.State() == GameState::Title) return;
    auto& ecs = sim.Ecs();
    float low = 0.f;
    if (ecs.IsAlive(sim.PlayerEntity())) {
        const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
        const float ratio = hp.hp / hp.maxHp;
        if (ratio < 0.3f && hp.hp > 0.f) low = (0.3f - ratio) / 0.3f * (0.5f + 0.5f * std::sin(clock * 5.f));
    }
    const float k = std::min(1.f, hurt + low * 0.7f);
    if (k <= 0.f) return;
    const sf::Vector2f size(t.getSize());
    const float w = size.x * 0.22f, h = size.y * 0.3f;
    const sf::Color edge(200, 30, 40, static_cast<std::uint8_t>(150 * k)), none(200, 30, 40, 0);
    sf::VertexArray va(sf::PrimitiveType::Triangles);
    auto quad = [&](sf::Vector2f a, sf::Vector2f b, sf::Vector2f c, sf::Vector2f d, sf::Color ca, sf::Color cb, sf::Color cc, sf::Color cd) {
        va.append({a, ca}); va.append({b, cb}); va.append({c, cc});
        va.append({a, ca}); va.append({c, cc}); va.append({d, cd});
    };
    quad({0, 0}, {size.x, 0}, {size.x, h}, {0, h}, edge, edge, none, none);
    quad({0, size.y - h}, {size.x, size.y - h}, {size.x, size.y}, {0, size.y}, none, none, edge, edge);
    quad({0, 0}, {w, 0}, {w, size.y}, {0, size.y}, edge, none, none, edge);
    quad({size.x - w, 0}, {size.x, 0}, {size.x, size.y}, {size.x - w, size.y}, none, edge, edge, none);
    t.draw(va);
}

// ------------------------------------------------------------------ HUD
void Renderer::DrawHud(sf::RenderTarget& t, GameSession& sim, bool showStatus) {
    if (sim.State() == GameState::Title) return;
    auto& ecs = sim.Ecs();
    auto& g = sim.Ctx();
    if (!ecs.IsAlive(sim.PlayerEntity())) return;
    const sf::Vector2f size(t.getSize());
    const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
    const auto& ex = ecs.GetComponent<Experience>(sim.PlayerEntity());
    const auto& inv = ecs.GetComponent<Inventory>(sim.PlayerEntity());
    const auto& pl = ecs.GetComponent<Player>(sim.PlayerEntity());
    const auto& st = ecs.GetComponent<PlayerStats>(sim.PlayerEntity());
    const auto& ars = ecs.GetComponent<Arsenal>(sim.PlayerEntity());
    char buf[160];

    // 最上段: 経験値(画面の幅いっぱい)とレベル
    Bar(t, {{0.f, 0.f}, {size.x, 11.f}}, static_cast<float>(ex.xp) / std::max(1, ex.next), sf::Color(76, 196, 230), sf::Color(14, 12, 20, 235));
    std::snprintf(buf, sizeof buf, "Lv %d", ex.level);
    Text(t, buf, {size.x - 12.f, 14.f}, 18, sf::Color(220, 240, 255), Align::Right);

    // 左上: HP と素材
    {
        const float x = 16.f, y = 22.f;
        Icon(t, art.heart, {x + 9.f, y + 9.f}, 2.f);
        const float ratio = hp.hp / hp.maxHp;
        const bool low = ratio < 0.3f;
        const sf::Color fill = low ? sf::Color(static_cast<std::uint8_t>(200 + 55 * std::sin(clock * 8.f)), 50, 60) : sf::Color(214, 64, 70);
        Bar(t, {{x + 24.f, y + 1.f}, {210.f, 16.f}}, ratio, fill);
        std::snprintf(buf, sizeof buf, "%d / %d", static_cast<int>(std::ceil(std::max(0.f, hp.hp))), static_cast<int>(hp.maxHp));
        Text(t, buf, {x + 24.f + 105.f, y - 1.f}, 13, sf::Color::White, Align::Center, 1.5f);

        float ry = y + 28.f;
        Icon(t, art.wood, {x + 10.f, ry + 8.f}, 1.5f);
        std::snprintf(buf, sizeof buf, "%d", inv.wood);
        Text(t, buf, {x + 26.f, ry - 1.f}, 16, sf::Color(255, 230, 190));
        Icon(t, art.stone, {x + 66.f, ry + 8.f}, 1.5f);
        std::snprintf(buf, sizeof buf, "%d", inv.stone);
        Text(t, buf, {x + 80.f, ry - 1.f}, 16, sf::Color(225, 225, 235));
        Icon(t, art.iconKill, {x + 122.f, ry + 8.f}, 1.5f);
        std::snprintf(buf, sizeof buf, "%d", g.kills);
        Text(t, buf, {x + 136.f, ry - 1.f}, 16, sf::Color(235, 235, 235));
    }

    // 上中央: 時間 / ボス
    {
        const float cx = size.x / 2;
        Panel(t, {{cx - 54.f, 16.f}, {108.f, 36.f}}, sf::Color(14, 12, 20, 170), sf::Color(255, 255, 255, 40), 1.f);
        Text(t, Clock(g.time), {cx, 15.f}, 28, sf::Color::White, Align::Center);
        if (!g.bossSpawned) {
            const float left = g.bal.duration - g.time;
            const bool soon = left < 30.f;
            std::snprintf(buf, sizeof buf, "ボスまで %s", Clock(left).c_str());
            Text(t, buf, {cx, 56.f}, 13, soon ? sf::Color(255, 160, 120, static_cast<std::uint8_t>(180 + 75 * std::sin(clock * 6.f))) : sf::Color(255, 215, 170), Align::Center);
        } else if (ecs.IsAlive(g.boss)) {
            const auto& bh = ecs.GetComponent<Health>(g.boss);
            Bar(t, {{cx - 220.f, 60.f}, {440.f, 14.f}}, bh.hp / bh.maxHp, sf::Color(224, 70, 50), sf::Color(30, 10, 14, 230));
            Text(t, "BOSS", {cx - 228.f, 56.f}, 15, sf::Color(255, 120, 100), Align::Right);
        }
    }

    // 下段左: 武器
    {
        const float slot = 46.f, gap = 8.f;
        float x = 16.f;
        const float y = size.y - 16.f - slot;
        for (const WeaponSlot& w : ars.slots) {
            const WeaponDef& def = g.bal.weapons[w.def];
            Panel(t, {{x, y}, {slot, slot}}, sf::Color(20, 18, 28, 210), sf::Color(255, 255, 255, 70), 2.f);
            const sf::Vector2f center{x + slot / 2, y + slot / 2};
            if (def.type == WeaponType::Bolt) Icon(t, art.iconBolt, center, 2.5f);
            else if (def.type == WeaponType::Bomb) Icon(t, art.bomb, center, 2.f);
            else Icon(t, art.iconBeam, center, 2.f);
            // クールダウン: 上から暗くなる
            const float total = WeaponSystem::Cooldown(def, w, st);
            const float k = total > 0.f ? std::clamp(w.cooldown / total, 0.f, 1.f) : 0.f;
            if (k > 0.f) {
                sf::RectangleShape dim({slot, slot * k});
                dim.setPosition({x, y});
                dim.setFillColor(sf::Color(0, 0, 0, 120));
                t.draw(dim);
            }
            // レベルの点
            for (int i = 0; i < std::min(w.level, 6); ++i) {
                sf::RectangleShape pip({5.f, 3.f});
                pip.setPosition({x + 5.f + i * 6.f, y + slot - 7.f});
                pip.setFillColor(sf::Color(255, 220, 120));
                t.draw(pip);
            }
            if (w.level > 6) {
                std::snprintf(buf, sizeof buf, "%d", w.level);
                Text(t, buf, {x + slot - 4.f, y + slot - 18.f}, 12, sf::Color(255, 220, 120), Align::Right);
            }
            if (def.type == WeaponType::Bolt && st.extraProjectiles > 0) {
                std::snprintf(buf, sizeof buf, "+%d", st.extraProjectiles);
                Text(t, buf, {x + slot - 3.f, y - 2.f}, 13, sf::Color(255, 240, 160), Align::Right);
            }
            x += slot + gap;
        }
    }

    // 下段右: 建築
    {
        const WallDef& wd = g.bal.walls[pl.selectedWall];
        const BuildPlan plan = sim.PreviewBuild();
        const int cost = WallWoodCost(g, pl.selectedWall);
        const float slot = 46.f;
        const float x = size.x - 16.f - slot, y = size.y - 16.f - slot;
        Panel(t, {{x, y}, {slot, slot}}, sf::Color(20, 18, 28, 210), plan.ok ? sf::Color(170, 255, 170, 120) : sf::Color(255, 120, 120, 120), 2.f);
        Icon(t, art.wall[0], {x + slot / 2, y + slot / 2}, 1.f);
        Text(t, wd.name, {x - 10.f, y - 2.f}, 15, sf::Color::White, Align::Right);
        Icon(t, art.wood, {x - 10.f - 56.f, y + 28.f}, 1.3f);
        std::snprintf(buf, sizeof buf, "%d", cost);
        Text(t, buf, {x - 10.f - 42.f, y + 19.f}, 15, inv.wood >= cost ? sf::Color(255, 230, 190) : sf::Color(255, 120, 120));
        const std::string hint = plan.ok ? (plan.pushes.empty() ? "右クリックで設置" : "右クリックで設置(押し出し)") : FailText(plan.reason);
        Text(t, hint, {x - 10.f, y + 19.f + 20.f}, 12, plan.ok ? sf::Color(170, 255, 170) : sf::Color(255, 150, 150), Align::Right);
        if (ars.slots.size() > 0) Text(t, "Tab: ステータス", {x + slot, y - 20.f}, 11, sf::Color(255, 255, 255, 110), Align::Right);
    }

    // ボス出現
    if (bossBanner > 0.f) {
        const float k = std::min(1.f, (3.f - bossBanner) / 0.25f);                 // 出
        const float out = std::min(1.f, bossBanner / 0.4f);                          // 消
        const std::uint8_t a = static_cast<std::uint8_t>(255 * std::min(k, out));
        const float scale = 1.f + (1.f - k) * 1.5f;
        Panel(t, {{0.f, size.y * 0.33f}, {size.x, 72.f}}, sf::Color(60, 10, 14, static_cast<std::uint8_t>(170 * std::min(k, out))), sf::Color(255, 80, 70, a), 2.f);
        Text(t, "ボス出現", {size.x / 2, size.y * 0.33f + 10.f}, static_cast<unsigned>(40 * scale), sf::Color(255, 90, 80, a), Align::Center);
    }

    if (showStatus) DrawStatusPanel(t, sim, {size.x / 2, size.y / 2});
}

void Renderer::DrawStatusPanel(sf::RenderTarget& t, GameSession& sim, sf::Vector2f center) {
    auto& ecs = sim.Ecs();
    auto& g = sim.Ctx();
    if (!ecs.IsAlive(sim.PlayerEntity())) return;
    const auto& st = ecs.GetComponent<PlayerStats>(sim.PlayerEntity());
    const auto& inv = ecs.GetComponent<Inventory>(sim.PlayerEntity());
    const auto& ars = ecs.GetComponent<Arsenal>(sim.PlayerEntity());
    const auto& hp = ecs.GetComponent<Health>(sim.PlayerEntity());
    char buf[96];
    const float w = 400.f, h = 300.f;
    const sf::FloatRect r({center.x - w / 2, center.y - h / 2}, {w, h});
    Panel(t, r, sf::Color(18, 16, 26, 235), sf::Color(255, 255, 255, 120));
    Text(t, "ステータス", {r.position.x + 18.f, r.position.y + 10.f}, 18, sf::Color(255, 240, 200));
    const float lx = r.position.x + 22.f, rx = r.position.x + w / 2 + 14.f;
    float y = r.position.y + 44.f;
    auto row = [&](float x, const char* label, const char* value) {
        Text(t, label, {x, y}, 14, sf::Color(190, 190, 200));
        Text(t, value, {x + 150.f, y}, 14, sf::Color::White, Align::Right);
    };
    std::snprintf(buf, sizeof buf, "%d / %d", static_cast<int>(std::ceil(hp.hp)), static_cast<int>(hp.maxHp)); row(lx, "HP", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.damageMul); row(lx, "攻撃力", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.attackSpeedMul); row(lx, "攻撃速度", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.moveSpeedMul); row(lx, "移動速度", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(st.damageReduction * 100.f + 0.5f)); row(lx, "ダメージ軽減", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.wallHpMul); row(lx, "壁の耐久", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "-%d", st.buildCostReduction); row(lx, "建築コスト", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.pushSpeedMul); row(lx, "岩を押す速さ", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "x%.2f", st.pickupRangeMul); row(lx, "回収範囲", buf); y += 22.f;

    y = r.position.y + 44.f;
    Text(t, "所持品", {rx, y}, 14, sf::Color(255, 240, 200)); y += 22.f;
    std::snprintf(buf, sizeof buf, "%d", inv.wood); row(rx, "木材", buf); y += 22.f;
    std::snprintf(buf, sizeof buf, "%d", inv.stone); row(rx, "石材", buf); y += 22.f;
    if (inv.scrap > 0) { std::snprintf(buf, sizeof buf, "%d", inv.scrap); row(rx, "鉄くず", buf); y += 22.f; }
    y += 10.f;
    Text(t, "武器", {rx, y}, 14, sf::Color(255, 240, 200)); y += 22.f;
    for (const WeaponSlot& ws : ars.slots) {
        std::snprintf(buf, sizeof buf, "Lv %d", ws.level);
        row(rx, g.bal.weapons[ws.def].name.c_str(), buf);
        y += 22.f;
    }
    if (st.extraProjectiles > 0) { std::snprintf(buf, sizeof buf, "+%d", st.extraProjectiles); row(rx, "追加の玉", buf); y += 22.f; }
    std::snprintf(buf, sizeof buf, "建てた壁 %d   押した岩 %d", g.wallsBuilt, g.rocksPushed);
    Text(t, buf, {r.position.x + w / 2, r.position.y + h - 26.f}, 12, sf::Color(190, 190, 200), Align::Center);
}

// ------------------------------------------------------------------ 画面
sf::FloatRect Renderer::CardRect(int i, int n, sf::Vector2f size) {
    const float w = 260.f, h = 210.f, gap = 22.f;
    const float total = n * w + (n - 1) * gap;
    return {{std::round(size.x / 2 - total / 2 + i * (w + gap)), std::round(size.y / 2 - h / 2 + 10.f)}, {w, h}};
}

sf::FloatRect Renderer::ModeRect(int i, int n, sf::Vector2f size) {
    const float w = 300.f, h = 74.f, gap = 18.f;
    const float total = n * w + (n - 1) * gap;
    return {{std::round(size.x / 2 - total / 2 + i * (w + gap)), std::round(size.y * 0.56f)}, {w, h}};
}

void Renderer::DrawOverlay(sf::RenderTarget& t, GameSession& sim) {
    const sf::Vector2f size(t.getSize());
    auto dim = [&](std::uint8_t a) {
        sf::RectangleShape r(size);
        r.setFillColor(sf::Color(10, 8, 16, a));
        t.draw(r);
    };
    switch (sim.State()) {
        case GameState::Title:
            dim(120);
            DrawTitle(t, sim);
            break;
        case GameState::Paused: {
            dim(150);
            Text(t, "一時停止", {size.x / 2, size.y * 0.16f}, 40, sf::Color::White, Align::Center);
            Text(t, "Esc で再開", {size.x / 2, size.y * 0.16f + 56.f}, 16, sf::Color(220, 220, 220), Align::Center);
            DrawStatusPanel(t, sim, {size.x / 2, size.y * 0.55f});
            const float y = size.y - 60.f;
            Text(t, "WASD 移動    マウス 向き    右クリック 壁を建てる    岩は押し続けると動く    Tab ステータス", {size.x / 2, y}, 13,
                 sf::Color(200, 200, 210), Align::Center);
            break;
        }
        case GameState::LevelUp:
            dim(150);
            DrawLevelUp(t, sim);
            break;
        case GameState::GameOver:
        case GameState::Result:
            dim(static_cast<std::uint8_t>(190 * std::min(1.f, stateTime / 0.5f)));
            DrawResult(t, sim);
            break;
        case GameState::InGame: break;
    }
}

void Renderer::DrawTitle(sf::RenderTarget& t, GameSession& sim) {
    const sf::Vector2f size(t.getSize());
    const float cx = size.x / 2;
    // 題字: 影を重ねて厚みを出す
    const float ty = size.y * 0.2f + std::sin(clock * 1.2f) * 3.f;
    Text(t, "Terrain Survivor", {cx + 4.f, ty + 5.f}, 64, sf::Color(40, 20, 24, 200), Align::Center, 0.f);
    Text(t, "Terrain Survivor", {cx, ty}, 64, sf::Color(255, 226, 150), Align::Center, 4.f);
    Text(t, "岩を押し、壁を建て、生き残れ", {cx, ty + 92.f}, 20, sf::Color(240, 240, 240), Align::Center);

    // モード
    const int n = static_cast<int>(sim.Modes().size());
    for (int i = 0; i < n; ++i) {
        const sf::FloatRect r = ModeRect(i, n, size);
        const bool sel = i == sim.Mode();
        const bool hover = r.contains(mouse);
        Panel(t, r, sel ? sf::Color(40, 44, 70, 235) : sf::Color(20, 18, 28, 210),
              sel ? sf::Color(255, 226, 150) : (hover ? sf::Color(200, 200, 220, 200) : sf::Color(255, 255, 255, 60)), sel ? 3.f : 2.f);
        char key[8];
        std::snprintf(key, sizeof key, "%d", i + 1);
        Panel(t, {{r.position.x + 12.f, r.position.y + 14.f}, {26.f, 26.f}}, sf::Color(0, 0, 0, 120), sf::Color(255, 255, 255, 90), 1.f);
        Text(t, key, {r.position.x + 25.f, r.position.y + 15.f}, 16, sel ? sf::Color(255, 226, 150) : sf::Color(200, 200, 210), Align::Center);
        Text(t, sim.Modes()[i].label, {r.position.x + 52.f, r.position.y + 9.f}, 20, sel ? sf::Color::White : sf::Color(190, 190, 200));
        Text(t, sim.Modes()[i].desc, {r.position.x + 52.f, r.position.y + 40.f}, 13, sel ? sf::Color(220, 230, 255) : sf::Color(150, 150, 165));
        if (sel) {   // 選択中の印
            sf::ConvexShape tri(3);
            tri.setPoint(0, {r.position.x - 14.f, r.position.y + r.size.y / 2 - 7.f});
            tri.setPoint(1, {r.position.x - 4.f, r.position.y + r.size.y / 2});
            tri.setPoint(2, {r.position.x - 14.f, r.position.y + r.size.y / 2 + 7.f});
            tri.setFillColor(sf::Color(255, 226, 150));
            t.draw(tri);
        }
    }

    const float py = ModeRect(0, std::max(1, n), size).position.y + 74.f + 36.f;
    const std::uint8_t a = static_cast<std::uint8_t>(150 + 105 * std::sin(clock * 3.f));
    Text(t, "Enter か クリックで開始", {cx, py}, 22, sf::Color(170, 255, 170, a), Align::Center);
    Text(t, "WASD 移動    マウス 向き    右クリック 壁を建てる    岩は押し続けると動く", {cx, size.y - 48.f}, 14, sf::Color(220, 220, 220, 200), Align::Center);
}

void Renderer::DrawLevelUp(sf::RenderTarget& t, GameSession& sim) {
    const sf::Vector2f size(t.getSize());
    auto& g = sim.Ctx();
    const int n = static_cast<int>(g.offer.size());
    char buf[64];
    const float k = std::min(1.f, stateTime / 0.18f);
    Text(t, "レベルアップ", {size.x / 2, size.y / 2 - 186.f - (1.f - k) * 20.f}, 34, sf::Color(255, 236, 150, static_cast<std::uint8_t>(255 * k)), Align::Center);
    Text(t, "強化をひとつ選ぶ(数字キー か クリック)", {size.x / 2, size.y / 2 - 136.f}, 14, sf::Color(220, 220, 230, static_cast<std::uint8_t>(255 * k)), Align::Center);
    const auto& slots = sim.Ecs().GetComponent<Arsenal>(sim.PlayerEntity()).slots;
    for (int i = 0; i < n; ++i) {
        sf::FloatRect cr = CardRect(i, n, size);
        const float ki = std::clamp((stateTime - i * 0.05f) / 0.18f, 0.f, 1.f);
        const float grow = 1.f - (1.f - ki) * (1.f - ki);
        cr.position.y += (1.f - grow) * 30.f;
        const std::uint8_t a = static_cast<std::uint8_t>(255 * grow);
        const UpgradeDef& u = g.bal.upgrades[g.offer[i]];
        const bool hover = cr.contains(mouse);
        if (hover) cr.position.y -= 4.f;
        const sf::Color cat = CategoryColor(u.category);
        Panel(t, cr, sf::Color(28, 28, 44, static_cast<std::uint8_t>(240 * grow)), hover ? sf::Color(255, 240, 180, a) : sf::Color(cat.r, cat.g, cat.b, a), hover ? 3.f : 2.f);
        // 上の帯
        sf::RectangleShape band({cr.size.x, 30.f});
        band.setPosition(cr.position);
        band.setFillColor(sf::Color(cat.r, cat.g, cat.b, static_cast<std::uint8_t>(220 * grow)));
        t.draw(band);
        std::snprintf(buf, sizeof buf, "%d", i + 1);
        Text(t, buf, {cr.position.x + 12.f, cr.position.y + 4.f}, 17, sf::Color(255, 255, 255, a));
        Text(t, u.category, {cr.position.x + cr.size.x / 2, cr.position.y + 5.f}, 15, sf::Color(255, 255, 255, a), Align::Center);
        bool owned = false;
        if (u.stat == UpgradeStat::GrantWeapon)
            for (const WeaponSlot& w : slots) owned |= w.def == u.weapon;
        Text(t, u.name, {cr.position.x + cr.size.x / 2, cr.position.y + 58.f}, 24, sf::Color(255, 255, 255, a), Align::Center);
        const std::string eff = EffectText(u);
        if (!eff.empty()) Text(t, eff, {cr.position.x + cr.size.x / 2, cr.position.y + 104.f}, 13, sf::Color(215, 222, 245, a), Align::Center);
        if (u.stat == UpgradeStat::GrantWeapon)
            Text(t, owned ? "持っている武器を強化 (Lv +1)" : "新しい攻撃手段", {cr.position.x + cr.size.x / 2, cr.position.y + 130.f}, 13,
                 owned ? sf::Color(170, 255, 170, a) : sf::Color(255, 226, 120, a), Align::Center);
        const int taken = g.taken.size() > static_cast<size_t>(g.offer[i]) ? g.taken[g.offer[i]] : 0;
        if (taken > 0) {
            for (int p = 0; p < std::min(taken, 8); ++p) {
                sf::RectangleShape pip({8.f, 8.f});
                pip.setPosition({cr.position.x + cr.size.x / 2 - std::min(taken, 8) * 6.f + p * 12.f, cr.position.y + cr.size.y - 24.f});
                pip.setFillColor(sf::Color(255, 220, 120, a));
                t.draw(pip);
            }
            if (taken > 8) {
                std::snprintf(buf, sizeof buf, "x%d", taken);
                Text(t, buf, {cr.position.x + cr.size.x / 2 + 56.f, cr.position.y + cr.size.y - 30.f}, 12, sf::Color(255, 220, 120, a));
            }
        } else {
            Text(t, "まだ取っていない", {cr.position.x + cr.size.x / 2, cr.position.y + cr.size.y - 30.f}, 11, sf::Color(150, 150, 170, a), Align::Center);
        }
    }
}

void Renderer::DrawResult(sf::RenderTarget& t, GameSession& sim) {
    const sf::Vector2f size(t.getSize());
    auto& g = sim.Ctx();
    const bool won = sim.Won();
    const float k = std::clamp((stateTime - 0.2f) / 0.4f, 0.f, 1.f);
    if (k <= 0.f) return;
    const std::uint8_t a = static_cast<std::uint8_t>(255 * k);
    const float w = 520.f, h = 360.f;
    const sf::FloatRect r({size.x / 2 - w / 2, size.y / 2 - h / 2 + (1.f - k) * 20.f}, {w, h});
    Panel(t, r, sf::Color(18, 16, 26, static_cast<std::uint8_t>(235 * k)), won ? sf::Color(255, 226, 150, a) : sf::Color(255, 110, 110, a), 3.f);
    Text(t, won ? "クリア" : "ゲームオーバー", {size.x / 2, r.position.y + 22.f}, 44, won ? sf::Color(255, 230, 120, a) : sf::Color(255, 120, 120, a), Align::Center);
    Text(t, won ? "ボスを倒した" : "力尽きた", {size.x / 2, r.position.y + 84.f}, 16, sf::Color(220, 220, 230, a), Align::Center);

    char buf[64];
    float y = r.position.y + 128.f;
    const float lx = r.position.x + 110.f, rx = r.position.x + w - 110.f;
    auto row = [&](const char* label, const std::string& value) {
        Text(t, label, {lx, y}, 16, sf::Color(190, 190, 200, a));
        Text(t, value, {rx, y}, 16, sf::Color(255, 255, 255, a), Align::Right);
        y += 28.f;
    };
    int level = 0;
    if (sim.Ecs().IsAlive(sim.PlayerEntity())) level = sim.Ecs().GetComponent<Experience>(sim.PlayerEntity()).level;
    row("生存時間", Clock(g.time));
    std::snprintf(buf, sizeof buf, "%d", level); row("到達レベル", buf);
    std::snprintf(buf, sizeof buf, "%d", g.kills); row("撃破数", buf);
    std::snprintf(buf, sizeof buf, "%d", g.wallsBuilt); row("建てた壁", buf);
    std::snprintf(buf, sizeof buf, "%d", g.rocksPushed); row("押した岩", buf);
    if (stateTime > 1.f) {
        const std::uint8_t b = static_cast<std::uint8_t>(150 + 105 * std::sin(clock * 3.f));
        Text(t, "Enter か クリックでタイトルへ", {size.x / 2, r.position.y + h - 44.f}, 17, sf::Color(170, 255, 170, b), Align::Center);
    }
}

}  // namespace app
