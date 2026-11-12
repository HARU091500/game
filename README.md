# LAST STORAGE

一人称3Dの倉庫オークション・探索・アイテム収集・鑑定・売却・所持金管理ゲームです。最初の MVP は **倉庫1室・アイテム10種類・シングルプレイ**です。

## 現在の開発方針

**会社PCには Unreal Engine、Visual Studio、その他の開発ツールをインストールしません。** 当面はブラウザから Codex Cloud と GitHub を使い、仕様書・ソースコード・UE不要のロジックテストを進めます。UE5の実行・ビルド・Blueprint・3D配置は後から自宅PCで実施します。会社PCでのインストールは開発を進めるための条件ではありません。

対象は `HARU091500/game`。既存のプロトタイプ用プラグインと配置補助は保持します。`.uproject` と保存済みの `.umap` / `.uasset` はまだありません。**UE5の正確なバージョンは未確定**で、既存の UE 固有コードの互換性・ビルドは未検証です。クラウドに UE Editor / UnrealBuildTool はありません。

## 仕様と進め方

- [ゲーム仕様](docs/GAME_DESIGN.md)：ゲームループ、10種類のアイテム、MVPの範囲と暫定ルール。
- [ロードマップ](docs/ROADMAP.md)：クラウドで進める工程と、自宅PCでの検証工程。
- [アーキテクチャ](docs/ARCHITECTURE.md)：UE非依存のデータ・売却計算と、後で接続するUE層。
- [エンドコンテンツ](docs/ENDGAME.md)：収集図鑑、周回記録、銅・銀・金の契約と称号。
- [エンドゲーム拡張設計](docs/ENDGAME_DESIGN.md)／[6段階の実装計画](docs/ENDGAME_ROADMAP.md)：ブラックオークション、レアリティ、保管、設備、最終倉庫の未実装仕様。
- [検証記録](docs/VALIDATION.md)：実施結果、未実行の確認、今回の変更ファイル。

ゲーム判定は C++、見た目・演出・配置・UI接続は Blueprint / Editor に分担します。現在は標準C++で1室の入札・落札・収集・鑑定・売却を実装し、クラウドでテキスト試遊できます。UE不要のテストが通っても、UEのゲームが起動・操作できるという判定には使いません。

## 今すぐクラウドで試せること

Codexに「テキスト試遊の自動デモを実行して」と依頼すると、10種類すべてを売る一周を実行できます。自分の操作を指定する対話形式も用意しています。操作手順は [テキスト試遊](docs/TEXT_TRIAL.md) を参照してください。

```bash
python3 Tools/run_session_demo.py --demo
```

実行確認済みの仮データでは、資金1000 → 落札費用300 → 売上1750 → 最終資金2450、利益1450になります。同じC++の進行処理を後からUEへ接続するためのサンプルで、3Dや音声はありません。

図鑑完成から銅→銀→金の契約へ進む4周の自動サンプルも用意しました。同じ1室・10種類を再利用し、図鑑10/10、称号「倉庫王」、累計利益3900、所持金4900になることを確認しています。進捗は実行中のみ保持し、終了後は保存しません。通常の対話モードは従来通り1室の練習です。

```bash
python3 Tools/run_session_demo.py --endgame-demo
```

## ファイル構成

| 場所 | 内容 |
|---|---|
| `Plugins/LastStorage/` | 調査機能の既存UEコードと、UE5 C++プロジェクトに取り込むゲームロジック |
| `Plugins/LastStorage/Source/LastStorageRuntime/Public/Economy/LastStorageEconomy.h` | UE非依存の種類定義・個体・所持金・鑑定額記録・売却コア |
| `Plugins/LastStorage/Source/LastStorageRuntime/Public/Gameplay/LastStorageSession.h` | 所持金・個体を所有し、1室の入札から全品売却まで進行を管理 |
| `Plugins/LastStorage/Source/LastStorageRuntime/Public/Progression/LastStorageEndgame.h` | 図鑑・周回記録・称号・段階契約の判定。ウォレットは変更しない |
| `Tests/economy_tests.cpp` / `Tools/run_economy_tests.py` | 実際のC++コアをビルドしてテスト。一時ディレクトリで実行し、出力を残さない |
| `Tests/session_tests.cpp` / `Examples/session_demo.cpp` | 進行テストと、同じ実装を使うテキスト試遊 |
| `Tests/endgame_tests.cpp` / `Examples/endgame_demo.cpp` | エンド進捗の検証と4周の自動試遊 |
| `Data/mvp_economy.json` / `Tools/mvp_fixture.py` | 10種類の仮価格と、既存の名前・説明データとの整合性検査 |
| `Tools/Unreal/create_warehouse.py` | Editor 上で新しい倉庫レベルを生成。既存レベルと未保存の作業を保護 |
| `Tools/Unreal/warehouse_layout.json` | 12×10×3 m の倉庫と10種類の仮アイテム定義 |
| `Tests/test_warehouse_layout.py` | UE 不要の配置データ・生成前チェックのテスト16件 |
| `.gitignore` / `.gitattributes` | 生成キャッシュを除外し、UE バイナリアセットを Git LFS 対象に設定 |
| `.github/workflows/cloud-checks.yml` | GitHubへプッシュ後の自動検証設定。GitHub上の実行は未確認 |
| `docs/` | 仕様・設計・ロードマップ・検証結果・後日の自宅PC用Editor操作・素材の出典 |

[調査機能の設計](docs/INSPECTION_DESIGN.md) と [素材の利用条件](docs/ASSET_REGISTER.md) も保持しています。[Windows / Unreal Editor 作成手順](docs/WINDOWS_QUICKSTART.md) は **後日、自宅PCで行う作業**の参考です。

## クラウドで実行できる検証

Codex Cloud のリポジトリ直下で実行します。会社PCへツールを導入する操作はありません。追加の外部パッケージは使いません。

```bash
python3 Tools/run_economy_tests.py --suite all
python3 Tools/run_economy_tests.py --suite all --sanitize
python3 -m unittest discover -s Tests -v
python3 Tools/run_session_demo.py --demo --sanitize
python3 Tools/run_session_demo.py --endgame-demo --sanitize
```

今回、C++テスト64件（経済23＋進行19＋エンド進捗22）、未定義動作検査付きの同64件、Pythonテスト27件が成功しました。二重操作・二重集計・契約条件未達・赤字・整数上限を確認しています。UEビルド・ゲーム内操作の成功を示す結果ではありません。

## 未完了と共有

移動や視点操作は後で公式 First Person テンプレートに接続します。10種類の専用モデル、NPCとの競りやタイマー、3D収集、鑑定UI、売却UI、セーブ、製品UIは未実装です。テキスト試遊の勝敗は手動指定、鑑定は固定データです。マルチプレイ、巨大マップ、複雑なAIは最初の範囲に含めません。

ソース・仕様・設定を Git で管理し、保存した UE アセットは Git LFS 対象にします。`Binaries`、`Intermediate`、`Saved`、`DerivedDataCache` は共有しません。別タスクや別PCへ渡す変更は作業ブランチにコミット・プッシュし、GitHub側のブランチとコミットIDを照合して保存を確認します。各文書の「未コミット・未プッシュ」は、その文書を作成した時点の履歴です。
