#include "InkPaintableComponent.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "InkPaintableVolume.h"
#include "InkPaintableSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogInkPaintable, Log, All);

#pragma region Lifecycle
UInkPaintableComponent::UInkPaintableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInkPaintableComponent::OnRegister()
{
	Super::OnRegister();
#if WITH_EDITOR
	if (GetWorld()->WorldType == EWorldType::Editor)
	{
		GEngine->OnActorMoved().AddUObject(this, &UInkPaintableComponent::OnOwnerMoved);
	}
#endif
	if (HasBegunPlay())
	{
		GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->OnVolumeChanged.AddUObject(this, &UInkPaintableComponent::UpdateVolume);
		UpdateVolume();
	}
}

void UInkPaintableComponent::OnUnregister()
{
#if WITH_EDITOR
	if (GEngine)
	{
		GEngine->OnActorMoved().RemoveAll(this);
	}
	FTSTicker::RemoveTicker(ResampleTickerHandle);
	ResampleTickerHandle.Reset();
#endif
	// Unregistration can also run after the world subsystem has been torn down.
	if (UWorld* World = GetWorld())
	{
		if (UInkPaintableSubsystem* Subsystem = World->GetSubsystem<UInkPaintableSubsystem>())
		{
			Subsystem->OnVolumeChanged.RemoveAll(this);
		}
	}
	UnregisterFromVolume();
	Super::OnUnregister();
}

void UInkPaintableComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetColorState();
	GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->OnVolumeChanged.AddUObject(this, &UInkPaintableComponent::UpdateVolume);
	UpdateVolume();
}

void UInkPaintableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->OnVolumeChanged.RemoveAll(this);
	UnregisterFromVolume();
	Super::EndPlay(EndPlayReason);
}
#pragma endregion

#pragma region Sampling
#if WITH_EDITOR
void UInkPaintableComponent::OnOwnerMoved(AActor* MovedActor)
{
	if (MovedActor != GetOwner()) return;

	// Wait for movement and collision updates to settle before tracing the surface.
	FTSTicker::RemoveTicker(ResampleTickerHandle);
	ResampleTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UInkPaintableComponent::ResampleAfterMove), 0.2f);
}

bool UInkPaintableComponent::ResampleAfterMove(float DeltaTime)
{
	ResampleTickerHandle.Reset();
	RebuildSamples();
	return false;
}
#endif

