// Fill out your copyright notice in the Description page of Project Settings.


#include "InkShooterPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "../InkShooterGameModeBase.h"
#include "../InkShooterGameState.h"
#include "Blueprint/UserWidget.h"
#include "Net/UnrealNetwork.h"
#include "Framework/Application/SlateApplication.h"

void AInkShooterPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController())
	{
		return;
	}
	
	LobbyWidget = CreateWidget<UUserWidget>(this, LobbyWidgetClass);
	BattleWidget = CreateWidget<UUserWidget>(this, BattleWidgetClass);
	ResultWidget = CreateWidget<UUserWidget>(this, ResultWidgetClass);
	ShowMatchWidget(LobbyWidget, false);
}

void AInkShooterPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!IsLocalController())
	{
		return;
	}
	const AInkShooterGameState* State = GetWorld()->GetGameState<AInkShooterGameState>();
	// GameState can arrive after the local controller on a joining client.
	if (!State || !LobbyWidget)
	{
		return;
	}
	const FName CurrentMatchState = State->GetMatchState();
	if (CurrentMatchState == DisplayedMatchState && bResultConfirmed == bDisplayedResultConfirmed)
	{
		return;
	}
	// Match state and the final result can arrive in different replication updates.
	if (CurrentMatchState == MatchState::WaitingPostMatch && !State->MatchResult.bReady)
	{
		return;
	}
	if (State->IsMatchInProgress())
	{
		ShowMatchWidget(BattleWidget, true);
	}
	else if (CurrentMatchState == MatchState::WaitingPostMatch && !bResultConfirmed)
	{
		ShowMatchWidget(ResultWidget, false);
	}
	else
	{
		ShowMatchWidget(LobbyWidget, false);
	}
	DisplayedMatchState = CurrentMatchState;
	bDisplayedResultConfirmed = bResultConfirmed;
}

void AInkShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AInkShooterPlayerController, bResultConfirmed, COND_OwnerOnly);
}

void AInkShooterPlayerController::ShowMatchWidget(UUserWidget* Widget, bool bGameplayInput)
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
	}
	ActiveWidget = Widget;
	Widget->SetIsFocusable(!bGameplayInput);
	Widget->AddToViewport();
	bShowMouseCursor = !bGameplayInput;
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
	SetIgnoreMoveInput(!bGameplayInput);
	SetIgnoreLookInput(!bGameplayInput);
	if (bGameplayInput)
	{
		SetInputMode(FInputModeGameOnly());
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(Widget->TakeWidget());
		SetInputMode(InputMode);
		FlushPressedKeys();
		FSlateApplication::Get().ReleaseAllPointerCapture();
	}
}

void AInkShooterPlayerController::ServerStartBattle_Implementation()
{
	GetWorld()->GetAuthGameMode<AInkShooterGameModeBase>()->StartBattle(this);
}

void AInkShooterPlayerController::ServerConfirmMatchResult_Implementation()
{
	AInkShooterGameModeBase* GameMode = GetWorld()->GetAuthGameMode<AInkShooterGameModeBase>();
	if (GameMode->GetMatchState() != MatchState::WaitingPostMatch || bResultConfirmed)
	{
		return;
	}
	bResultConfirmed = true;
	ForceNetUpdate();
	GameMode->TryReturnToLobby();
}

void AInkShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!IsLocalController())
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	for (UInputMappingContext* MappingContext : DefaultMappingContexts)
	{
		Subsystem->AddMappingContext(MappingContext, 0);
	}
}
