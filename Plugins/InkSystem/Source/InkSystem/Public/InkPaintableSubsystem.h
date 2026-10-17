#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "InkPaintableSubsystem.generated.h"

class AInkPaintableVolume;

DECLARE_MULTICAST_DELEGATE(FInkVolumeChanged);

// Owns the world's paintable volume and ink colors.
UCLASS()
class INKSYSTEM_API UInkPaintableSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Ink Paintable Subsystem")
	FLinearColor GetTeamColor(uint8 TeamId) const;

	UFUNCTION(BlueprintCallable, Category = "Ink Paintable Subsystem")
	void SetTeamColor(uint8 TeamId, FLinearColor Color);

	void RegisterVolume(AInkPaintableVolume* Volume);
	void UnregisterVolume(AInkPaintableVolume* Volume);
	FInkVolumeChanged OnVolumeChanged;

	UFUNCTION(BlueprintPure, Category = "Ink Paintable Subsystem")
	AInkPaintableVolume* GetPaintableVolume() const;

	// Discovers a volume whose actor registration preceded subsystem initialization.
	virtual void PostInitialize() override;

	// Releases the volume and its change listeners during world teardown.
	virtual void Deinitialize() override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Paintable Subsystem", meta = (AllowPrivateAccess = "true"))
	FLinearColor TeamColorA = FLinearColor(1.f, 0.f, 0.332325f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ink Paintable Subsystem", meta = (AllowPrivateAccess = "true"))
	FLinearColor TeamColorB = FLinearColor(0.016329458f, 0.201706305f, 0.963541687f);

	UPROPERTY(Transient)
	TWeakObjectPtr<AInkPaintableVolume> PaintableVolume;
};
