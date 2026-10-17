// Fill out your copyright notice in the Description page of Project Settings.


#include "InkShooterPlayerState.h"

#include "Net/UnrealNetwork.h"

void AInkShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AInkShooterPlayerState, TeamId);
	DOREPLIFETIME(AInkShooterPlayerState, bHost);
	DOREPLIFETIME(AInkShooterPlayerState, bLoaded);
	DOREPLIFETIME(AInkShooterPlayerState, RespawnAt);
}

void AInkShooterPlayerState::CopyProperties(APlayerState* Other)
{
	Super::CopyProperties(Other);
	
	AInkShooterPlayerState* State = CastChecked<AInkShooterPlayerState>(Other);
	State->TeamId = TeamId;
	State->bHost = bHost;
	State->bLoaded = false;
	State->RespawnAt = 0;
}
