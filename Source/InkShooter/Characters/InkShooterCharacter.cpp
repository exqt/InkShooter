// Fill out your copyright notice in the Description page of Project Settings.


#include "InkShooterCharacter.h"
#include "InkShooterMovementComponent.h"
#include "InkSampleDetectorComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Net/UnrealNetwork.h"
#include "../Weapons/InkProjectile.h"
#include "../Player/InkShooterPlayerState.h"
#include "../InkShooterGameState.h"
#include "InkPaintableSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"


// Sets default values
AInkShooterCharacter::AInkShooterCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer.SetDefaultSubobjectClass<UInkShooterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Face the controller's yaw while keeping pitch and roll independent.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	InkSampleDetector = CreateDefaultSubobject<UInkSampleDetectorComponent>(TEXT("InkSampleDetector"));

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	SquidMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SquidMesh"));
	SquidMesh->SetupAttachment(RootComponent);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GetMesh(), TEXT("WeaponSocket"));
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FTransform AInkShooterCharacter::GetLeftHandGripTransform() const
{
	FTransform GripTransform = GunMesh->GetSocketTransform(LeftHandGripSocketName, RTS_Component) * GunMesh->GetRelativeTransform() * GetMesh()->GetSocketTransform(GunMesh->GetAttachSocketName(), RTS_ParentBoneSpace);
	GripTransform.SetScale3D(FVector::OneVector);
	return GripTransform;
}

float AInkShooterCharacter::GetHandIKWeight() const
{
	return !bIsSquidForm && GunMesh->DoesSocketExist(LeftHandGripSocketName) ? 1.f : 0.f;
}

void AInkShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AInkShooterCharacter, bIsSquidForm);
	DOREPLIFETIME(AInkShooterCharacter, Health);
}

void AInkShooterCharacter::BeginPlay()
{
	Super::BeginPlay();
	OnRep_SquidForm();
}

void AInkShooterCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FireTimer);
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	Super::EndPlay(EndPlayReason);
}

void AInkShooterCharacter::StopMatch_Implementation()
{
	DoFireEnd();
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

float AInkShooterCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || Health <= 0 || DamageAmount <= 0.f || GetWorld()->GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0)
	{
		return 0.f;
	}

	const float AppliedDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (AppliedDamage <= 0.f)
	{
		return 0.f;
	}

	const int32 PreviousHealth = Health;
	Health = FMath::Max(0, Health - FMath::CeilToInt(AppliedDamage));
	OnRep_Health();
	ForceNetUpdate();
	if (Health == 0)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AInkShooterCharacter::Respawn, 3.f, false);
	}
	else
	{
		MulticastPlayHitMontage();
	}
	return PreviousHealth - Health;
}

void AInkShooterCharacter::MulticastPlayHitMontage_Implementation()
{
	if (GetNetMode() != NM_DedicatedServer && Health > 0 && HitMontage)
	{
		PlayAnimMontage(HitMontage);
	}
}

void AInkShooterCharacter::SetSquidForm(bool bNewSquidForm)
{
	if (Health <= 0)
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerSetSquidForm(bNewSquidForm);
		return;
	}

	bIsSquidForm = bNewSquidForm;
	OnRep_SquidForm();
}

void AInkShooterCharacter::ServerSetSquidForm_Implementation(bool bNewSquidForm)
{
	if (Health <= 0)
	{
		return;
	}
	bIsSquidForm = bNewSquidForm;
	OnRep_SquidForm();
}

void AInkShooterCharacter::OnRep_SquidForm()
{
	SquidMesh->SetVisibility(bIsSquidForm);
	GetMesh()->SetVisibility(!bIsSquidForm);
	GunMesh->SetVisibility(!bIsSquidForm);
}

void AInkShooterCharacter::OnRep_Health()
{
	if (Health > 0)
	{
		return;
	}

	DoFireEnd();
	StopJumping();
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
}

void AInkShooterCharacter::Respawn()
{
	if (GetWorld()->GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0)
	{
		return;
	}
	AController* PlayerController = GetController();
	// The player may have disconnected while waiting to respawn.
	if (!IsValid(PlayerController))
	{
		Destroy();
		return;
	}

	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	PlayerController->UnPossess();
	Destroy();
	GameMode->RestartPlayer(PlayerController);
}

void AInkShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AInkShooterCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AInkShooterCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AInkShooterCharacter::Look);

		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Started, this, &AInkShooterCharacter::StartFire);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AInkShooterCharacter::StopFire);
		EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Canceled, this, &AInkShooterCharacter::StopFire);
		EnhancedInputComponent->BindAction(SquidFormAction, ETriggerEvent::Started, this, &AInkShooterCharacter::StartSquidForm);
		EnhancedInputComponent->BindAction(SquidFormAction, ETriggerEvent::Completed, this, &AInkShooterCharacter::StopSquidForm);
		EnhancedInputComponent->BindAction(SquidFormAction, ETriggerEvent::Canceled, this, &AInkShooterCharacter::StopSquidForm);
	}
}

void AInkShooterCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AInkShooterCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AInkShooterCharacter::StartFire(const FInputActionValue& InputActionValue)
{
	DoFireStart();
}

void AInkShooterCharacter::StopFire(const FInputActionValue& InputActionValue)
{
	DoFireEnd();
}

