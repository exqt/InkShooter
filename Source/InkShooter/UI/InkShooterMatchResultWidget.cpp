#include "InkShooterMatchResultWidget.h"

#include "../InkShooterGameState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

void UInkShooterMatchResultWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const AInkShooterGameState* State = GetWorld()->GetGameState<AInkShooterGameState>();
	const float TeamACoverage = State->GetTeamCoverage(0);
	const float TeamBCoverage = State->GetTeamCoverage(1);
	
	FNumberFormattingOptions PercentFormat;
	PercentFormat.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
	TeamAPercent->SetText(FText::AsPercent(TeamACoverage, &PercentFormat));
	TeamBPercent->SetText(FText::AsPercent(TeamBCoverage, &PercentFormat));
	TeamABar->SetPercent(TeamACoverage);
	TeamBBar->SetPercent(TeamBCoverage);

	switch (State->GetWinningTeam())
	{
	case 0:
		WinnerText->SetText(NSLOCTEXT("InkShooterMatchResult", "TeamAWins", "TEAM A WINS"));
		break;
	case 1:
		WinnerText->SetText(NSLOCTEXT("InkShooterMatchResult", "TeamBWins", "TEAM B WINS"));
		break;
	default:
		WinnerText->SetText(NSLOCTEXT("InkShooterMatchResult", "Draw", "DRAW"));
		break;
	}
}
