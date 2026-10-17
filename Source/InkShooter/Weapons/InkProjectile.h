#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InkProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class INKSHOOTER_API AInkProjectile : public AActor
{
	GENERATED_BODY()

public:
	AInkProjectile();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Projectile")
	TObjectPtr<UStaticMeshComponent> ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Projectile")
	TObjectPtr<UNiagaraComponent> Trail;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Projectile", meta = (ClampMin = "0.1", Units = "s"))
	float FlightLifetime = 3.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ink Projectile")
	FLinearColor InkColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Projectile", meta = (ClampMin = "1.0", Units = "cm"))
	float PaintRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Projectile", meta = (ClampMin = "1"))
	int32 Damage = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Ink Projectile")
	uint8 PaintColorId = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ink Projectile")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	// Runs on the server after painting the impact area.
	UFUNCTION(BlueprintImplementableEvent, Category = "Ink Projectile")
	void OnInkImpact(const FHitResult& Hit);

	UPROPERTY(ReplicatedUsing = OnRep_Impact, BlueprintReadOnly, Category = "Ink Projectile")
	FHitResult ImpactHit;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool bImpactEffectsPlayed = false;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_Impact();
};
