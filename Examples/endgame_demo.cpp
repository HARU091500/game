#include "Gameplay/LastStorageSession.h"
#include "Progression/LastStorageEndgame.h"
#include "LastStorageMvpFixture.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    namespace Economy = LastStorage::Economy;
    namespace Gameplay = LastStorage::Gameplay;
    namespace Progression = LastStorage::Progression;
    namespace Samples = LastStorage::Samples;

    struct RoundPlan
    {
        Progression::Challenge Contract;
        const char* Name;
        const char* RunId;
        Economy::Money Bid;
    };

    const char* RankName(Progression::Rank rank)
    {
        switch (rank)
        {
        case Progression::Rank::Rookie: return "見習い";
        case Progression::Rank::Collector: return "収集家";
        case Progression::Rank::Dealer: return "目利きバイヤー";
        case Progression::Rank::Expert: return "熟練鑑定士";
        case Progression::Rank::Master: return "倉庫王";
        }
        return "不明";
    }

    bool SessionStep(const Gameplay::SessionResult& result, std::string_view operation)
    {
        if (result.Succeeded()) { return true; }
        std::cerr << operation << "に失敗しました。進行エラー="
                  << static_cast<int>(result.Code) << "、経済エラー="
                  << static_cast<int>(result.EconomyCode) << '\n';
        return false;
    }

    bool ProgressStep(const Progression::ProgressResult& result, std::string_view operation)
    {
        if (result.Succeeded()) { return true; }
        std::cerr << operation << "に失敗しました。進捗エラー="
                  << static_cast<int>(result.Code) << '\n';
        return false;
    }

    void ShowProgress(const Progression::EndgameProgress& progress)
    {
        std::cout << "図鑑: " << progress.CollectionCount() << '/' << progress.CollectionTarget()
                  << " / クリア: " << progress.ClearCount()
                  << " / 黒字クリア: " << progress.ProfitableClearCount() << '\n'
                  << "累計利益: " << progress.TotalProfit()
                  << " / 最高利益: " << progress.BestProfit()
                  << " / 称号: " << RankName(progress.CurrentRank()) << '\n'
                  << "解放済み契約: 銅=" << static_cast<int>(progress.IsUnlocked(Progression::Challenge::Bronze))
                  << " 銀=" << static_cast<int>(progress.IsUnlocked(Progression::Challenge::Silver))
                  << " 金=" << static_cast<int>(progress.IsUnlocked(Progression::Challenge::Gold)) << '\n';
    }

    bool RunRound(const RoundPlan& plan, const Gameplay::WarehouseDefinition& source,
                  Progression::EndgameProgress& progress, Economy::Money& funds)
    {
        // Eligibility is checked before this round can change the profile.
        if (!progress.IsUnlocked(plan.Contract))
        {
            std::cerr << plan.Name << "はまだ解放されていません。\n";
            return false;
        }
        Gameplay::WarehouseDefinition warehouse = source;
        const auto rule = Progression::RuleFor(plan.Contract);
        warehouse.OpeningBid = std::max(warehouse.OpeningBid, rule.MinimumPurchaseCost);

        std::cout << "\n--- " << plan.Name << " ---\n"
                  << "引き継ぐ所持金: " << funds
                  << " / 開始価格: " << warehouse.OpeningBid
                  << " / 今回の入札: " << plan.Bid << '\n';

        Gameplay::WarehouseSession session;
        if (!SessionStep(session.Start(warehouse, funds), "倉庫の開始")
            || !SessionStep(session.Bid(plan.Bid), "入札")
            || !SessionStep(session.ResolveAuction(true), "試遊用の落札確定"))
        {
            return false;
        }
        for (const auto& item : session.Warehouse().Items)
        {
            if (!SessionStep(session.Collect(item.InstanceId), "収集")
                || !SessionStep(session.Appraise(item.InstanceId), "鑑定")
                || !SessionStep(session.Sell(item.InstanceId), "売却"))
            {
                return false;
            }
        }
        if (!SessionStep(session.FinishRound(), "全品売却後の終了")) { return false; }

        const auto registered = progress.RecordClear(plan.RunId, session, plan.Contract);
        if (!ProgressStep(registered, "クリア結果の登録")) { return false; }
        if (plan.Contract != Progression::Challenge::None
            && (!registered.ChallengePassed || !registered.NewlyCompleted))
        {
            std::cerr << "この自動試遊では契約の新規達成が必要です。金額条件を確認してください。\n";
            return false;
        }

        // Only the real session balance is carried forward. Titles never credit money.
        funds = session.Economy().Balance();
        std::cout << "落札費用: " << session.PurchaseCost()
                  << " / 売上: " << session.SaleRevenue()
                  << " / 今回の利益: " << session.Profit()
                  << " / 終了所持金: " << funds << '\n';
        if (registered.NewlyCompleted)
        {
            std::cout << "契約を新規達成し、称号を獲得しました。追加の現金報酬はありません。\n";
        }
        ShowProgress(progress);
        return true;
    }
}

int main()
{
    std::cout << "LAST STORAGE エンドコンテンツ試遊\n"
              << "UE・3D版ではありません。UE5の正確なバージョンは未確定です。\n"
              << "同じ1室・10種類を再利用し、固定価格と手動の勝敗確定で4周を試します。\n"
              << "NPC・タイマー・新しい3D素材・Steam連携はありません。進捗は保存しません。\n";

    const auto warehouse = Samples::MakeMvpWarehouse();
    std::vector<std::string> catalog;
    catalog.reserve(warehouse.Items.size());
    for (const auto& item : warehouse.Items) { catalog.push_back(item.Definition.TypeId); }

    Progression::EndgameProgress progress;
    if (!ProgressStep(progress.Configure(catalog), "図鑑の設定")) { return 1; }
    ShowProgress(progress);

    const std::array<RoundPlan, 4> plans{{
        {Progression::Challenge::None, "通常倉庫：図鑑を完成", "endgame-run-001", Samples::DemoBid},
        {Progression::Challenge::Bronze, "銅契約：黒字の達人", "endgame-run-002", Samples::DemoBid},
        {Progression::Challenge::Silver, "銀契約：大口勝負", "endgame-run-003", 1000},
        {Progression::Challenge::Gold, "金契約：最後の保管庫", "endgame-run-004", 1500},
    }};
    Economy::Money funds = Samples::StartingFunds;
    for (const auto& plan : plans)
    {
        if (!RunRound(plan, warehouse, progress, funds)) { return 1; }
    }

    std::cout << "\nENDGAME clears=" << progress.ClearCount()
              << " collection=" << progress.CollectionCount() << '/' << progress.CollectionTarget()
              << " profit=" << progress.TotalProfit()
              << " best=" << progress.BestProfit()
              << " balance=" << funds
              << " bronze=" << static_cast<int>(progress.IsCompleted(Progression::Challenge::Bronze))
              << " silver=" << static_cast<int>(progress.IsCompleted(Progression::Challenge::Silver))
              << " gold=" << static_cast<int>(progress.IsCompleted(Progression::Challenge::Gold)) << '\n';
    return 0;
}
