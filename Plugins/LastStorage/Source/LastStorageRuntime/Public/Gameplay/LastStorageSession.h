#pragma once

#include "Economy/LastStorageEconomy.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Portable, one-room prototype rules. UE5's exact version is not selected;
// this is not an Unreal Actor, Blueprint API, persistence layer or 3D simulation.
namespace LastStorage::Gameplay
{
using Money = ::LastStorage::Economy::Money;

enum class Phase
{
    NotStarted,
    Auction,
    Exploration,
    Finished
};

enum class Outcome
{
    Pending,
    Won,
    Lost
};

enum class SessionError
{
    None,
    InvalidPhase,
    InvalidWarehouse,
    InvalidAmount,
    InsufficientFunds,
    BidTooLow,
    NoBid,
    ItemNotFound,
    AlreadyCollected,
    ItemNotCollected,
    EconomyFailure,
    RoundIncomplete
};

struct SessionResult
{
    SessionError Code = SessionError::None;
    ::LastStorage::Economy::Error EconomyCode = ::LastStorage::Economy::Error::None;
    Money Amount = 0;

    bool Succeeded() const noexcept { return Code == SessionError::None; }
};

struct WarehouseItem
{
    ::LastStorage::Economy::ItemDefinition Definition;
    std::string InstanceId;
    Money AppraisalValue = 0;
};

struct WarehouseDefinition
{
    std::string WarehouseId;
    Money OpeningBid = 100;
    std::vector<WarehouseItem> Items;
};

// One session owns the only mutable economy. Ordinary rule failures change no
// state. Allocation failure/crash recovery and concurrent access are out of scope.
class WarehouseSession
{
public:
    const ::LastStorage::Economy::PlayerEconomy& Economy() const noexcept { return PlayerState; }
    const WarehouseDefinition& Warehouse() const noexcept { return WarehouseData; }
    Phase CurrentPhase() const noexcept { return PhaseValue; }
    Outcome AuctionOutcome() const noexcept { return OutcomeValue; }
    Money CurrentBid() const noexcept { return BidValue; }
    Money OpeningBalance() const noexcept { return OpeningFunds; }
    Money PurchaseCost() const noexcept { return PaidCost; }
    Money SaleRevenue() const noexcept { return Revenue; }
    Money Profit() const noexcept { return Revenue - PaidCost; }

    bool IsCollected(std::string_view InstanceId) const noexcept
    {
        const std::size_t Index = FindWorldItem(InstanceId);
        return Index < Collected.size() && Collected[Index] != 0;
    }

    SessionResult Start(const WarehouseDefinition& Definition, Money StartingBalance)
    {
        if (PhaseValue != Phase::NotStarted)
        {
            return Failure(SessionError::InvalidPhase);
        }
        if (StartingBalance < 0)
        {
            return Failure(SessionError::InvalidAmount);
        }
        if (!IsValidId(Definition.WarehouseId) || Definition.OpeningBid <= 0 || Definition.Items.empty())
        {
            return Failure(SessionError::InvalidWarehouse);
        }

        // Reuse economy validation without registering the world items as owned.
        ::LastStorage::Economy::PlayerEconomy Validation;
        for (const WarehouseItem& Item : Definition.Items)
        {
            if (Item.AppraisalValue < 0)
            {
                return Failure(SessionError::InvalidWarehouse);
            }
            const auto Valid = Validation.AcquireItem(Item.Definition, Item.InstanceId);
            if (!Valid.Succeeded())
            {
                return {SessionError::InvalidWarehouse, Valid.Code, 0};
            }
        }

        // Stage every allocation before publishing any session state.
        WarehouseDefinition NewWarehouse = Definition;
        std::vector<unsigned char> NewCollected(Definition.Items.size(), 0);
        ::LastStorage::Economy::PlayerEconomy NewEconomy;
        const auto InitialFunds = NewEconomy.Credit(StartingBalance);
        if (!InitialFunds.Succeeded())
        {
            return EconomyFailure(InitialFunds);
        }
        WarehouseData = std::move(NewWarehouse);
        Collected = std::move(NewCollected);
        PlayerState = std::move(NewEconomy);
        OpeningFunds = StartingBalance;
        PhaseValue = Phase::Auction;
        return {};
    }

    SessionResult Bid(Money Amount) noexcept
    {
        if (PhaseValue != Phase::Auction)
        {
            return Failure(SessionError::InvalidPhase);
        }
        if (Amount <= 0)
        {
            return Failure(SessionError::InvalidAmount);
        }
        if (Amount < WarehouseData.OpeningBid || Amount <= BidValue)
        {
            return Failure(SessionError::BidTooLow);
        }
        if (Amount > PlayerState.Balance())
        {
            return Failure(SessionError::InsufficientFunds);
        }
        BidValue = Amount;
        return {};
    }

