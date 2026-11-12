#include "Economy/LastStorageEconomy.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace LastStorage::Economy;

namespace
{
    // This executable calls the actual portable core, without Unreal substitutes.
    #define CHECK(condition) \
        do { if (!(condition)) { std::cerr << "  line " << __LINE__ << ": " #condition "\n"; return false; } } while (false)

    ItemDefinition Radio()
    {
        return {"radio", "Radio", "A portable radio.", 250};
    }

    bool SameRecords(const std::vector<ItemInstance>& a, const std::vector<ItemInstance>& b)
    {
        if (a.size() != b.size()) { return false; }
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (a[i].InstanceId != b[i].InstanceId || a[i].TypeId != b[i].TypeId
                || a[i].Appraisal != b[i].Appraisal || a[i].AppraisedValue != b[i].AppraisedValue
                || a[i].State != b[i].State) { return false; }
        }
        return true;
    }

    template <typename Operation>
    bool RejectedWithoutChanges(PlayerEconomy& economy, Operation operation, Error expected)
    {
        const Money balance = economy.Balance();
        const auto records = economy.Records();
        const Result result = operation();
        return !result.Succeeded() && result.Code == expected && result.Amount == 0
            && economy.Balance() == balance && SameRecords(economy.Records(), records);
    }

    bool PrepareAppraised(PlayerEconomy& economy, Money value = 250)
    {
        return economy.AcquireItem(Radio(), "room01-radio-001").Succeeded()
            && economy.AppraiseItem("room01-radio-001", value).Succeeded();
    }

    bool EmptySession()
    {
        const PlayerEconomy economy;
        CHECK(economy.Balance() == 0);
        CHECK(economy.OwnedItemCount() == 0);
        CHECK(economy.Records().empty());
        CHECK(economy.FindItem("unknown") == nullptr);
        return true;
    }

    bool AcquireStartsUnappraised()
    {
        PlayerEconomy economy;
        CHECK(economy.AcquireItem(Radio(), "room01-radio-001").Succeeded());
        const auto* item = economy.FindItem("room01-radio-001");
        CHECK(item != nullptr);
        CHECK(item->TypeId == "radio");
        CHECK(item->InstanceId != item->TypeId);
        CHECK(item->State == Ownership::InInventory);
        CHECK(item->Appraisal == AppraisalState::Unappraised);
        CHECK(item->AppraisedValue == 0);
        CHECK(economy.OwnedItemCount() == 1);
        CHECK(economy.Balance() == 0);
        return true;
    }

    bool SaleCreditsAndRemovesOwnership()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(1000).Succeeded());
        CHECK(PrepareAppraised(economy));
        const auto result = economy.SellItem("room01-radio-001");
        CHECK(result.Succeeded() && result.Amount == 250);
        CHECK(economy.Balance() == 1250);
        CHECK(economy.OwnedItemCount() == 0);
        CHECK(economy.FindItem("room01-radio-001")->State == Ownership::Sold);
        CHECK(economy.Records().size() == 1);
        return true;
    }

    bool UnappraisedSaleRejected()
    {
        PlayerEconomy economy;
        CHECK(economy.AcquireItem(Radio(), "room01-radio-001").Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.SellItem("room01-radio-001"); }, Error::NotAppraised));
        return true;
    }

    bool DoubleSaleRejected()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.SellItem("room01-radio-001"); }, Error::AlreadySold));
        return true;
    }

    bool SoldIdentityCannotBeReacquired()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(Radio(), "room01-radio-001"); }, Error::DuplicateInstanceId));
        return true;
    }

    bool SameTypeHasIndependentInstances()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(economy.AcquireItem(Radio(), "room01-radio-002").Succeeded());
        CHECK(economy.AppraiseItem("room01-radio-002", 75).Succeeded());
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(economy.OwnedItemCount() == 1);
        CHECK(economy.FindItem("room01-radio-002")->State == Ownership::InInventory);
        CHECK(economy.SellItem("room01-radio-002").Succeeded());
        CHECK(economy.Balance() == 325);
        return true;
    }

    bool UnknownSaleRejected()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.SellItem("room01-missing-999"); }, Error::ItemNotFound));
        return true;
    }

    bool InvalidInstanceIdentifiersRejected()
    {
        PlayerEconomy economy;
        for (const std::string id : {"", "RADIO-001", "none", "item 1", "item/1", "item.1", "\xe5\x93\x81"})
        {
            CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(Radio(), id); }, Error::InvalidInstanceId));
        }
        return true;
    }

    bool InvalidDefinitionsRejected()
    {
        PlayerEconomy economy;
        auto definition = Radio();
        for (const std::string id : {"", "Radio", "none", "radio/item"})
        {
            definition.TypeId = id;
            CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(definition, "item-001"); }, Error::InvalidDefinition));
        }
        definition = Radio();
        definition.DisplayName.clear();
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(definition, "item-001"); }, Error::InvalidDefinition));
        definition = Radio();
        definition.ReferenceValue = -1;
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(definition, "item-001"); }, Error::InvalidDefinition));
        return true;
    }

    bool DuplicateOwnedIdentityRejected()
    {
        PlayerEconomy economy;
        CHECK(economy.AcquireItem(Radio(), "room01-radio-001").Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AcquireItem(Radio(), "room01-radio-001"); }, Error::DuplicateInstanceId));
        return true;
    }

    bool NegativeAppraisalRejected()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AppraiseItem("room01-radio-001", -1); }, Error::InvalidAmount));
        return true;
    }

    bool UnknownAppraisalRejected()
    {
        PlayerEconomy economy;
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AppraiseItem("missing", 100); }, Error::ItemNotFound));
        return true;
    }

    bool ReappraisalBeforeSale()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        const auto appraisal = economy.AppraiseItem("room01-radio-001", 99);
        CHECK(appraisal.Succeeded() && appraisal.Amount == 0);
        CHECK(economy.Balance() == 0);
        CHECK(economy.SellItem("room01-radio-001").Amount == 99);
        CHECK(economy.Balance() == 99);
        return true;
    }

    bool SoldItemCannotBeAppraised()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy));
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.AppraiseItem("room01-radio-001", 999); }, Error::AlreadySold));
        return true;
    }

    bool ZeroSaleIsValidAtMaximumBalance()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(std::numeric_limits<Money>::max()).Succeeded());
        CHECK(PrepareAppraised(economy, 0));
        const auto sale = economy.SellItem("room01-radio-001");
        CHECK(sale.Succeeded() && sale.Amount == 0);
        CHECK(economy.Balance() == std::numeric_limits<Money>::max());
        CHECK(economy.OwnedItemCount() == 0);
        return true;
    }

    bool NegativeCreditRejected()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(1000).Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.Credit(-1); }, Error::InvalidAmount));
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.Credit(std::numeric_limits<Money>::min()); }, Error::InvalidAmount));
        return true;
    }

    bool CreditOverflowRejected()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(std::numeric_limits<Money>::max()).Succeeded());
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.Credit(1); }, Error::BalanceOverflow));
        CHECK(economy.Credit(0).Succeeded());
        return true;
    }

    bool SaleOverflowPreservesOwnershipAndBalance()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(std::numeric_limits<Money>::max() - 10).Succeeded());
        CHECK(PrepareAppraised(economy, 11));
        CHECK(RejectedWithoutChanges(economy, [&] { return economy.SellItem("room01-radio-001"); }, Error::BalanceOverflow));
        CHECK(economy.OwnedItemCount() == 1);
        return true;
    }

    bool SaleCanReachExactMaximum()
    {
        PlayerEconomy economy;
        CHECK(economy.Credit(std::numeric_limits<Money>::max() - 250).Succeeded());
        CHECK(PrepareAppraised(economy));
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(economy.Balance() == std::numeric_limits<Money>::max());
        return true;
    }

    bool MaximumAppraisalFromZeroBalance()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy, std::numeric_limits<Money>::max()));
        CHECK(economy.SellItem("room01-radio-001").Succeeded());
        CHECK(economy.Balance() == std::numeric_limits<Money>::max());
        return true;
    }

    bool CallerInputsAreCopied()
    {
        PlayerEconomy economy;
        auto definition = Radio();
        std::string id = "room01-radio-001";
        CHECK(economy.AcquireItem(definition, id).Succeeded());
        definition.TypeId = "changed";
        id = "changed";
        CHECK(economy.FindItem("room01-radio-001") != nullptr);
        CHECK(economy.FindItem("room01-radio-001")->TypeId == "radio");
        return true;
    }

    bool AppraisedValueOverridesReferencePrice()
    {
        PlayerEconomy economy;
        CHECK(PrepareAppraised(economy, 17));
        CHECK(economy.SellItem("room01-radio-001").Amount == 17);
        CHECK(economy.Balance() == 17);
        return true;
    }
}

