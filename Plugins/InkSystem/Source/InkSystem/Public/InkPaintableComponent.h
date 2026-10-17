#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InkSample.h"
#include "InkBrush.h"
#if WITH_EDITOR
#include "Containers/Ticker.h"
#endif
#include "InkPaintableComponent.generated.h"


class AInkPaintableVolume;

struct FInkResamplingContext
{
	UPrimitiveComponent* Primitive;
	ECollisionChannel TraceChannel;
	FIntVector FirstCell;
	TArray<FInkSample> ResultSamples;
};

UCLASS(BlueprintType, ClassGroup = (InkSystem), meta = (BlueprintSpawnableComponent, DisplayName = "Ink Paintable"))
class INKSYSTEM_API UInkPaintableComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region Lifecycle
public:
	UInkPaintableComponent();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#pragma endregion

#pragma region Sampling
public:
	UPROPERTY(EditAnywhere, Category = "Ink Paintable Component", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSize = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Ink Paintable Component", meta = (ToolTip = "Sample upward-facing surfaces with downward traces along world Z."))
	bool bSampleFromAbove = true;

	UPROPERTY(EditAnywhere, Category = "Ink Paintable Component", meta = (ToolTip = "Sample side-facing surfaces from both directions along world X and Y."))
	bool bSampleFromSides = false;

	UPROPERTY(EditAnywhere, Category = "Ink Paintable Component")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UFUNCTION(CallInEditor, Category = "Ink Paintable Component")
	void RebuildSamples();

	void SampleFace(FInkResamplingContext* Context, const FBox& Bounds, const FIntVector& Counts, int32 TraceAxis, bool bFromPositive);
	void SampleSurfacePoint(FInkResamplingContext* Context, const FVector& Start, const FVector& End);

	const TArray<FInkSample>& GetSamples() const { return Samples; }
	void SetSamples(const TArray<FInkSample>& InSamples);
	void ClearSamples();

	UFUNCTION(BlueprintCallable, Category = "Ink Paintable Component")
	UPrimitiveComponent* GetSamplePrimitive();
	
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UPrimitiveComponent> PrimitiveComponent;

private:
#if WITH_EDITOR
	FTSTicker::FDelegateHandle ResampleTickerHandle;
	void OnOwnerMoved(AActor* MovedActor);
	bool ResampleAfterMove(float DeltaTime);
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Paintable Component", meta = (AllowPrivateAccess = "true", ToolTip = "Baked surface samples in world space. Automatically rebuilt after moving, rotating or scaling the owning actor in the editor. Saved with the owning actor when saving the level."))
	TArray<FInkSample> Samples;
#pragma endregion

#pragma region Painting
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ink Paintable Component", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Minimum dot product between the impact normal and a sample normal. 0.9 allows approximately 26 degrees."))
	float PaintNormalSimilarity = 0.9f;

	// Paints only the hit component's samples, filtered by the impact normal.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ink Paintable Component")
	int32 Paint(const FHitResult& HitResult, float Radius, uint8 ColorId);
	int32 PaintWithBrush(const FHitResult& HitResult, float Radius, uint8 ColorId, const FInkBrush& Brush, const FInkRenderSettings& Settings, float RotationDegrees);

private:
	UPROPERTY(Transient, DuplicateTransient)
	TArray<uint8> ColorState;

	static bool IsSampleInPaintArea(const FInkSample& Sample, const FVector& Center, const FVector& Normal, double RadiusSquared, float NormalSimilarity);
	void ResetColorState();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPaint(FVector Center, FVector Normal, float Radius, float NormalSimilarity, uint8 ColorId, FVector2D UV, bool bHasUV, FInkBrush Brush, FInkRenderSettings Settings, float RotationDegrees);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastClearPaint();

	FORCEINLINE uint8 GetSampleColor(const uint32 SampleIndex) const
	{
		// Baked samples can load before their transient color state.
		return ColorState.IsValidIndex(SampleIndex) ? ColorState[SampleIndex] : 0;
	}

	FORCEINLINE void SetSampleColor(const uint32 SampleIndex, const uint8 ColorId)
	{
		ColorState[SampleIndex] = ColorId;
	}
#pragma endregion

#pragma region Rendering
public:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Ink Paintable Component")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Ink Paintable Component")
	TObjectPtr<UTextureRenderTarget2D> NormalRenderTarget;

	UPROPERTY(BlueprintReadWrite, Category = "Ink Paintable Component")
	TObjectPtr<UMaterialInstanceDynamic> DynMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> NormalDynMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> SurfaceDynMaterials;

	bool InitPaintingResources(const FInkRenderSettings& Settings);

private:
	bool FindPaintUV(const FHitResult& HitResult, FVector2D& OutUV);
	bool FindClampedPaintUV(const FHitResult& HitResult, FVector2D& OutUV);
	bool TracePaintSurface(const FVector& Position, const FVector& Normal, FHitResult& OutHit);
	void DrawPaintTexture(FVector2D UV, const FInkBrush& Brush, float RotationDegrees);
	void DrawBrushToRenderTarget(UTextureRenderTarget2D* Target, UMaterialInstanceDynamic* Material, FVector2D UV, float RotationDegrees);

	UPROPERTY(Transient)
	FInkRenderSettings ActiveRenderSettings;
#pragma endregion

#pragma region Volume Registration
public:
	UFUNCTION(BlueprintPure, Category = "Ink Paintable Component")
	AInkPaintableVolume* GetPaintableVolume() const;

private:
	friend class AInkPaintableVolume;

	UPROPERTY(Transient, DuplicateTransient)
	TWeakObjectPtr<AInkPaintableVolume> RegisteredVolume;

	void UnregisterFromVolume();
	void UpdateVolume();
#pragma endregion
};
