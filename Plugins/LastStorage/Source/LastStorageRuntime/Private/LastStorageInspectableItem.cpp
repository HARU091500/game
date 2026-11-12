#include "LastStorageInspectableItem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ALastStorageInspectableItem::ALastStorageInspectableItem()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Mesh->SetSimulatePhysics(false);

    // Epic's built-in cube is only a blockout placeholder. No third-party content required.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube.Succeeded())
    {
        Mesh->SetStaticMesh(Cube.Object);
    }
    ItemName = NSLOCTEXT("LastStorage", "UnidentifiedItem", "Unidentified item");
}

bool ALastStorageInspectableItem::CanInspect_Implementation(APawn* InspectingPawn) const
{
    return IsValid(InspectingPawn);
}

FLastStorageInspectionInfo ALastStorageInspectableItem::GetInspectionInfo_Implementation() const
{
    FLastStorageInspectionInfo Info;
    Info.ItemId = ItemId;
    Info.ItemName = ItemName;
    Info.Description = Description;
    return Info;
}

void ALastStorageInspectableItem::Inspect_Implementation(APawn* InspectingPawn)
{
    if (!ILastStorageInspectable::Execute_CanInspect(this, InspectingPawn))
    {
        return;
    }
    bInspected = true;
    ReceiveInspected(InspectingPawn);
}
