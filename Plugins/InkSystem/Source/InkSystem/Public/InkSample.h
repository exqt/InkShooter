#pragma once

#include "CoreMinimal.h"
#include "InkSample.generated.h"

USTRUCT(BlueprintType)
struct INKSYSTEM_API FInkSample
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Sample", meta = (ToolTip = "Surface sample position in world space."))
	FVector Position = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Sample", meta = (ToolTip = "Surface sample normal in world space."))
	FVector Normal = FVector::UpVector;
};
