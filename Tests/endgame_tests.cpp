#include "Progression/LastStorageEndgame.h"

#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace P = LastStorage::Progression;
namespace G = LastStorage::Gameplay;
namespace E = LastStorage::Economy;
using P::Challenge;
using P::EndgameProgress;
using P::Money;
using P::ProgressError;
using P::Rank;

namespace
{
    #define CHECK(condition) \
        do { if (!(condition)) { std::cerr << "  line " << __LINE__ << ": " #condition "\n"; return false; } } while (false)

    G::WarehouseDefinition Single(Money Price = 1750, const std::string& Type = "radio")
    {
        return {"warehouse-01", 1, {{{Type, "Item", "A test item.", Price}, "item-001", Price}}};
    }

    bool Clear(G::WarehouseSession& Session, const G::WarehouseDefinition& Definition = Single(),
        Money Funds = 1000, Money Bid = 300)
    {
        if (!Session.Start(Definition, Funds).Succeeded() || !Session.Bid(Bid).Succeeded()
            || !Session.ResolveAuction(true).Succeeded()) { return false; }
        for (const auto& Item : Session.Warehouse().Items)
        {
            if (!Session.Collect(Item.InstanceId).Succeeded() || !Session.Appraise(Item.InstanceId).Succeeded()
                || !Session.Sell(Item.InstanceId).Succeeded()) { return false; }
        }
        return Session.FinishRound().Succeeded();
    }

    std::string Snapshot(const EndgameProgress& Progress)
    {
        std::ostringstream Out;
        Out << Progress.IsConfigured() << ':' << Progress.CollectionCount() << ':' << Progress.CollectionTarget()
            << ':' << Progress.ClearCount() << ':' << Progress.ProfitableClearCount() << ':' << Progress.TotalProfit()
            << ':' << Progress.BestProfit() << ':' << static_cast<int>(Progress.CurrentRank());
        for (const auto Value : {Challenge::None, Challenge::Bronze, Challenge::Silver, Challenge::Gold})
        {
            Out << ':' << Progress.IsUnlocked(Value) << ':' << Progress.IsCompleted(Value);
        }
        for (const auto* Type : {"radio", "camera", "clock", "toolbox", "record", "vase", "console", "watch", "painting", "chest"})
        {
            Out << ':' << Progress.HasDiscovered(Type);
        }
        return Out.str();
    }

    template <typename Operation>
    bool RejectedWithoutChanges(EndgameProgress& Progress, Operation Call, ProgressError Expected)
    {
        const auto Before = Snapshot(Progress);
        const auto Result = Call();
        return !Result.Succeeded() && Result.Code == Expected && !Result.ChallengePassed && !Result.NewlyCompleted
            && Before == Snapshot(Progress);
    }

    bool Record(EndgameProgress& Progress, const std::string& Run, Challenge Contract, Money Bid = 300)
    {
        G::WarehouseSession Session;
        return Clear(Session, Single(), 10000, Bid) && Progress.RecordClear(Run, Session, Contract).Succeeded();
    }

    bool UnlockSilver(EndgameProgress& Progress)
    {
        return Progress.Configure({"radio"}).Succeeded()
            && Record(Progress, "normal-001", Challenge::None)
            && Record(Progress, "bronze-001", Challenge::Bronze);
    }

    bool DefaultsAreLocked()
    {
        EndgameProgress Progress;
        CHECK(!Progress.IsConfigured());
        CHECK(Progress.CollectionCount() == 0 && Progress.CollectionTarget() == 0);
        CHECK(Progress.ClearCount() == 0 && Progress.ProfitableClearCount() == 0);
        CHECK(Progress.TotalProfit() == 0 && Progress.BestProfit() == 0);
        CHECK(Progress.CurrentRank() == Rank::Rookie);
        CHECK(!Progress.IsUnlocked(Challenge::None) && !Progress.IsUnlocked(Challenge::Bronze));
        return true;
    }

    bool InvalidCatalogsPreserveState()
    {
        EndgameProgress Progress;
        for (const auto& Catalog : std::vector<std::vector<std::string>>{{}, {"radio", "radio"}, {"Radio"}, {"none"}, {"bad id"}})
        {
            CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.Configure(Catalog); }, ProgressError::InvalidCatalog));
        }
        CHECK(Progress.Configure({"radio", "camera"}).Succeeded());
        CHECK(Progress.IsUnlocked(Challenge::None));
        CHECK(Progress.CollectionTarget() == 2);
        return true;
    }

    bool ConfigurationIsCopiedAndCannotRepeat()
    {
        EndgameProgress Progress;
        std::vector<std::string> Catalog{"radio"};
        CHECK(Progress.Configure(Catalog).Succeeded());
        Catalog[0] = "camera";
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.Configure(Catalog); }, ProgressError::AlreadyConfigured));
        CHECK(Record(Progress, "run-001", Challenge::None));
        CHECK(Progress.HasDiscovered("radio") && !Progress.HasDiscovered("camera"));
        return true;
    }

    bool UnconfiguredRegistrationRejected()
    {
        EndgameProgress Progress;
        G::WarehouseSession Session;
        CHECK(Clear(Session));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session); }, ProgressError::NotConfigured));
        return true;
    }

    bool InvalidRunIdentifiersRejected()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Session;
        CHECK(Clear(Session));
        for (const auto* Id : {"", "none", "Run-001", "run/001", "run 001"})
        {
            CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear(Id, Session); }, ProgressError::InvalidRunId));
        }
        CHECK(Progress.RecordClear("run-001", Session).Succeeded());
        return true;
    }

    bool IncompleteAndLostRoundsRejected()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Session;
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session); }, ProgressError::RoundNotCleared));
        CHECK(Session.Start(Single(), 1000).Succeeded());
        CHECK(Session.Bid(300).Succeeded());
        CHECK(Session.ResolveAuction(false).Succeeded());
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session); }, ProgressError::RoundNotCleared));
        G::WarehouseSession Cleared;
        CHECK(Clear(Cleared));
        CHECK(Progress.RecordClear("run-001", Cleared).Succeeded());
        return true;
    }

    bool AllItemsSoldStillRequiresFinish()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Session;
        CHECK(Session.Start(Single(), 1000).Succeeded());
        CHECK(Session.Bid(300).Succeeded() && Session.ResolveAuction(true).Succeeded());
        CHECK(Session.Collect("item-001").Succeeded());
        CHECK(Session.Appraise("item-001").Succeeded() && Session.Sell("item-001").Succeeded());
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session); }, ProgressError::RoundNotCleared));
        CHECK(Session.FinishRound().Succeeded());
        CHECK(Progress.RecordClear("run-001", Session).Succeeded());
        return true;
    }

    bool DuplicateRunDoesNotCountAgain()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Session;
        CHECK(Clear(Session));
        const auto First = Progress.RecordClear("run-001", Session);
        CHECK(First.Succeeded() && !First.ChallengePassed && !First.NewlyCompleted);
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session); }, ProgressError::DuplicateRun));
        CHECK(Progress.ClearCount() == 1 && Progress.TotalProfit() == 1450);
        return true;
    }

    bool UnknownTypesCannotPartiallyRegister()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Unknown;
        auto Definition = Single();
        Definition.Items.push_back({{"camera", "Camera", "A camera.", 50}, "item-002", 50});
        CHECK(Clear(Unknown, Definition));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Unknown); }, ProgressError::UnknownType));
        CHECK(!Progress.HasDiscovered("radio"));
        G::WarehouseSession Valid;
        CHECK(Clear(Valid));
        CHECK(Progress.RecordClear("run-001", Valid).Succeeded());
        return true;
    }

    bool MultipleIndividualsCountOneType()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        auto Definition = Single(800);
        Definition.Items.push_back({{"radio", "Radio", "Another radio.", 900}, "item-002", 900});
        G::WarehouseSession Session;
        CHECK(Clear(Session, Definition));
        CHECK(Progress.RecordClear("run-001", Session).Succeeded());
        CHECK(Progress.CollectionCount() == 1 && Progress.CollectionTarget() == 1);
        CHECK(Record(Progress, "run-002", Challenge::None));
        CHECK(Progress.CollectionCount() == 1);
        return true;
    }

    bool CollectionCanFillAcrossDifferentRounds()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio", "camera"}).Succeeded());
        CHECK(Record(Progress, "run-001", Challenge::None));
        CHECK(Progress.CollectionCount() == 1 && Progress.CurrentRank() == Rank::Rookie);
        CHECK(!Progress.IsUnlocked(Challenge::Bronze));
        G::WarehouseSession Camera;
        CHECK(Clear(Camera, Single(1750, "camera")));
        CHECK(Progress.RecordClear("run-002", Camera).Succeeded());
        CHECK(Progress.CollectionCount() == 2 && Progress.CurrentRank() == Rank::Collector);
        CHECK(Progress.IsUnlocked(Challenge::Bronze));
        return true;
    }

    bool NegativeAndZeroProfitsAreNotDiscarded()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Loss;
        CHECK(Clear(Loss, Single(250), 1000, 300));
        CHECK(Progress.RecordClear("loss-001", Loss).Succeeded());
        CHECK(Progress.TotalProfit() == -50 && Progress.BestProfit() == -50);
        CHECK(Progress.ProfitableClearCount() == 0 && !Progress.IsUnlocked(Challenge::Bronze));
        G::WarehouseSession Even;
        CHECK(Clear(Even, Single(300), 1000, 300));
        CHECK(Progress.RecordClear("even-001", Even).Succeeded());
        CHECK(Progress.TotalProfit() == -50 && Progress.BestProfit() == 0);
        CHECK(Progress.ProfitableClearCount() == 0);
        CHECK(Record(Progress, "positive-001", Challenge::None));
        CHECK(Progress.TotalProfit() == 1400 && Progress.BestProfit() == 1450);
        CHECK(Progress.ProfitableClearCount() == 1 && Progress.IsUnlocked(Challenge::Bronze));
        return true;
    }

    bool CannotUnlockAndCompleteInSameRegistration()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        G::WarehouseSession Session;
        CHECK(Clear(Session));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session, Challenge::Bronze); }, ProgressError::ChallengeLocked));
        CHECK(Progress.RecordClear("run-001", Session).Succeeded());
        CHECK(Progress.IsUnlocked(Challenge::Bronze) && !Progress.IsCompleted(Challenge::Bronze));
        CHECK(!Progress.IsUnlocked(Challenge::Silver) && !Progress.IsUnlocked(Challenge::Gold));
        return true;
    }

    bool LockedLaterContractRejected()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        CHECK(Record(Progress, "normal-001", Challenge::None));
        G::WarehouseSession Session;
        CHECK(Clear(Session, Single(), 10000, 1000));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("silver-001", Session, Challenge::Silver); }, ProgressError::ChallengeLocked));
        return true;
    }

    bool MissedContractStillCountsNormalClear()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        CHECK(Record(Progress, "normal-001", Challenge::None));
        G::WarehouseSession Session;
        CHECK(Clear(Session, Single(), 10000, 1000));
        const auto Result = Progress.RecordClear("bronze-miss", Session, Challenge::Bronze);
        CHECK(Result.Succeeded() && !Result.ChallengePassed && !Result.NewlyCompleted);
        CHECK(Progress.ClearCount() == 2 && Progress.TotalProfit() == 2200);
        CHECK(!Progress.IsCompleted(Challenge::Bronze) && !Progress.IsUnlocked(Challenge::Silver));
        return true;
    }

    bool ContractChecksActualPurchaseCost()
    {
        EndgameProgress Progress;
        CHECK(UnlockSilver(Progress));
        G::WarehouseSession Cheap;
        CHECK(Clear(Cheap, Single(), 10000, 300));
        const auto Result = Progress.RecordClear("silver-cheap", Cheap, Challenge::Silver);
        CHECK(Result.Succeeded() && !Result.ChallengePassed && !Result.NewlyCompleted);
        CHECK(!Progress.IsCompleted(Challenge::Silver) && !Progress.IsUnlocked(Challenge::Gold));
        return true;
    }

    bool ContractChecksActualProfit()
    {
        EndgameProgress Progress;
        CHECK(UnlockSilver(Progress));
        CHECK(Record(Progress, "silver-001", Challenge::Silver, 1000));
        G::WarehouseSession LowProfit;
        CHECK(Clear(LowProfit, Single(), 10000, 1600));
        const auto Result = Progress.RecordClear("gold-low", LowProfit, Challenge::Gold);
        CHECK(Result.Succeeded() && !Result.ChallengePassed && !Result.NewlyCompleted);
        CHECK(!Progress.IsCompleted(Challenge::Gold));
        return true;
    }

    bool RepeatingCompletedContractDoesNotReawardTitle()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        CHECK(Record(Progress, "normal-001", Challenge::None));
        G::WarehouseSession First;
        CHECK(Clear(First));
        const auto A = Progress.RecordClear("bronze-001", First, Challenge::Bronze);
        CHECK(A.Succeeded() && A.ChallengePassed && A.NewlyCompleted);
        G::WarehouseSession Repeat;
        CHECK(Clear(Repeat));
        const Money Balance = Repeat.Economy().Balance();
        const auto B = Progress.RecordClear("bronze-002", Repeat, Challenge::Bronze);
        CHECK(B.Succeeded() && B.ChallengePassed && !B.NewlyCompleted);
        CHECK(Progress.ClearCount() == 3 && Repeat.Economy().Balance() == Balance);
        return true;
    }

    bool InvalidChallengeValuesRejected()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        const auto Invalid = static_cast<Challenge>(99);
        CHECK(!Progress.IsUnlocked(Invalid) && !Progress.IsCompleted(Invalid));
        G::WarehouseSession Session;
        CHECK(Clear(Session));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("run-001", Session, Invalid); }, ProgressError::InvalidChallenge));
        CHECK(Progress.RecordClear("run-001", Session).Succeeded());
        return true;
    }

    bool PositiveProfitOverflowPreservesState()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        const Money Maximum = (std::numeric_limits<Money>::max)();
        G::WarehouseSession Huge;
        CHECK(Clear(Huge, Single(Maximum), 1, 1));
        CHECK(Progress.RecordClear("huge-001", Huge).Succeeded());
        CHECK(Progress.TotalProfit() == Maximum - 1);
        G::WarehouseSession Two;
        CHECK(Clear(Two, Single(3), 1, 1));
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("small-001", Two); }, ProgressError::Overflow));
        G::WarehouseSession One;
        CHECK(Clear(One, Single(2), 1, 1));
        CHECK(Progress.RecordClear("small-001", One).Succeeded());
        CHECK(Progress.TotalProfit() == Maximum);
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("small-002", One); }, ProgressError::Overflow));
        return true;
    }

    bool NegativeProfitOverflowPreservesState()
    {
        EndgameProgress Progress;
        CHECK(Progress.Configure({"radio"}).Succeeded());
        const Money Maximum = (std::numeric_limits<Money>::max)();
        const Money Minimum = (std::numeric_limits<Money>::min)();
        G::WarehouseSession HugeLoss;
        CHECK(Clear(HugeLoss, Single(0), Maximum, Maximum));
        CHECK(Progress.RecordClear("huge-loss", HugeLoss).Succeeded());
        CHECK(Progress.TotalProfit() == -Maximum && Progress.BestProfit() == -Maximum);
        G::WarehouseSession OneLoss;
        CHECK(Clear(OneLoss, Single(0), 1, 1));
        CHECK(Progress.RecordClear("loss-001", OneLoss).Succeeded());
        CHECK(Progress.TotalProfit() == Minimum && Progress.BestProfit() == -1);
        CHECK(RejectedWithoutChanges(Progress, [&] { return Progress.RecordClear("retry-001", OneLoss); }, ProgressError::Overflow));
        G::WarehouseSession OneProfit;
        CHECK(Clear(OneProfit, Single(2), 1, 1));
        CHECK(Progress.RecordClear("retry-001", OneProfit).Succeeded());
        CHECK(Progress.TotalProfit() == Minimum + 1);
        return true;
    }

    bool FourRoundTenTypeSampleCarriesWallet()
    {
        const std::vector<std::string> Types{"radio", "camera", "clock", "toolbox", "record", "vase", "console", "watch", "painting", "chest"};
        const Money Values[]{250, 300, 150, 100, 80, 120, 400, 200, 100, 50};
        G::WarehouseDefinition Definition{"warehouse-01", 100, {}};
        for (std::size_t Index = 0; Index < Types.size(); ++Index)
        {
            Definition.Items.push_back({{Types[Index], "Item", "A test item.", Values[Index]},
                "item-" + std::to_string(Index), Values[Index]});
        }
        EndgameProgress Progress;
        CHECK(Progress.Configure(Types).Succeeded());
        Money Funds = 1000;
        const Challenge Contracts[]{Challenge::None, Challenge::Bronze, Challenge::Silver, Challenge::Gold};
        const Money Bids[]{300, 300, 1000, 1500};
        const Rank Ranks[]{Rank::Collector, Rank::Dealer, Rank::Expert, Rank::Master};
        for (std::size_t Index = 0; Index < 4; ++Index)
        {
            CHECK(Progress.IsUnlocked(Contracts[Index]));
            const auto Rule = P::RuleFor(Contracts[Index]);
            Definition.OpeningBid = Rule.MinimumPurchaseCost > 100 ? Rule.MinimumPurchaseCost : 100;
            G::WarehouseSession Session;
            CHECK(Clear(Session, Definition, Funds, Bids[Index]));
            Funds = Session.Economy().Balance();
            const auto Result = Progress.RecordClear("run-" + std::to_string(Index), Session, Contracts[Index]);
            CHECK(Result.Succeeded());
            CHECK(Result.ChallengePassed == (Contracts[Index] != Challenge::None));
            CHECK(Result.NewlyCompleted == (Contracts[Index] != Challenge::None));
            CHECK(Session.Economy().Balance() == Funds);
            CHECK(Progress.CurrentRank() == Ranks[Index]);
        }
        CHECK(Progress.CollectionCount() == 10 && Progress.CollectionTarget() == 10);
        CHECK(Progress.ClearCount() == 4 && Progress.ProfitableClearCount() == 4);
        CHECK(Progress.TotalProfit() == 3900 && Progress.BestProfit() == 1450);
        CHECK(Funds == 4900);
        return true;
    }
}

