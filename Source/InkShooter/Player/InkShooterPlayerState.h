// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "InkShooterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class INKSHOOTER_API AInkShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

#pragma region Replication
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* Other) override;
#pragma endregion

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category = "Ink Shooter Player State", meta = (ClampMin = "0", ClampMax = "1"))
	uint8 TeamId = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Player State")
	bool bHost = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Player State")
	bool bLoaded = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Player State")
	double RespawnAt = 0;
};
