# 決定事項
(採用した決定を、日付とともに追記します)

## 2026-11-05 初期セットアップ(タスク0001)

### 動作確認の方法(検証済み)
- ビルド/テスト/起動のコマンドは `.claude/skills/build/SKILL.md`、画面確認は `.claude/skills/playtest/SKILL.md`。
  - `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` → `cmake --build build --config Release`
  - ロジックの合否: `./build/Release/ts_tests.exe`(SFML 非依存。§19.2 の合格条件のうちロジックで確認できるものを検証。約20秒、うち11分相当のフルランを含む)。
  - 見た目: `./build/Release/terrain_survivor.exe --frames N --screenshot out/x.png [--seed S] [--script "hold:D@0-60;aim:900,500@0-300;build@70"] [--bot]`。ウィンドウを開かず固定ステップで N フレーム進めて PNG に保存する。
- ウィンドウの描画結果は `display()` 後のバックバッファが読めないため、RenderTexture に描いて保存する。
