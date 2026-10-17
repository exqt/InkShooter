// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "InkShooterGameModeBase.generated.h"

/**
 * 
 */
UCLASS()
class INKSHOOTER_API AInkShooterGameModeBase : public AGameMode
{
	GENERATED_BODY()

public:
	AInkShooterGameModeBase();
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Ink Shooter Game Mode Base")
	void StartBattle(APlayerController* RequestingPlayer);

	void TryReturnToLobby(AController* ExitingPlayer = nullptr);

protected:
	virtual void HandleMatchHasStarted() override;
	virtual bool ReadyToEndMatch_Implementation() override;
	virtual void HandleMatchHasEnded() override;

	UPROPERTY(EditDefaultsOnly, Category = "Ink Shooter Game Mode Base", meta = (ClampMin = "1.0", Units = "s"))
	float MatchDuration = 180.f;
};
