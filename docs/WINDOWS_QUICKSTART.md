# LAST STORAGE — Windows / Unreal Editor での最初の作成

> **実施場所：後日の自宅PC。** 現在は会社PCにUE / Visual Studio等を導入せず、Codex CloudとGitHubで開発を進めます。この文書のインストール・Editor操作は今すぐ必要な作業ではありません。[現在の開発方針](../README.md) と [ロードマップ](ROADMAP.md) を先に参照してください。

このリポジトリには、まだ `.uproject`、First Person テンプレート、保存済み `.umap` / `.uasset` はありません。Linux のクラウド環境に Unreal Engine はありません。以下は **Windows の Unreal Editor でこれから実施する作業**です。倉庫の生成スクリプトと C++ プラグインは準備していますが、UE のビルド・Play 動作は未検証です。

## 1. UE バージョンを確認して記録

まだ未インストールとのことなので、先に Windows へ [Epic Games Launcher](https://www.unrealengine.com/download) を入れます。Launcher の Unreal Engine → ライブラリでエンジンを追加し、**安定版の UE5（Preview / Early Access 以外）** と First Person テンプレートを導入します。インストール先は十分な空き容量のあるドライブにします。Visual Studio は下記4の対応表を見て導入してください。この Linux クラウドにあるコードだけでは Windows Editor は起動できません。

1. Epic Games Launcher の「Unreal Engine → ライブラリ」で、使う **UE 5 の正確なバージョン**を確認します。Editor では「Help → About Unreal Editor」で確認できます。
2. 既存の Windows プロジェクトが別にある場合は、新規作成せず `.uproject` と `Source` / `Config` / `Content` の構成を先に確認してください。別バージョンへ勝手に変換しないでください。
3. この時点では UE バージョン未確定です。既存プロジェクトのバージョン指定を捏造せず、実際に作成した `.uproject` の `EngineAssociation` と上記のバージョンを Git に記録します。
4. Visual Studio 2022 Installer で「Game development with C++」「Desktop development with C++」、Windows SDK、使用する UE に対応した MSVC を導入します。対応する VS / SDK のバージョンは [Epic の Visual Studio 設定資料](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine) と Launcher のエンジン要件で確認してください。

## 2. First Person の C++ プロジェクトを新規作成

1. 先にこの Git リポジトリを Windows に取得します。例では `D:\Git\game` とします。
2. UE を起動して「Games → First Person」を選びます。
3. **C++** を選び、プロジェクト名を **`LastStorage`** にします。製品名は LAST STORAGE、C++ の識別子・ファイル名は LastStorage です。
4. テンプレートの Variant 選択があるバージョンでは「None / 基本」を選びます。Arena Shooter、Survival Horror などは追加しません。Starter Content、Ray Tracing などの追加項目があれば最初は無効にします。
5. 保存先は既存リポジトリとは別の空フォルダ、例 `D:\UEProjects\LastStorage` にします。既存ファイルへの上書きを避けるためです。
6. 作成・初回ビルド後、Editor と Visual Studio を閉じます。
7. 生成された `LastStorage.uproject`、`Source`、`Config`、`Content` だけを `D:\Git\game` 直下へコピーします。**コピー先の同名ファイル・フォルダが存在する場合は止めてください。** `Binaries`、`Intermediate`、`Saved`、`.vs`、`.sln` はコピー不要です。

PowerShell でコピーする場合、次は対象が一つでも既存なら全コピーを開始しません。パスは自分の環境に直してください。

```powershell
$template = 'D:\UEProjects\LastStorage'
$checkout = 'D:\Git\game'
$entries = @('LastStorage.uproject', 'Source', 'Config', 'Content')
foreach ($entry in $entries) {
    if (!(Test-Path (Join-Path $template $entry))) { throw "生成元がありません: $entry" }
    if (Test-Path (Join-Path $checkout $entry)) { throw "コピー先が既に存在します: $entry" }
}
foreach ($entry in $entries) {
    Copy-Item (Join-Path $template $entry) -Destination $checkout -Recurse
}
```

8. リポジトリの `Plugins/LastStorage` はこのまま使います。プラグインフォルダを二重に入れないでください。
9. コピー先の `.uproject` を右クリックして「Generate Visual Studio project files」。表示されない場合は「その他のオプションを確認」も確認します。
10. `.sln` を開き、構成 `Development Editor`、プラットフォーム `Win64`、ターゲット `LastStorage` をビルドします。新しい UCLASS / ヘッダーを追加した初回は Editor を閉じてフルビルドし、Live Coding だけに頼らないでください。
11. `.uproject` を開き「Edit → Plugins」で **LAST STORAGE Prototype** を有効にします。見つからない場合、配置が `game/Plugins/LastStorage/LastStorage.uplugin` か確認します。再起動・再ビルドを求められたら実行してください。

ここまでで First Person テンプレートそのものを UE が生成しています。このリポジトリのスクリプトが First Person テンプレートを模倣したものではありません。

## 3. E キーを C++ の調査機能へ接続

移動・マウス視点は公式 First Person の仕組みを使います。調査の判定・状態変更は C++、キーの接続と表示は Blueprint です。

1. テンプレートの **実際にプレイヤーとして使われる** Character Blueprint を開きます。GameMode の「Default Pawn Class」から探せます。`BP_FirstPersonCharacter` などの名前・パスは UE バージョンによって異なります。
2. Components の「Add」から **Last Storage Inspection Component** を追加し、名前を `Inspection` にします。見つからなければプラグインのフルビルドが成功したか確認します。
3. Content Browser で `/Game/LastStorage/Input` を作り、右クリック「Input → Input Action」で `IA_Inspect` を作成します。Value Type は **Digital (bool)** です。
4. テンプレートがプレイ開始時に有効化する **Input Mapping Context** を探します。Character / PlayerController の `Add Mapping Context` または既存の移動入力の参照から確認してください。名前を `IMC_Default` と決め打ちしないでください。
5. その Context に `IA_Inspect` を追加し、キーを **E** にします。E の既存割り当てがあれば競合を確認します。別 Context を作る場合はプレイ開始時に Local Player の Enhanced Input Local Player Subsystem へ `Add Mapping Context` で追加する必要があります。
6. Character の Event Graph に `IA_Inspect` の Enhanced Input イベントを配置します。**Started** ピンを使い、Components から `Inspection` をドラッグして `Inspect` を呼びます。Triggered を使うと押し続けて毎フレーム呼ばれる設定になり得ます。
7. `Inspection` のイベント **On Item Inspected** を追加します。出力の `Item Name` と `Description` を `Format Text` で `{Name}\n{Description}` にまとめ、`Print String` に接続します（Text → String 変換は自動変換または `To String (Text)`）。Duration は 3〜5 秒程度にします。これは開発用表示です。製品 UI は後で UMG に置き換えます。
8. Blueprint を Compile → Save。`Inspect` の返り値 false は「対象がない・届かない・遮られている」などを表します。必要なら失敗音やメッセージを Blueprint で追加できますが、最初は不要です。
9. 照準は **画面中央**です。必要なら Widget Blueprint に中央配置した小さな `+` を置き、BeginPlay で `Create Widget → Add to Viewport` してください。マウスカーソルの位置ではありません。

調査距離の初期値は 300 cm。棚や壁も Visibility の Trace を Block する必要があります。アイテムは内部で Visibility を Block します。調べても削除・売却はしません。

## 4. 倉庫 1 室と仮アイテム 10 種を作る

### 推奨：Editor Python で新規マップを生成

1. 「Edit → Plugins」で **Python Editor Script Plugin** と **Editor Scripting Utilities** を有効にし、必要なら再起動します。これらは Editor 用であり、ゲームプレイの実行時には不要です。
   Content Browser の表示設定で「Show C++ Classes」「Show Plugin Content」を有効にし、C++ Classes → LastStorageRuntime の **LastStorageInspectableItem** を右クリックして Blueprint 子クラスを作ります。保存先を `/Game/LastStorage/Blueprints/BP_InspectableItem` にして Compile → Save。継承した Mesh の Cube・Scale はこの段階では変更しません。後の見た目・演出はこの Blueprint に設定します。倉庫生成はこの Blueprint を10個配置します。
2. Character を使う First Person **GameMode Blueprint** を見つけます。「Class Defaults → Default Pawn Class」が手順3で編集した Character か確認します。
3. GameMode を右クリックして「Copy Reference」。例えば `Blueprint'/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode.BP_FirstPersonGameMode'` なら、使うパスは `/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode` です。自分の実際のパスを使ってください。
4. **File → Save All**。未保存のレベル・アセットがあるとスクリプトは中止します。
5. 「Tools → Debug → Output Log」（UI は版によって場所が異なります）を開き、入力モードを **Python** にして次を実行します。`runpy.run_path` によってスクリプト自身の場所が確定するため、JSON の読み取り先も安定します。

```python
import runpy
runpy.run_path(r'D:\Git\game\Tools\Unreal\create_warehouse.py', run_name='__main__', init_globals={'GAME_MODE_ASSET_PATH': '/Game/FirstPerson/Blueprints/BP_FirstPersonGameMode'})
```

6. 作成先は `/Game/LastStorage/Maps/L_Warehouse` です。**既存マップがあると上書きせず中止**します。途中で UE API の版差などのエラーが出ると新しいマップが一部だけ作成されている可能性があります。自動削除はしません。Output Log を確認し、既存の部分マップを確認・保存してから、JSON 内の `map_path` を別名にして再試行してください。
7. 成功すると新規マップに床・天井・4壁、ライト4個、PlayerStart 1個、仮アイテム10個が保存されます。World Settings の GameMode Override には指定した GameMode を設定します。Project Settings のデフォルトマップは自動変更しません。
8. 「Project Settings → Maps & Modes」で Editor Startup Map と Game Default Map にこのマップを選び、設定を保存します。後の Windows パッケージテストでは「Packaging → List of maps to include」にも追加してください。

内寸は **12 m × 10 m × 3 m**、壁厚50 cmのブロックアウトです。アイテムは全て 65 cm の箱で、種類を ID / 名前 / 説明で区別する仮表現です。個別の見た目・鑑定額・売却・オークションは未実装です。データは `Tools/Unreal/warehouse_layout.json` にあります。

### 手動の代替手順

Python が使えない場合も同じ構成を Editor で作れます。

1. 「File → New Level → Empty Level」で新規作成し、`/Game/LastStorage/Maps/L_Warehouse` へ **新しい名前で** 保存します。既存レベルの上書きをしないでください。
2. Place Actors から Cube を追加します。UE の Cube は一辺100 cmです。次の Location / Scale を Details → Transform に入力します（Rotation は全て0）。

| Actor | Location (X,Y,Z) cm | Scale (X,Y,Z) |
|---|---|---|
| 床 | 0,0,-25 | 13,11,0.5 |
| 天井 | 0,0,325 | 13,11,0.5 |
| 西壁 | -625,0,150 | 0.5,11,3 |
| 東壁 | 625,0,150 | 0.5,11,3 |
| 南壁 | 0,-525,150 | 12,0.5,3 |
| 北壁 | 0,525,150 | 12,0.5,3 |

3. 各 Cube の Collision Presets は **BlockAll**。PlayerStart を Location `0,-350,110`、Yaw `90` に置きます。
4. Point Light を `-300,-200,240`、`300,-200,240`、`-300,250,240`、`300,250,240` に置き、Mobility を Movable、Intensity を4000、Attenuation Radius を1000程度にします。露出や版によって明るさが異なるので調整してください。
5. Content Browser の C++ Classes → LastStorageRuntime から **LastStorageInspectableItem** の Blueprint 子クラス `BP_InspectableItem` を `/Game/LastStorage/Blueprints` に作ります（C++ Classes が隠れている場合は表示設定で C++ Classes / Plugin Content を表示）。直接 C++ Actor を配置しても構いません。
6. 10個配置して Scale を全て `0.65,0.65,0.65`、Z を32.5にします。X は `-400,-200,0,200,400`、Y は0と250の2列です。各 Actor の Item Id / Item Name / Description を JSON の10定義で設定します。ID は重複させません。
7. World Settings の GameMode Override を手順3の Character が使われる GameMode に設定し、Save All。後で見た目を作る時はこの Blueprint にメッシュや演出を設定します。

## 5. 初回の動作確認

結果は実際に Windows で実行して記録してください。この文書の存在は成功の証拠ではありません。

- `Development Editor / Win64` のフルビルドが成功する。
- 倉庫マップを開いて Play。WASD とマウスで室内を移動できる。床を抜けず、4壁と天井に衝突する。
- 10個が全て存在し、各 Item Id が異なる。
- 対象の近くで画面中央を合わせて E を1回押すと、対象の名前と説明が表示される。
- 同じ対象にもう一度 E を押してもクラッシュせず、再表示できる。
- 遠くから、空を向いて、壁や別の衝突オブジェクト越しに E を押しても対象を調査できない。
- `Show Collision` などで対象の衝突を確認する。通常カメラのまま PlayerStart からプレイし、Simulate で操作確認を代用しない。
- Save All → Editor を閉じる → `.uproject` を再度開く → 同じマップで再確認する。
- ビルドエラー・Blueprint エラー・Output Log の異常があれば、エンジン版とエラー全文を記録する。成功扱いにはしない。

売却や価格計算を追加する前に、このチェックを完了します。

## 6. Git とアセットの保存

`Source`、`Plugins/LastStorage/Source`、`.uproject`、`Config`、生成して保存した `Content` は Git で管理します。`.uasset` / `.umap` はバイナリなので、リポジトリの `.gitattributes` を使う場合は最初の追加前に `git lfs install --local` を実行してください。Git LFS のリモート保存容量・利用条件も確認します。

`Binaries`、`Intermediate`、`DerivedDataCache`、`Saved`、IDEキャッシュは追加しません。`git status --short` で追跡対象を確認します。別PCへ渡す前にソース・マップ・Blueprint・入力アセットをコミットし、必要な場合に GitHub へプッシュします。C++ ソースだけでは作成済みのレベルや入力設定は共有されません。

## 素材とライセンス

この段階では外部素材をダウンロードしていません。床・壁・仮アイテムは UE 付属の `/Engine/BasicShapes/Cube`、移動は UE 公式 First Person テンプレートを使います。これらは Unreal Engine の利用条件に従う Engine Content / Examples として扱います。UE は条件付きで商用利用できますが、無条件のパブリックドメイン素材ではありません。[Unreal Engine EULA](https://www.unrealengine.com/en-US/eula/unreal) の Examples、Engine Content、Royalty 条件を公開前に確認してください。Engine Content を単独の素材集として再配布せず、ゲームとしての配布条件に従います。Fab 素材などを追加する時は、その素材ごとの商用利用・再配布条件を別途記録します。
