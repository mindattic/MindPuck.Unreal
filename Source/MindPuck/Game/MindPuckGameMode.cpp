#include "MindPuckGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GazeCalibrator.h"
#include "GazeTrackerSubsystem.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "MindPuck.h"
#include "MindPuckHUD.h"
#include "MindPuckPawn.h"
#include "MindPuckPlayerController.h"
#include "MindPuckTable.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	/** The name a mode is saved under and given on the command line. */
	const TCHAR* ModeKey(MindPuck::EGazeMode Mode)
	{
		return Mode == MindPuck::EGazeMode::Attract ? TEXT("attract") : Mode == MindPuck::EGazeMode::Steer ? TEXT("steer") : TEXT("repel");
	}

	bool ParseMode(const FString& Text, MindPuck::EGazeMode& OutMode)
	{
		for (const MindPuck::EGazeMode Candidate : MindPuck::GazeModes)
		{
			if (Text.Equals(ModeKey(Candidate), ESearchCase::IgnoreCase))
			{
				OutMode = Candidate;
				return true;
			}
		}
		return false;
	}

	const TCHAR* ModeHelp(MindPuck::EGazeMode Mode)
	{
		return Mode == MindPuck::EGazeMode::Attract ? TEXT("Magnetic: the puck goes to where you look.")
			: Mode == MindPuck::EGazeMode::Steer ? TEXT("Steer: look at the edge of the puck on the side it should go. Look at its centre to let it be.")
			: TEXT("Repulsor: the puck is pushed away from where you look.");
	}

	const double GameRailWidth = 0.4; // table units, the same as the table actor's rails

	// One line per level, with enough digits to compare against the web core's output for the
	// same seed and index (node tools/levels.mjs in the MindPuck repo prints kind, obstacles, par
	// and candidates).
	void LogLevelSummary(const MindPuck::FLevel& Level, double Millis)
	{
		UE_LOG(LogMindPuck, Log,
			TEXT("Level %d (seed %u): %s, %d obstacles, par %.1f s, magnetic %.1f s, steer %.1f s, %d candidates, start (%.6f, %.6f), goal (%.6f, %.6f), generated in %.0f ms"),
			Level.Index + 1, Level.Seed,
			Level.Kind == MindPuck::ELevelKind::Cup ? TEXT("cup") : TEXT("zone"),
			static_cast<int32>(Level.Obstacles.size()), Level.Par, Level.ParAttract, Level.ParSteer, Level.Attempts,
			Level.Start.X, Level.Start.Y, Level.Goal.X, Level.Goal.Y, Millis);
	}
}

AMindPuckGameMode::AMindPuckGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	DefaultPawnClass = AMindPuckPawn::StaticClass();
	PlayerControllerClass = AMindPuckPlayerController::StaticClass();
	HUDClass = AMindPuckHUD::StaticClass();
}

AMindPuckGameMode::~AMindPuckGameMode() = default;

