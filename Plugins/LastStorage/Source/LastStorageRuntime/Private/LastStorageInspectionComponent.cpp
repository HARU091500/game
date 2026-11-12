#include "LastStorageInspectionComponent.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "LastStorageInspectable.h"

ULastStorageInspectionComponent::ULastStorageInspectionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool ULastStorageInspectionComponent::Inspect()
{
    APawn* const Pawn = Cast<APawn>(GetOwner());
    APlayerController* const Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
    UWorld* const World = GetWorld();
    if (!World || !Controller || !Controller->IsLocalController() || Controller->GetPawn() != Pawn
        || !FMath::IsFinite(InspectionDistance) || InspectionDistance <= 0.0f)
    {
        return false;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const float Distance = FMath::Clamp(InspectionDistance, 1.0f, 1000.0f);
    const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * Distance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LastStorageInspect), false, Pawn);
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
    {
        return false;
    }

    AActor* const Item = Hit.GetActor();
    if (!IsValid(Item) || !Item->GetClass()->ImplementsInterface(ULastStorageInspectable::StaticClass())
        || !ILastStorageInspectable::Execute_CanInspect(Item, Pawn) || !IsValid(Item))
    {
        return false;
    }

    ILastStorageInspectable::Execute_Inspect(Item, Pawn);
    // A Blueprint implementation may destroy itself during Inspect.
    if (!IsValid(Item))
    {
        return true;
    }
    const FLastStorageInspectionInfo Info = ILastStorageInspectable::Execute_GetInspectionInfo(Item);
    OnItemInspected.Broadcast(Item, Info.ItemName, Info.Description);
    return true;
}
