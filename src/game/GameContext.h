#pragma once
#include <vector>

#include "ecs/ECSManager.h"
#include "game/Balance.h"
#include "game/Components.h"
#include "game/MapManager.h"
#include "game/Rules.h"

namespace game {

enum class GameState { Title, InGame, LevelUp, Paused, GameOver, Result };

// 1ステップ分の入力(SFML 非依存)。app 層が実入力/スクリプトから作る。
struct InputState {
    Vec2 move;                 // WASD 合成(未正規化可)
    Vec2 aimWorld;             // マウス位置(ワールド座標)
    bool buildPressed = false; // 右クリック(エッジ)
    int wallSelect = -1;       // 数字キー(0 始まり)。-1 は変更なし
    bool pausePressed = false; // Esc(エッジ)
    bool startPressed = false; // Title / Result からの開始(エッジ)
    int choose = -1;           // レベルアップ選択(0 始まり)
    int modeSelect = -1;       // タイトル画面でのゲームモード選択(0 始まり)
};

struct DamageEvent {
    EntityID target = ECS::NULL_ENTITY;
    float amount = 0.f;
};

struct DeathRecord {
    Vec2 pos;
    int enemyDef = 0;
};

// 空間ハッシュ。毎フレーム先頭で作り直し、近傍探索(敵の分離・自動攻撃・弾・建築の押し出し)に使う。
class SpatialGrid {
public:
    void Reset(float cellSize, float worldW, float worldH);
    void Insert(EntityID e, Vec2 p);
    // 円と重なりうるセル内の Entity を列挙する(厳密な判定は呼び出し側)。
    template <typename Fn>
    void Query(Vec2 c, float r, Fn&& fn) const {
        const int x0 = Clamp(static_cast<int>((c.x - r) / cell), cols), x1 = Clamp(static_cast<int>((c.x + r) / cell), cols);
        const int y0 = Clamp(static_cast<int>((c.y - r) / cell), rows), y1 = Clamp(static_cast<int>((c.y + r) / cell), rows);
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                for (EntityID e : cells[static_cast<size_t>(y) * cols + x]) fn(e);
    }

private:
    static int Clamp(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }
    float cell = 64.f;
    int cols = 1, rows = 1;
    std::vector<std::vector<EntityID>> cells = std::vector<std::vector<EntityID>>(1);  // Reset 前に Query されても安全
};

struct GameContext {
    const core::Config& cfg;
    Balance bal;
    ECS::ECSManager& ecs;
    MapManager& map;
    core::Rng rng;

    InputState input;
    EntityID player = ECS::NULL_ENTITY;
    GameState state = GameState::Title;

    float time = 0.f;
    int kills = 0;
    bool bossSpawned = false;
    EntityID boss = ECS::NULL_ENTITY;
    bool bossKilled = false;
    bool spawnEnabled = true;   // false なら敵(ボス含む)が自然に出現しない(デバッグ用)
    int pendingLevelUps = 0;
    int enemyCount = 0, pickupCount = 0;  // SpatialIndexSystem が毎フレーム更新
    std::vector<int> offer;   // レベルアップ候補(bal.upgrades の添字)
    std::vector<int> taken;   // 取得済み強化の回数(統計用)

    SpatialGrid enemyGrid, pickupGrid;
    std::vector<EntityID> projectiles, bombs;   // 描画用(SpatialIndexSystem が毎フレーム更新)
    std::vector<DamageEvent> damage;
    std::vector<DeathRecord> deaths;
    BuildFail lastBuildFail = BuildFail::None;
    bool lastBuildOk = false;

    GameContext(const core::Config& c, ECS::ECSManager& e, MapManager& m, std::uint32_t seed)
        : cfg(c), bal(Balance::Load(c)), ecs(e), map(m), rng(seed) {}
};

// --- Entity ファクトリ(生成はここに集約) ---
EntityID CreatePlayer(GameContext& g, Vec2 pos);
EntityID CreateRock(GameContext& g, Vec2i tile);
EntityID CreateWall(GameContext& g, Vec2i tile, int def);
EntityID CreateEnemy(GameContext& g, int def, Vec2 pos);
EntityID CreatePickup(GameContext& g, PickupKind kind, int amount, Vec2 pos);
EntityID CreateProjectile(GameContext& g, Vec2 pos, Vec2 vel, float damage, float life, float radius, bool pierce = false, int kind = 0, bool hostile = false);
EntityID CreateBomb(GameContext& g, Vec2 pos, float fuse, float blastRadius, float damage, float flash);

// 敵の移動・見通しの判定で使う「通れないもの」の種類(壁をすり抜ける敵は Ghost)。
inline MapManager::Who WhoOf(const EnemyDef& def) { return def.ignoresWalls ? MapManager::Who::Ghost : MapManager::Who::Enemy; }

// --- 共通の問い合わせ ---
// 建築プレビュー・実行が共有する計画(§14.3)。
BuildPlan ComputeBuildPlan(GameContext& g);
int WallWoodCost(GameContext& g, int wallDef);
bool CircleOverlapsTile(const MapManager& map, Vec2 c, float r, Vec2i tile);
// 岩の移動先タイルに敵が重なっていないか(重なっていれば押せない)
bool EnemyOverlapsTile(GameContext& g, Vec2i tile);
// プレイヤーの円がタイルに重なっているか(敵が押す岩・設置する壁でプレイヤーを押し潰さないため)
bool PlayerOverlapsTile(GameContext& g, Vec2i tile);

// --- 衝突(連続座標の円 vs タイルの固体) ---
Vec2 MoveAndSlide(const MapManager& map, Vec2 pos, Vec2 delta, float radius, MapManager::Who who);
Vec2 Depenetrate(const MapManager& map, Vec2 pos, float radius, MapManager::Who who);

}  // namespace game
