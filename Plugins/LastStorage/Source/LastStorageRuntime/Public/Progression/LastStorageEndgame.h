#pragma once

#include "Gameplay/LastStorageSession.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Portable C++17 progression. UE5's exact version is unselected; this is not a
// Blueprint API or a save system. A trusted caller assigns one stable ID per run.
namespace LastStorage::Progression
{
using Money = ::LastStorage::Economy::Money;

enum class Challenge { None, Bronze, Silver, Gold };
enum class Rank { Rookie, Collector, Dealer, Expert, Master };
enum class ProgressError
{
    None, NotConfigured, AlreadyConfigured, InvalidCatalog, InvalidRunId,
    DuplicateRun, RoundNotCleared, UnknownType, InvalidChallenge, ChallengeLocked, Overflow
};

struct ProgressResult
{
    ProgressError Code = ProgressError::None;
    bool ChallengePassed = false;
    bool NewlyCompleted = false;
    bool Succeeded() const noexcept { return Code == ProgressError::None; }
};

struct ChallengeRule
{
    Money MinimumPurchaseCost = 0;
    Money MinimumProfit = 0;
};

inline ChallengeRule RuleFor(Challenge Value) noexcept
{
    switch (Value)
    {
    case Challenge::None: return {};
    case Challenge::Bronze: return {0, 1000};
    case Challenge::Silver: return {1000, 500};
    case Challenge::Gold: return {1500, 200};
    }
    return {}; // RecordClear separately rejects invalid enum values.
}

class EndgameProgress
{
public:
    bool IsConfigured() const noexcept { return Configured; }
    std::size_t CollectionTarget() const noexcept { return Catalog.size(); }
    std::uint64_t ClearCount() const noexcept { return Clears; }
    std::uint64_t ProfitableClearCount() const noexcept { return ProfitableClears; }
    Money TotalProfit() const noexcept { return CumulativeProfit; }
    Money BestProfit() const noexcept { return HighestProfit; }

    std::size_t CollectionCount() const noexcept
    {
        std::size_t Count = 0;
        for (const unsigned char Value : Discovered) { if (Value != 0) { ++Count; } }
        return Count;
    }

    bool HasDiscovered(std::string_view TypeId) const noexcept
    {
        const std::size_t Index = FindType(TypeId);
        return Index < Discovered.size() && Discovered[Index] != 0;
    }

    bool IsUnlocked(Challenge Value) const noexcept
    {
        if (!Configured) { return false; }
        switch (Value)
        {
        case Challenge::None: return true;
        case Challenge::Bronze: return CollectionCount() == Catalog.size() && ProfitableClears != 0;
        case Challenge::Silver: return Completed[0] != 0;
        case Challenge::Gold: return Completed[1] != 0;
        }
        return false;
    }

    bool IsCompleted(Challenge Value) const noexcept
    {
        switch (Value)
        {
        case Challenge::Bronze: return Completed[0] != 0;
        case Challenge::Silver: return Completed[1] != 0;
        case Challenge::Gold: return Completed[2] != 0;
        case Challenge::None: return false;
        }
        return false;
    }

    Rank CurrentRank() const noexcept
    {
        if (IsCompleted(Challenge::Gold)) { return Rank::Master; }
        if (IsCompleted(Challenge::Silver)) { return Rank::Expert; }
        if (IsCompleted(Challenge::Bronze)) { return Rank::Dealer; }
        if (Configured && Clears != 0 && CollectionCount() == Catalog.size()) { return Rank::Collector; }
        return Rank::Rookie;
    }

    ProgressResult Configure(const std::vector<std::string>& CatalogTypeIds)
    {
        if (Configured) { return Failure(ProgressError::AlreadyConfigured); }
        if (CatalogTypeIds.empty()) { return Failure(ProgressError::InvalidCatalog); }
        for (std::size_t Index = 0; Index < CatalogTypeIds.size(); ++Index)
        {
            if (!IsValidId(CatalogTypeIds[Index])) { return Failure(ProgressError::InvalidCatalog); }
            for (std::size_t Earlier = 0; Earlier < Index; ++Earlier)
            {
                if (CatalogTypeIds[Index] == CatalogTypeIds[Earlier]) { return Failure(ProgressError::InvalidCatalog); }
            }
        }
        auto NewCatalog = CatalogTypeIds;
        std::vector<unsigned char> NewDiscovered(CatalogTypeIds.size(), 0);
        Catalog = std::move(NewCatalog);
        Discovered = std::move(NewDiscovered);
        Configured = true;
        return {};
    }

