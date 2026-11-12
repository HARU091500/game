# LAST STORAGE｜main作成とUE5統合の準備

確認日：2026-11-12。会社PCへのインストールは行わず、Unreal Editorの操作は自宅のWindows PCで実施する。**UE5の正確なバージョンは未確定。このクラウドにはUE Editor / UnrealBuildToolがなく、UEビルド・UHT・MSVC・PIEは未検証。**

## 1. GitHubの確認結果

| 項目 | 確認結果 |
|---|---|
| リポジトリ | `HARU091500/game` |
| 初回保存ブランチ | `codex/last-storage-initial-save` |
| 初回保存コミット | `88a443c0d7c13ed586d3ebca0294d7dd6835926d` |
| mainの事前状態 | `git ls-remote` で存在しないことを再確認 |
| mainの作成 | 初回保存と同じコミットを通常pushで作成。履歴の再作成・squash・強制pushなし |
| 既存40ファイル | mainへ同じ内容を引き継ぐ。ファイル自体を編集・削除しない |
| 本文書の作業ブランチ | `codex/ue5-integration-prep`。mainから分岐し、本文書だけを追加 |
| GitHubのデフォルト | main作成後もリモートHEADは `codex/last-storage-initial-save` を指している |

mainを作ることと、GitHubのデフォルトブランチ設定を変えることは別の操作。`git remote set-head` を変更してもGitHub側のデフォルトは変更されない。GitHub APIの取得は `Forbidden` になったため、設定変更は行っていない。Gitの読み書きは可能で、ブランチとコミットはGit経由で確認する。

### GitHub画面でmainをデフォルトにする

管理権限を持つアカウントで、以下を実施する。会社PCではブラウザだけで操作できる。

