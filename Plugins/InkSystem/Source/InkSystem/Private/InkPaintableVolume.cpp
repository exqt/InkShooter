#include "InkPaintableVolume.h"

#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "InkPaintableComponent.h"
#include "InkPaintableSubsystem.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogInkPaintableVolume, Log, All);

const FLinearColor SampleDebugColors[] = {FLinearColor::White, FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue};

AInkPaintableVolume::AInkPaintableVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// Refresh the preview in editor worlds before BeginPlay.
	bAllowTickBeforeBeginPlay = true;
	
	GetBrushComponent()->SetHiddenInGame(true);
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetBrushComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetBrushComponent()->SetGenerateOverlapEvents(false);
	GetBrushComponent()->SetCanEverAffectNavigation(false);
}

int32 AInkPaintableVolume::Paint(const FHitResult& HitResult, float Radius, uint8 ColorId)
{
	if (!HasAuthority() || !HitResult.bBlockingHit || !HitResult.GetActor())
	{
		return 0;
	}

	const FInkBrush SelectedBrush = InkBrushes.IsEmpty() ? FInkBrush() : InkBrushes[FMath::RandHelper(InkBrushes.Num())];
	const float RotationDegrees = FMath::FRandRange(0.0f, 360.0f);
	int PaintedCount = 0;
	for (UInkPaintableComponent* Component : PaintableComponents)
	{
		if (Component->GetSamplePrimitive()->Bounds.GetBox().ComputeSquaredDistanceToPoint(HitResult.ImpactPoint) < Radius*Radius)
		{
			PaintedCount += Component->PaintWithBrush(HitResult, Radius, ColorId, SelectedBrush, RenderSettings, RotationDegrees);
		}
	}
	
	return PaintedCount;
}

TArray<FInkSample> AInkPaintableVolume::FindSamplesInRadius(const FVector& WorldPosition, float Radius) const
{
	TArray<FInkSample> FoundSamples;
	const double RadiusSquared = FMath::Square(Radius);
	for (UInkPaintableComponent* Component : PaintableComponents)
	{
		if (Component->GetSamplePrimitive()->Bounds.GetBox().ComputeSquaredDistanceToPoint(WorldPosition) >= RadiusSquared)
		{
			continue;
		}
		const TArray<FInkSample>& ComponentSamples = Component->GetSamples();
		for (const FInkSample& Sample : ComponentSamples)
		{
			if (FVector::DistSquared(WorldPosition, Sample.Position) < RadiusSquared)
			{
				FoundSamples.Add(Sample);
			}
		}
	}
	return FoundSamples;
}

float AInkPaintableVolume::GetSampleColorRatioInRadius(const FVector& WorldPosition, float Radius, const FVector& SurfaceNormal, float MaxSurfaceDistance, float NormalSimilarity, uint8 ColorId) const
{
	const double RadiusSquared = FMath::Square(Radius);
	const FVector Normal = SurfaceNormal.GetSafeNormal();
	int32 TotalCount = 0;
	int32 MatchingCount = 0;
	for (UInkPaintableComponent* Component : PaintableComponents)
	{
		if (Component->GetSamplePrimitive()->Bounds.GetBox().ComputeSquaredDistanceToPoint(WorldPosition) >= RadiusSquared)
		{
			continue;
		}
		const TArray<FInkSample>& ComponentSamples = Component->GetSamples();
		for (int32 SampleIndex = 0; SampleIndex < ComponentSamples.Num(); ++SampleIndex)
		{
			const FInkSample& Sample = ComponentSamples[SampleIndex];
			if (FMath::Abs(FVector::DotProduct(Sample.Position - WorldPosition, Normal)) > MaxSurfaceDistance || !UInkPaintableComponent::IsSampleInPaintArea(Sample, WorldPosition, Normal, RadiusSquared, NormalSimilarity))
			{
				continue;
			}
			++TotalCount;
			MatchingCount += Component->GetSampleColor(SampleIndex) == ColorId ? 1 : 0;
		}
	}
	return TotalCount > 0 ? static_cast<float>(MatchingCount) / TotalCount : 0.f;
}

void AInkPaintableVolume::ClearPaint()
{
	if (!HasAuthority())
	{
		return;
	}
	for (UInkPaintableComponent* Component : PaintableComponents)
	{
		Component->MulticastClearPaint();
	}
}

int64 AInkPaintableVolume::GetTotalSampleCount() const
{
	int64 Total = 0;
	for (const UInkPaintableComponent* Component : PaintableComponents)
	{
		for (const FInkSample& Sample : Component->GetSamples())
		{
			Total += EncompassesPoint(Sample.Position) ? 1 : 0;
		}
	}
	return Total;
}


TArray<int64> AInkPaintableVolume::GetSampleColorCounts() const
{
	TArray<int64> Counts;
	Counts.Init(0, 4);
	for (const UInkPaintableComponent* Component : PaintableComponents)
	{
		for (int32 SampleIndex = 0; SampleIndex < Component->GetSamples().Num(); ++SampleIndex)
		{
			if (EncompassesPoint(Component->GetSamples()[SampleIndex].Position))
			{
				++Counts[Component->GetSampleColor(SampleIndex)];
			}
		}
	}
	return Counts;
}

