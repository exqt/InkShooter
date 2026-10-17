// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "InkShooterCharacter.generated.h"

class UInputAction;
class UStaticMeshComponent;
class AInkProjectile;
class UAnimMontage;
class UNiagaraSystem;
class USoundBase;
class UInkSampleDetectorComponent;
struct FInputActionValue;

UCLASS()
class INKSHOOTER_API AInkShooterCharacter : public ACharacter
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* SquidMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Shooter Character", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> GunMesh;

public:
	AInkShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Shooter Character")
	TObjectPtr<UInkSampleDetectorComponent> InkSampleDetector;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(NetMulticast, Reliable)
	void StopMatch();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	FName LeftHandGripSocketName = TEXT("LeftHandGrip");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TSubclassOf<AInkProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	FName MuzzleSocketName = TEXT("Muzzle");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character", meta = (ClampMin = "100", Units = "cm"))
	float AimTraceDistance = 10000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TEnumAsByte<ECollisionChannel> AimTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TObjectPtr<UNiagaraSystem> MuzzleEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TObjectPtr<UAnimMontage> HitMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character", meta = (ClampMin = "0"))
	float FireSoundVolume = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character", meta = (ClampMin = "0.1"))
	float FireSoundPitch = 1.f;

public:
	UFUNCTION(BlueprintPure, Category = "Ink Shooter Character")
	FTransform GetLeftHandGripTransform() const;

	UFUNCTION(BlueprintPure, Category = "Ink Shooter Character")
	float GetHandIKWeight() const;

#pragma region State
	UPROPERTY(ReplicatedUsing = OnRep_SquidForm)
	bool bIsSquidForm = false;

	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Ink Shooter Character")
	int32 Health = 3;

	void SetSquidForm(bool bNewSquidForm);

	UFUNCTION(Server, Reliable)
	void ServerSetSquidForm(bool bNewSquidForm);

	UFUNCTION()
	void OnRep_SquidForm();

	UFUNCTION()
	void OnRep_Health();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayHitMontage();

	void Respawn();
	FTimerHandle RespawnTimer;
#pragma endregion

#pragma region Input
protected:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SquidFormAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Shooter Character", meta = (ClampMin = "0.05", Units = "s"))
	float FireInterval = 0.2f;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartFire(const FInputActionValue& InputActionValue);
	void StopFire(const FInputActionValue& InputActionValue);
	void FireShot();
	void PlayFireEffects(const FVector& MuzzleLocation, const FVector& ShotDirection, uint8 TeamId);

	UFUNCTION(Server, Reliable)
	void ServerFireShot(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal ShotDirection);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireEffects(FVector_NetQuantize MuzzleLocation, FVector_NetQuantizeNormal ShotDirection, uint8 TeamId);

	FTimerHandle FireTimer;
	bool bFireInputHeld = false;
	double LastFireTime = -1000.0;
	void StartSquidForm(const FInputActionValue& InputActionValue);
	void StopSquidForm(const FInputActionValue& InputActionValue);

public:
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSquidFormStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	void DoSquidFormEnd();

	UFUNCTION(BlueprintCallable, Category = "Ink Shooter Character")
	void DoFireStart();

	UFUNCTION(BlueprintCallable, Category = "Ink Shooter Character")
	void DoFireEnd();
#pragma endregion

public:
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
