#include "MindPuckHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GazeCalibrator.h"
#include "MindPuckGameMode.h"
#include "MindPuckTable.h"

namespace
{
	const FLinearColor HudText(0.91f, 0.93f, 0.99f);
	const FLinearColor HudDim(0.55f, 0.62f, 0.75f);
	const FLinearColor HudWarn(1.0f, 0.72f, 0.25f);
	const FLinearColor HudAccent(0.5f, 0.83f, 1.0f);
	const FLinearColor HudGoal(0.45f, 1.0f, 0.6f);
	const FLinearColor HudCover(0.02f, 0.027f, 0.05f, 1.0f);
}

void AMindPuckHUD::DrawLabel(const FString& Text, const FLinearColor& Color, double X, double Y, UFont* Font, double Scale)
{
	DrawText(Text, Color, static_cast<float>(X), static_cast<float>(Y), Font, static_cast<float>(Scale));
}

void AMindPuckHUD::DrawCentred(const FString& Text, const FLinearColor& Color, double CentreX, double Y, UFont* Font, double Scale)
{
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, static_cast<float>(Scale));
	DrawText(Text, Color, static_cast<float>(CentreX - Width * 0.5), static_cast<float>(Y), Font, static_cast<float>(Scale));
}

void AMindPuckHUD::DrawRing(const FVector2D& Centre, double Radius, const FLinearColor& Color, double Thickness)
{
	const int32 Segments = 40;
	FVector2D Previous(Centre.X + Radius, Centre.Y);
	for (int32 i = 1; i <= Segments; i++)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * i / Segments;
		const FVector2D Next(Centre.X + FMath::Cos(Angle) * Radius, Centre.Y + FMath::Sin(Angle) * Radius);
		DrawLine(static_cast<float>(Previous.X), static_cast<float>(Previous.Y), static_cast<float>(Next.X), static_cast<float>(Next.Y), Color, static_cast<float>(Thickness));
		Previous = Next;
	}
}

void AMindPuckHUD::DrawCalibration(const UGazeCalibrator* Calibrator, double Scale)
{
	const double W = Canvas->SizeX;
	const double H = Canvas->SizeY;
	UFont* Medium = GEngine->GetMediumFont();

	// Cover the game: the targets span the whole viewport, which is the area the gaze then maps to.
	DrawRect(HudCover, 0.f, 0.f, static_cast<float>(W), static_cast<float>(H));

	if (Calibrator->HasTarget())
	{
		const FVector2D Target = Calibrator->GetCurrentTarget();
		const FVector2D At(Target.X * W, Target.Y * H);
		DrawRing(At, 22.0 * Scale, HudAccent, 3.0 * Scale);
		// The centre dot grows while the tracker samples, as on the web.
		const double DotRadius = (4.0 + 14.0 * Calibrator->GetCollectProgress()) * Scale;
		Canvas->K2_DrawPolygon(nullptr, At, FVector2D(DotRadius, DotRadius), 24, FLinearColor::White);

		const FString Progress = FString::Printf(TEXT("Look at the dot until it fills. Keep your head still.   %d / %d   (C cancels)"),
			Calibrator->GetTargetIndex() + 1, Calibrator->GetTargetCount());
		// Keep the line away from the target: at the bottom, or at the top for the bottom row.
		DrawCentred(Progress, HudDim, W * 0.5, Target.Y > 0.7 ? H * 0.30 : H * 0.72, Medium, 1.2 * Scale);
	}
	else
	{
		DrawCentred(Calibrator->GetStatusText(), HudText, W * 0.5, H * 0.45, Medium, 1.8 * Scale);
	}
}

void AMindPuckHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || !GEngine)
	{
		return;
	}
	const AMindPuckGameMode* Game = GetWorld() ? GetWorld()->GetAuthGameMode<AMindPuckGameMode>() : nullptr;
	if (!Game)
	{
		return;
	}

	const double W = Canvas->SizeX;
	const double H = Canvas->SizeY;
	const double Scale = FMath::Max(0.6, H / 1080.0);
	UFont* Medium = GEngine->GetMediumFont();
	UFont* Large = GEngine->GetLargeFont();

	const UGazeCalibrator* Calibrator = Game->GetCalibrator();
	if (Calibrator && Calibrator->IsActive())
	{
		DrawCalibration(Calibrator, Scale);
		return;
	}

	APlayerController* Player = GetOwningPlayerController();
	const AMindPuckTable* Table = Game->GetTable();

	// The bot's route, then the ring where the gaze lands on the table.
	if (Player && Table)
	{
		if (const std::vector<MindPuck::FVec>* Path = Game->GetBotPath())
		{
			FVector2D Previous(0.0, 0.0);
			bool bHavePrevious = false;
			for (const MindPuck::FVec& Point : *Path)
			{
				FVector2D Screen;
				const bool bOnScreen = Player->ProjectWorldLocationToScreen(Table->TableToWorld(Point.X, Point.Y, AMindPuckTable::GazePlaneHeight), Screen);
				if (bOnScreen && bHavePrevious)
				{
					DrawLine(static_cast<float>(Previous.X), static_cast<float>(Previous.Y), static_cast<float>(Screen.X), static_cast<float>(Screen.Y),
						FLinearColor(0.5f, 0.83f, 1.0f, 0.45f), static_cast<float>(2.0 * Scale));
				}
				Previous = Screen;
				bHavePrevious = bOnScreen;
			}
		}

		FVector2D GazeOnTable;
		if (Game->GetGazeOnTable(GazeOnTable))
		{
			FVector2D Screen;
			if (Player->ProjectWorldLocationToScreen(Table->TableToWorld(GazeOnTable.X, GazeOnTable.Y, AMindPuckTable::GazePlaneHeight), Screen))
			{
				DrawRing(Screen, 18.0 * Scale, HudAccent, 2.0 * Scale);
			}
		}
	}

	// Top left: level, clock and par.
	const double Pad = 28.0 * Scale;
	DrawLabel(FString::Printf(TEXT("Level %d"), Game->GetLevelNumber()), HudText, Pad, Pad, Large, 1.4 * Scale);
	const bool bBeat = Game->IsWon() && !Game->IsBotPlaying() && Game->GetTime() <= Game->GetPar();
	DrawLabel(FString::Printf(TEXT("Time %.1f s"), Game->GetTime()), bBeat ? HudGoal : HudText, Pad, Pad + 44.0 * Scale, Medium, 1.5 * Scale);
	DrawLabel(FString::Printf(TEXT("Par %.1f s"), Game->GetPar()), HudDim, Pad, Pad + 74.0 * Scale, Medium, 1.5 * Scale);

	// Top right: the run's seed, so a run can be started again with -Seed=.
	{
		const FString SeedText = FString::Printf(TEXT("seed %u"), Game->GetSeed());
		float TextW = 0.f;
		float TextH = 0.f;
		GetTextSize(SeedText, TextW, TextH, Medium, static_cast<float>(1.1 * Scale));
		DrawLabel(SeedText, HudDim, W - Pad - TextW, Pad, Medium, 1.1 * Scale);
	}

	// Bottom: where the gaze comes from, then the keys.
	bool bWarning = false;
	const FString Status = Game->GetStatusText(bWarning);
	DrawCentred(Status, bWarning ? HudWarn : HudText, W * 0.5, H - 86.0 * Scale, Medium, 1.4 * Scale);
	DrawCentred(TEXT("R restart    N skip    B bot demo    C calibrate    M mouse stand-in"), HudDim, W * 0.5, H - 50.0 * Scale, Medium, 1.2 * Scale);

	if (Game->IsWon())
	{
		DrawCentred(TEXT("GOAL"), HudGoal, W * 0.5, H * 0.36, Large, 5.0 * Scale);
		DrawCentred(Game->GetGoalDetail(), HudText, W * 0.5, H * 0.36 + 120.0 * Scale, Medium, 1.8 * Scale);
	}
	else if (Game->IsHolding())
	{
		DrawCentred(TEXT("Ready"), HudDim, W * 0.5, H * 0.12, Medium, 1.6 * Scale);
	}
}
