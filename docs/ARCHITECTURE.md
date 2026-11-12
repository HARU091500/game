# LAST STORAGE アーキテクチャ

## 前提と現在地

2026-11-12 の方針変更に基づき、会社PCではブラウザの Codex Cloud / GitHub のみを使います。設計・データ・ロジックの実装とテストをクラウドで進め、UEの実行・ビルド・3D配置は自宅PCで後から行います。**UE5の正確なバージョンは未確定**です。

現在のリポジトリには `LastStorageRuntime` プラグイン、調査用のUEコード、Pythonによる倉庫配置補助があります。`.uproject`、テンプレート由来のゲームモジュール、Blueprint、保存済みマップはありません。これらを生成・検証済みとは扱いません。

## 層と依存方向

```text
Blueprint / UMG / Level（自宅PCで後日作成）
          ↓ 表示・入力・演出
UEアダプター（今後実装、Subsystem等の型はUE版確定後に選定）
          ↓ C++で型変換・状態の所有
LastStorage::Gameplay::WarehouseSession（進行と操作権限）
          ↓ 内部の経済状態は読み取り専用で公開
LastStorage::Economy::PlayerEconomy（標準 C++17 の金額・個体管理）
          ↑ クラウドの単体テストとテキスト試遊から呼び出す
```

コアは `Plugins/LastStorage/Source/LastStorageRuntime/Public/Economy/LastStorageEconomy.h` に置くヘッダーのみの小さな実装です。UEマクロ、UObject、Blueprint、FName、FText、外部ライブラリに依存させません。既存プラグインの Public ヘッダーなので、後でUEのC++クラスから同じ実装を include できます。外部DLLや別のゲームロジックのコピーは不要です。

同じ Public 以下の `Gameplay/LastStorageSession.h` が所持金・倉庫の定義・個体・進行を所有します。CLIや将来のUEアダプターから、落札前の収集や任意価格での鑑定を行わせない窓口です。ゲーム側は `Bid`、`ResolveAuction`、`Collect`、`Appraise`、`Sell`、`FinishRound` を呼びます。

標準C++のテストでビジネスルールを検証してから、UE層を少しずつ接続します。UHT / UBT / MSVCでの互換性検証は別途必要です。既存のUEクラスや Blueprint 公開APIの追加・変更は今回のコア実装では行いません。

## アイテムと所持金

| データ | 役割 | 不変条件 |
|---|---|---|
| `ItemDefinition` | 種類ID、表示名、説明、参考価格。radio / camera等の種類を表す | IDと表示名が有効、参考価格は0以上 |
| `ItemInstance` | 個体ID、種類ID、鑑定状態、鑑定額、所有状態 | 個体IDはセッション内で再利用しない。種類IDとは別に持つ |
| `Money` | 最小通貨単位の整数 | `std::int64_t`、所持金・価格は0以上。円等の実通貨とは未連携 |
| `PlayerEconomy` | 所持金と取得済み個体の記録を一括管理 | 呼び出し側へ書き換え可能な内部状態を公開しない |
| `Result` / `Error` | 成功・失敗と移動した金額 | 失敗時は金額0、所持金と個体状態を変えない |

金額に浮動小数点は使いません。最初の売却額は **個体に記録された鑑定額そのもの**です。相場変動、状態倍率、手数料は後続工程とし、参考価格を売却時に無条件で使いません。未鑑定は売却不可を暫定MVPルールとして明記します。

IDは小文字ASCII英数字・`_`・`-`で表し、空や `none` は拒否します。UEのFNameとの大文字小文字の扱いの差を持ち込まないため、型変換時もこの表記を維持します。表示名は日本語を含むUTF-8テキストであり、IDとして使いません。

## コアの最初の操作

| 操作 | 内容 | 主な失敗条件 |
|---|---|---|
| `Credit(amount)` | 初期資金等を整数で加算 | 負額、所持金の上限超過 |
| `Spend(amount)` | 落札等の費用を整数で減算 | 負額、残高不足 |
| `AcquireItem(definition, instanceId)` | 信頼できる呼び出し側から個体を登録。未鑑定・所有中で開始 | 無効なデータ、以前に登録した個体IDの再利用 |
| `AppraiseItem(instanceId, value)` | 所有中の個体に確定済み鑑定額を記録 | 個体不明、売却済み、負額 |
| `SellItem(instanceId)` | 未鑑定を拒否し、鑑定額加算と売却済み状態への遷移を行う | 個体不明、未鑑定、売却済み、所持金の上限超過 |

上記は低水準の経済APIです。現在のSessionでは開始時だけ初期資金を付与し、落札時に費用を一度だけ引きます。`Credit` を売却UIへ直接公開したり、UIから好きな鑑定額を渡したりしません。MVPの初期資金案1000はデータの仮値で、コアの定数として固定しません。3D操作やUEの画面からの呼び出しは未接続です。

