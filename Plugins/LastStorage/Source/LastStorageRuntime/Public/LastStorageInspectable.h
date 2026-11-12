#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "LastStorageInspectable.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct LASTSTORAGERUNTIME_API FLastStorageInspectionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspection")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspection")
    FText ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspection", meta = (MultiLine = "true"))
    FText Description;
};

UINTERFACE(BlueprintType, Blueprintable)
class LASTSTORAGERUNTIME_API ULastStorageInspectable : public UInterface
{
    GENERATED_BODY()
};

// Inspection does not depend on a concrete item class; future furniture can implement this too.
class LASTSTORAGERUNTIME_API ILastStorageInspectable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "LAST STORAGE|Inspection")
    bool CanInspect(APawn* InspectingPawn) const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "LAST STORAGE|Inspection")
    FLastStorageInspectionInfo GetInspectionInfo() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "LAST STORAGE|Inspection")
    void Inspect(APawn* InspectingPawn);
};