int main()
{
    using Test = std::pair<const char*, bool (*)()>;
    const std::vector<Test> tests = {
        {"empty_session", EmptySession},
        {"acquire_starts_unappraised", AcquireStartsUnappraised},
        {"sale_credits_and_removes_ownership", SaleCreditsAndRemovesOwnership},
        {"unappraised_sale_rejected", UnappraisedSaleRejected},
        {"double_sale_rejected", DoubleSaleRejected},
        {"sold_identity_cannot_be_reacquired", SoldIdentityCannotBeReacquired},
        {"same_type_has_independent_instances", SameTypeHasIndependentInstances},
        {"unknown_sale_rejected", UnknownSaleRejected},
        {"invalid_instance_identifiers_rejected", InvalidInstanceIdentifiersRejected},
        {"invalid_definitions_rejected", InvalidDefinitionsRejected},
        {"duplicate_owned_identity_rejected", DuplicateOwnedIdentityRejected},
        {"negative_appraisal_rejected", NegativeAppraisalRejected},
        {"unknown_appraisal_rejected", UnknownAppraisalRejected},
        {"reappraisal_before_sale", ReappraisalBeforeSale},
        {"sold_item_cannot_be_appraised", SoldItemCannotBeAppraised},
        {"zero_sale_valid_at_max_balance", ZeroSaleIsValidAtMaximumBalance},
        {"negative_credit_rejected", NegativeCreditRejected},
        {"credit_overflow_rejected", CreditOverflowRejected},
        {"sale_overflow_preserves_ownership", SaleOverflowPreservesOwnershipAndBalance},
        {"sale_reaches_exact_maximum", SaleCanReachExactMaximum},
        {"maximum_appraisal_from_zero", MaximumAppraisalFromZeroBalance},
        {"caller_inputs_are_copied", CallerInputsAreCopied},
        {"appraised_value_overrides_reference", AppraisedValueOverridesReferencePrice},
    };
    std::size_t passed = 0;
    for (const auto& test : tests)
    {
        const bool success = test.second();
        std::cout << (success ? "PASS " : "FAIL ") << test.first << '\n';
        if (success) { ++passed; }
    }
    std::cout << passed << '/' << tests.size() << " tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
