#include "InkPaintableVolume.h"
#include "InkPaintableSubsystem.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogInkConsoleCommands, Log, All);

#pragma region Debug Console
namespace
{
	void SetInkSamplesVisible(IConsoleVariable* Variable)
	{
		const bool bShow = Variable->GetInt() != 0;
		for (TObjectIterator<UInkPaintableSubsystem> It; It; ++It)
		{
			if (AInkPaintableVolume* Volume = It->GetPaintableVolume(); Volume && Volume->HasActorRegisteredAllComponents() && !Volume->IsActorBeingDestroyed())
			{
				Volume->SetShowSamples(bShow);
			}
		}
	}

	void LogInkStatistics(UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogInkConsoleCommands, Warning, TEXT("Ink.LogStatistics: no world was supplied."));
			return;
		}
		if (AInkPaintableVolume* Volume = World->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume())
		{
			Volume->LogStatistics();
		}
		else
		{
			UE_LOG(LogInkConsoleCommands, Warning, TEXT("Ink.LogStatistics: no Ink Paintable Volume found in world '%s'."), *World->GetName());
		}
	}

	TAutoConsoleVariable<int32> ShowSamplesVariable(TEXT("Ink.ShowSamples"), 0, TEXT("Show sampled ink surface points in all worlds. 0: hidden, 1: visible. Retained across PIE sessions and map changes."), FConsoleVariableDelegate::CreateStatic(&SetInkSamplesVisible));
	FAutoConsoleCommandWithWorld LogStatisticsCommand(TEXT("Ink.LogStatistics"), TEXT("Log component counts, sample totals, and color counts for the Ink Paintable Volume in the current world."), FConsoleCommandWithWorldDelegate::CreateStatic(&LogInkStatistics));
}
#pragma endregion
