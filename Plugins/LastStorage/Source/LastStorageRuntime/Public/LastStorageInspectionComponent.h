#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LastStorageInspectionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FLastStorageItemInspected,
    AActor*, Item, FText, ItemName, FText, Description);

// Add to the First Person character Blueprint. IA_Inspect.Started calls Inspect exactly once.
UCLASS(ClassGroup = (LastStorage), meta = (BlueprintSpawnableComponent))
class LASTSTORAGERUNTIME_API ULastStorageInspectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    ULastStorageInspectionComponent();

    // Centimetres. A single Visibility hit ensures walls and nearer items occlude the target.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAST STORAGE|Inspection",
        meta = (ClampMin = "1.0", ClampMax = "1000.0", Units = "cm"))
    float InspectionDistance = 300.0f;

    UPROPERTY(BlueprintAssignable, Category = "LAST STORAGE|Inspection")
    FLastStorageItemInspected OnItemInspected;

    // False for no possession, nonlocal player, no hit, occlusion or a non-inspectable actor.
    UFUNCTION(BlueprintCallable, Category = "LAST STORAGE|Inspection")
    bool Inspect();
};
