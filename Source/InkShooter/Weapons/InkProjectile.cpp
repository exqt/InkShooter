#include "InkProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/World.h"
#include "InkPaintableSubsystem.h"
#include "InkPaintableVolume.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "../Characters/InkShooterCharacter.h"
#include "../InkShooterGameState.h"

AInkProjectile::AInkProjectile()
{
	bReplicates = true;
	SetNetUpdateFrequency(30.f);
	SetMinNetUpdateFrequency(15.f);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(3.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetGenerateOverlapEvents(false);
	Collision->OnComponentHit.AddDynamic(this, &AInkProjectile::HandleHit);

	ProjectileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMesh"));
	ProjectileMesh->SetupAttachment(Collision);
	ProjectileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMesh->SetCastShadow(false);

	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Collision);
	Trail->SetAutoActivate(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = 2400.f;
	ProjectileMovement->MaxSpeed = 2400.f;
	ProjectileMovement->Velocity = FVector::ForwardVector;
	ProjectileMovement->ProjectileGravityScale = 0.25f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bForceSubStepping = true;
}

void AInkProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	Collision->IgnoreActorWhenMoving(GetOwner(), true);
	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
		InstigatorPawn->MoveIgnoreActorAdd(this);
	}
}

void AInkProjectile::BeginPlay()
{
	Super::BeginPlay();
	InkColor = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetTeamColor(PaintColorId - 1);

	if (ImpactHit.bBlockingHit)
	{
		OnRep_Impact();
		return;
	}
	
	if (!HasAuthority())
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	else
	{
		SetLifeSpan(FlightLifetime);
	}
	
	if (GetNetMode() != NM_DedicatedServer)
	{
		ProjectileMesh->SetVectorParameterValueOnMaterials(TEXT("InkColor"), FVector(InkColor.R, InkColor.G, InkColor.B));
		Trail->SetVariableLinearColor(TEXT("User.InkColor"), InkColor);
		Trail->Activate();
	}
}

void AInkProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* InstigatorPawn = GetInstigator())
	{
		InstigatorPawn->MoveIgnoreActorRemove(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AInkProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInkProjectile, ImpactHit);
	DOREPLIFETIME_CONDITION(AInkProjectile, PaintColorId, COND_InitialOnly);
}

void AInkProjectile::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || ImpactHit.bBlockingHit || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}
	if (GetWorld()->GetGameState<AInkShooterGameState>()->GetRemainingMatchTime() <= 0.0)
	{
		Destroy();
		return;
	}
	
	ImpactHit = Hit;
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	
	SetActorEnableCollision(false);
	OnRep_Impact();
	
	if (AInkShooterCharacter* Character = Cast<AInkShooterCharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(Character, Damage, GetInstigatorController(), this, nullptr);
	}
	
	GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume()->Paint(Hit, PaintRadius, PaintColorId);
	
	OnInkImpact(Hit);
	ForceNetUpdate();
	SetLifeSpan(0.35f);
}

void AInkProjectile::OnRep_Impact()
{
	if (bImpactEffectsPlayed)
	{
		return;
	}
	
	bImpactEffectsPlayed = true;
	// Initial impact replication can arrive before BeginPlay.
	InkColor = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetTeamColor(PaintColorId - 1);
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ProjectileMesh->SetVisibility(false);
	Trail->DeactivateImmediate();
	
	SetActorEnableCollision(false);
	
	if (ImpactEffect && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, ImpactHit.ImpactPoint + ImpactHit.ImpactNormal, ImpactHit.ImpactNormal.Rotation(), FVector::OneVector, true, false);
		if (Effect)
		{
			Effect->SetVariableLinearColor(TEXT("User.InkColor"), InkColor);
			Effect->Activate();
		}
	}
}
