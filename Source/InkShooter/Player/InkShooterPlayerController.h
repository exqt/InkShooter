// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InkShooterPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 * 
 */
UCLASS()
class INKSHOOTER_API AInkShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void PlayerTick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ink Shooter Player Controller")
	void ServerStartBattle();

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Ink Shooter Player Controller")
	void ServerConfirmMatchResult();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Ink Shooter Player Controller")
	bool bResultConfirmed = false;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Ink Shooter Player Controller")
	TSubclassOf<UUserWidget> LobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ink Shooter Player Controller")
	TSubclassOf<UUserWidget> BattleWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Ink Shooter Player Controller")
	TSubclassOf<UUserWidget> ResultWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LobbyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> BattleWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ResultWidget;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveWidget;

	FName DisplayedMatchState;
	bool bDisplayedResultConfirmed = false;
	void ShowMatchWidget(UUserWidget* Widget, bool bGameplayInput);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Player Controller")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	virtual void SetupInputComponent() override;
};
