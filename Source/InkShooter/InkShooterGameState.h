#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "InkShooterGameState.generated.h"

USTRUCT(BlueprintType)
struct FInkShooterMatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ink Shooter Match Result")
	int64 TeamASamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ink Shooter Match Result")
	int64 TeamBSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ink Shooter Match Result")
	int64 TotalSamples = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ink Shooter Match Result")
	int32 WinningTeam = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Ink Shooter Match Result")
	bool bReady = false;
};

UCLASS()
class INKSHOOTER_API AInkShooterGameState : public AGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Game State")
	FText GetTeamPlayerList(uint8 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Game State")
	double GetRemainingMatchTime() const;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Game State")
	float GetTeamCoverage(uint8 TeamId) const;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Game State")
	int32 GetWinningTeam() const { return MatchResult.WinningTeam; }

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Game State")
	double MatchEndTime = 0.0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Game State")
	FInkShooterMatchResult MatchResult;

protected:
	virtual void HandleMatchIsWaitingToStart() override;
};