int main()
{
    using Test = std::pair<const char*, bool (*)()>;
    const std::vector<Test> Tests{
        {"defaults_are_locked", DefaultsAreLocked},
        {"invalid_catalogs_preserve_state", InvalidCatalogsPreserveState},
        {"configuration_copied_once", ConfigurationIsCopiedAndCannotRepeat},
        {"unconfigured_registration_rejected", UnconfiguredRegistrationRejected},
        {"invalid_run_ids_rejected", InvalidRunIdentifiersRejected},
        {"incomplete_and_lost_rounds_rejected", IncompleteAndLostRoundsRejected},
        {"all_sold_still_requires_finish", AllItemsSoldStillRequiresFinish},
        {"duplicate_run_not_recounted", DuplicateRunDoesNotCountAgain},
        {"unknown_types_no_partial_registration", UnknownTypesCannotPartiallyRegister},
        {"same_type_counted_once", MultipleIndividualsCountOneType},
        {"collection_across_rounds", CollectionCanFillAcrossDifferentRounds},
        {"negative_and_zero_profit_recorded", NegativeAndZeroProfitsAreNotDiscarded},
        {"eligibility_before_registration", CannotUnlockAndCompleteInSameRegistration},
        {"locked_later_contract_rejected", LockedLaterContractRejected},
        {"missed_contract_counts_normal_clear", MissedContractStillCountsNormalClear},
        {"actual_purchase_cost_required", ContractChecksActualPurchaseCost},
        {"actual_profit_required", ContractChecksActualProfit},
        {"repeat_contract_no_title_reaward", RepeatingCompletedContractDoesNotReawardTitle},
        {"invalid_challenge_rejected", InvalidChallengeValuesRejected},
        {"positive_profit_overflow_preserves_state", PositiveProfitOverflowPreservesState},
        {"negative_profit_overflow_preserves_state", NegativeProfitOverflowPreservesState},
        {"four_round_ten_type_carryover", FourRoundTenTypeSampleCarriesWallet},
    };
    std::size_t Passed = 0;
    for (const auto& Test : Tests)
    {
        const bool Success = Test.second();
        std::cout << (Success ? "PASS " : "FAIL ") << Test.first << '\n';
        if (Success) { ++Passed; }
    }
    std::cout << Passed << '/' << Tests.size() << " endgame tests passed\n";
    return Passed == Tests.size() ? 0 : 1;
}
