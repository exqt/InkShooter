#include "InkShooterMovementComponent.h"

#include "InkShooterCharacter.h"
#include "InkSampleDetectorComponent.h"
#include "../Player/InkShooterPlayerState.h"

bool UInkShooterMovementComponent::IsOnTeamInk() const
{
	return IsMovingOnGround() && TeamInkRatio > 0.f && TeamInkRatio >= MinimumTeamInkRatio;
}

bool UInkShooterMovementComponent::IsOnOtherInk() const
{
	return IsMovingOnGround() && OtherInkRatio > 0.f && OtherInkRatio >= MinimumOtherInkRatio;
}

float UInkShooterMovementComponent::GetMaxSpeed() const
{
	const float MaxSpeed = Super::GetMaxSpeed();
	if (!IsMovingOnGround())
	{
		return MaxSpeed;
	}
	if (IsOnTeamInk())
	{
		return MaxSpeed * (CastChecked<AInkShooterCharacter>(CharacterOwner)->bIsSquidForm ? TeamInkSpeedMultiplier : 1.f);
	}
	return MaxSpeed * (IsOnOtherInk() ? OtherInkSpeedMultiplier : 1.f);
}

void UInkShooterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
	TeamInkRatio = 0.f;
	OtherInkRatio = 0.f;
	if (!IsMovingOnGround())
	{
		return;
	}
	const AInkShooterCharacter* Character = CastChecked<AInkShooterCharacter>(CharacterOwner);
	const AInkShooterPlayerState* State = Character->GetPlayerState<AInkShooterPlayerState>();
	// PlayerState can arrive after the pawn on clients.
	if (!State || State->TeamId > 1)
	{
		return;
	}
	TeamInkRatio = Character->InkSampleDetector->GetFloorColorRatio(State->TeamId + 1);
	const uint8 OtherColorId = State->TeamId == 0 ? 2 : 1;
	OtherInkRatio = Character->InkSampleDetector->GetFloorColorRatio(OtherColorId);
}
