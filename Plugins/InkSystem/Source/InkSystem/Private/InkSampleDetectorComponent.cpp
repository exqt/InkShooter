#include "InkSampleDetectorComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InkPaintableSubsystem.h"
#include "InkPaintableVolume.h"

UInkSampleDetectorComponent::UInkSampleDetectorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<FInkSample> UInkSampleDetectorComponent::DetectSamples() const
{
	// This spawnable component can be placed on actors that do not have a character capsule.
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return {};
	}
	// Preview worlds and worlds being initialized may not have this subsystem.
	UInkPaintableSubsystem* Subsystem = GetWorld()->GetSubsystem<UInkPaintableSubsystem>();
	if (!Subsystem)
	{
		return {};
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector DetectionPosition = Capsule->GetComponentLocation() - Capsule->GetUpVector() * Capsule->GetScaledCapsuleHalfHeight();
	if (AInkPaintableVolume* Volume = Subsystem->GetPaintableVolume(); Volume && Volume->EncompassesPoint(DetectionPosition))
	{
		return Volume->FindSamplesInRadius(DetectionPosition, DetectionRadius);
	}
	return {};
}

float UInkSampleDetectorComponent::GetFloorColorRatio(uint8 ColorId) const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return 0.f;
	}
	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement->IsMovingOnGround())
	{
		return 0.f;
	}
	FFindFloorResult Floor;
	Movement->FindFloor(Character->GetCapsuleComponent()->GetComponentLocation(), Floor, false);
	if (!Floor.IsWalkableFloor())
	{
		return 0.f;
	}
	AInkPaintableVolume* Volume = GetWorld()->GetSubsystem<UInkPaintableSubsystem>()->GetPaintableVolume();
	if (!Volume || !Volume->EncompassesPoint(Floor.HitResult.ImpactPoint))
	{
		return 0.f;
	}
	return Volume->GetSampleColorRatioInRadius(Floor.HitResult.ImpactPoint, DetectionRadius, Floor.HitResult.ImpactNormal, MaxFloorDistance, FloorNormalSimilarity, ColorId);
}
