#pragma once

namespace game {

// 仕様書 §16 の1フレーム処理。System は「フェーズ」に所属し、フェーズ間だけが順序を持つ。
// 同じフェーズの System は、どの順で実行しても結果が変わらない(同じフェーズ内で互いの書き込みを読まない)。
// テストは EnableIntraPhaseShuffle でフェーズ内の順序を毎フレーム乱して、この性質を検証する。
//
//  Prepare    空間ハッシュを作る                                 SpatialIndex
//  Control    プレイヤーの移動(衝突解決込み)                      PlayerMovement
//  Interact   照準・岩押し・敵の出現(移動後の位置を読む)            PlayerAim, Push, EnemySpawn
//  EnemyThink 敵の行動決定(追跡/壁攻撃/岩押し)                   EnemyAI
//  EnemyPath  必要な経路の再探索                                  Pathfinding
//  EnemyMove  敵の移動(分離・衝突)                               EnemyMovement
//  Build      建築要求の検証・実行                                Building
//  Attack     攻撃の発動(自分の玉・爆弾・ビーム、敵の遠距離攻撃)              BoltWeapon, BombWeapon, BeamWeapon, EnemyShoot
//  Hazard     弾の移動・命中、爆弾の導火線と爆発                  Projectile, Bomb
//  Damage     ダメージ適用(接触ダメージ含む)                      Damage
//  Resolve    敵の死亡・壁の破壊                                  Death, Destruction
//  Reward     ドロップ生成・回収                                  ItemDrop, ItemPickup
//  Progress   経験値・レベル                                       Experience
enum class Phase : int {
    Prepare = 0, Control, Interact, EnemyThink, EnemyPath, EnemyMove, Build, Attack, Hazard, Damage, Resolve, Reward, Progress
};

}  // namespace game