    SessionResult ResolveAuction(bool PlayerWon) noexcept
    {
        if (PhaseValue != Phase::Auction)
        {
            return Failure(SessionError::InvalidPhase);
        }
        if (BidValue == 0)
        {
            return Failure(SessionError::NoBid);
        }
        if (!PlayerWon)
        {
            OutcomeValue = Outcome::Lost;
            PhaseValue = Phase::Finished;
            return {};
        }
        const auto Payment = PlayerState.Spend(BidValue);
        if (!Payment.Succeeded())
        {
            return EconomyFailure(Payment);
        }
        PaidCost = BidValue;
        OutcomeValue = Outcome::Won;
        PhaseValue = Phase::Exploration;
        return {SessionError::None, ::LastStorage::Economy::Error::None, Payment.Amount};
    }

    SessionResult Collect(std::string_view InstanceId)
    {
        if (PhaseValue != Phase::Exploration)
        {
            return Failure(SessionError::InvalidPhase);
        }
        const std::size_t Index = FindWorldItem(InstanceId);
        if (Index == WarehouseData.Items.size())
        {
            return Failure(SessionError::ItemNotFound);
        }
        if (Collected[Index] != 0)
        {
            return Failure(SessionError::AlreadyCollected);
        }
        const WarehouseItem& Item = WarehouseData.Items[Index];
        const auto Acquired = PlayerState.AcquireItem(Item.Definition, Item.InstanceId);
        if (!Acquired.Succeeded())
        {
            return EconomyFailure(Acquired);
        }
        Collected[Index] = 1;
        return {};
    }

    SessionResult Appraise(std::string_view InstanceId) noexcept
    {
        if (PhaseValue != Phase::Exploration)
        {
            return Failure(SessionError::InvalidPhase);
        }
        const std::size_t Index = FindWorldItem(InstanceId);
        if (Index == WarehouseData.Items.size())
        {
            return Failure(SessionError::ItemNotFound);
        }
        if (Collected[Index] == 0)
        {
            return Failure(SessionError::ItemNotCollected);
        }
        const auto Appraisal = PlayerState.AppraiseItem(InstanceId, WarehouseData.Items[Index].AppraisalValue);
        return Appraisal.Succeeded() ? SessionResult{} : EconomyFailure(Appraisal);
    }

    SessionResult Sell(std::string_view InstanceId) noexcept
    {
        if (PhaseValue != Phase::Exploration)
        {
            return Failure(SessionError::InvalidPhase);
        }
        const auto Sale = PlayerState.SellItem(InstanceId);
        if (!Sale.Succeeded())
        {
            return EconomyFailure(Sale);
        }
        // Balance = opening funds - cost + revenue. Deriving the accumulated
        // revenue from nonnegative balances avoids a second unchecked addition.
        Revenue = PlayerState.Balance() - (OpeningFunds - PaidCost);
        return {SessionError::None, ::LastStorage::Economy::Error::None, Sale.Amount};
    }

    SessionResult FinishRound() noexcept
    {
        if (PhaseValue != Phase::Exploration)
        {
            return Failure(SessionError::InvalidPhase);
        }
        for (const unsigned char ItemCollected : Collected)
        {
            if (ItemCollected == 0)
            {
                return Failure(SessionError::RoundIncomplete);
            }
        }
        if (PlayerState.OwnedItemCount() != 0)
        {
            return Failure(SessionError::RoundIncomplete);
        }
        PhaseValue = Phase::Finished;
        return {};
    }

private:
    static SessionResult Failure(SessionError Code) noexcept { return {Code, ::LastStorage::Economy::Error::None, 0}; }

    static SessionResult EconomyFailure(const ::LastStorage::Economy::Result& Result) noexcept
    {
        return {SessionError::EconomyFailure, Result.Code, 0};
    }

    static bool IsValidId(std::string_view Id) noexcept
    {
        if (Id.empty() || Id == "none") { return false; }
        for (const char Character : Id)
        {
            if (!((Character >= 'a' && Character <= 'z') || (Character >= '0' && Character <= '9')
                || Character == '_' || Character == '-')) { return false; }
        }
        return true;
    }

    std::size_t FindWorldItem(std::string_view InstanceId) const noexcept
    {
        for (std::size_t Index = 0; Index < WarehouseData.Items.size(); ++Index)
        {
            if (std::string_view(WarehouseData.Items[Index].InstanceId) == InstanceId) { return Index; }
        }
        return WarehouseData.Items.size();
    }

    WarehouseDefinition WarehouseData;
    std::vector<unsigned char> Collected;
    ::LastStorage::Economy::PlayerEconomy PlayerState;
    Phase PhaseValue = Phase::NotStarted;
    Outcome OutcomeValue = Outcome::Pending;
    Money BidValue = 0;
    Money OpeningFunds = 0;
    Money PaidCost = 0;
    Money Revenue = 0;
};
}
