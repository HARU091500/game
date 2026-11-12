#include "Gameplay/LastStorageSession.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace LastStorage::Gameplay;
namespace Eco = LastStorage::Economy;

namespace
{
    #define CHECK(condition) \
        do { if (!(condition)) { std::cerr << "  line " << __LINE__ << ": " #condition "\n"; return false; } } while (false)

    WarehouseDefinition Fixture()
    {
        return {"warehouse-01", 100, {
            {{"radio", "Radio", "A portable radio.", 250}, "room01-radio-001", 250},
            {{"camera", "Camera", "A film camera.", 200}, "room01-camera-001", 175}
        }};
    }

    std::string Snapshot(const WarehouseSession& Session)
    {
        std::ostringstream Out;
        Out << static_cast<int>(Session.CurrentPhase()) << ':' << static_cast<int>(Session.AuctionOutcome())
            << ':' << Session.CurrentBid() << ':' << Session.OpeningBalance() << ':' << Session.PurchaseCost()
            << ':' << Session.SaleRevenue() << ':' << Session.Profit() << ':' << Session.Economy().Balance()
            << ':' << Session.Economy().OwnedItemCount() << ':' << Session.Warehouse().WarehouseId
            << ':' << Session.Warehouse().OpeningBid;
        for (const auto& Item : Session.Warehouse().Items)
        {
            Out << '|' << Item.InstanceId << ':' << Item.Definition.TypeId << ':' << Item.Definition.DisplayName
                << ':' << Item.Definition.Description << ':' << Item.Definition.ReferenceValue << ':' << Item.AppraisalValue
                << ':' << Session.IsCollected(Item.InstanceId);
        }
        for (const auto& Item : Session.Economy().Records())
        {
            Out << '|' << Item.InstanceId << ':' << Item.TypeId << ':' << static_cast<int>(Item.Appraisal)
                << ':' << Item.AppraisedValue << ':' << static_cast<int>(Item.State);
        }
        return Out.str();
    }

    template <typename Operation>
    bool RejectsWithoutChanges(WarehouseSession& Session, Operation Call, SessionError Error,
        Eco::Error EconomyCode = Eco::Error::None)
    {
        const std::string Before = Snapshot(Session);
        const auto Result = Call();
        return !Result.Succeeded() && Result.Code == Error && Result.EconomyCode == EconomyCode
            && Result.Amount == 0 && Before == Snapshot(Session);
    }

    bool StartExploration(WarehouseSession& Session, const WarehouseDefinition& Definition = Fixture(),
        Money StartingBalance = 1000, Money Bid = 300)
    {
        return Session.Start(Definition, StartingBalance).Succeeded() && Session.Bid(Bid).Succeeded()
            && Session.ResolveAuction(true).Succeeded();
    }

    bool SellAll(WarehouseSession& Session)
    {
        for (const auto& Item : Session.Warehouse().Items)
        {
            if (!Session.Collect(Item.InstanceId).Succeeded() || !Session.Appraise(Item.InstanceId).Succeeded()
                || !Session.Sell(Item.InstanceId).Succeeded()) { return false; }
        }
        return true;
    }

    bool SpendRejectsInvalidAmounts()
    {
        Eco::PlayerEconomy Economy;
        CHECK(Economy.Spend(0).Succeeded());
        CHECK(Economy.Credit(1000).Succeeded());
        CHECK(Economy.Spend(-1).Code == Eco::Error::InvalidAmount);
        CHECK(Economy.Spend((std::numeric_limits<Money>::min)()).Code == Eco::Error::InvalidAmount);
        CHECK(Economy.Spend(1001).Code == Eco::Error::InsufficientFunds);
        CHECK(Economy.Balance() == 1000);
        const auto Paid = Economy.Spend(1000);
        CHECK(Paid.Succeeded() && Paid.Amount == 1000 && Economy.Balance() == 0);
        CHECK(Economy.Spend(1).Code == Eco::Error::InsufficientFunds);
        return true;
    }

    bool SpendMaximumBalance()
    {
        Eco::PlayerEconomy Economy;
        const Money Maximum = (std::numeric_limits<Money>::max)();
        CHECK(Economy.Credit(Maximum).Succeeded());
        CHECK(Economy.Spend(Maximum).Succeeded());
        CHECK(Economy.Balance() == 0);
        return true;
    }

    bool NoOperationsBeforeStart()
    {
        WarehouseSession Session;
        CHECK(Session.CurrentPhase() == Phase::NotStarted);
        CHECK(Session.AuctionOutcome() == Outcome::Pending);
        CHECK(Session.Economy().Balance() == 0);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(100); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(true); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Appraise("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::InvalidPhase));
        return true;
    }

    bool InvalidStartPreservesEmptySession()
    {
        WarehouseSession Session;
        auto Definition = Fixture();
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, -1); }, SessionError::InvalidAmount));
        Definition.WarehouseId = "Bad Id";
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); }, SessionError::InvalidWarehouse));
        Definition = Fixture();
        Definition.OpeningBid = 0;
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); }, SessionError::InvalidWarehouse));
        Definition = Fixture();
        Definition.Items.clear();
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); }, SessionError::InvalidWarehouse));
        Definition = Fixture();
        Definition.Items[0].AppraisalValue = -1;
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); }, SessionError::InvalidWarehouse));
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        return true;
    }

    bool InvalidItemDataRejectedBeforeStart()
    {
        WarehouseSession Session;
        auto Definition = Fixture();
        Definition.Items[1].InstanceId = Definition.Items[0].InstanceId;
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); },
            SessionError::InvalidWarehouse, Eco::Error::DuplicateInstanceId));
        Definition = Fixture();
        Definition.Items[0].Definition.ReferenceValue = -1;
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); },
            SessionError::InvalidWarehouse, Eco::Error::InvalidDefinition));
        Definition = Fixture();
        Definition.Items[0].InstanceId = "NONE";
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); },
            SessionError::InvalidWarehouse, Eco::Error::InvalidInstanceId));
        Definition = Fixture();
        Definition.Items[0].Definition.DisplayName.clear();
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Definition, 1000); },
            SessionError::InvalidWarehouse, Eco::Error::InvalidDefinition));
        return true;
    }

    bool StartCopiesDataAndCannotRepeat()
    {
        WarehouseSession Session;
        auto Definition = Fixture();
        CHECK(Session.Start(Definition, 1000).Succeeded());
        Definition.Items[0].InstanceId = "changed";
        Definition.Items[0].AppraisalValue = 9999;
        CHECK(Session.Warehouse().Items[0].InstanceId == "room01-radio-001");
        CHECK(Session.Warehouse().Items[0].AppraisalValue == 250);
        CHECK(Session.OpeningBalance() == 1000);
        CHECK(Session.Economy().Records().empty());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Start(Fixture(), 5000); }, SessionError::InvalidPhase));
        return true;
    }

    bool BidsValidatedWithoutDebit()
    {
        WarehouseSession Session;
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(-1); }, SessionError::InvalidAmount));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(0); }, SessionError::InvalidAmount));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(99); }, SessionError::BidTooLow));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(1001); }, SessionError::InsufficientFunds));
        CHECK(Session.Bid(100).Succeeded());
        CHECK(Session.CurrentBid() == 100 && Session.Economy().Balance() == 1000);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(100); }, SessionError::BidTooLow));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(99); }, SessionError::BidTooLow));
        CHECK(Session.Bid(101).Succeeded());
        CHECK(Session.CurrentBid() == 101 && Session.Economy().Balance() == 1000);
        return true;
    }

    bool NoBidCannotResolve()
    {
        WarehouseSession Session;
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(true); }, SessionError::NoBid));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(false); }, SessionError::NoBid));
        return true;
    }

    bool WinChargesExactlyOnce()
    {
        WarehouseSession Session;
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        CHECK(Session.Bid(300).Succeeded());
        const auto Won = Session.ResolveAuction(true);
        CHECK(Won.Succeeded() && Won.Amount == 300);
        CHECK(Session.CurrentPhase() == Phase::Exploration && Session.AuctionOutcome() == Outcome::Won);
        CHECK(Session.Economy().Balance() == 700 && Session.PurchaseCost() == 300);
        CHECK(Session.Profit() == -300 && Session.SaleRevenue() == 0);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(true); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(false); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Bid(400); }, SessionError::InvalidPhase));
        return true;
    }

    bool LossDoesNotChargeOrPermitCollection()
    {
        WarehouseSession Session;
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        CHECK(Session.Bid(300).Succeeded());
        CHECK(Session.ResolveAuction(false).Succeeded());
        CHECK(Session.CurrentPhase() == Phase::Finished && Session.AuctionOutcome() == Outcome::Lost);
        CHECK(Session.Economy().Balance() == 1000 && Session.PurchaseCost() == 0 && Session.Profit() == 0);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.ResolveAuction(true); }, SessionError::InvalidPhase));
        return true;
    }

    bool AuctionCannotCollectAppraiseOrSell()
    {
        WarehouseSession Session;
        CHECK(Session.Start(Fixture(), 1000).Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Appraise("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::InvalidPhase));
        return true;
    }

    bool CollectTransfersToOwnedInventory()
    {
        WarehouseSession Session;
        CHECK(StartExploration(Session));
        CHECK(!Session.IsCollected("room01-radio-001"));
        CHECK(Session.Collect("room01-radio-001").Succeeded());
        CHECK(Session.IsCollected("room01-radio-001"));
        CHECK(Session.Economy().OwnedItemCount() == 1 && Session.Economy().Balance() == 700);
        CHECK(Session.Economy().FindItem("room01-radio-001")->Appraisal == Eco::AppraisalState::Unappraised);
        CHECK(!Session.IsCollected("room01-camera-001"));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("room01-radio-001"); }, SessionError::AlreadyCollected));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("unknown"); }, SessionError::ItemNotFound));
        return true;
    }

    bool AppraisalRequiresCollectionAndUsesDataPrice()
    {
        WarehouseSession Session;
        CHECK(StartExploration(Session));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Appraise("room01-camera-001"); }, SessionError::ItemNotCollected));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Appraise("unknown"); }, SessionError::ItemNotFound));
        CHECK(Session.Collect("room01-camera-001").Succeeded());
        CHECK(Session.Appraise("room01-camera-001").Succeeded());
        CHECK(Session.Economy().FindItem("room01-camera-001")->AppraisedValue == 175);
        CHECK(Session.Economy().Balance() == 700);
        CHECK(Session.Appraise("room01-camera-001").Succeeded());
        return true;
    }

    bool SaleRequiresAppraisalAndCannotRepeat()
    {
        WarehouseSession Session;
        CHECK(StartExploration(Session));
        CHECK(Session.Collect("room01-radio-001").Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); },
            SessionError::EconomyFailure, Eco::Error::NotAppraised));
        CHECK(Session.Appraise("room01-radio-001").Succeeded());
        const auto Sold = Session.Sell("room01-radio-001");
        CHECK(Sold.Succeeded() && Sold.Amount == 250);
        CHECK(Session.Economy().Balance() == 950 && Session.SaleRevenue() == 250 && Session.Profit() == -50);
        CHECK(Session.Economy().OwnedItemCount() == 0);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); },
            SessionError::EconomyFailure, Eco::Error::AlreadySold));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Appraise("room01-radio-001"); },
            SessionError::EconomyFailure, Eco::Error::AlreadySold));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Collect("room01-radio-001"); }, SessionError::AlreadyCollected));
        return true;
    }

    bool FinishRequiresAllCollectedAndSold()
    {
        WarehouseSession Session;
        CHECK(StartExploration(Session));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::RoundIncomplete));
        CHECK(Session.Collect("room01-radio-001").Succeeded());
        CHECK(Session.Collect("room01-camera-001").Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::RoundIncomplete));
        CHECK(Session.Appraise("room01-radio-001").Succeeded());
        CHECK(Session.Sell("room01-radio-001").Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::RoundIncomplete));
        CHECK(Session.Appraise("room01-camera-001").Succeeded());
        CHECK(Session.Sell("room01-camera-001").Succeeded());
        CHECK(Session.CurrentPhase() == Phase::Exploration);
        CHECK(Session.FinishRound().Succeeded());
        CHECK(Session.CurrentPhase() == Phase::Finished);
        CHECK(Session.SaleRevenue() == 425 && Session.PurchaseCost() == 300 && Session.Profit() == 125);
        CHECK(Session.Economy().Balance() == 1125);
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.FinishRound(); }, SessionError::InvalidPhase));
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); }, SessionError::InvalidPhase));
        return true;
    }

    bool FullTenItemLoop()
    {
        WarehouseDefinition Definition{"warehouse-01", 100, {}};
        for (int Index = 0; Index < 10; ++Index)
        {
            const std::string Suffix = std::to_string(Index);
            Definition.Items.push_back({{"item-" + Suffix, "Item " + Suffix, "Prototype.", 100}, "instance-" + Suffix, 175});
        }
        WarehouseSession Session;
        CHECK(StartExploration(Session, Definition));
        CHECK(SellAll(Session));
        CHECK(Session.FinishRound().Succeeded());
        CHECK(Session.Economy().Balance() == 2450 && Session.Profit() == 1450 && Session.SaleRevenue() == 1750);
        CHECK(Session.Economy().Records().size() == 10 && Session.Economy().OwnedItemCount() == 0);
        return true;
    }

    bool ZeroValueItemCanComplete()
    {
        auto Definition = Fixture();
        for (auto& Item : Definition.Items) { Item.AppraisalValue = 0; }
        WarehouseSession Session;
        CHECK(StartExploration(Session, Definition));
        CHECK(SellAll(Session));
        CHECK(Session.FinishRound().Succeeded());
        CHECK(Session.SaleRevenue() == 0 && Session.Profit() == -300 && Session.Economy().Balance() == 700);
        return true;
    }

    bool BidAndCostCanReachMaximum()
    {
        auto Definition = Fixture();
        Definition.Items.resize(1);
        Definition.Items[0].AppraisalValue = (std::numeric_limits<Money>::max)();
        WarehouseSession Session;
        const Money Maximum = (std::numeric_limits<Money>::max)();
        CHECK(StartExploration(Session, Definition, Maximum, Maximum));
        CHECK(Session.Economy().Balance() == 0 && Session.Profit() == -Maximum);
        CHECK(SellAll(Session));
        CHECK(Session.SaleRevenue() == Maximum && Session.PurchaseCost() == Maximum && Session.Profit() == 0);
        CHECK(Session.FinishRound().Succeeded());
        CHECK(Session.Economy().Balance() == Maximum);
        return true;
    }

    bool SaleOverflowPreservesWholeSession()
    {
        auto Definition = Fixture();
        Definition.Items.resize(1);
        Definition.Items[0].AppraisalValue = 101;
        WarehouseSession Session;
        CHECK(StartExploration(Session, Definition, (std::numeric_limits<Money>::max)(), 100));
        CHECK(Session.Collect("room01-radio-001").Succeeded());
        CHECK(Session.Appraise("room01-radio-001").Succeeded());
        CHECK(RejectsWithoutChanges(Session, [&] { return Session.Sell("room01-radio-001"); },
            SessionError::EconomyFailure, Eco::Error::BalanceOverflow));
        CHECK(Session.Economy().OwnedItemCount() == 1 && Session.SaleRevenue() == 0);
        return true;
    }
}

