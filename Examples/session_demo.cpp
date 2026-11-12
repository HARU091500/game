#include "Gameplay/LastStorageSession.h"
#include "LastStorageMvpFixture.h"

#include <charconv>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
    namespace Economy = LastStorage::Economy;
    namespace Gameplay = LastStorage::Gameplay;
    namespace Samples = LastStorage::Samples;
    using Gameplay::WarehouseSession;

    const char* PhaseName(Gameplay::Phase phase)
    {
        switch (phase)
        {
        case Gameplay::Phase::NotStarted: return "未開始";
        case Gameplay::Phase::Auction: return "オークション";
        case Gameplay::Phase::Exploration: return "倉庫内の整理";
        case Gameplay::Phase::Finished: return "終了";
        }
        return "不明";
    }

    const char* PhaseCode(Gameplay::Phase phase)
    {
        switch (phase)
        {
        case Gameplay::Phase::NotStarted: return "not_started";
        case Gameplay::Phase::Auction: return "auction";
        case Gameplay::Phase::Exploration: return "exploration";
        case Gameplay::Phase::Finished: return "finished";
        }
        return "unknown";
    }

    const char* EconomyErrorName(Economy::Error error)
    {
        switch (error)
        {
        case Economy::Error::None: return "なし";
        case Economy::Error::InvalidDefinition: return "品物の定義が不正です";
        case Economy::Error::InvalidInstanceId: return "個体IDが不正です";
        case Economy::Error::DuplicateInstanceId: return "同じ個体は再取得できません";
        case Economy::Error::ItemNotFound: return "その個体を持っていません";
        case Economy::Error::NotAppraised: return "先に鑑定してください";
        case Economy::Error::AlreadySold: return "売却済みです";
        case Economy::Error::InvalidAmount: return "金額が不正です";
        case Economy::Error::BalanceOverflow: return "残高の整数上限を超えます";
        case Economy::Error::InsufficientFunds: return "所持金が足りません";
        }
        return "不明な経済エラー";
    }

    void PrintFailure(const Gameplay::SessionResult& result)
    {
        std::cout << "実行できません: ";
        switch (result.Code)
        {
        case Gameplay::SessionError::None: std::cout << "なし"; break;
        case Gameplay::SessionError::InvalidPhase: std::cout << "現在の段階ではその操作はできません"; break;
        case Gameplay::SessionError::InvalidWarehouse: std::cout << "倉庫データが不正です"; break;
        case Gameplay::SessionError::InvalidAmount: std::cout << "金額が不正です"; break;
        case Gameplay::SessionError::InsufficientFunds: std::cout << "所持金を超える入札はできません"; break;
        case Gameplay::SessionError::BidTooLow: std::cout << "開始価格以上、かつ前の入札より高い額が必要です"; break;
        case Gameplay::SessionError::NoBid: std::cout << "先に入札してください"; break;
        case Gameplay::SessionError::ItemNotFound: std::cout << "その品物は倉庫にありません"; break;
        case Gameplay::SessionError::AlreadyCollected: std::cout << "その個体は収集済みです"; break;
        case Gameplay::SessionError::ItemNotCollected: std::cout << "先に品物を収集してください"; break;
        case Gameplay::SessionError::EconomyFailure: std::cout << EconomyErrorName(result.EconomyCode); break;
        case Gameplay::SessionError::RoundIncomplete: std::cout << "全ての品物を収集・鑑定・売却してから終了してください"; break;
        }
        std::cout << '\n';
    }

    bool Report(const Gameplay::SessionResult& result, std::string_view message)
    {
        if (!result.Succeeded())
        {
            PrintFailure(result);
            return false;
        }
        std::cout << message << '\n';
        return true;
    }

    void ShowStatus(const WarehouseSession& session)
    {
        std::cout << "段階: " << PhaseName(session.CurrentPhase())
                  << " / 所持金: " << session.Economy().Balance()
                  << " / 所持品: " << session.Economy().OwnedItemCount() << '\n';
        if (session.CurrentPhase() == Gameplay::Phase::Auction)
        {
            std::cout << "開始価格: " << session.Warehouse().OpeningBid
                      << " / 現在の入札: " << session.CurrentBid() << '\n';
        }
        if (session.AuctionOutcome() == Gameplay::Outcome::Won)
        {
            std::cout << "落札費用: " << session.PurchaseCost()
                      << " / 売上: " << session.SaleRevenue()
                      << " / 今回の利益: " << session.Profit() << '\n';
        }
    }

    void ShowItems(const WarehouseSession& session)
    {
        if (session.AuctionOutcome() != Gameplay::Outcome::Won)
        {
            std::cout << "落札後に倉庫の中身を確認できます。\n";
            return;
        }
        const auto& items = session.Warehouse().Items;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            std::cout << i + 1 << ": " << item.Definition.DisplayName << " [" << item.InstanceId << "] ";
            const auto* record = session.Economy().FindItem(item.InstanceId);
            if (!record)
            {
                std::cout << "倉庫内";
            }
            else if (record->State == Economy::Ownership::Sold)
            {
                std::cout << "売却済み " << record->AppraisedValue;
            }
            else if (record->Appraisal == Economy::AppraisalState::Appraised)
            {
                std::cout << "所持 / 鑑定額 " << record->AppraisedValue;
            }
            else
            {
                std::cout << "所持 / 未鑑定";
            }
            std::cout << '\n';
        }
    }

    void ShowHelp()
    {
        std::cout << "help                 操作一覧\n"
                  << "status               所持金と進行状況\n"
                  << "list                 落札した倉庫の品物一覧\n"
                  << "bid 300              金額を指定して入札\n"
                  << "win / lose           今回の試遊用に落札・落選を確定\n"
                  << "collect 1            一覧の番号または個体IDを指定して収集\n"
                  << "appraise 1           収集した品物を固定データで鑑定\n"
                  << "sell 1               鑑定済み品を売却\n"
                  << "finish               全品売却後に今回の倉庫を終了\n"
                  << "quit                 試遊を終了（セーブはしません）\n";
    }

    bool ParseMoney(std::string_view text, Economy::Money& value)
    {
        if (text.empty()) { return false; }
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
    }

    std::string ResolveItemId(const WarehouseSession& session, const std::string& target)
    {
        // Indices are only a UI convenience. Identity and ownership are checked by Session.
        std::size_t number = 0;
        const auto parsed = std::from_chars(target.data(), target.data() + target.size(), number);
        if (parsed.ec == std::errc{} && parsed.ptr == target.data() + target.size())
        {
            if (number == 0 || number > session.Warehouse().Items.size()) { return {}; }
            return session.Warehouse().Items[number - 1].InstanceId;
        }
        return target;
    }

    void PrintSummary(const WarehouseSession& session)
    {
        ShowStatus(session);
        if (session.AuctionOutcome() == Gameplay::Outcome::Lost)
        {
            std::cout << "今回は落選しました。支払いはありません。\n";
        }
        std::cout << "RESULT balance=" << session.Economy().Balance()
                  << " revenue=" << session.SaleRevenue()
                  << " cost=" << session.PurchaseCost()
                  << " profit=" << session.Profit()
                  << " phase=" << PhaseCode(session.CurrentPhase()) << '\n';
    }

    bool ExecuteCommand(WarehouseSession& session, const std::string& line, bool& quit)
    {
        std::istringstream input(line);
        std::string command;
        std::string target;
        std::string extra;
        if (!(input >> command)) { return true; }
        input >> target;
        if (input >> extra)
        {
            std::cout << "引数が多すぎます。help で操作を確認してください。\n";
            return false;
        }

        const bool itemCommand = command == "collect" || command == "appraise" || command == "sell";
        const bool hasArgument = command == "bid" || itemCommand;
        if (hasArgument == target.empty())
        {
            std::cout << (hasArgument ? "金額または品物の番号・IDを指定してください。\n" : "この操作に引数は不要です。\n");
            return false;
        }

        if (command == "help") { ShowHelp(); return true; }
        if (command == "status") { ShowStatus(session); return true; }
        if (command == "list") { ShowItems(session); return true; }
        if (command == "quit") { quit = true; return true; }
        if (command == "bid")
        {
            Economy::Money amount = 0;
            if (!ParseMoney(target, amount))
            {
                std::cout << "金額は64ビット整数で入力してください。\n";
                return false;
            }
            const bool success = Report(session.Bid(amount), "入札を受け付けました。まだ支払いは発生しません。");
            if (success) { ShowStatus(session); }
            return success;
        }
        if (command == "win")
        {
            const bool success = Report(session.ResolveAuction(true), "落札しました。倉庫の中身を確認できます。");
            if (success) { ShowStatus(session); ShowItems(session); }
            return success;
        }
        if (command == "lose")
        {
            return Report(session.ResolveAuction(false), "落選を確定しました。所持金は変わりません。");
        }
        if (command == "finish")
        {
            return Report(session.FinishRound(), "倉庫内の全品を売却し、今回の整理を終えました。");
        }
        if (itemCommand)
        {
            const auto id = ResolveItemId(session, target);
            if (id.empty())
            {
                std::cout << "品物の番号は1から" << session.Warehouse().Items.size() << "です。\n";
                return false;
            }
            if (command == "collect") { return Report(session.Collect(id), "収集しました。"); }
            if (command == "appraise")
            {
                if (!Report(session.Appraise(id), "鑑定しました。")) { return false; }
                const auto* item = session.Economy().FindItem(id);
                if (item) { std::cout << "鑑定額: " << item->AppraisedValue << '\n'; }
                return true;
            }
            const auto result = session.Sell(id);
            if (!Report(result, "売却しました。")) { return false; }
            std::cout << "売上: " << result.Amount << " / 所持金: " << session.Economy().Balance() << '\n';
            return true;
        }
        std::cout << "不明な操作です。help で操作を確認してください。\n";
        return false;
    }

    bool RunDemo(WarehouseSession& session)
    {
        std::cout << "固定データによる自動試遊を開始します。\n";
        if (!Report(session.Bid(Samples::DemoBid), "入札しました。")) { return false; }
        if (!Report(session.ResolveAuction(true), "落札しました。")) { return false; }
        ShowStatus(session);
        for (const auto& item : session.Warehouse().Items)
        {
            std::cout << "\n" << item.Definition.DisplayName << "\n";
            if (!Report(session.Collect(item.InstanceId), "  収集しました。")) { return false; }
            if (!Report(session.Appraise(item.InstanceId), "  鑑定しました。")) { return false; }
            const auto result = session.Sell(item.InstanceId);
            if (!Report(result, "  売却しました。")) { return false; }
            std::cout << "  売上: " << result.Amount << '\n';
        }
        return Report(session.FinishRound(), "\n10種類の全品を売却し、1倉庫の進行が完了しました。");
    }
}