void AMindPuckGameMode::StartPlay()
{
	// Which run and level: -Seed=123 -Level=4 on the command line, like ?seed=123&level=4 on the web.
	int32 SeedArg = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("Seed="), SeedArg))
	{
		RunSeed = static_cast<uint32>(SeedArg);
	}
	else
	{
		RunSeed = static_cast<uint32>(FDateTime::UtcNow().GetTicks() / 10000) & 0x7fffffffu;
	}
	int32 LevelArg = 1;
	if (FParse::Value(FCommandLine::Get(), TEXT("Level="), LevelArg))
	{
		LevelIndex = FMath::Max(0, LevelArg - 1);
	}

	// The gaze mode: the saved choice, or -Mode=attract|steer|repel for this run.
	FString SavedMode;
	if (GConfig && GConfig->GetString(TEXT("MindPuck"), TEXT("Mode"), SavedMode, GGameUserSettingsIni))
	{
		ParseMode(SavedMode, GazeMode);
	}
	FString ModeArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("Mode="), ModeArg))
	{
		ParseMode(ModeArg, GazeMode);
	}

	Table = GetWorld()->SpawnActor<AMindPuckTable>(AMindPuckTable::StaticClass(), FTransform::Identity);
	Calibrator = NewObject<UGazeCalibrator>(this);
	Calibrator->OnFinished.AddDynamic(this, &AMindPuckGameMode::HandleCalibrationFinished);

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		Tracker = GameInstance->GetSubsystem<UGazeTrackerSubsystem>();
	}
	if (Tracker)
	{
		FString Url = TEXT("ws://localhost:8787/gaze");
		FString Room = TEXT("default");
		FParse::Value(FCommandLine::Get(), TEXT("GazeUrl="), Url);
		FParse::Value(FCommandLine::Get(), TEXT("GazeRoom="), Room);
		Tracker->ClientName = TEXT("MindPuck");
		Tracker->Connect(Url, Room);
	}

	UE_LOG(LogMindPuck, Log, TEXT("MindPuck game mode started: seed %u, level %d, %s mode."), RunSeed, LevelIndex + 1, ModeKey(GazeMode));

	// -DumpLevels=12 logs the first twelve levels of the run, like tools/levels.mjs on the web.
	int32 DumpCount = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("DumpLevels="), DumpCount))
	{
		for (int32 Index = 0; Index < FMath::Min(DumpCount, 100); Index++)
		{
			const double Began = FPlatformTime::Seconds();
			const MindPuck::FLevel Dumped = MindPuck::GenerateLevel(RunSeed, Index, Params);
			LogLevelSummary(Dumped, (FPlatformTime::Seconds() - Began) * 1000.0);
		}
	}
	LoadLevel();
	if (FParse::Param(FCommandLine::Get(), TEXT("Bot")))
	{
		ToggleBot();
	}

	// Diagnostics: -Calibrate starts a calibration as soon as a tracker is present.
	// -Shot=C:/path/shot.png -ShotAfter=4,20 saves the viewport after 4 and after 20 seconds (the
	// second file gets _2 before the extension) and then quits. -QuitAfter=30 quits after 30 seconds.
	bCalibrateAtStart = FParse::Param(FCommandLine::Get(), TEXT("Calibrate"));
	FParse::Value(FCommandLine::Get(), TEXT("Shot="), ShotPath);
	FString ShotTimes;
	if (FParse::Value(FCommandLine::Get(), TEXT("ShotAfter="), ShotTimes, false))
	{
		TArray<FString> Parts;
		ShotTimes.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			ShotAfter.Add(FCString::Atod(*Part));
		}
	}
	if (!ShotPath.IsEmpty() && ShotAfter.Num() == 0)
	{
		ShotAfter.Add(3.0);
	}
	FParse::Value(FCommandLine::Get(), TEXT("QuitAfter="), QuitAfter);

	Super::StartPlay();
}

void AMindPuckGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Calibrator)
	{
		Calibrator->OnFinished.RemoveAll(this);
		Calibrator->Cancel();
	}
	if (Tracker)
	{
		Tracker->Disconnect();
	}
	Bot.Reset();
	Sim.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMindPuckGameMode::LoadLevel()
{
	// The simulation and the bot point at the level, so they go first.
	const bool bHadBot = Bot.IsValid();
	Bot.Reset();
	Sim.Reset();

	const double Began = FPlatformTime::Seconds();
	CurrentLevel = MindPuck::GenerateLevel(RunSeed, LevelIndex, Params);
	const double Millis = (FPlatformTime::Seconds() - Began) * 1000.0;

	LogLevelSummary(CurrentLevel, Millis);

	Sim = MakeUnique<MindPuck::FPuckSim>(CurrentLevel, Params);
	Sim->Mode = GazeMode;
	if (bHadBot)
	{
		MakeBot();
	}
	if (Table)
	{
		Table->BuildLevel(CurrentLevel);
		Table->SetReticleStyle(ModeColor(GazeMode), GazeMode == MindPuck::EGazeMode::Repel ? 1.0 : GazeMode == MindPuck::EGazeMode::Steer ? 0.45 : 0.6);
	}
	HoldRemaining = HoldSeconds;
	GoalRemaining = 0.0;
	GoalDetail.Reset();
	bGazeValid = false;
}

