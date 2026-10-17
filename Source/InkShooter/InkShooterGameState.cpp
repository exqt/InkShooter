#include "InkShooterGameState.h"

#include "Player/InkShooterPlayerState.h"
#include "Net/UnrealNetwork.h"

FText AInkShooterGameState::GetTeamPlayerList(uint8 TeamId) const
{
	FString PlayerList;
	for (APlayerState* Player : PlayerArray)
	{
		const AInkShooterPlayerState* State = CastChecked<AInkShooterPlayerState>(Player);
		if (State->TeamId == TeamId)
		{
			PlayerList += State->GetPlayerName() + (State->bHost ? TEXT("  [HOST]") : TEXT("")) + TEXT("\n");
		}
	}
	return FText::FromString(PlayerList);
}

void AInkShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInkShooterGameState, MatchEndTime);
	DOREPLIFETIME(AInkShooterGameState, MatchResult);
}

double AInkShooterGameState::GetRemainingMatchTime() const
{
	return IsMatchInProgress() ? FMath::Max(0.0, MatchEndTime - GetServerWorldTimeSeconds()) : 0.0;
}

float AInkShooterGameState::GetTeamCoverage(uint8 TeamId) const
{
	if (TeamId > 1 || MatchResult.TotalSamples == 0)
	{
		return 0.f;
	}
	return static_cast<float>(TeamId == 0 ? MatchResult.TeamASamples : MatchResult.TeamBSamples) / MatchResult.TotalSamples;
}

void AInkShooterGameState::HandleMatchIsWaitingToStart()
{
	Super::HandleMatchIsWaitingToStart();
	ElapsedTime = 0;
}