void UInkPaintableComponent::RebuildSamples()
{
	AActor* Owner = GetOwner();
	UE_LOG(LogInkPaintable, Log, TEXT("[%s] Sampling started: CellSize=%.2f, FromAbove=%d, FromSides=%d"), *GetPathName(), CellSize, bSampleFromAbove, bSampleFromSides);

	if (!Owner->IsRootComponentStatic() || (!bSampleFromAbove && !bSampleFromSides))
	{
		UE_LOG(LogInkPaintable, Log, TEXT("[%s] Sampling skipped: RootStatic=%d, FromAbove=%d, FromSides=%d. Samples cleared."), *GetPathName(), Owner->IsRootComponentStatic(), bSampleFromAbove, bSampleFromSides);
		ClearSamples();
		return;
	}

	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	Owner->GetComponents(Primitives);
	Primitives.RemoveAll([&](const UPrimitiveComponent* Primitive)
	{
		return 
			!Primitive->IsRegistered() || 
			Primitive->GetMobility() != EComponentMobility::Static || 
			!Primitive->IsQueryCollisionEnabled() || 
			Primitive->GetCollisionResponseToChannel(TraceChannel) != ECR_Block;
	});
	
	if (Primitives.Num() == 0)
	{
		UE_LOG(LogInkPaintable, Log, TEXT("[%s] Sampling skipped: no registered static primitive blocks TraceChannel=%d. Samples cleared."), *GetPathName(), static_cast<int32>(TraceChannel.GetValue()));
		ClearSamples();
		return;
	}

	FBox Bounds(ForceInit);
	for (const UPrimitiveComponent* Primitive : Primitives)
	{
		Bounds += Primitive->Bounds.GetBox();
	}
	if (!Bounds.IsValid)
	{
		UE_LOG(LogInkPaintable, Warning, TEXT("[%s] Sampling skipped: invalid bounds. Samples cleared."), *GetPathName());
		ClearSamples();
		return;
	}

	// Use one world-space lattice instead of resizing a grid to each actor's bounds.
	const FIntVector FirstCell(FMath::CeilToInt((Bounds.Min.X - UE_KINDA_SMALL_NUMBER) / CellSize - 0.5), FMath::CeilToInt((Bounds.Min.Y - UE_KINDA_SMALL_NUMBER) / CellSize - 0.5), FMath::CeilToInt((Bounds.Min.Z - UE_KINDA_SMALL_NUMBER) / CellSize - 0.5));
	const FIntVector LastCell(FMath::FloorToInt((Bounds.Max.X + UE_KINDA_SMALL_NUMBER) / CellSize - 0.5), FMath::FloorToInt((Bounds.Max.Y + UE_KINDA_SMALL_NUMBER) / CellSize - 0.5), FMath::FloorToInt((Bounds.Max.Z + UE_KINDA_SMALL_NUMBER) / CellSize - 0.5));
	const int64 CountX = FMath::Max(0, LastCell.X - FirstCell.X + 1);
	const int64 CountY = FMath::Max(0, LastCell.Y - FirstCell.Y + 1);
	const int64 CountZ = bSampleFromSides ? FMath::Max(0, LastCell.Z - FirstCell.Z + 1) : 1;
	const int64 RayCount = (bSampleFromAbove ? CountX * CountY : 0) + (bSampleFromSides ? 2 * (CountX * CountZ + CountY * CountZ) : 0);
	
	FInkResamplingContext Context;
	
	Context.Primitive = GetSamplePrimitive();
	Context.TraceChannel = TraceChannel;
	Context.FirstCell = FirstCell;
	UE_LOG(LogInkPaintable, Log, TEXT("[%s] Sampling primitive=%s, EligiblePrimitives=%d, Grid=(%lld,%lld,%lld), PlannedTraces=%lld"), *GetPathName(), *Context.Primitive->GetPathName(), Primitives.Num(), CountX, CountY, CountZ, RayCount);
	
	const FIntVector Counts(static_cast<int32>(CountX), static_cast<int32>(CountY), static_cast<int32>(CountZ));
	Context.ResultSamples.Reserve(static_cast<int32>(RayCount));

	if (bSampleFromAbove)
	{
		SampleFace(&Context, Bounds, Counts, 2, true);
	}
	if (bSampleFromSides)
	{
		SampleFace(&Context, Bounds, Counts, 0, true);
		SampleFace(&Context, Bounds, Counts, 0, false);
		SampleFace(&Context, Bounds, Counts, 1, true);
		SampleFace(&Context, Bounds, Counts, 1, false);
	}

	SetSamples(Context.ResultSamples);
	UE_LOG(LogInkPaintable, Log, TEXT("[%s] Sampling completed: Attempts=%lld, Samples=%d, Misses=%lld"), *GetPathName(), RayCount, Samples.Num(), RayCount - Samples.Num());
}

void UInkPaintableComponent::SampleFace(FInkResamplingContext* Context, const FBox& Bounds, const FIntVector& Counts, const int32 TraceAxis, const bool bFromPositive)
{
	const int32 AxisU = (TraceAxis + 1) % 3;
	const int32 AxisV = (TraceAxis + 2) % 3;
		
	for (int32 VIndex = 0; VIndex < Counts[AxisV]; ++VIndex)
	{
		for (int32 UIndex = 0; UIndex < Counts[AxisU]; ++UIndex)
		{
			FVector Start = Bounds.GetCenter();
				
			Start[AxisU] = (Context->FirstCell[AxisU] + UIndex + 0.5) * CellSize;
			Start[AxisV] = (Context->FirstCell[AxisV] + VIndex + 0.5) * CellSize;
			Start[TraceAxis] = bFromPositive ? Bounds.Max[TraceAxis] + 1.0 : Bounds.Min[TraceAxis] - 1.0;
			FVector End = Start;
			End[TraceAxis] = bFromPositive ? Bounds.Min[TraceAxis] - 1.0 : Bounds.Max[TraceAxis] + 1.0;
				
			SampleSurfacePoint(Context, Start, End);
		}
	}
}

void UInkPaintableComponent::SampleSurfacePoint(FInkResamplingContext* Context, const FVector& Start, const FVector& End)
{
	FHitResult Hit;
	FCollisionQueryParams SurfaceParams(SCENE_QUERY_STAT(InkSampleSurface), true);
	if (Context->Primitive->GetWorld()->LineTraceSingleByChannel(Hit, Start, End, Context->TraceChannel, SurfaceParams))
	{
		if (Hit.bStartPenetrating || Hit.GetComponent() != Context->Primitive) return;

		const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
		FCollisionQueryParams OcclusionParams(SCENE_QUERY_STAT(InkSampleOcclusion), false);
		OcclusionParams.AddIgnoredComponent(Context->Primitive);
		
		// A line trace starting inside another collider can miss it. Check the sample's exposed side too.
		if (Context->Primitive->GetWorld()->OverlapBlockingTestByChannel(Hit.ImpactPoint + Normal * 0.5, FQuat::Identity, Context->TraceChannel, FCollisionShape::MakeSphere(0.25f), OcclusionParams)) return;

		FInkSample& Sample = Context->ResultSamples.AddDefaulted_GetRef();
		Sample.Position = Hit.ImpactPoint;
		Sample.Normal = Normal;
	}
}

