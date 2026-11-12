# 検証記録

## 今回：図鑑・周回記録・契約・称号

同じ1室と10種類を再利用するエンドコンテンツの基礎を標準C++で実装しました。通常の1室試遊と既存の経済・進行コードは変更せず、結果登録を別の進捗クラスへ分離しています。

| 実行 | 結果 |
|---|---|
| `python3 Tools/run_economy_tests.py --suite all` | 経済23＋進行19＋エンド進捗22＝64件成功 |
| 同コマンドに `--sanitize` を追加 | 同64件成功、UBSanの検出エラーなし |
| `python3 -m unittest discover -s Tests -v` | Python27件成功 |
| `python3 Tools/run_session_demo.py --demo` | 既存の1室試遊を再確認、残高2450・利益1450 |
| `python3 Tools/run_session_demo.py --endgame-demo` | 通常→銅→銀→金、図鑑10/10、4クリア、称号「倉庫王」 |
| 同エンド試遊に `--sanitize` を追加 | 全契約達成、累計利益3900・最高利益1450・終了残高4900、検出エラーなし |

新規22件では、契約の解放を結果登録前に判定すること、実費・利益の両条件、条件未達でも通常記録が増えること、称号の再取得なし、図鑑種類の重複なし、未完了・落選・未知種類・既登録RunIdの拒否、初回赤字の最高記録、符号付き累計利益の上下オーバーフローと失敗時の進捗保持を確認しました。C++64件を2つの検査モードで実行したもので、128種類のテストではありません。

今回の変更は新規4・更新9の計13ファイルです。既存ファイルの削除はありません。未コミット・未プッシュです。

| 区分 | ファイル |
|---|---|
| 新規 | `Plugins/LastStorage/Source/LastStorageRuntime/Public/Progression/LastStorageEndgame.h` |
| 新規 | `Tests/endgame_tests.cpp` |
| 新規 | `Examples/endgame_demo.cpp` |
| 新規 | `docs/ENDGAME.md` |
| 更新 | `Tools/run_economy_tests.py`、`Tools/run_session_demo.py` |
| 更新 | `.github/workflows/cloud-checks.yml` |
| 更新 | `README.md`、`docs/GAME_DESIGN.md`、`docs/ROADMAP.md`、`docs/ARCHITECTURE.md`、`docs/TEXT_TRIAL.md`、`docs/VALIDATION.md` |

進捗は同一実行内のメモリのみです。通常の対話CLIへの進捗表示、永続化・復元、Steam実績、UEの表示・ビルド・3D実行は未実装または未検証です。UE5の正確なバージョンも未確定です。GitHub Actionsは設定のみ更新し、Runner上での実行は確認していません。報酬によるウォレットの変更はなく、デモの所持金は前の周回の終了残高を引き継いでいます。

## 前回：1室の進行とテキスト試遊

UEに接続する前の、入札・落札・収集・固定値鑑定・売却・終了の流れを実装しました。会社PCへのインストールは行わず、クラウドで実行した結果です。UE版未確定、UE実行・ビルド・Blueprint・3D配置は引き続き未確認です。

| 実行 | 結果 |
|---|---|
| `python3 Tools/run_economy_tests.py --suite all` | 経済23件＋進行19件＝42件成功 |
| 同コマンドに `--sanitize` を追加 | 同42件成功、UBSanの検出エラーなし |
| `python3 -m unittest discover -s Tests -v` | 既存配置16件＋仮データ11件＝27件成功 |
| `python3 Tools/run_session_demo.py --demo` | 10品の一周が終了、最終所持金2450／利益1450 |
| 同自動デモに `--sanitize` を追加 | 同じ結果で終了、検出エラーなし |
| ネイティブCLIの入力確認 | 不正・上限超過金額、所持金超過、落選、未収集鑑定、未鑑定売却、二重売却・落札、早期終了、余分な引数、EOF、無効な起動引数を確認 |
| 構文・設定 | 全34ファイルの空白、Python構文、JSON、Markdownのファイルリンク、Workflow YAMLの読み取りを確認 |

進行テストでは失敗前後の所持金・個体・倉庫・進行状態・収支が変わらないことを確認します。自動試遊の鑑定額はJSONから生成した実データをC++へ渡しており、PythonやCLIで売却計算を置き換えていません。

サンプルは初期資金1000、落札費用300、売上1750、最終所持金2450、利益1450です。NPCの競りや査定のランダム生成はありません。実行後の状態保存もありません。

今回の変更は以下の16ファイルです。既存ファイルの削除はなく、コミット・プッシュは未実施です。

