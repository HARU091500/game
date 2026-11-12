#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "LastStorageInspectable.h"
#include "LastStorageInspectableItem.h"
#include "LastStorageInspectionComponent.h"

namespace LastStorageTests
{
    // CreateWorld performs initialization. These state tests do not exercise scene traces.
    struct FTestWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

        ~FTestWorld()
        {
            if (World)
            {
                World->DestroyWorld(false);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLastStorageItemStateTest,
    "LastStorage.Inspection.ItemState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLastStorageItemStateTest::RunTest(const FString& Parameters)
{
    LastStorageTests::FTestWorld Fixture;
    if (!TestNotNull(TEXT("Transient world created"), Fixture.World))
    {
        return false;
    }
    ALastStorageInspectableItem* Item = Fixture.World->SpawnActor<ALastStorageInspectableItem>();
    APawn* Pawn = Fixture.World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Item created"), Item) || !TestNotNull(TEXT("Pawn created"), Pawn))
    {
        return false;
    }

    Item->ItemId = TEXT("radio");
    Item->ItemName = FText::FromString(TEXT("Radio"));
    Item->Description = FText::FromString(TEXT("A dusty portable radio."));
    const FLastStorageInspectionInfo Info = ILastStorageInspectable::Execute_GetInspectionInfo(Item);
    TestEqual(TEXT("Stable item ID is returned"), Info.ItemId, FName(TEXT("radio")));
    TestTrue(TEXT("Item name is returned"), Info.ItemName.EqualTo(Item->ItemName));
    TestTrue(TEXT("Description is returned"), Info.Description.EqualTo(Item->Description));
    TestFalse(TEXT("Not inspected on spawn"), Item->IsInspected());
    TestFalse(TEXT("Null pawn cannot inspect"), ILastStorageInspectable::Execute_CanInspect(Item, nullptr));
    ILastStorageInspectable::Execute_Inspect(Item, nullptr);
    TestFalse(TEXT("Invalid request does not mutate state"), Item->IsInspected());
    TestTrue(TEXT("Valid pawn may inspect"), ILastStorageInspectable::Execute_CanInspect(Item, Pawn));
    ILastStorageInspectable::Execute_Inspect(Item, Pawn);
    TestTrue(TEXT("Inspect marks session state"), Item->IsInspected());
    ILastStorageInspectable::Execute_Inspect(Item, Pawn);
    TestTrue(TEXT("Repeated inspection remains allowed"), Item->IsInspected());
    TestEqual(TEXT("Placeholder blocks visibility"), Item->Mesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
    TestEqual(TEXT("Placeholder blocks pawn"), Item->Mesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLastStorageUnpossessedTest,
    "LastStorage.Inspection.UnpossessedPawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLastStorageUnpossessedTest::RunTest(const FString& Parameters)
{
    LastStorageTests::FTestWorld Fixture;
    if (!TestNotNull(TEXT("Transient world created"), Fixture.World))
    {
        return false;
    }
    APawn* Pawn = Fixture.World->SpawnActor<APawn>();
    if (!TestNotNull(TEXT("Pawn created"), Pawn))
    {
        return false;
    }
    ULastStorageInspectionComponent* Component = NewObject<ULastStorageInspectionComponent>(Pawn);
    if (!TestNotNull(TEXT("Inspection component created"), Component))
    {
        return false;
    }
    Pawn->AddInstanceComponent(Component);
    Component->RegisterComponent();
    TestFalse(TEXT("Unpossessed pawn returns false without crashing"), Component->Inspect());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
