# E キー調査機能の設計

現在は会社PCへのインストールを行わず、Codex Cloud / GitHubで開発します。本書のEditor接続と動作確認は自宅PCで後から実施します。UE5の正確なバージョンと本コードのUEビルドは未確認です。売却コアとの接点は [ARCHITECTURE.md](ARCHITECTURE.md) を参照してください。

## 責務と接続

```text
公式 First Person Character Blueprint
  IA_Inspect (Digital / E) の Started
    → LastStorageInspectionComponent.Inspect()
      → 所有 Pawn のローカル PlayerController / カメラ視点
      → Visibility の単一ライン判定（初期300 cm）
      → 最初のヒットが LastStorageInspectable を実装？
      → CanInspect → Inspect → GetInspectionInfo
      → OnItemInspected(Item, ItemName, Description)
    → Blueprint で表示（最初は Print String、後で UMG）
```

プラグインフォルダは `LastStorage`、C++ モジュールは **LastStorageRuntime** です。新規プロジェクト `LastStorage` のゲームモジュールと名前が衝突しません。プラグインは Runtime モジュールのみで、Editor Python / Blueprint 入力への C++ 依存はありません。

## C++ が担当すること

- `ILastStorageInspectable`：調査の可否、調査処理、情報取得の契約。将来の棚や家具も実装できる。
- `ULastStorageInspectionComponent`：所有者がローカルプレイヤーの Pawn であることを確認し、カメラから前方に `ECC_Visibility` の最初の遮蔽ヒットだけを調べる。所有 Pawn を除外する。
- `ALastStorageInspectableItem`：ID、表示名、説明、今回のプレイ中に調査したかという状態を持つ。`Inspect` で状態を更新し、`ReceiveInspected` で演出を Blueprint に渡す。
- `FLastStorageInspectionInfo`：IDと表示情報を返す Blueprint 対応の構造体。IDは種類を表し、同種アイテムを複数置く場合の個体識別子ではない。

距離は cm。初期値300、設定範囲1〜1000です。非数・0以下は失敗として扱います。Controller 不在、空振り、非対応 Actor、距離不足、遮蔽は `false` で終了し、成功した時だけ結果イベントを出します。調査中に独自 Blueprint が対象を破棄した場合は結果通知を省略します。対象ごとの常時 Tick は使いません。

最初のアイテムは Cube を持ち、Pawn と Visibility を Block し、物理シミュレーションを使いません。繰り返し調査を許可し、消費・獲得・価格の確定はしません。状態は一時的で、ゲーム終了後には保持しません。

## Blueprint / Editor が担当すること

- 公式 First Person の移動・視点と Enhanced Input の Context を利用する。
- Character に調査コンポーネントを追加し、E の **Started** から呼ぶ。C++ 側でキーやテンプレートのクラス名を固定しない。
- `BP_InspectableItem` を C++ Actor の子として作る。仮 Cube の形状は保持し、後でメッシュ・マテリアル・音・UI を追加する。
- 調査結果イベントを表示する。`Print String` は開発用であり、Shipping 製品 UI として扱わない。
- GameMode / Default Pawn を指定して、室内に PlayerStart と10個のアイテムを配置する。

Blueprint で `CanInspect` を上書きする時は、副作用のない判定にします。Actor の直接呼び出しにも対応するため、可否が複数回問い合わせられる場合があります。初期実装のままなら上書きは不要です。将来メッシュを置き換える場合は、カメラと Pawn の両方に必要なコリジョンを確認します。

## 今回の完了条件と保留

Windows で First Person テンプレートの C++ ビルドが通り、生成した倉庫で歩行・衝突・10個の調査・遮蔽を確認して初めてプレイ可能な最小構成が完成します。現時点ではその Editor 作業と検証が残っています。

売却・所持品・真贋・価格・入札・セーブは別の小さな変更として追加します。この段階でネットワーク同期、巨大マップ、AI は導入しません。