| 区分 | ファイル |
|---|---|
| 更新 | `Plugins/LastStorage/Source/LastStorageRuntime/Public/Economy/LastStorageEconomy.h`：費用支払いと不足金エラー |
| 新規 | `Plugins/LastStorage/Source/LastStorageRuntime/Public/Gameplay/LastStorageSession.h`：1室の進行 |
| 新規 | `Tests/session_tests.cpp`：19件の進行・支払い検証 |
| 新規 | `Examples/session_demo.cpp`：実際のSessionを呼ぶテキスト試遊 |
| 新規 | `Data/mvp_economy.json`：10種の仮価格 |
| 新規 | `Tools/mvp_fixture.py`：配置との整合性確認・一時C++データ生成 |
| 新規 | `Tests/test_mvp_fixture.py`：11件のデータ検証 |
| 新規 | `Tools/run_session_demo.py`：一時ビルドと試遊起動 |
| 更新 | `Tools/run_economy_tests.py`：経済・進行・両方のテスト選択 |
| 新規 | `.github/workflows/cloud-checks.yml`：GitHub自動検証設定 |
| 新規 | `docs/TEXT_TRIAL.md`：試遊手順 |
| 更新 | `README.md`、`docs/GAME_DESIGN.md`、`docs/ROADMAP.md`、`docs/ARCHITECTURE.md`、`docs/VALIDATION.md` |

GitHub Actionsは設定のみ追加しました。同じコマンドはクラウド内で成功しましたが、GitHubのRunner上ではまだ実行していません。CI成功・UE動作成功・3Dゲーム完成とは報告しません。

## 前回：クラウド開発方針と最小売却コア

会社PCへUE / Visual Studio等は導入せず、Codex Cloud / GitHubで仕様とロジックを進めます。UE実行・ビルド・3D配置は自宅PCで後から行います。**UE5の正確なバージョンは未確定**です。

仕様書4点を先に作成・更新した後、UEに依存しない標準C++17のデータと売却コアを追加しました。3Dの拾得や鑑定・売却UIとの接続は行っていません。

| コマンド | 実行結果 | 範囲 |
|---|---|---|
| `python3 Tools/run_economy_tests.py` | C++23件成功 | 実際のコアを GCC 14.2.0 / C++17 / 警告エラー化 / 例外・RTTI無効でビルドし実行 |
| `python3 Tools/run_economy_tests.py --sanitize` | 同23件成功、検出エラーなし | UndefinedBehaviorSanitizerを有効化。整数上限付近の加算と売却も実行 |
| `python3 -m unittest discover -s Tests -v` | 既存16件成功 | 配置データと生成前ガード。Editor APIはスタブ |

売却成功時の残高加算と所有状態変更、同種別個体の区別、二重売却・売却済みIDの再取得の拒否、未鑑定、個体不明、不正ID、不正定義、負額、0価格、整数上限、失敗時の全状態保持を確認しました。C++23件は異なる検査モードで2回実行したもので、46種類のテストがあるわけではありません。

今回の変更は既存4ファイルの更新と、新規6ファイルです。既存のソース・アセットの削除や上書きはしていません。コミット・GitHubへのプッシュは未実施です。

| 区分 | ファイル |
|---|---|
| 更新 | `README.md` |
| 新規 | `docs/GAME_DESIGN.md` |
| 新規 | `docs/ROADMAP.md` |
| 新規 | `docs/ARCHITECTURE.md` |
| 新規 | `Plugins/LastStorage/Source/LastStorageRuntime/Public/Economy/LastStorageEconomy.h` |
| 新規 | `Tests/economy_tests.cpp` |
| 新規 | `Tools/run_economy_tests.py` |
| 更新 | `docs/WINDOWS_QUICKSTART.md`：自宅PCで後日行う手順と明記 |
| 更新 | `docs/INSPECTION_DESIGN.md`：現在方針と経済コアとの関係を追記 |
| 更新 | `docs/VALIDATION.md`：今回の記録を追加 |

UEアダプター、UHT / UBT / MSVCのビルド、Blueprint・所持金UI、3D配置・操作、セーブ、Windowsパッケージは今回も未実行または未実装です。クラウドテスト成功をこれらの完了として扱いません。

## 以前のプロトタイプ準備の記録

以下は方針変更前の初回19ファイル追加時の記録です。現在の導入指示は会社PC向けではありません。今回の結果と変更一覧は上記を参照してください。

2026-11-12 時点。対象は空の `HARU091500/game` に追加したプロトタイプ用ファイルです。Windows の UE は未インストールとの回答があり、正確な UE5 バージョンは未確定です。Linux クラウドに UE Editor / UnrealBuildTool は見つかりませんでした。

