---
name: kabeuchi
description: 設計や方針の判断を、Opus の2つの役(architect と critic)に壁打ちさせて、結論を docs/discussions に残す。「壁打ち」「/kabeuchi」と言われたときに使う。
argument-hint: <議題> [rounds=3]
---
# kabeuchi(壁打ち)

議題: $ARGUMENTS

`architect`(案を出す)と `critic`(批判する)の2つのサブエージェントで、案を磨きます。結論は人が確認してから採用されます。

## 準備
1. `pmg usage` を実行する。5時間枠が **80%以上** なら、壁打ちは使用量が大きいと伝え、続けてよいか人に確認する(答えが来るまで始めない)。
2. `pmg discussion new --topic "<議題>"` を実行し、出力された `D-NNNN` のフォルダを使う(`docs/discussions/D-NNNN/`)。
3. ラウンド数 `rounds` は既定3、最大5。

## 各ラウンド r
1. `architect` を呼ぶ。議題・これまでの `round-*-critic.md`・関係する文書の場所を渡し、`round-r-architect.md` に案を書かせる。
2. `critic` を呼ぶ。`round-r-architect.md` を渡し、`round-r-critic.md` に指摘を書かせる。指摘は **A(致命的)/B(重要)/C(軽微)** に分け、末尾に `新しいA指摘: N件` と書かせる。
3. 書き込み先は `docs/discussions/D-NNNN/` の中だけ。それ以外のファイルを変更させない。

## 止める条件(どれかに当てはまったら止める)
- 新しいA指摘が0件になった
- ラウンド数が上限に達した
- 同じA指摘が2ラウンド続けて未解決(膠着)

## 結論
主セッション(あなた)が `docs/discussions/D-NNNN/conclusion.md` を書く。形式は固定:

```
---
pmg_discussion: 1
rounds: <実施したラウンド数>
stop_reason: <no_critical | max_rounds | deadlock>
---
# 結論: <議題>
## 決定事項
- …
## 却下案と理由
- …
## 未解決
- …
## ユーザーへの質問
- …
## 提案タスク
- [実装/通常] タスクのタイトル
```

`## 提案タスク` の各行は `- [種別/優先度] タイトル` の形にする(種別: 調査|設計|実装|テスト|レビュー|文書化|環境構築|確認、優先度: 緊急|高|通常|低)。
`conclusion.md` を書いたら、人が確認画面で採用・却下します。`docs/decisions.md` やタスクは、あなたが直接書き換えない。
