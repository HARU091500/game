# クラウドで試せる1室のテキスト試遊

このサンプルでは、実際のC++ゲーム進行を使って **入札→落札→収集→鑑定→売却→収支確認** を試します。会社PCへのインストールは不要で、Codex Cloud上で実行します。UE5の正確なバージョンは未確定です。UE / 3D / Blueprint / 音声は実行していません。

## まず結果を見る

Codexに「テキスト試遊の自動デモを実行して」と依頼するか、クラウドのリポジトリ直下で次を実行します。

```bash
python3 Tools/run_session_demo.py --demo
```

コンパイラとPythonはクラウドにあるものを使い、外部パッケージを追加しません。一時ディレクトリでビルド・実行し、終了時にビルド出力を消します。セーブはありません。

現在の固定データで実際に確認した結果です。

| 項目 | 金額 |
|---|---:|
| 初期所持金 | 1000 |
| 落札費用 | 300 |
| 10品の売上合計 | 1750 |
| 最終所持金 | 2450 |
| 利益（売上−落札費用） | 1450 |

```text
RESULT balance=2450 revenue=1750 cost=300 profit=1450 phase=finished
```

## 自分で操作を指定する

クラウドで次を実行すると対話形式になります。会社PCのターミナルで実行する指示ではありません。

```bash
python3 Tools/run_session_demo.py
```

| 入力 | 内容 |
|---|---|
| `help` | 操作一覧 |
| `status` | 所持金と進行状況 |
| `bid 300` | 300で入札。まだ支払いは発生しない |
| `win` | 試遊用に落札を確定し、支払って倉庫を開く |
| `lose` | 試遊用に落選を確定。支払いなしで終了 |
| `list` | 落札後の品物と状態を表示 |
| `collect 1` | 一覧の1番を収集 |
| `appraise 1` | 1番を固定データの金額で鑑定 |
| `sell 1` | 1番を売却 |
| `finish` | 全10品の売却後に今回の倉庫を終了 |
| `quit` | 途中で試遊を終える。保存しない |

品物は1〜10の番号か表示された個体IDで指定します。最初は `bid 300`、`win`、`collect 1`、`appraise 1`、`sell 1` の順で試せます。残りの品物も同じ手順です。

落札前に収集する、所持金を超えて入札する、未鑑定で売る、同じ個体を何度も売る操作は拒否します。失敗しても残高・所有状態・進行を変えません。未鑑定品の金額は表示せず、落札前は品物一覧も出しません。

## 実装と今後のUE接続

`Examples/session_demo.cpp` は入力の解析と表示だけを担当します。ゲームルールは `LastStorageSession.h` から既存の `LastStorageEconomy.h` を呼びます。後でUEのアダプターも同じSessionを所有し、Blueprintへ結果を返す方針です。

名前・説明は `Tools/Unreal/warehouse_layout.json`、仮価格は `Data/mvp_economy.json` を参照します。`Tools/mvp_fixture.py` が整合性を検査し、一時的なC++データへ変換します。生成ヘッダーをリポジトリへコピーせず、価格判定をPythonで再実装しません。参考価格と鑑定額は別の項目にしており、売却には鑑定額を使います。

NPCの競り、鑑定結果のランダム生成、途中撤退、複数倉庫、セーブ、実際の3D操作・映像は対象外です。固定値による鑑定と手動の勝敗指定は開発用の暫定ルールです。3D版は自宅PCでUEを導入して接続・検証する必要があります。

## エンドコンテンツの自動試遊

通常の試遊とは別に、図鑑完成と3段階の契約を4周で確認する自動サンプルがあります。

```bash
python3 Tools/run_session_demo.py --endgame-demo
```

図鑑10/10、銅・銀・金達成、称号「倉庫王」、累計利益3900、終了所持金4900を確認しています。この進捗は同じ実行中のみ保持し、通常の対話CLIやUE画面へは未接続です。[エンドコンテンツ仕様](ENDGAME.md)に条件と結果を記載しました。

## 自動チェック

```bash
python3 Tools/run_economy_tests.py --suite all
python3 Tools/run_economy_tests.py --suite all --sanitize
python3 -m unittest discover -s Tests -v
python3 Tools/run_session_demo.py --demo --sanitize
python3 Tools/run_session_demo.py --endgame-demo --sanitize
```

GitHub向けに `.github/workflows/cloud-checks.yml` も用意しています。GitHubへプッシュ後に同じチェックを行う設定ですが、現在は **GitHub上での実行結果は未確認**です。クラウド内での成功と区別します。
