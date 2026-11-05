---
name: playtest
description: ゲームを自動で数フレーム動かしてスクリーンショットを撮り、見た目と挙動を確認する。「動作確認」「スクショ」「プレイテスト」と言われたとき、描画や挙動を変更した後に使う。
---
# playtest

人間のようにウィンドウを見られないため、固定ステップで N フレーム進めて PNG に保存する専用モードを使う。先に `build` スキルでビルドしておく。

```bash
cd build/Release
# 入力なしで 120 フレーム進めて保存
./terrain_survivor.exe --frames 120 --screenshot out/idle.png
# 入力スクリプト付き(例: 右へ移動しつつ、照準を右下にして壁を設置)
./terrain_survivor.exe --frames 300 --script "hold:D@0-120;aim:400,300@0-300;build@130" --screenshot out/build.png
# ボットに任せて実戦の見た目を見る(周回しながら最寄りの敵を撃ち、壁を建て、強化は先頭を選ぶ)
./terrain_survivor.exe --frames 3600 --bot --seed 4 --screenshot out/bot.png
# 強化を最初から付けて新しい攻撃手段を見る(id は balance.cfg の upgrades: bomb / beam / multishot ほか)
./terrain_survivor.exe --frames 900 --bot --give bomb,beam,multishot,multishot --screenshot out/weapons.png
# 設定を上書きして撮る(大型敵とボスをすぐ出す例。キーは balance.cfg のもの)
./terrain_survivor.exe --frames 700 --bot --set spawn.big_start=0 --set spawn.big_ratio_start=0.3 --set game.duration=8 --screenshot out/boss.png
# タイトル画面を撮る(自動開始しない)
./terrain_survivor.exe --frames 5 --no-autostart --screenshot out/title.png
# 決定的に再現したいとき
./terrain_survivor.exe --seed 1 --frames 600 --screenshot out/seed1.png
```

- スクリプトの書式: `hold:W|A|S|D@開始-終了` / `aim:x,y@開始`(画面座標) / `build@F` / `wall:N@F` / `pause@F` / `start@F` / `choose:N@F` / `click:x,y@F`(フレーム番号は 0 始まり)。
- 実行後に state/time/敵数/撃破数/HP/素材が標準出力に出る(画像を見なくても概況が分かる)。
- 保存した PNG は Read ツールで開いて内容を確認する。
- ロジック(建築判定・岩押し・A*・敵出現・勝敗)の合否は画面ではなく `./ts_tests.exe` で確認する。スクリーンショットは描画の確認用。
- 確認していない項目を「確認した」と書かない。

## デバッグ表示
- `--debug-draw` で当たり判定(円・通行不可タイル)を重ねて撮る。`--debug-paths` は敵の経路も描く。
- ImGui のウィンドウ(F1 / `--debug`)はヘッドレスのスクリーンショットには写らない。実ウィンドウを起動して確認する。
- exe が起動したままだと `build/Release/terrain_survivor.exe` を上書きできずリンクに失敗する。先に閉じる。