void AMindPuckGameMode::MakeBot()
{
	// The bot replays the run that proved the level in the current mode: the same gaze-noise seed
	// the generator's check used, so the demo always wins and takes exactly the par time.
	MindPuck::FBotOptions Options;
	Options.Mode = GazeMode;
	Options.Seed = MindPuck::MixSeed(CurrentLevel.Seed, CurrentLevel.Index, 1000 + CurrentLevel.Attempts - 1);
	Bot = MakeUnique<MindPuck::FSolverBot>(CurrentLevel, Params, Options);
}

void AMindPuckGameMode::Restart()
{
	if (!Sim)
	{
		return;
	}
	Sim->Reset();
	Sim->Mode = GazeMode;
	if (Bot)
	{
		MakeBot();
	}
	HoldRemaining = HoldSeconds;
	GoalRemaining = 0.0;
	GoalDetail.Reset();
	if (Table)
	{
		Table->SetGoalLit(false);
		Table->SetPuck(Sim->Pos.X, Sim->Pos.Y);
	}
}

void AMindPuckGameMode::NextLevel()
{
	LevelIndex++;
	LoadLevel();
}

void AMindPuckGameMode::ToggleBot()
{
	if (Bot)
	{
		Bot.Reset();
	}
	else
	{
		MakeBot();
	}
	Restart();
}