void UInkPaintableComponent::SetSamples(const TArray<FInkSample>& InSamples)
{
#if WITH_EDITOR
	if (const UWorld* World = GetWorld(); World && World->WorldType == EWorldType::Editor)
	{
		// Modify marks the owning level or external actor package dirty before changing baked data.
		Modify();
	}
#endif
	Samples = InSamples;
	ResetColorState();
	if (HasBegunPlay())
	{
		UpdateVolume();
	}
}

void UInkPaintableComponent::ClearSamples()
{
	SetSamples({});
}


UPrimitiveComponent* UInkPaintableComponent::GetSamplePrimitive()
{
	if (PrimitiveComponent != nullptr) return PrimitiveComponent;
	
	TInlineComponentArray<UPrimitiveComponent*> Primitives;
	GetOwner()->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives)
	{
		if (Primitive->IsRegistered() && Primitive->GetMobility() == EComponentMobility::Static && Primitive->IsQueryCollisionEnabled() && Primitive->GetCollisionResponseToChannel(TraceChannel) == ECR_Block)
		{
			PrimitiveComponent = Primitive;
			return Primitive;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region Painting
int32 UInkPaintableComponent::Paint(const FHitResult& HitResult, float Radius, uint8 ColorId)
{
	// A component outside all paintable volumes has no painting settings.
	if (AInkPaintableVolume* Volume = RegisteredVolume.Get(); Volume && GetOwner()->HasAuthority())
	{
		const FInkBrush SelectedBrush = Volume->InkBrushes.IsEmpty() ? FInkBrush() : Volume->InkBrushes[FMath::RandHelper(Volume->InkBrushes.Num())];
		const float RotationDegrees = FMath::FRandRange(0.0f, 360.0f);
		return PaintWithBrush(HitResult, Radius, ColorId, SelectedBrush, Volume->RenderSettings, RotationDegrees);
	}
	return 0;
}

int32 UInkPaintableComponent::PaintWithBrush(const FHitResult& HitResult, float Radius, uint8 ColorId, const FInkBrush& Brush, const FInkRenderSettings& Settings, float RotationDegrees)
{
	AActor* Owner = GetOwner();
	
	if (!Owner->HasAuthority() || !HitResult.bBlockingHit)
	{
		return 0;
	}
	
	const double RadiusSquared = FMath::Square(Radius);
	const FVector Normal = HitResult.ImpactNormal.GetSafeNormal();
	const float NormalSimilarity = FMath::Clamp(PaintNormalSimilarity, 0.0f, 1.0f);
	int32 PaintedCount = 0;
	
	for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
	{
		if (GetSampleColor(SampleIndex) != ColorId && IsSampleInPaintArea(Samples[SampleIndex], HitResult.ImpactPoint, Normal, RadiusSquared, NormalSimilarity))
		{
			++PaintedCount;
		}
	}

	FVector2D UV = FVector2D::ZeroVector;
	const bool bHasUV = FindPaintUV(HitResult, UV);
	
	MulticastPaint(HitResult.ImpactPoint, Normal, Radius, NormalSimilarity, ColorId, UV, bHasUV, Brush, Settings, RotationDegrees);
	
	return PaintedCount;
}

bool UInkPaintableComponent::IsSampleInPaintArea(const FInkSample& Sample, const FVector& Center, const FVector& Normal, double RadiusSquared, float NormalSimilarity)
{
	return FVector::DistSquared(Center, Sample.Position) < RadiusSquared && FVector::DotProduct(Normal, Sample.Normal.GetSafeNormal()) >= NormalSimilarity;
}

void UInkPaintableComponent::ResetColorState()
{
	ColorState.Init(0, Samples.Num());
}

void UInkPaintableComponent::MulticastClearPaint_Implementation()
{
	ResetColorState();
	// Painting resources are created lazily, so untouched surfaces have no targets.
	if (RenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, FLinearColor::Black);
	}
	if (NormalRenderTarget)
	{
		UKismetRenderingLibrary::ClearRenderTarget2D(this, NormalRenderTarget, FLinearColor(0.5f, 0.5f, 1.f, 1.f));
	}
}

void UInkPaintableComponent::MulticastPaint_Implementation(FVector Center, FVector Normal, float Radius, float NormalSimilarity, uint8 ColorId, FVector2D UV, bool bHasUV, FInkBrush Brush, FInkRenderSettings Settings, float RotationDegrees)
{
	const double RadiusSquared = FMath::Square(Radius);
	for (int32 SampleIndex = 0; SampleIndex < Samples.Num(); ++SampleIndex)
	{
		if (GetSampleColor(SampleIndex) != ColorId && IsSampleInPaintArea(Samples[SampleIndex], Center, Normal, RadiusSquared, NormalSimilarity))
		{
			SetSampleColor(SampleIndex, ColorId);
		}
	}
	if (bHasUV && InitPaintingResources(Settings))
	{
		const FLinearColor InkColors[] = {FLinearColor::Black, FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue};
		DynMaterial->SetVectorParameterValue(TEXT("Color"), InkColors[ColorId]);
		DrawPaintTexture(UV, Brush, RotationDegrees);
	}
}
#pragma endregion

#pragma region Rendering
bool UInkPaintableComponent::InitPaintingResources(const FInkRenderSettings& Settings)
{
	UMeshComponent* Mesh = Cast<UMeshComponent>(GetSamplePrimitive());
	if (GetNetMode() == NM_DedicatedServer || !Mesh || !Settings.BrushMaterial || !Settings.NormalBrushMaterial)
	{
		return false;
	}

	const int32 Resolution = FMath::Max(Settings.RenderTargetResolution, 1);
	if (DynMaterial && ActiveRenderSettings.BrushMaterial == Settings.BrushMaterial && ActiveRenderSettings.NormalBrushMaterial == Settings.NormalBrushMaterial && RenderTarget->SizeX == Resolution && RenderTarget->SizeY == Resolution) return true;

	if (!RenderTarget)
	{
		RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Resolution, Resolution, RTF_RGBA8, FLinearColor::Black, true);
		NormalRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(this, Resolution, Resolution, RTF_RGBA8, FLinearColor(0.5f, 0.5f, 1.0f, 1.0f), true);
		NormalRenderTarget->bForceLinearGamma = true;
		NormalRenderTarget->UpdateResourceImmediate(true);
	}
	else if (RenderTarget->SizeX != Resolution || RenderTarget->SizeY != Resolution)
	{
		RenderTarget->ResizeTarget(Resolution, Resolution);
		NormalRenderTarget->ResizeTarget(Resolution, Resolution);
	}
	
	ActiveRenderSettings = Settings;
	
	DynMaterial = UMaterialInstanceDynamic::Create(Settings.BrushMaterial, this);
	DynMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::Red);
	
	NormalDynMaterial = UMaterialInstanceDynamic::Create(Settings.NormalBrushMaterial, this);
	
	SurfaceDynMaterials.Reset();
	for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetNumMaterials(); ++MaterialIndex)
	{
		// Empty material slots cannot create a dynamic instance.
		if (UMaterialInstanceDynamic* SurfaceMaterial = Mesh->CreateDynamicMaterialInstance(MaterialIndex))
		{
			SurfaceMaterial->SetTextureParameterValue(TEXT("InkTextureObject"), RenderTarget);
			SurfaceMaterial->SetTextureParameterValue(TEXT("InkNormalTexture"), NormalRenderTarget);
			SurfaceDynMaterials.Add(SurfaceMaterial);
		}
	}
	
	if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Mesh))
	{
		StaticMesh->SetForceDisableNanite(true);
	}
	
	return true;
}

