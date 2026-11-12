#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Engine-independent C++17 domain logic. UE5's exact version is not yet selected;
// inclusion in a UE module and Blueprint integration still require UE validation.
namespace LastStorage::Economy
{
using Money = std::int64_t;

enum class AppraisalState
{
    Unappraised,
    Appraised
};

enum class Ownership
{
    InInventory,
    Sold
};

struct ItemDefinition
{
    std::string TypeId;
    std::string DisplayName;
    std::string Description;
    Money ReferenceValue = 0;
};

struct ItemInstance
{
    std::string InstanceId;
    std::string TypeId;
    AppraisalState Appraisal = AppraisalState::Unappraised;
    Money AppraisedValue = 0;
    Ownership State = Ownership::InInventory;
};

enum class Error
{
    None,
    InvalidDefinition,
    InvalidInstanceId,
    DuplicateInstanceId,
    ItemNotFound,
    NotAppraised,
    AlreadySold,
    InvalidAmount,
    BalanceOverflow,
    InsufficientFunds
};

struct Result
{
    Error Code = Error::None;
    Money Amount = 0;

    bool Succeeded() const noexcept { return Code == Error::None; }
};

// One session's economy, used from one thread. Ordinary domain failures leave
// all state unchanged. Allocation failure, persistence and crash recovery are
// outside this class's contract. Sold records remain to prevent ID reuse.
class PlayerEconomy
{
public:
    Money Balance() const noexcept { return BalanceValue; }

    std::size_t OwnedItemCount() const noexcept
    {
        std::size_t Count = 0;
        for (const ItemInstance& Item : ItemRecords)
        {
            if (Item.State == Ownership::InInventory)
            {
                ++Count;
            }
        }
        return Count;
    }

    // Includes sold history. References/pointers may be invalidated by AcquireItem.
    const std::vector<ItemInstance>& Records() const noexcept { return ItemRecords; }

    const ItemInstance* FindItem(std::string_view InstanceId) const noexcept
    {
        for (const ItemInstance& Item : ItemRecords)
        {
            if (std::string_view(Item.InstanceId) == InstanceId)
            {
                return &Item;
            }
        }
        return nullptr;
    }

    // Initialization/grants from trusted game logic, not a public UI command.
    Result Credit(Money Amount) noexcept
    {
        if (Amount < 0)
        {
            return {Error::InvalidAmount, 0};
        }
        if (!CanCredit(Amount))
        {
            return {Error::BalanceOverflow, 0};
        }
        BalanceValue += Amount;
        return {Error::None, Amount};
    }

    Result Spend(Money Amount) noexcept
    {
        if (Amount < 0)
        {
            return {Error::InvalidAmount, 0};
        }
        if (Amount > BalanceValue)
        {
            return {Error::InsufficientFunds, 0};
        }
        BalanceValue -= Amount;
        return {Error::None, Amount};
    }

    Result AcquireItem(const ItemDefinition& Definition, std::string_view InstanceId)
    {
        if (!IsValidId(Definition.TypeId) || !HasDisplayText(Definition.DisplayName)
            || Definition.ReferenceValue < 0)
        {
            return {Error::InvalidDefinition, 0};
        }
        if (!IsValidId(InstanceId))
        {
            return {Error::InvalidInstanceId, 0};
        }
        if (FindItem(InstanceId))
        {
            return {Error::DuplicateInstanceId, 0};
        }

        ItemInstance Item;
        Item.InstanceId = std::string(InstanceId);
        Item.TypeId = Definition.TypeId;
        ItemRecords.push_back(std::move(Item));
        return {Error::None, 0};
    }

    // The appraisal system supplies a confirmed value. This does not calculate
    // a market price and does not interpret inspection as appraisal.
    Result AppraiseItem(std::string_view InstanceId, Money Value) noexcept
    {
        if (!IsValidId(InstanceId))
        {
            return {Error::InvalidInstanceId, 0};
        }
        ItemInstance* Item = FindMutableItem(InstanceId);
        if (!Item)
        {
            return {Error::ItemNotFound, 0};
        }
        if (Item->State == Ownership::Sold)
        {
            return {Error::AlreadySold, 0};
        }
        if (Value < 0)
        {
            return {Error::InvalidAmount, 0};
        }

        Item->Appraisal = AppraisalState::Appraised;
        Item->AppraisedValue = Value;
        return {Error::None, 0};
    }

    Result SellItem(std::string_view InstanceId) noexcept
    {
        if (!IsValidId(InstanceId))
        {
            return {Error::InvalidInstanceId, 0};
        }
        ItemInstance* Item = FindMutableItem(InstanceId);
        if (!Item)
        {
            return {Error::ItemNotFound, 0};
        }
        if (Item->State == Ownership::Sold)
        {
            return {Error::AlreadySold, 0};
        }
        if (Item->Appraisal != AppraisalState::Appraised)
        {
            return {Error::NotAppraised, 0};
        }
        if (!CanCredit(Item->AppraisedValue))
        {
            return {Error::BalanceOverflow, 0};
        }

        // Validation is complete. These writes cannot allocate or fail; callers
        // observe both the credited balance and logical removal from inventory.
        const Money Proceeds = Item->AppraisedValue;
        BalanceValue += Proceeds;
        Item->State = Ownership::Sold;
        return {Error::None, Proceeds};
    }

private:
    static bool IsValidId(std::string_view Id) noexcept
    {
        if (Id.empty() || Id == "none")
        {
            return false;
        }
        for (const char Character : Id)
        {
            if (!((Character >= 'a' && Character <= 'z')
                || (Character >= '0' && Character <= '9')
                || Character == '_' || Character == '-'))
            {
                return false;
            }
        }
        return true;
    }

    static bool HasDisplayText(std::string_view Text) noexcept
    {
        for (const char Character : Text)
        {
            if (Character != ' ' && Character != '\t' && Character != '\n'
                && Character != '\r' && Character != '\f' && Character != '\v')
            {
                return true;
            }
        }
        return false;
    }

    bool CanCredit(Money Amount) const noexcept
    {
        // Parentheses also tolerate Windows headers defining a max macro.
        return Amount >= 0 && Amount <= (std::numeric_limits<Money>::max)() - BalanceValue;
    }

    ItemInstance* FindMutableItem(std::string_view InstanceId) noexcept
    {
        for (ItemInstance& Item : ItemRecords)
        {
            if (std::string_view(Item.InstanceId) == InstanceId)
            {
                return &Item;
            }
        }
        return nullptr;
    }

    Money BalanceValue = 0;
    std::vector<ItemInstance> ItemRecords;
};
}