void AInkShooterCharacter::DoFireStart()
{
	if (Health <= 0 || !IsLocallyControlled() || bFireInputHeld)
	{
		return;
	}
	bFireInputHeld = true;
	const float Interval = FMath::Max(FireInterval, 0.05f);
	const float Delay = FMath::Max(0.f, Interval - static_cast<float>(GetWorld()->GetTimeSeconds() - LastFireTime));
	GetWorldTimerManager().SetTimer(FireTimer, this, &AInkShooterCharacter::FireShot, Interval, true, Delay > 0.f ? Delay : Interval);
	if (Delay <= 0.f)
	{
		FireShot();
	}
}

void AInkShooterCharacter::DoFireEnd()
{
	if (!bFireInputHeld)
	{
		return;
	}
	bFireInputHeld = false;
	GetWorldTimerManager().ClearTimer(FireTimer);
}

void AInkShooterCharacter::FireShot()
{
	if (Health <= 0 || !IsLocallyControlled() || GetWorld()->GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0)
	{
		GetWorldTimerManager().ClearTimer(FireTimer);
		return;
	}
	const AInkShooterPlayerState* State = GetPlayerState<AInkShooterPlayerState>();
	if (bIsSquidForm || !ProjectileClass || !GunMesh->DoesSocketExist(MuzzleSocketName) || !State || State->TeamId > 1)
	{
		return;
	}
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}
	int32 Width, Height;
	PlayerController->GetViewportSize(Width, Height);
	FVector ViewOrigin;
	FVector AimDirection;
	if (!PlayerController->DeprojectScreenPositionToWorld(Width * 0.5f, Height * 0.5f, ViewOrigin, AimDirection))
	{
		return;
	}
	LastFireTime = GetWorld()->GetTimeSeconds();
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InkAim), false, this);
	const FVector TraceEnd = ViewOrigin + AimDirection * AimTraceDistance;
	FHitResult AimHit;
	GetWorld()->LineTraceSingleByChannel(AimHit, ViewOrigin, TraceEnd, AimTraceChannel, QueryParams);
	const FVector AimTarget = AimHit.bBlockingHit ? AimHit.ImpactPoint : TraceEnd;
	FVector MuzzleLocation = GunMesh->GetSocketLocation(MuzzleSocketName);
	FHitResult MuzzleHit;
	const float ProjectileRadius = ProjectileClass.GetDefaultObject()->Collision->GetScaledSphereRadius();
	if (GetWorld()->SweepSingleByChannel(MuzzleHit, GunMesh->GetComponentLocation(), MuzzleLocation, FQuat::Identity, AimTraceChannel, FCollisionShape::MakeSphere(ProjectileRadius), QueryParams))
	{
		MuzzleLocation = MuzzleHit.Location + MuzzleHit.ImpactNormal;
	}
	FVector ShotDirection = (AimTarget - MuzzleLocation).GetSafeNormal();
	if (FVector::DotProduct(ShotDirection, AimDirection) <= 0.f)
	{
		ShotDirection = AimDirection;
	}
	PlayFireEffects(GunMesh->GetSocketLocation(MuzzleSocketName), ShotDirection, State->TeamId);
	ServerFireShot(MuzzleLocation, ShotDirection);
}

void AInkShooterCharacter::ServerFireShot_Implementation(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal ShotDirection)
{
	if (Health <= 0 || GetWorld()->GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0)
	{
		return;
	}
	const AInkShooterPlayerState* State = GetPlayerState<AInkShooterPlayerState>();
	if (!State || State->TeamId > 1) return;

	const FTransform SpawnTransform(ShotDirection.Rotation(), SpawnLocation);
	if (AInkProjectile* Projectile = GetWorld()->SpawnActorDeferred<AInkProjectile>(ProjectileClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Projectile->PaintColorId = State->TeamId + 1;
		Projectile->FinishSpawning(SpawnTransform);
		MulticastFireEffects(GunMesh->GetSocketLocation(MuzzleSocketName), ShotDirection, State->TeamId);
	}
}

void AInkShooterCharacter::MulticastFireEffects_Implementation(FVector_NetQuantize MuzzleLocation, FVector_NetQuantizeNormal ShotDirection, uint8 TeamId)
{
	if (!IsLocallyControlled())
	{
		PlayFireEffects(MuzzleLocation, ShotDirection, TeamId);
	}
}

void AInkShooterCharacter::PlayFireEffects(const FVector& MuzzleLocation, const FVector& ShotDirection, uint8 TeamId)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (FireMontage)
	{
		GetMesh()->GetAnimInstance()->Montage_Play(FireMontage);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation, FRotator::ZeroRotator, FireSoundVolume, FireSoundPitch);
	}
	if (MuzzleEffect)
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleEffect, MuzzleLocation, ShotDirection.Rotation(), FVector::OneVector, true, false);
		if (Effect)
		{
			Effect->SetVariableLinearColor(TEXT("User.InkColor"), GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetTeamColor(TeamId));
			Effect->Activate();
		}
	}
}

void AInkShooterCharacter::StartSquidForm(const FInputActionValue& InputActionValue)
{
	DoSquidFormStart();
}

void AInkShooterCharacter::StopSquidForm(const FInputActionValue& InputActionValue)
{
	DoSquidFormEnd();
}

void AInkShooterCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AInkShooterCharacter::DoLook(float Yaw, float Pitch)
{
	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AInkShooterCharacter::DoJumpStart()
{
	Jump();
}

void AInkShooterCharacter::DoJumpEnd()
{
	StopJumping();
}

void AInkShooterCharacter::DoSquidFormStart()
{
	SetSquidForm(true);
}

void AInkShooterCharacter::DoSquidFormEnd()
{
	SetSquidForm(false);
}