bool UInkPaintableComponent::FindPaintUV(const FHitResult& HitResult, FVector2D& OutUV)
{
	if (HitResult.GetComponent() != GetSamplePrimitive())
	{
		return FindClampedPaintUV(HitResult, OutUV);
	}

	FHitResult SurfaceHit;
	if (!TracePaintSurface(HitResult.ImpactPoint, HitResult.ImpactNormal, SurfaceHit))
	{
		return false;
	}

	return UGameplayStatics::FindCollisionUV(SurfaceHit, 0, OutUV);
}

bool UInkPaintableComponent::FindClampedPaintUV(const FHitResult& HitResult, FVector2D& OutUV)
{
	UPrimitiveComponent* SampleComponent = GetSamplePrimitive();
	const FTransform& ComponentTransform = SampleComponent->GetComponentTransform();
	const FVector LocalImpactPoint = ComponentTransform.InverseTransformPosition(HitResult.ImpactPoint);
	const FBox LocalBounds = SampleComponent->GetLocalBounds().GetBox();
	const FVector ClampedPoint = LocalBounds.GetClosestPointTo(LocalImpactPoint);
	const FVector Start = ComponentTransform.TransformPosition(ClampedPoint + (ClampedPoint - LocalImpactPoint) * 0.001f);

	FHitResult SurfaceHit;
	if (!TracePaintSurface(Start, HitResult.ImpactNormal, SurfaceHit))
	{
		return false;
	}

	// FindCollisionUV uses Location. Extrapolate the hit face's UVs at the original brush center.
	SurfaceHit.Location = HitResult.ImpactPoint;
	return UGameplayStatics::FindCollisionUV(SurfaceHit, 0, OutUV);
}