void AInkPaintableVolume::LogStatistics()
{
	UpdateComponentList();
	const TArray<int64> Counts = GetSampleColorCounts();
	UE_LOG(LogInkPaintableVolume, Log, TEXT("[%s] Statistics: Components=%d, Samples=%lld, Unpainted=%lld, Red=%lld, Green=%lld, Blue=%lld"), *GetPathName(), PaintableComponents.Num(), GetTotalSampleCount(), Counts[0], Counts[1], Counts[2], Counts[3]);
}

void AInkPaintableVolume::UpdateComponentList()
{
	PaintableComponents.Reset();
	
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UInkPaintableComponent*> Components;
		It->GetComponents(Components);
		for (UInkPaintableComponent* Component : Components)
		{
			if (GetWorld()->IsGameWorld())
			{
				if (Component->IsRegistered() && Component->GetPaintableVolume() == this)
				{
					PaintableComponents.AddUnique(Component);
				}
				continue;
			}
			if (Component->IsRegistered() && Component->GetSamples().ContainsByPredicate([this](const FInkSample& Sample) { return EncompassesPoint(Sample.Position); }))
			{
				PaintableComponents.Add(Component);
			}
		}
	}
}

void AInkPaintableVolume::RegisterComponent(UInkPaintableComponent* Component)
{
	Component->UnregisterFromVolume();
	PaintableComponents.AddUnique(Component);
	Component->RegisteredVolume = this;
}

void AInkPaintableVolume::UnregisterComponent(UInkPaintableComponent* Component)
{
	PaintableComponents.Remove(Component);
	if (Component->RegisteredVolume == this)
	{
		Component->RegisteredVolume.Reset();
	}
}

#pragma region Debug Visualization
void AInkPaintableVolume::SetShowSamples(bool bShow)
{
	SetActorTickEnabled(bShow);
	if (bShowSamples == bShow)
	{
		if (bShow)
		{
			DrawSamples();
		}
		return;
	}

	if (bShow)
	{
		SamplePreview = NewObject<ULineBatchComponent>(GetWorld());
		SamplePreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SamplePreview->SetCanEverAffectNavigation(false);
		SamplePreview->SetHiddenInGame(false);
		SamplePreview->bCalculateAccurateBounds = true;
		SamplePreview->PrimaryComponentTick.bCanEverTick = false;
		SamplePreview->RegisterComponentWithWorld(GetWorld());
	}
	else
	{
		SamplePreview->DestroyComponent();
		SamplePreview = nullptr;
	}
	bShowSamples = bShow;
	if (bShow)
	{
		DrawSamples();
	}
}

void AInkPaintableVolume::DrawSamples()
{
	UpdateComponentList();
	SamplePreview->Flush();
	for (const UInkPaintableComponent* Component : PaintableComponents)
	{
		const TArray<FInkSample>& ComponentSamples = Component->GetSamples();
		for (int32 SampleIndex = 0; SampleIndex < ComponentSamples.Num(); ++SampleIndex)
		{
			const FInkSample& Sample = ComponentSamples[SampleIndex];
			if (EncompassesPoint(Sample.Position))
			{
				SamplePreview->DrawPoint(Sample.Position + Sample.Normal.GetSafeNormal() * 2.0, SampleDebugColors[Component->GetSampleColor(SampleIndex)], 6.0f, SDPG_World);
			}
		}
	}
	SamplePreview->UpdateBounds();
	SamplePreview->MarkRenderTransformDirty();
}

void AInkPaintableVolume::ApplySampleVisualizationSetting()
{
	if (const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Ink.ShowSamples")))
	{
		SetShowSamples(Variable->GetInt() != 0);
	}
}

void AInkPaintableVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	// Actors may register before their world subsystem is initialized.
	if (UInkPaintableSubsystem* Subsystem = GetWorld()->GetSubsystem<UInkPaintableSubsystem>())
	{
		Subsystem->RegisterVolume(this);
	}
	ApplySampleVisualizationSetting();
}

void AInkPaintableVolume::PostUnregisterAllComponents()
{
	if (UInkPaintableSubsystem* Subsystem = GetWorld()->GetSubsystem<UInkPaintableSubsystem>())
	{
		Subsystem->UnregisterVolume(this);
	}
	SetShowSamples(false);
	Super::PostUnregisterAllComponents();
}

void AInkPaintableVolume::BeginPlay()
{
	Super::BeginPlay();
	UpdateComponentList();
	ApplySampleVisualizationSetting();
}

void AInkPaintableVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bShowSamples)
	{
		DrawSamples();
	}
}

bool AInkPaintableVolume::ShouldTickIfViewportsOnly() const
{
	return bShowSamples;
}

#pragma endregion
