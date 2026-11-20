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
	const FLinearColor HudAmber(1.0f, 0.72f, 0.25f);
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
		// The head-movement steps have an amber dot, so they read as a different instruction.
		const bool bHead = Calibrator->IsHeadMovementStep();
		// The follow-the-dot pass: the target moves every frame and the player's eyes follow it.
		const bool bPursuit = Calibrator->IsPursuitStep();
		DrawRing(At, 22.0 * Scale, bHead ? HudAmber : HudAccent, 3.0 * Scale);
		// The centre dot grows while the tracker samples, as on the web. On the moving target it
		// keeps one size (the web's 2.4 on its scale of 1 to 4.6).
		const double DotRadius = (4.0 + 14.0 * (bPursuit ? (2.4 - 1.0) / (4.6 - 1.0) : Calibrator->GetCollectProgress())) * Scale;
		Canvas->K2_DrawPolygon(nullptr, At, FVector2D(DotRadius, DotRadius), 24, bHead ? HudAmber : FLinearColor::White);

		if (bPursuit)
		{
			// At the top centre, the one stretch of the edge the moving dot never comes near, so
			// the line can stay put for the whole pass. The seconds left count down beside it.
			const int32 Left = FMath::CeilToInt32((1.0 - Calibrator->GetCollectProgress()) * Calibrator->PursuitSeconds);
			DrawCentred(FString::Printf(TEXT("%s   %d s   (C cancels)"), *Calibrator->GetInstruction(), Left), HudText, W * 0.5, H * 0.03, Medium, 1.5 * Scale);
		}
		else
		{
			const FString Progress = bHead
				? FString::Printf(TEXT("%s   (C cancels)"), *Calibrator->GetInstruction())
				: FString::Printf(TEXT("%s   %d / %d   (C cancels)"), *Calibrator->GetInstruction(),
					Calibrator->GetTargetIndex() + 1, Calibrator->GetDotCount());
			// Keep the line away from the target: at the bottom, or at the top for the bottom row.
			DrawCentred(Progress, bHead ? HudText : HudDim, W * 0.5, Target.Y > 0.6 ? H * 0.30 : H * 0.72, Medium, (bHead ? 1.5 : 1.2) * Scale);
		}
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
				// Wide for the repulsor, smaller for the magnet, smallest for steer, in the mode's colour.
				const MindPuck::EGazeMode Mode = Game->GetMode();
				const double RingRadius = Mode == MindPuck::EGazeMode::Repel ? 18.0 : Mode == MindPuck::EGazeMode::Steer ? 8.0 : 11.0;
				DrawRing(Screen, RingRadius * Scale, AMindPuckGameMode::ModeColor(Mode), 2.0 * Scale);
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
	const FString Keys = TEXT("R restart    N skip    B bot demo    C calibrate    M mouse stand-in");
	{
		// A dark plate behind the two lines, because they sit over the table. They are at the top
		// centre, so that the bottom centre belongs to the mode label alone.
		float StatusW = 0.f;
		float StatusH = 0.f;
		float KeysW = 0.f;
		float KeysH = 0.f;
		GetTextSize(Status, StatusW, StatusH, Medium, static_cast<float>(1.4 * Scale));
		GetTextSize(Keys, KeysW, KeysH, Medium, static_cast<float>(1.2 * Scale));
		const double PlateW = FMath::Max(StatusW, KeysW) + 36.0 * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), static_cast<float>(W * 0.5 - PlateW * 0.5), static_cast<float>(6.0 * Scale),
			static_cast<float>(PlateW), static_cast<float>(72.0 * Scale));
	}
	DrawCentred(Status, bWarning ? HudWarn : HudText, W * 0.5, 14.0 * Scale, Medium, 1.4 * Scale);
	DrawCentred(Keys, HudDim, W * 0.5, 46.0 * Scale, Medium, 1.2 * Scale);

	// Bottom centre, always: which control scheme is live. "Mode 2 of 3  STEER  F1 next".
	{
		const MindPuck::EGazeMode Mode = Game->GetMode();
		const FString Left = FString::Printf(TEXT("Mode %d of %d"), Game->GetModeNumber(), MindPuck::GazeModeCount);
		const FString Name = FString(AMindPuckGameMode::ModeName(Mode)).ToUpper();
		const FString Right = TEXT("F1 next");
		const float SmallScale = static_cast<float>(1.3 * Scale);
		const float NameScale = static_cast<float>(2.0 * Scale);
		float LeftW = 0.f;
		float LeftH = 0.f;
		float NameW = 0.f;
		float NameH = 0.f;
		float RightW = 0.f;
		float RightH = 0.f;
		GetTextSize(Left, LeftW, LeftH, Medium, SmallScale);
		GetTextSize(Name, NameW, NameH, Medium, NameScale);
		GetTextSize(Right, RightW, RightH, Medium, SmallScale);
		const double Gap = 14.0 * Scale;
		const double Total = LeftW + Gap + NameW + Gap + RightW;
		const double PlateH = NameH + 16.0 * Scale;
		const double PlateY = H - PlateH - 12.0 * Scale;
		double X = W * 0.5 - Total * 0.5;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), static_cast<float>(X - 18.0 * Scale), static_cast<float>(PlateY),
			static_cast<float>(Total + 36.0 * Scale), static_cast<float>(PlateH));
		const double MidY = PlateY + PlateH * 0.5;
		DrawLabel(Left, HudDim, X, MidY - LeftH * 0.5, Medium, SmallScale);
		X += LeftW + Gap;
		DrawLabel(Name, AMindPuckGameMode::ModeColor(Mode), X, MidY - NameH * 0.5, Medium, NameScale);
		X += NameW + Gap;
		DrawLabel(Right, HudDim, X, MidY - RightH * 0.5, Medium, SmallScale);
	}

	if (Game->IsWon())
	{
		DrawCentred(TEXT("GOAL"), HudGoal, W * 0.5, H * 0.36, Large, 5.0 * Scale);
		DrawCentred(Game->GetGoalDetail(), HudText, W * 0.5, H * 0.36 + 120.0 * Scale, Medium, 1.8 * Scale);
	}
	else if (Game->IsHolding())
	{
		// In steer mode the hold is when the game finds where the player's gaze sits on the puck.
		const bool bFindPuck = Game->GetMode() == MindPuck::EGazeMode::Steer && !Game->IsBotPlaying();
		DrawCentred(bFindPuck ? TEXT("Look at the puck") : TEXT("Ready"), HudDim, W * 0.5, H * 0.12, Medium, 1.6 * Scale);
	}
}
