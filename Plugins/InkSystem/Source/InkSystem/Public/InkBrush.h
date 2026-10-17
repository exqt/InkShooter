#pragma once

#include "CoreMinimal.h"
#include "InkBrush.generated.h"

class UTexture2D;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct INKSYSTEM_API FInkBrush
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Brush")
	TObjectPtr<UTexture2D> MaskTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Brush")
	TObjectPtr<UTexture2D> NormalTexture = nullptr;
};

USTRUCT(BlueprintType)
struct INKSYSTEM_API FInkRenderSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Render Settings", meta = (ClampMin = "1", UIMin = "64", UIMax = "4096", ToolTip = "Square resolution shared by the ink and normal render targets."))
	int32 RenderTargetResolution = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Render Settings")
	TObjectPtr<UMaterialInterface> BrushMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Render Settings")
	TObjectPtr<UMaterialInterface> NormalBrushMaterial = nullptr;

};
