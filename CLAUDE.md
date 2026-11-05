# Game_Terrain Survivor
ヴァンパイアサバイバー系のサバイバーアクションに、岩を押す・壁を建てるなどの地形操作を組み合わせたゲーム(C++17 / SFML 3)。仕様は `docs/TerrainSurvivor_統合ゲーム仕様書_v0.1.md`。流用元は https://github.com/KDT-Hora/rimrim。

## 構成
- `src/ecs` `src/core` `src/game`: ゲーム本体。**SFML に依存させない**(`ts_tests` でヘッドレスに検証するため)。
  - `src/game/Systems/`: System を1つ1ファイル(`XxxSystem.h`)で置く。フェーズは `Phase.h`、登録は `ECSRegister.cpp`。
  - `MapManager`(タイル占有の唯一の管理者)、`GameSession`(ゲーム全体の進行。テストが直接動かす)。
- `src/app`: SFML の描画・入力・起動(確認用の `--frames` / `--screenshot` モード付き)。`DebugUi.*` は F1 のデバッグモード(Dear ImGui)。ImGui は `src/app` と `third_party/imgui*` だけに置き、`src/game` には持ち込まない。
- `data/balance.cfg`: バランス数値。ソースに数値を直書きしない。
- `third_party/SFML`: ビルド済み SFML 3(静的リンク)。
- Visual Studio 2022 で開く: `build/TerrainSurvivor.sln`、または「フォルダーを開く」(`CMakePresets.json`)。詳細は `build` スキル。
- ビルドと確認は `build` / `playtest` スキル。役は `.claude/agents` の implementer / reviewer / qa。

## 進め方
- 作業を始める前に、セッション開始時に注入された「前回の作業・次の作業」と未完了タスクを確認する。
- タスクは `docs/tasks/NNNN.md`。**status は編集しない**。状態の変更と作業ログは `pmg task` で行う(使い方は `pmg-task` スキル)。
- 作業の区切りや終了前に `/handoff` を実行し、`docs/progress/handoff.md` を更新する。
- 設計の判断で迷ったら `/kabeuchi <議題>` で壁打ちする。結論は人の確認を経てから採用される。
- 採用した決定事項は `docs/decisions.md` に残す。

## 禁止事項
- タスクファイルの `status` `source` `supersedes` `created` `updated` を編集しない。
- `docs/tasks/` のファイルを削除・改名しない。番号は再利用しない。
- `.env` などの秘密情報を読まない・出力しない。
- `git push` や再帰的な削除は、人の確認なしに実行しない。

## 参照
- 進捗: `docs/progress/handoff.md`(前回と次の作業)、`docs/progress/history.md`(ここまでの作業)
- タスク: `docs/tasks/`
