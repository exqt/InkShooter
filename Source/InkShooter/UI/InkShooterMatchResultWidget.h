#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InkShooterMatchResultWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS(Abstract)
class INKSHOOTER_API UInkShooterMatchResultWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WinnerText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TeamAPercent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TeamBPercent;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> TeamABar;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> TeamBBar;
};
