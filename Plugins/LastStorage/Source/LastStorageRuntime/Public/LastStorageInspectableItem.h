#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LastStorageInspectable.h"
#include "LastStorageInspectableItem.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class LASTSTORAGERUNTIME_API ALastStorageInspectableItem : public AActor, public ILastStorageInspectable
{
    GENERATED_BODY()

public:
    ALastStorageInspectableItem();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LAST STORAGE|Item")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LAST STORAGE|Item")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LAST STORAGE|Item")
    FText ItemName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LAST STORAGE|Item", meta = (MultiLine = "true"))
    FText Description;

    UFUNCTION(BlueprintPure, Category = "LAST STORAGE|Item")
    bool IsInspected() const { return bInspected; }

    virtual bool CanInspect_Implementation(APawn* InspectingPawn) const override;
    virtual FLastStorageInspectionInfo GetInspectionInfo_Implementation() const override;
    virtual void Inspect_Implementation(APawn* InspectingPawn) override;

protected:
    // Presentation only: override this event for sound, highlighting or a widget animation.
    UFUNCTION(BlueprintImplementableEvent, Category = "LAST STORAGE|Item")
    void ReceiveInspected(APawn* InspectingPawn);

private:
    // Session-only state. Saving, appraisal values and selling are later MVP tasks.
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "LAST STORAGE|Item",
        meta = (AllowPrivateAccess = "true"))
    bool bInspected = false;
};
