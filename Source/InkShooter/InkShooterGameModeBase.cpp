// Fill out your copyright notice in the Description page of Project Settings.


#include "InkShooterGameModeBase.h"

#include "InkShooterGameState.h"
#include "Player/InkShooterPlayerState.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Characters/InkShooterCharacter.h"
#include "Player/InkShooterPlayerController.h"
#include "Weapons/InkProjectile.h"
#include "InkPaintableSubsystem.h"
#include "InkPaintableVolume.h"

AInkShooterGameModeBase::AInkShooterGameModeBase()
{
	bDelayedStart = true;
	GameStateClass = AInkShooterGameState::StaticClass();
}

void AInkShooterGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (GetNumPlayers() >= 6)
	{
		ErrorMessage = TEXT("Room is full.");
	}
}

void AInkShooterGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	
	AInkShooterPlayerState* NewState = NewPlayer->GetPlayerState<AInkShooterPlayerState>();
	int32 TeamCounts[2] = {0, 0};
	bool bHasHost = false;
	
	for (APlayerState* Player : GameState->PlayerArray)
	{
		const AInkShooterPlayerState* State = CastChecked<AInkShooterPlayerState>(Player);
		if (State != NewState)
		{
			++TeamCounts[State->TeamId];
			bHasHost |= State->bHost;
		}
	}
	
	NewState->TeamId = TeamCounts[0] <= TeamCounts[1] ? 0 : 1;
	NewState->bHost = !bHasHost;
	NewState->SetPlayerName(FGuid::NewGuid().ToString().Left(8));
	NewState->ForceNetUpdate();
}

void AInkShooterGameModeBase::Logout(AController* Exiting)
{
	const bool bWasHost = Exiting->GetPlayerState<AInkShooterPlayerState>()->bHost;
	Super::Logout(Exiting);
	
	if (bWasHost)
	{
		for (APlayerState* Player : GameState->PlayerArray)
		{
			if (Player != Exiting->PlayerState)
			{
				AInkShooterPlayerState* State = CastChecked<AInkShooterPlayerState>(Player);
				State->bHost = true;
				State->ForceNetUpdate();
				break;
			}
		}
	}
	TryReturnToLobby(Exiting);
}

AActor* AInkShooterGameModeBase::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
	const FName TeamTag = Player->GetPlayerState<AInkShooterPlayerState>()->TeamId == 0 ? TEXT("Red") : TEXT("Green");
	APawn* DefaultPawn = GetDefaultPawnClassForController(Player)->GetDefaultObject<APawn>();
	TArray<APlayerStart*> TeamStarts;
	TArray<APlayerStart*> AvailableStarts;
	
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		
		if (Start->PlayerStartTag != TeamTag)
		{
			continue;
		}
		
		TeamStarts.Add(Start);
		
		if (!GetWorld()->EncroachingBlockingGeometry(DefaultPawn, Start->GetActorLocation(), Start->GetActorRotation()))
		{
			AvailableStarts.Add(Start);
		}
	}
	
	if (!AvailableStarts.IsEmpty())
	{
		return AvailableStarts[FMath::RandHelper(AvailableStarts.Num())];
	}
	
	if (!TeamStarts.IsEmpty())
	{
		return TeamStarts[FMath::RandHelper(TeamStarts.Num())];
	}
	
	UE_LOG(LogTemp, Error, TEXT("No PlayerStart with PlayerStartTag '%s'."), *TeamTag.ToString());
	
	return nullptr;
}

void AInkShooterGameModeBase::StartBattle(APlayerController* RequestingPlayer)
{
	if (GetMatchState() != MatchState::WaitingToStart || !RequestingPlayer->GetPlayerState<AInkShooterPlayerState>()->bHost)
	{
		return;
	}
	
	StartMatch();
}

void AInkShooterGameModeBase::HandleMatchHasStarted()
{
	AInkShooterGameState* State = GetGameState<AInkShooterGameState>();
	State->MatchEndTime = State->GetServerWorldTimeSeconds() + MatchDuration;
	State->MatchResult = FInkShooterMatchResult();
	State->ForceNetUpdate();
	Super::HandleMatchHasStarted();
}

bool AInkShooterGameModeBase::ReadyToEndMatch_Implementation()
{
	return GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0;
}

void AInkShooterGameModeBase::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();
	AInkShooterGameState* State = GetGameState<AInkShooterGameState>();
	AInkPaintableVolume* Volume = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume();
	// A missing scoring volume must not crash the server at match end.
	if (ensureMsgf(Volume, TEXT("InkShooter requires an Ink Paintable Volume for match scoring.")))
	{
		const TArray<int64> Counts = Volume->GetSampleColorCounts();
		State->MatchResult.TeamASamples = Counts[1];
		State->MatchResult.TeamBSamples = Counts[2];
		State->MatchResult.TotalSamples = Volume->GetTotalSampleCount();
		State->MatchResult.WinningTeam = Counts[1] == Counts[2] ? -1 : (Counts[1] > Counts[2] ? 0 : 1);
	}
	State->MatchResult.bReady = true;
	State->ForceNetUpdate();

	for (TActorIterator<AInkProjectile> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	for (TActorIterator<AInkShooterCharacter> It(GetWorld()); It; ++It)
	{
		It->StopMatch();
	}
}

void AInkShooterGameModeBase::TryReturnToLobby(AController* ExitingPlayer)
{
	if (GetWorld()->bIsTearingDown || GetMatchState() != MatchState::WaitingPostMatch)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get() != ExitingPlayer && !CastChecked<AInkShooterPlayerController>(It->Get())->bResultConfirmed)
		{
			return;
		}
	}

	for (TActorIterator<AInkShooterCharacter> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
	AInkPaintableVolume* Volume = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume();
	if (Volume)
	{
		Volume->ClearPaint();
	}
	AInkShooterGameState* State = GetGameState<AInkShooterGameState>();
	State->MatchResult = FInkShooterMatchResult();
	State->MatchEndTime = 0.0;
	State->ForceNetUpdate();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AInkShooterPlayerController* Player = CastChecked<AInkShooterPlayerController>(It->Get());
		Player->bResultConfirmed = false;
		Player->ForceNetUpdate();
	}
	SetMatchState(MatchState::WaitingToStart);
}
