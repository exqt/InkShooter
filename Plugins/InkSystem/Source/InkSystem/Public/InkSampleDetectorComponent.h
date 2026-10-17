#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InkSample.h"
#include "InkSampleDetectorComponent.generated.h"

UCLASS(BlueprintType, ClassGroup = (InkSystem), meta = (BlueprintSpawnableComponent, DisplayName = "Ink Sample Detector"))
class INKSYSTEM_API UInkSampleDetectorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInkSampleDetectorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Sample Detector Component", meta = (ClampMin = "1.0", Units = "cm"))
	float DetectionRadius = 75.0f;

	// Returns samples near the owning character's capsule bottom. Call on the game thread.
	UFUNCTION(BlueprintCallable, Category = "Ink Sample Detector Component")
	TArray<FInkSample> DetectSamples() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Sample Detector Component", meta = (ClampMin = "0.0", Units = "cm", ToolTip = "Maximum sample distance from the supporting floor plane. Excludes nearby floors at different heights."))
	float MaxFloorDistance = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Sample Detector Component", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Minimum dot product between the floor normal and sample normal. Excludes walls and ceilings."))
	float FloorNormalSimilarity = 0.9f;

	// Returns zero while airborne or when no floor samples are found.
	UFUNCTION(BlueprintPure, Category = "Ink Sample Detector Component")
	float GetFloorColorRatio(uint8 ColorId) const;
};
