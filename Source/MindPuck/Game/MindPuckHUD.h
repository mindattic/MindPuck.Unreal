#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MindPuckHUD.generated.h"

class UFont;

/**
 * Everything written on the screen, drawn straight onto the canvas: the level number, the clock
 * and par, where the gaze is coming from, the key hints, the GOAL banner, a ring where the gaze
 * lands, the bot's route, and the calibration targets.
 */
UCLASS()
class AMindPuckHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLabel(const FString& Text, const FLinearColor& Color, double X, double Y, UFont* Font, double Scale);
	void DrawCentred(const FString& Text, const FLinearColor& Color, double CentreX, double Y, UFont* Font, double Scale);
	void DrawRing(const FVector2D& Centre, double Radius, const FLinearColor& Color, double Thickness);
	void DrawCalibration(const class UGazeCalibrator* Calibrator, double Scale);
};