int main()
{
    using Test = std::pair<const char*, bool (*)()>;
    const std::vector<Test> Tests = {
        {"spend_rejects_invalid_amounts", SpendRejectsInvalidAmounts},
        {"spend_maximum_balance", SpendMaximumBalance},
        {"no_operations_before_start", NoOperationsBeforeStart},
        {"invalid_start_preserves_empty_session", InvalidStartPreservesEmptySession},
        {"invalid_item_data_rejected_before_start", InvalidItemDataRejectedBeforeStart},
        {"start_copies_data_and_cannot_repeat", StartCopiesDataAndCannotRepeat},
        {"bids_validated_without_debit", BidsValidatedWithoutDebit},
        {"no_bid_cannot_resolve", NoBidCannotResolve},
        {"win_charges_exactly_once", WinChargesExactlyOnce},
        {"loss_does_not_charge_or_permit_collection", LossDoesNotChargeOrPermitCollection},
        {"auction_cannot_collect_appraise_or_sell", AuctionCannotCollectAppraiseOrSell},
        {"collect_transfers_to_owned_inventory", CollectTransfersToOwnedInventory},
        {"appraisal_requires_collection_and_uses_data_price", AppraisalRequiresCollectionAndUsesDataPrice},
        {"sale_requires_appraisal_and_cannot_repeat", SaleRequiresAppraisalAndCannotRepeat},
        {"finish_requires_all_collected_and_sold", FinishRequiresAllCollectedAndSold},
        {"full_ten_item_loop", FullTenItemLoop},
        {"zero_value_item_can_complete", ZeroValueItemCanComplete},
        {"bid_and_cost_can_reach_maximum", BidAndCostCanReachMaximum},
        {"sale_overflow_preserves_whole_session", SaleOverflowPreservesWholeSession},
    };
    std::size_t Passed = 0;
    for (const auto& Test : Tests)
    {
        const bool Success = Test.second();
        std::cout << (Success ? "PASS " : "FAIL ") << Test.first << '\n';
        if (Success) { ++Passed; }
    }
    std::cout << Passed << '/' << Tests.size() << " session tests passed\n";
    return Passed == Tests.size() ? 0 : 1;
}
