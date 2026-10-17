#include "InkPaintableSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "InkPaintableVolume.h"

DEFINE_LOG_CATEGORY_STATIC(LogInkPaintableSubsystem, Log, All);

FLinearColor UInkPaintableSubsystem::GetTeamColor(uint8 TeamId) const
{
	return TeamId == 0 ? TeamColorA : TeamColorB;
}

void UInkPaintableSubsystem::SetTeamColor(uint8 TeamId, FLinearColor Color)
{
	check(TeamId < 2);
	(TeamId == 0 ? TeamColorA : TeamColorB) = Color;
}

void UInkPaintableSubsystem::RegisterVolume(AInkPaintableVolume* Volume)
{
	if (PaintableVolume == Volume) return;
	if (PaintableVolume.IsValid())
	{
		UE_LOG(LogInkPaintableSubsystem, Warning, TEXT("Only one Ink Paintable Volume is supported per world. Ignoring '%s'; '%s' is already registered."), *Volume->GetPathName(), *PaintableVolume->GetPathName());
		return;
	}

	PaintableVolume = Volume;
	OnVolumeChanged.Broadcast();
}

void UInkPaintableSubsystem::UnregisterVolume(AInkPaintableVolume* Volume)
{
	if (PaintableVolume != Volume) return;

	PaintableVolume.Reset();
	OnVolumeChanged.Broadcast();
}

AInkPaintableVolume* UInkPaintableSubsystem::GetPaintableVolume() const
{
	return PaintableVolume.Get();
}

void UInkPaintableSubsystem::PostInitialize()
{
	Super::PostInitialize();
	if (TActorIterator<AInkPaintableVolume> It(GetWorld()); It)
	{
		RegisterVolume(*It);
	}
}

void UInkPaintableSubsystem::Deinitialize()
{
	OnVolumeChanged.Clear();
	PaintableVolume.Reset();
	Super::Deinitialize();
}