void AMindPuckGameMode::ToggleMode()
{
	GazeMode = MindPuck::GazeModes[GetModeNumber() % MindPuck::GazeModeCount];
	if (GConfig)
	{
		GConfig->SetString(TEXT("MindPuck"), TEXT("Mode"), ModeKey(GazeMode), GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	UE_LOG(LogMindPuck, Log, TEXT("Mode switched to %s."), ModeKey(GazeMode));
	ApplyMode();
	Restart();
	Flash(ModeHelp(GazeMode));
}

double AMindPuckGameMode::GetPar() const
{
	return GazeMode == MindPuck::EGazeMode::Attract ? CurrentLevel.ParAttract
		: GazeMode == MindPuck::EGazeMode::Steer ? CurrentLevel.ParSteer
		: CurrentLevel.Par;
}

int32 AMindPuckGameMode::GetModeNumber() const
{
	for (int32 Index = 0; Index < MindPuck::GazeModeCount; Index++)
	{
		if (MindPuck::GazeModes[Index] == GazeMode)
		{
			return Index + 1;
		}
	}
	return 1;
}

const TCHAR* AMindPuckGameMode::ModeName(MindPuck::EGazeMode Mode)
{
	return Mode == MindPuck::EGazeMode::Attract ? TEXT("Magnetic") : Mode == MindPuck::EGazeMode::Steer ? TEXT("Steer") : TEXT("Repulsor");
}

FLinearColor AMindPuckGameMode::ModeColor(MindPuck::EGazeMode Mode)
{
	// The web's #ff9ad5, #5fe3a1 and #7fd4ff.
	return Mode == MindPuck::EGazeMode::Attract ? FLinearColor(FColor(0xff, 0x9a, 0xd5))
		: Mode == MindPuck::EGazeMode::Steer ? FLinearColor(FColor(0x5f, 0xe3, 0xa1))
		: FLinearColor(FColor(0x7f, 0xd4, 0xff));
}

void AMindPuckGameMode::ApplyMode()
{
	if (Sim)
	{
		Sim->Mode = GazeMode;
	}
	if (Table)
	{
		Table->SetReticleStyle(ModeColor(GazeMode), GazeMode == MindPuck::EGazeMode::Repel ? 1.0 : GazeMode == MindPuck::EGazeMode::Steer ? 0.45 : 0.6);
	}
}

void AMindPuckGameMode::ToggleMouse()
{
	bMouseStandIn = !bMouseStandIn;
}

void AMindPuckGameMode::Calibrate()
{
	if (!Calibrator)
	{
		return;
	}
	if (Calibrator->IsActive())
	{
		Calibrator->Cancel();
		return;
	}
	if (!TrackerPresent())
	{
		Flash(TEXT("No tracker is connected yet. Open http://localhost:8787/tracker/"));
		return;
	}
	if (bMouseStandIn)
	{
		Flash(TEXT("Calibration is for eye tracking. Press M to leave the mouse stand-in first."));
		return;
	}
	if (!Calibrator->Start(Tracker))
	{
		Flash(FString::Printf(TEXT("Calibration could not start: %s."), *Calibrator->GetResult().Reason));
	}
}

void AMindPuckGameMode::HandleCalibrationFinished(const FGazeCalibrationResult& Result)
{
	UE_LOG(LogMindPuck, Log, TEXT("Calibration result: ok %s, targets %d, samples %d, fit error %.4f, cross error %.4f, reason \"%s\"."),
		Result.bOk ? TEXT("true") : TEXT("false"), Result.Targets, Result.Samples, Result.FitError, Result.CrossError, *Result.Reason);
	if (Result.bOk)
	{
		Flash(FString::Printf(TEXT("Calibrated: about %d%% of the screen off, typically."), FMath::RoundToInt32(Result.CrossError * 100.0)));
	}
	else if (Result.Reason != TEXT("cancelled"))
	{
		Flash(FString::Printf(TEXT("Calibration failed: %s."), *Result.Reason));
	}
	Restart();
}

void AMindPuckGameMode::Flash(const FString& Text)
{
	FlashText = Text;
	FlashRemaining = 4.5;
}

bool AMindPuckGameMode::TrackerPresent() const
{
	return Tracker && Tracker->IsConnected() && Tracker->GetTrackerCount() > 0;
}

bool AMindPuckGameMode::UsingMouse() const
{
	// The mouse stands in whenever no tracker is connected, so the game is playable without a camera.
	return bMouseStandIn || !TrackerPresent();
}

bool AMindPuckGameMode::PlayerGaze(APlayerController* Player, MindPuck::FVec& OutGaze) const
{
	if (!Player || !Table)
	{
		return false;
	}

	FVector2D Screen;
	if (UsingMouse())
	{
		float MouseX = 0.f;
		float MouseY = 0.f;
		if (!Player->GetMousePosition(MouseX, MouseY))
		{
			return false;
		}
		Screen = FVector2D(MouseX, MouseY);
	}
	else
	{
		// No push while the confidence is zero (a blink, or no face) or the sample is stale.
		if (!Tracker->IsTracking(GazeMaxAgeSeconds))
		{
			return false;
		}
		// Normalized gaze, 0..1 with y down, covers the whole viewport.
		Screen = Tracker->GetViewportPoint();
	}

	FVector Origin;
	FVector Direction;
	if (!UGameplayStatics::DeprojectScreenToWorld(Player, Screen, Origin, Direction))
	{
		return false;
	}
	FVector2D OnTable;
	if (!Table->RayToTable(Origin, Direction, OnTable))
	{
		return false;
	}
	OutGaze.X = OnTable.X;
	OutGaze.Y = OnTable.Y;
	return true;
}

void AMindPuckGameMode::HandleKeys(APlayerController* Player)
{
	if (!Player)
	{
		return;
	}
	if (Player->WasInputKeyJustPressed(EKeys::C))
	{
		Calibrate();
	}
	if (Calibrator && Calibrator->IsActive())
	{
		return;
	}
	// F1 steps to the next mode, as on the web. Tab does the same, because the editor keeps F1 for itself in Play In Editor.
	if (Player->WasInputKeyJustPressed(EKeys::F1) || Player->WasInputKeyJustPressed(EKeys::Tab))
	{
		ToggleMode();
	}
	if (Player->WasInputKeyJustPressed(EKeys::R))
	{
		Restart();
	}
	if (Player->WasInputKeyJustPressed(EKeys::N))
	{
		NextLevel();
	}
	if (Player->WasInputKeyJustPressed(EKeys::B))
	{
		ToggleBot();
	}
	if (Player->WasInputKeyJustPressed(EKeys::M))
	{
		ToggleMouse();
	}
}

void AMindPuckGameMode::TickDiagnostics(double DeltaSeconds)
{
	RunSeconds += DeltaSeconds;

	// One line when the number of trackers changes, and one when the first gaze sample arrives.
	const int32 Trackers = Tracker && Tracker->IsConnected() ? Tracker->GetTrackerCount() : 0;
	if (Trackers != LoggedTrackerCount)
	{
		UE_LOG(LogMindPuck, Log, TEXT("Trackers in the room: %d (was %d)."), Trackers, FMath::Max(LoggedTrackerCount, 0));
		LoggedTrackerCount = Trackers;
		if (Trackers == 0)
		{
			bLoggedFirstGaze = false;
		}
	}
	if (!bLoggedFirstGaze && Tracker && Tracker->HasSample())
	{
		const FGazeSample& Sample = Tracker->GetLatest();
		UE_LOG(LogMindPuck, Log, TEXT("First gaze sample: x %.4f, y %.4f, raw (%.4f, %.4f), confidence %.2f, face %s, blink %s, calibrated %s, t %.1f."),
			Sample.X, Sample.Y, Sample.RawX, Sample.RawY, Sample.Confidence,
			Sample.bFace ? TEXT("true") : TEXT("false"), Sample.bBlink ? TEXT("true") : TEXT("false"),
			Sample.bCalibrated ? TEXT("true") : TEXT("false"), Sample.T);
		bLoggedFirstGaze = true;
	}

	if (bCalibrateAtStart && TrackerPresent() && Calibrator && !Calibrator->IsActive())
	{
		bCalibrateAtStart = false;
		UE_LOG(LogMindPuck, Log, TEXT("Starting the calibration asked for with -Calibrate."));
		Calibrate();
	}

	if (!ShotPath.IsEmpty() && ShotsTaken < ShotAfter.Num() && RunSeconds >= ShotAfter[ShotsTaken])
	{
		FString File = ShotPath;
		if (ShotsTaken > 0)
		{
			File = FPaths::GetPath(ShotPath) / FString::Printf(TEXT("%s_%d%s"), *FPaths::GetBaseFilename(ShotPath), ShotsTaken + 1, *FPaths::GetExtension(ShotPath, true));
		}
		UE_LOG(LogMindPuck, Log, TEXT("Saving a screenshot to %s at %.1f s."), *File, RunSeconds);
		FScreenshotRequest::RequestScreenshot(File, false, false);
		ShotsTaken++;
		if (ShotsTaken == ShotAfter.Num() && QuitAfter <= 0.0)
		{
			QuitAfter = RunSeconds + 2.0;
		}
	}
	if (QuitAfter > 0.0 && RunSeconds >= QuitAfter && !bQuitRequested)
	{
		bQuitRequested = true;
		UE_LOG(LogMindPuck, Log, TEXT("Quitting after %.1f s, as asked on the command line."), RunSeconds);
		FPlatformMisc::RequestExit(false);
	}
}

void AMindPuckGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!bFramed && Player && Table)
	{
		if (AMindPuckPawn* View = Cast<AMindPuckPawn>(Player->GetPawn()))
		{
			const double Scale = AMindPuckTable::UnitScale;
			View->Frame(Table->GetActorLocation(), (CurrentLevel.Width + GameRailWidth * 2.0) * Scale, (CurrentLevel.Height + GameRailWidth * 2.0) * Scale);
			bFramed = true;
		}
	}

	HandleKeys(Player);
	TickDiagnostics(DeltaSeconds);

	const double Elapsed = FMath::Min(static_cast<double>(DeltaSeconds), 0.1);
	if (FlashRemaining > 0.0)
	{
		FlashRemaining -= Elapsed;
	}
	if (!Sim)
	{
		return;
	}

	const bool bCalibrating = Calibrator && Calibrator->IsActive();
	bGazeValid = false;

	if (!bCalibrating)
	{
		// The reticle is drawn even while the puck is held or the level is won.
		if (Bot)
		{
			GazeTable = Bot->Gaze;
			bGazeValid = !Sim->bWon;
		}
		else
		{
			bGazeValid = PlayerGaze(Player, GazeTable);
		}

		if (Sim->bWon)
		{
			GoalRemaining -= Elapsed;
			if (GoalRemaining <= 0.0)
			{
				NextLevel();
			}
		}
		else if (HoldRemaining > 0.0)
		{
			// The start of an attempt: the puck stays put and the clock does not run.
			HoldRemaining -= Elapsed;
		}
		else
		{
			if (Bot)
			{
				MindPuck::FSolverBot* Solver = Bot.Get();
				MindPuck::FPuckSim* Puck = Sim.Get();
				Sim->AdvanceWith(Elapsed, [Solver, Puck]() { return &Solver->Update(*Puck); });
				GazeTable = Bot->Gaze;
			}
			else
			{
				Sim->Advance(Elapsed, bGazeValid ? &GazeTable : nullptr);
			}

			if (Sim->bWon)
			{
				UE_LOG(LogMindPuck, Log, TEXT("Level %d won by %s in %s mode in %.1f s (par %.1f s)."),
					LevelIndex + 1, Bot ? TEXT("the solver bot") : TEXT("the player"), ModeKey(GazeMode), Sim->Time, GetPar());
				GoalRemaining = GoalSeconds;
				if (Bot)
				{
					GoalDetail = FString::Printf(TEXT("The solver took %.1f s"), Sim->Time);
				}
				else if (Sim->Time <= GetPar())
				{
					GoalDetail = FString::Printf(TEXT("%.1f s, under par"), Sim->Time);
				}
				else
				{
					GoalDetail = FString::Printf(TEXT("%.1f s (par %.1f s)"), Sim->Time, GetPar());
				}
				if (Table)
				{
					Table->SetGoalLit(true);
				}
			}
		}
	}

	if (Table && Sim)
	{
		Table->SetPuck(Sim->Pos.X, Sim->Pos.Y);
		Table->SetReticle(bGazeValid, GazeTable.X, GazeTable.Y);
	}
}