bool UInkPaintableComponent::TracePaintSurface(const FVector& Position, const FVector& Normal, FHitResult& OutHit)
{
	const FVector TraceOffset = Normal * 25.0f;
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = true;
	return GetSamplePrimitive()->LineTraceComponent(OutHit, Position + TraceOffset, Position - TraceOffset, QueryParams);
}

void UInkPaintableComponent::DrawPaintTexture(FVector2D UV, const FInkBrush& Brush, float RotationDegrees)
{
	UTexture* MaskTexture = Brush.MaskTexture.Get();
	UTexture* NormalTexture = Brush.NormalTexture.Get();
	
	if (!MaskTexture) ActiveRenderSettings.BrushMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("InkMask")), MaskTexture);
	if (!NormalTexture) ActiveRenderSettings.NormalBrushMaterial->GetTextureParameterValue(FMaterialParameterInfo(TEXT("InkNormal")), NormalTexture);
	
	DynMaterial->SetTextureParameterValue(TEXT("InkMask"), MaskTexture);
	NormalDynMaterial->SetTextureParameterValue(TEXT("InkMask"), MaskTexture);
	NormalDynMaterial->SetTextureParameterValue(TEXT("InkNormal"), NormalTexture);
	NormalDynMaterial->SetScalarParameterValue(TEXT("BrushRotationDegrees"), RotationDegrees);
	
	DrawBrushToRenderTarget(RenderTarget, DynMaterial, UV, RotationDegrees);
	DrawBrushToRenderTarget(NormalRenderTarget, NormalDynMaterial, UV, RotationDegrees);
}

void UInkPaintableComponent::DrawBrushToRenderTarget(UTextureRenderTarget2D* Target, UMaterialInstanceDynamic* Material, FVector2D UV, float RotationDegrees)
{
	if (!Target || !Material)
	{
		return;
	}

	UCanvas* Canvas;
	FVector2D RTSize;
	FDrawToRenderTargetContext Context;

	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(this, Target, Canvas, RTSize, Context);

	// Rendering is unavailable on dedicated servers, so Begin can return no Canvas.
	if (!Canvas)
	{
		return;
	}

	const FVector2D BrushSize = RTSize * 0.125f; // Preserve the original 64 / 512 UV footprint.
	const FVector2D Position = UV * RTSize - BrushSize * 0.5f;
	Canvas->K2_DrawMaterial(Material, Position, BrushSize, FVector2D::ZeroVector, FVector2D::UnitVector, RotationDegrees, FVector2D(0.5f, 0.5f));

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(this, Context);
	// Canvas resolves mip 0; regenerate the mip chain without clearing accumulated paint.
	Target->UpdateResourceImmediate(false);
}
#pragma endregion

#pragma region Volume Registration
AInkPaintableVolume* UInkPaintableComponent::GetPaintableVolume() const
{
	return RegisteredVolume.Get();
}

void UInkPaintableComponent::UpdateVolume()
{
	AInkPaintableVolume* Volume = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume();
	if (Volume && Samples.ContainsByPredicate([Volume](const FInkSample& Sample) { return Volume->EncompassesPoint(Sample.Position); }))
	{
		if (RegisteredVolume != Volume)
		{
			Volume->RegisterComponent(this);
		}
		InitPaintingResources(Volume->RenderSettings);
		return;
	}
	UnregisterFromVolume();
}

void UInkPaintableComponent::UnregisterFromVolume()
{
	if (AInkPaintableVolume* Volume = RegisteredVolume.Get())
	{
		Volume->UnregisterComponent(this);
	}
	RegisteredVolume.Reset();
}
#pragma endregion