## 1室の進行状態

```text
NotStarted --有効な定義と初期資金--> Auction
Auction --入札して落札確定・一度だけ支払い--> Exploration
Auction --入札して落選確定・支払いなし--> Finished (Lost)
Exploration --全品を収集・鑑定・売却してfinish--> Finished (Won)
```

入札自体は支払いません。落札後の `Collect` が経済コアへの登録成功を確認してから倉庫側を収集済みにします。`Appraise` は登録した固定データから価格を取り、`Sell` は既存の売却ロジックを呼びます。売上合計は `現在残高−(初期残高−落札費用)` から導き、別の上限未確認加算を増やしません。Session外から所持金や記録を書き換える参照は返しません。

`Examples/session_demo.cpp` はこのSessionへの入力と表示だけを持ちます。`Data/mvp_economy.json` と配置JSONの名前・説明を `Tools/mvp_fixture.py` で検証・結合し、一時ディレクトリに試遊用C++データを生成します。これはUEのDataAssetやセーブデータではありません。CLIの手順は [TEXT_TRIAL.md](TEXT_TRIAL.md) にあります。

標準C++での呼び出し例です。UEでは後でアダプターがこの状態を所有します。このコードの存在だけでBlueprintから売却できるようにはなりません。

```cpp
#include "Economy/LastStorageEconomy.h"

bool RunSaleExample()
{
    LastStorage::Economy::PlayerEconomy Economy;
    const LastStorage::Economy::ItemDefinition Radio{
        "radio", "Radio", "A portable radio.", 250
    };
    if (!Economy.Credit(1000).Succeeded()) { return false; }
    if (!Economy.AcquireItem(Radio, "room01-radio-001").Succeeded()) { return false; }
    if (!Economy.AppraiseItem("room01-radio-001", 250).Succeeded()) { return false; }
    const auto Sold = Economy.SellItem("room01-radio-001");
    return Sold.Succeeded() && Sold.Amount == 250 && Economy.Balance() == 1250;
}
```

## 売却の状態遷移

1. 対象の個体IDと所持記録を確認する。
2. 売却済み・未鑑定を拒否する。
3. `value <= max(int64) - balance` を確認する。超過する場合は全状態を保つ。
4. 所持金へ鑑定額を加算し、個体を売却済みにする。
5. 成功と加算額を返す。表示・演出は呼び出し元が行う。

売却済み個体の記録はセッション内に残して二重売却・再取得を拒否し、所有アイテム数から除外します。検証後の更新は単一スレッドのメモリ内処理です。DBトランザクション、永続化、クラッシュ復旧、ネットワーク同期を保証するものではありません。アダプターはゲームスレッドから呼ぶ前提です。

## 既存コードとの接点

既存の `ALastStorageInspectableItem::ItemId` は種類IDです。Eキーで「調べた」状態と、価格が確定した「鑑定済み」は区別します。今回 `IsInspected()` を鑑定状態や所有状態に読み替えません。

将来はUE側がSessionの個体IDをワールドActorへ対応付け、拾得要求で `Collect`、鑑定要求で `Appraise`、売却要求で `Sell` を呼びます。内部の経済APIをUIへ直接渡しません。成功前にワールドActorを破棄したり、UIだけで所持金を書き換えたりしません。種類の表示データを `UDataAsset` / DataTable にするかは、UE版確定後のアダプター工程で選定します。

## エンド進捗の所有と結果登録

`Progression/LastStorageEndgame.h` の `EndgameProgress` が、図鑑・クリア数・累計と最高利益・契約達成・称号を所有します。Sessionやウォレットは所有せず、終了した勝利Sessionを `RecordClear` で登録します。契約解放は登録前の状態、達成条件は実際の費用と利益で判定します。

呼び出し元が実プレイ1回に固定したRunIdを使い、再送には同じIDを使います。既登録IDを拒否しますが、別IDを付けた偽装の防止やオンライン認証は対象外です。未知の種類、未完了や落選、数値上限超過では進捗を一切変えません。赤字も累計し、最高利益は初回の実値で初期化します。

`Examples/endgame_demo.cpp` は図鑑と通常→銅→銀→金を同一プロセスで実行します。次のSessionには前の終了残高だけを渡し、称号で残高を増やしません。現在の進捗はメモリのみで、通常CLI・UE表示・SaveGameへの接続は未実装です。[仕様](ENDGAME.md)も参照してください。

## 検証範囲

クラウドでは実際のC++コアをコンパイルし、成功・未鑑定・個体不明・重複ID・二重売却・0価格・整数上限・失敗時の状態保持をテストします。配置Pythonの既存テストも保持します。

UE側のコンパイル、Blueprint公開、カメラ・入力・衝突、マップ保存、パッケージ化は自宅PCでの未実行項目です。検証結果は [VALIDATION.md](VALIDATION.md) に分けて記録します。
