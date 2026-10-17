#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Volume.h"
#include "InkSample.h"
#include "InkBrush.h"
#include "InkPaintableVolume.generated.h"

class UInkPaintableComponent;
class ULineBatchComponent;

UCLASS(meta = (DisplayName = "Ink Paintable Volume"))
class INKSYSTEM_API AInkPaintableVolume : public AVolume
{
	GENERATED_BODY()

#pragma region Lifecycle
public:
	AInkPaintableVolume();

private:
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	virtual void BeginPlay() override;

	// Draws managed samples only while debug visualization is enabled.
	virtual void Tick(float DeltaTime) override;

	// Allows debug rendering to update in editor viewports without starting gameplay.
	virtual bool ShouldTickIfViewportsOnly() const override;
#pragma endregion

#pragma region Painting
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Paintable Volume", meta = (TitleProperty = "MaskTexture"))
	TArray<FInkBrush> InkBrushes;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Paintable Volume")
	FInkRenderSettings RenderSettings;

	UFUNCTION(BlueprintCallable, Category = "Ink Paintable Volume")
	int32 Paint(const FHitResult& HitResult, float Radius, uint8 ColorId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ink Paintable Volume")
	void ClearPaint();
#pragma endregion

#pragma region Query
public:
	UFUNCTION(BlueprintCallable, Category = "Ink Paintable Volume")
	TArray<FInkSample> FindSamplesInRadius(const FVector& WorldPosition, float Radius) const;

	// Counts all nearby surface samples, including unpainted and opposing colors.
	UFUNCTION(BlueprintPure, Category = "Ink Paintable Volume")
	float GetSampleColorRatioInRadius(const FVector& WorldPosition, float Radius, const FVector& SurfaceNormal, float MaxSurfaceDistance, float NormalSimilarity, uint8 ColorId) const;
#pragma endregion

#pragma region Statistics
public:
	UFUNCTION(BlueprintPure, Category = "Ink Paintable Volume")
	int64 GetTotalSampleCount() const;

	UFUNCTION(BlueprintPure, Category = "Ink Paintable Volume")
	TArray<int64> GetSampleColorCounts() const;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Ink Paintable Volume")
	void LogStatistics();
#pragma endregion

#pragma region Component Registration
public:
	void UpdateComponentList();

private:
	friend class UInkPaintableComponent;
	
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<UInkPaintableComponent>> PaintableComponents;
	
	void RegisterComponent(UInkPaintableComponent* Component);
	void UnregisterComponent(UInkPaintableComponent* Component);
#pragma endregion

#pragma region Debug Visualization
public:
	UFUNCTION(BlueprintCallable, Category = "Ink Paintable Volume")
	void SetShowSamples(bool bShow);

private:
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<ULineBatchComponent> SamplePreview;
	bool bShowSamples = false;
	void DrawSamples();
	void ApplySampleVisualizationSetting();
#pragma endregion
};