int main(int argc, char** argv)
{
    const bool demo = argc == 2 && std::string_view(argv[1]) == "--demo";
    if (argc > 2 || (argc == 2 && !demo))
    {
        std::cerr << "使い方: session_demo [--demo]\n";
        return 2;
    }
    std::cout << "LAST STORAGE テキスト試遊 / UE・3D版ではありません\n"
              << "固定価格・手動の落札確定を使う開発用サンプルです。\n";
    WarehouseSession session;
    if (!Report(session.Start(Samples::MakeMvpWarehouse(), Samples::StartingFunds), "倉庫1室の試遊を開始しました。"))
    {
        return 1;
    }
    ShowStatus(session);
    if (demo)
    {
        if (!RunDemo(session)) { return 1; }
        PrintSummary(session);
        return 0;
    }

    ShowHelp();
    bool quit = false;
    std::string line;
    while (!quit && session.CurrentPhase() != Gameplay::Phase::Finished)
    {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) { break; }
        ExecuteCommand(session, line, quit);
    }
    if (session.CurrentPhase() == Gameplay::Phase::Finished)
    {
        PrintSummary(session);
    }
    else
    {
        ShowStatus(session);
        std::cout << "試遊は未完了で終了しました。進行は保存されません。\n";
    }
    return 0;
}