1. [リポジトリのSettings](https://github.com/HARU091500/game/settings) を開く。
2. **General → Default branch** の切替ボタンを選ぶ。
3. `main` を選択し、確認画面で更新を確定する。
4. Code画面のブランチ表示でmainがdefaultになったことを確認する。
5. クラウドでは `git ls-remote --symref origin HEAD` を再実行し、`ref: refs/heads/main HEAD` が返ることを確認する。

Settingsや切替項目が見えない場合は、管理権限のあるアカウントで操作する。保存用ブランチやmainを削除して対処しない。本書だけの作業ブランチはmainへまだマージしていないため、mainには元の40ファイル、準備ブランチには本文書を含む41ファイルがある。

## 2. 実際にあるUE向け構成

```text
Plugins/LastStorage/
  LastStorage.uplugin
  Source/LastStorageRuntime/
    LastStorageRuntime.Build.cs
    Public/
      LastStorageInspectable.h
      LastStorageInspectableItem.h
      LastStorageInspectionComponent.h
      Economy/LastStorageEconomy.h
      Gameplay/LastStorageSession.h
      Progression/LastStorageEndgame.h
    Private/
      LastStorageModule.cpp
      LastStorageInspectableItem.cpp
      LastStorageInspectionComponent.cpp
      Tests/LastStorageInspectionTests.cpp
Tools/Unreal/
  create_warehouse.py
  warehouse_layout.json
```

- プラグインのRuntimeモジュール名は `LastStorageRuntime`。`.uplugin`、Build.cs、`IMPLEMENT_MODULE`、APIマクロの名前は整合している。新規プロジェクト名 `LastStorage` とモジュール名が衝突しない。
- 現在の依存は `Core`、`CoreUObject`、`Engine`。入力はCharacter Blueprintから呼ぶ設計で、C++内ではEnhanced Inputへ依存しない。
- プラグインは `CanContainContent=false`。最初のBlueprint、入力アセット、マップはプロジェクト側の `/Game/LastStorage` に置く。
- 調査距離は初期300 cm。Visibilityの最初の衝突を調べ、調査結果をBlueprintイベントへ渡す。売却・所有・鑑定の接続とは別機能。
- 既存 `ItemId` は種類ID。個体IDとの対応、拾得、所持金UI、セーブは未接続。
- 標準C++の経済・進行・進捗はクラウドのテストとテキスト試遊で使用している。**現在のプラグインCPPはこれらのヘッダーを使用していないため、プラグインだけのUEビルド成功でドメイン全体のMSVC互換性を確認したとは扱えない。**

### まだ存在しないもの

`LastStorage.uproject`、プロジェクト側の `Source/LastStorage`、Game / Editorの `*.Target.cs`、プロジェクト `Config`、`Content`、保存済みマップ、CharacterやItemのBlueprint、Input Action / Mapping Context、UMG、UE側のSessionアダプター。

プラグインを含むソース集であり、今の40ファイルだけでUnreal Editorから開ける完成プロジェクトではない。実装済みの標準C++試作と、[今後の拡張仕様](ENDGAME_DESIGN.md)を区別する。

## 3. 統合作業の優先順位

| 優先 | 作業 | 完了とする証拠 |
|---|---|---|
| P0 | 自宅でUE5の正確な版と、それに対応するVisual Studio / MSVC / Windows SDKを確認。Git / Git LFSを準備 | 実際の版とインストール済み構成を記録。架空のEngineAssociationを設定しない |
| P1 | 別の空フォルダで公式First Person **C++** プロジェクト `LastStorage` を生成し、上書きせず作業ブランチへ取り込む | `.uproject`、Source、Targets、Config、Contentが存在し、公式テンプレート単体をビルドできる |
| P2 | 既存プラグインを有効にしてEditorを閉じたフルビルド。次の小さな変更でドメインを実際に使用するUEテストも作る | Development Editor / Win64でUHT / UBT成功。Economy / Session / Progressionをincludeして呼ぶTUとMSVCでの動作も確認 |
| P3 | Eキーの調査と倉庫1室・10個を接続する | PIEで歩行・衝突・距離・遮蔽・調査結果表示。保存して再起動しても設定が残る |
| P4 | 現行Sessionへの薄いUEアダプター、個体ID、設定データ、最低限のUIを順に追加する | Blueprintの要求が実際のSessionを呼び、入札から売却まで表示と状態が一致する。二重操作でも資金が壊れない |
| P5 | 保存・復元、図鑑・契約表示、レアリティ・保管の新版モデルへ段階的に移行する | 再起動後の所持金・所有・進捗が一致。新旧の仕様とテストを区別し、結果再適用・再抽選がない |

P1〜P3を先に通し、「1室を歩きEで調べる」ことを最初の実機目標にする。ブラックオークションや最終倉庫の設計を、この変更に一度に実装しない。詳細なエンドゲームの順序は [ENDGAME_ROADMAP.md](ENDGAME_ROADMAP.md) を参照する。

## 4. 自宅PCで次に行う操作

### A. 取得と作業ブランチ

現在の準備文書も取得する例。既にチェックアウトがある場合は複製せず、未コミット変更を確認した上でfetch・ブランチ切替を行う。クラウドと自宅で同じ作業ブランチへ同時に変更を積まず、役割ごとに分ける。

```powershell
git clone --branch codex/ue5-integration-prep https://github.com/HARU091500/game.git D:\Git\game
Set-Location D:\Git\game
git switch -c feature/ue5-first-person
git lfs install --local
git status --short
```

この準備ブランチがmainへ取り込まれた後は、mainを取得元にしてよい。mainへ直接実験的な変更を積まない。

### B. 公式テンプレートの生成と取り込み

1. 自宅PCに安定版UE5を導入し、Launcher / Help → Aboutの正確なバージョンを確認する。対応するVS・SDKは [既存のWindows手順](WINDOWS_QUICKSTART.md) にあるEpic公式資料で確認する。
2. Games → First Person → C++ を選び、追加Variantがあれば基本/Noneを選ぶ。プロジェクト名は `LastStorage`、保存先は例 `D:\UEProjects\LastStorage` の空フォルダ。
3. 生成したテンプレートを初回ビルド・起動し、EditorとIDEを閉じる。
4. `LastStorage.uproject`、`Source`、`Config`、`Content` をリポジトリへ取り込む。コピー先に同名が存在した場合は停止し、既存ファイルを上書きしない。具体的な保護付きコピー例は [Windows手順](WINDOWS_QUICKSTART.md) にある。
5. 既存 `Plugins/LastStorage` はそのまま使用する。`.sln`、Binaries、Intermediate、Saved、DerivedDataCacheはコピー・コミットしない。
6. コピー先でVSプロジェクトを生成し、Editorを閉じたままDevelopment Editor / Win64をフルビルドする。初回の新規UCLASSをLive Codingだけで検証しない。
7. Pluginsで `LAST STORAGE Prototype` を確認する。失敗した場合はUE版とエラーを記録し、成功と報告しない。

### C. 調査と最小倉庫

1. 実際にDefault Pawnへ指定されているCharacter Blueprintへ調査Componentを追加する。
2. Digital型 `IA_Inspect` をEへ割り当て、有効なMapping Contextで使う。`Started` からComponentの `Inspect` を呼び、`OnItemInspected` を開発用の表示へ接続する。
3. `LastStorageInspectableItem` のBlueprint子を `/Game/LastStorage/Blueprints/BP_InspectableItem` に作り、Compile / Saveする。
4. 実際のFirst Person用GameMode Blueprintのパスを確認する。テンプレートがC++のGameModeだけなら、そのBlueprint子を作り、編集したCharacter BPをDefault Pawnへ設定する。スクリプトへネイティブ `/Script/...` パスを渡さない。
5. Save All後、`create_warehouse.py` へそのGameMode BPの実パスを渡して新規マップを生成する。Python Editor Script Plugin / Editor Scripting Utilitiesが必要。スクリプトはGameMode BPとItem BPを事前要求し、既存マップや未保存作業がある場合は中止する。
6. Python APIの版差で失敗した場合は、自動削除や上書きをせず、部分生成された新しいマップを確認する。既存の [手動配置手順](WINDOWS_QUICKSTART.md) も使える。
7. Maps & ModesでGameModeと開始マップを確認し、PIEで歩行と調査を試す。保存・終了・再起動後にも同じ操作を確認する。

この段階では10種類の見た目は仮のCube。調査しただけで所持品へ移動したり、鑑定額が確定したり、売却されたりはしない。

### D. その後のC++接続

現行 `WarehouseSession` の所有場所をUE側に一つ用意する。候補はGameInstanceSubsystem等だが、具体的なクラスとライフサイクルはUE版確定後に選ぶ。Blueprintへ公開する情報はUSTRUCT等へ変換し、標準C++のコンテナや書き換え可能なウォレットを直接公開しない。

ゲームモジュールのCPPがプラグイン型を使う場合は、実際のincludeに応じて `LastStorageRuntime` のモジュール依存を追加する。既存のヘッダーがフォルダにあるだけではMSVCでの使用確認にならないため、3つのドメインを実際に呼び出すUEの小さなテストから始める。これは将来の作業で、今回追加したコードではない。

種類IDと個体IDを分け、倉庫ActorをSessionの個体へ対応付ける。C++の収集成功前にActorを破棄しない。売却・鑑定・支払いの判断をBlueprintへ複製せず、既存Sessionの結果をUIへ表示する。将来のCareer・保管・ブラック機能は設計書のStage単位で別に接続する。

## 5. 検証とGit管理の境界

- 今回の変更はmainの参照作成と本書だけ。元の40ファイルのハッシュと内容を維持する。
- UEプラグインの名前とファイル構造は静的に調査したが、構造の整合性をUEビルド成功と読み替えない。
- 過去のC++64件・Python27件は標準C++/Pythonの検証であり、UEのMSVC・UHT・Blueprint・3Dの成功を意味しない。今回は文書のみなのでゲームテストの再実行は行わない。
- 自宅で生成した `.uproject`、Source、Config、Content、入力・Blueprint・マップはGitへ保存する。`.uasset` / `.umap` はGit LFS対象。既存 `.gitignore` / `.gitattributes` を使い、大きなキャッシュを追加しない。
- GitHub Actionsの実行結果はこの環境から確認していない。設定が存在することと、Runnerが成功したことは分ける。