bool AMindPuckGameMode::GetGazeOnTable(FVector2D& OutTable) const
{
	OutTable = FVector2D(GazeTable.X, GazeTable.Y);
	return bGazeValid;
}

const std::vector<MindPuck::FVec>* AMindPuckGameMode::GetBotPath() const
{
	return Bot ? &Bot->Path : nullptr;
}

FString AMindPuckGameMode::GetStatusText(bool& bOutWarning) const
{
	bOutWarning = false;
	if (FlashRemaining > 0.0)
	{
		bOutWarning = true;
		return FlashText;
	}
	if (Bot)
	{
		return GazeMode == MindPuck::EGazeMode::Attract ? TEXT("The solver is playing. Watch where it looks: along the route, a step ahead of the puck.")
			: GazeMode == MindPuck::EGazeMode::Steer ? TEXT("The solver is playing. Watch where it looks: just off the puck, on the side it wants it to go.")
			: TEXT("The solver is playing. Watch where it looks: behind the puck, never at the goal.");
	}
	if (!TrackerPresent())
	{
		bOutWarning = true;
		return TEXT("Mouse stand-in: waiting for tracker - open http://localhost:8787/tracker/");
	}
	if (bMouseStandIn)
	{
		return FString::Printf(TEXT("Mouse stand-in (M for the eye tracker). %s"), ModeHelp(GazeMode));
	}
	if (Tracker->HasSample() && !Tracker->GetLatest().bFace)
	{
		bOutWarning = true;
		return TEXT("Eye tracker: your face is out of view of the camera.");
	}
	if (Tracker->HasSample() && !Tracker->GetLatest().bCalibrated)
	{
		bOutWarning = true;
		return TEXT("Eye tracker: not calibrated yet, press C. Until then the gaze point is a rough guess.");
	}
	return FString::Printf(TEXT("Eye tracker. %s"), ModeHelp(GazeMode));
}