    ProgressResult RecordClear(std::string_view RunId, const ::LastStorage::Gameplay::WarehouseSession& Session,
        Challenge RequestedChallenge = Challenge::None)
    {
        if (!Configured) { return Failure(ProgressError::NotConfigured); }
        if (!IsValidChallenge(RequestedChallenge)) { return Failure(ProgressError::InvalidChallenge); }
        if (!IsValidId(RunId)) { return Failure(ProgressError::InvalidRunId); }
        for (const auto& Seen : RecordedRuns)
        {
            if (std::string_view(Seen) == RunId) { return Failure(ProgressError::DuplicateRun); }
        }
        if (Session.CurrentPhase() != ::LastStorage::Gameplay::Phase::Finished
            || Session.AuctionOutcome() != ::LastStorage::Gameplay::Outcome::Won)
        {
            return Failure(ProgressError::RoundNotCleared);
        }
        // Eligibility comes from prior results, never the clear being registered.
        if (!IsUnlocked(RequestedChallenge)) { return Failure(ProgressError::ChallengeLocked); }

        for (const auto& Item : Session.Warehouse().Items)
        {
            if (FindType(Item.Definition.TypeId) == Catalog.size()) { return Failure(ProgressError::UnknownType); }
            const auto* Record = Session.Economy().FindItem(Item.InstanceId);
            if (!Session.IsCollected(Item.InstanceId) || !Record
                || Record->Appraisal != ::LastStorage::Economy::AppraisalState::Appraised
                || Record->State != ::LastStorage::Economy::Ownership::Sold)
            {
                return Failure(ProgressError::RoundNotCleared);
            }
        }

        const Money Profit = Session.Profit();
        const Money Maximum = (std::numeric_limits<Money>::max)();
        const Money Minimum = (std::numeric_limits<Money>::min)();
        if (Clears == (std::numeric_limits<std::uint64_t>::max)()
            || (Profit > 0 && ProfitableClears == (std::numeric_limits<std::uint64_t>::max)())
            || (Profit > 0 && CumulativeProfit > Maximum - Profit)
            || (Profit < 0 && CumulativeProfit < Minimum - Profit))
        {
            return Failure(ProgressError::Overflow);
        }

        // Stage allocation before publishing state. Ordinary rule failures leave
        // the entire profile unchanged; allocation failure/crash recovery is not a save guarantee.
        auto NewDiscovered = Discovered;
        for (const auto& Item : Session.Warehouse().Items) { NewDiscovered[FindType(Item.Definition.TypeId)] = 1; }
        auto NewRuns = RecordedRuns;
        NewRuns.emplace_back(RunId);
        const auto Rule = RuleFor(RequestedChallenge);
        const bool Passed = RequestedChallenge != Challenge::None
            && Session.PurchaseCost() >= Rule.MinimumPurchaseCost && Profit >= Rule.MinimumProfit;
        const bool Newly = Passed && !IsCompleted(RequestedChallenge);

        Discovered = std::move(NewDiscovered);
        RecordedRuns = std::move(NewRuns);
        if (Clears == 0 || Profit > HighestProfit) { HighestProfit = Profit; }
        ++Clears;
        if (Profit > 0) { ++ProfitableClears; }
        CumulativeProfit += Profit;
        if (Passed) { Completed[ChallengeIndex(RequestedChallenge)] = 1; }
        return {ProgressError::None, Passed, Newly};
    }

private:
    static ProgressResult Failure(ProgressError Code) noexcept { return {Code, false, false}; }

    static bool IsValidChallenge(Challenge Value) noexcept
    {
        return Value == Challenge::None || Value == Challenge::Bronze || Value == Challenge::Silver || Value == Challenge::Gold;
    }

    static std::size_t ChallengeIndex(Challenge Value) noexcept
    {
        return Value == Challenge::Bronze ? 0 : (Value == Challenge::Silver ? 1 : 2);
    }

    static bool IsValidId(std::string_view Value) noexcept
    {
        if (Value.empty() || Value == "none") { return false; }
        for (const char Character : Value)
        {
            if (!((Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9')
                || Character == '_' || Character == '-')) { return false; }
        }
        return true;
    }

    std::size_t FindType(std::string_view TypeId) const noexcept
    {
        for (std::size_t Index = 0; Index < Catalog.size(); ++Index)
        {
            if (std::string_view(Catalog[Index]) == TypeId) { return Index; }
        }
        return Catalog.size();
    }

    bool Configured = false;
    std::vector<std::string> Catalog;
    std::vector<unsigned char> Discovered;
    std::vector<std::string> RecordedRuns;
    std::array<unsigned char, 3> Completed{};
    std::uint64_t Clears = 0;
    std::uint64_t ProfitableClears = 0;
    Money CumulativeProfit = 0;
    Money HighestProfit = 0;
};
}
