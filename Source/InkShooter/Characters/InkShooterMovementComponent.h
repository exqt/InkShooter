#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InkShooterMovementComponent.generated.h"

UCLASS()
class INKSHOOTER_API UInkShooterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Shooter Movement Component", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumTeamInkRatio = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Shooter Movement Component", meta = (ClampMin = "1.0"))
	float TeamInkSpeedMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Shooter Movement Component", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumOtherInkRatio = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Shooter Movement Component", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OtherInkSpeedMultiplier = 0.5f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink Shooter Movement Component")
	float TeamInkRatio = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink Shooter Movement Component")
	float OtherInkRatio = 0.f;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Movement Component")
	bool IsOnTeamInk() const;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Movement Component")
	bool IsOnOtherInk() const;

	virtual float GetMaxSpeed() const override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
};