## この環境で実行した検証

| チェック | 結果 | 確認できること |
|---|---|---|
| `python3 -m unittest discover -s Tests -v` | 16件成功 | 10種類の配置データ、ID重複、寸法、壁・床・アイテム・開始位置の重複、既存マップ・未保存作業に対する事前中止 |
| Python 構文、JSON 読み取り | 成功 | スクリプトの構文とデータ形式。Unreal API の互換性は確認しない |
| 全追加ファイルの空白エラー・プラグイン設定 | 成功 | 追加19ファイルを確認。モジュール名と Build.cs のパスが一致 |
| Git 除外・LFS 属性 | 成功 | キャッシュが除外され、`.uasset` / `.umap` が LFS 対象。ローカル LFS hook を初期化済み |

生成前の中止テストは `unreal` API の **スタブ**を使います。本物の Editor のマップ生成や保存処理、衝突・歩行を実行したものではありません。

## 追加したファイル

以下の19ファイルを新規追加しました。既存の追跡ファイルの変更・削除はありません。未コミット・未プッシュです。別途、ローカル Git LFS の設定と hook を初期化しています。

```text
.gitattributes
.gitignore
README.md
Plugins/LastStorage/LastStorage.uplugin
Plugins/LastStorage/Source/LastStorageRuntime/LastStorageRuntime.Build.cs
Plugins/LastStorage/Source/LastStorageRuntime/Public/LastStorageInspectable.h
Plugins/LastStorage/Source/LastStorageRuntime/Public/LastStorageInspectableItem.h
Plugins/LastStorage/Source/LastStorageRuntime/Public/LastStorageInspectionComponent.h
Plugins/LastStorage/Source/LastStorageRuntime/Private/LastStorageModule.cpp
Plugins/LastStorage/Source/LastStorageRuntime/Private/LastStorageInspectableItem.cpp
Plugins/LastStorage/Source/LastStorageRuntime/Private/LastStorageInspectionComponent.cpp
Plugins/LastStorage/Source/LastStorageRuntime/Private/Tests/LastStorageInspectionTests.cpp
Tools/Unreal/create_warehouse.py
Tools/Unreal/warehouse_layout.json
Tests/test_warehouse_layout.py
docs/WINDOWS_QUICKSTART.md
docs/INSPECTION_DESIGN.md
docs/ASSET_REGISTER.md
docs/VALIDATION.md
```

## 未実行

- UnrealHeaderTool / UnrealBuildTool による C++ ビルド。
- UE Automation：`LastStorage.Inspection.ItemState`、`LastStorage.Inspection.UnpossessedPawn` の2件。
- 倉庫 `.umap` / アイテム Blueprint / Input Action の実生成・保存。
- PIE での歩行、衝突、E 入力、距離・遮蔽・再調査・表示。
- Windows パッケージ作成・Shipping 実行。

これらは UE 不在で未実行です。失敗した UE テストを成功扱いにしたり、構文確認を UE のビルド成功に読み替えたりしません。

## Windows でのビルド

[作成手順](WINDOWS_QUICKSTART.md) でプロジェクトを取り込んだ後、Editor を閉じて PowerShell で実行できます。`UE_5.x` は実際に導入したバージョンへ変更します。

```powershell
$ue = 'C:\Program Files\Epic Games\UE_5.x'
$project = 'D:\Git\game\LastStorage.uproject'
& "$ue\Engine\Build\BatchFiles\Build.bat" LastStorageEditor Win64 Development $project -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "UE ビルド失敗: $LASTEXITCODE" }
```

## Windows での UE Automation

ビルド成功後に同じ PowerShell で実行します。新しい出力先を使い、過去のレポートを今回の結果と混同しません。

```powershell
$report = Join-Path (Split-Path $project) ('Saved\Automation\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
if (Test-Path $report) { throw '別のレポート出力先を指定してください' }
& "$ue\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $project -unattended -nop4 -nosplash -NullRHI -stdout '-ExecCmds=Automation RunTests LastStorage.Inspection' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$report"
if ($LASTEXITCODE -ne 0) { throw "UE Automation 失敗: $LASTEXITCODE" }
```

今回のレポートが生成され、上記のテスト **2件が実際に実行・完了**したことを確認します。0件、未検出、スキップ、エラーは成功と扱いません。NullRHI の状態テストは映像・カメラのライン判定・入力を検証しません。

最後に通常の Editor で倉庫を開き、[初回の動作確認](WINDOWS_QUICKSTART.md#5-初回の動作確認) を実施します。結果と正確な UE / MSVC バージョン、必要ならエラーログを記録してください。
