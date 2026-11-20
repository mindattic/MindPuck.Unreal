#pragma once

#include "CoreMinimal.h"
#include "Core/MindPuckCore.h"
#include "GameFramework/GameModeBase.h"
#include "GazeTypes.h"
#include "MindPuckGameMode.generated.h"

class AMindPuckTable;
class UGazeCalibrator;
class UGazeTrackerSubsystem;

/**
 * The game loop. It owns the level, the puck simulation and (in the demo) the solver bot, reads
 * the gaze from the eye tracker or the mouse, and tells the table actor where things are. This is
 * the counterpart of MindPuck.Web/src/main.ts.
 */
UCLASS()
class AMindPuckGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMindPuckGameMode();
	virtual ~AMindPuckGameMode() override;

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Put the puck back at the start of the current level. */
	void Restart();
	void NextLevel();
	/** Let the solver bot play, or hand the puck back. */
	void ToggleBot();
	/** Use the mouse as the gaze even when a tracker is connected, or stop doing so. */
	void ToggleMouse();
	/** Step to the next mode (magnetic, steer, repulsor), remember the choice, and restart the attempt. */
	void ToggleMode();
	/** Start the calibration, or cancel the one in progress. */
	void Calibrate();

	// What the HUD shows.
	int32 GetLevelNumber() const { return LevelIndex + 1; }
	uint32 GetSeed() const { return RunSeed; }
	double GetTime() const { return Sim ? Sim->Time : 0.0; }
	/** The solver's time in the mode being played. */
	double GetPar() const;
	MindPuck::EGazeMode GetMode() const { return GazeMode; }
	/** The place of the current mode in the order F1 steps through, starting at 1. */
	int32 GetModeNumber() const;
	/** "Magnetic", "Steer" or "Repulsor". */
	static const TCHAR* ModeName(MindPuck::EGazeMode Mode);
	/** Pink for magnetic, green for steer, blue for the repulsor. */
	static FLinearColor ModeColor(MindPuck::EGazeMode Mode);
	bool IsWon() const { return Sim && Sim->bWon; }
	bool IsBotPlaying() const { return Bot.IsValid(); }
	bool IsHolding() const { return HoldRemaining > 0.0; }
	/** The hold at the start of an attempt in the mode being played: longer in steer mode. */
	double GetHoldSeconds() const { return GazeMode == MindPuck::EGazeMode::Steer ? SteerHoldSeconds : HoldSeconds; }
	const FString& GetGoalDetail() const { return GoalDetail; }
	/** One line about where the gaze is coming from, and whether it should stand out as a warning. */
	FString GetStatusText(bool& bOutWarning) const;
	/** The gaze point on the table this frame. False when nothing is pushing. */
	bool GetGazeOnTable(FVector2D& OutTable) const;
	/** The route the bot is following, or null when the bot is not playing. */
	const std::vector<MindPuck::FVec>* GetBotPath() const;
	AMindPuckTable* GetTable() const { return Table; }
	UGazeCalibrator* GetCalibrator() const { return Calibrator; }

	/** Seconds the puck is held still at the start of every attempt, so the player can find it. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double HoldSeconds = 0.9;

	/** The hold in steer mode. It is longer, because the player's eyes must be on the puck before it starts. */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerHoldSeconds = 1.7;

	/** Steer anchoring: how fast the standing gaze offset is followed while the puck is held (a time constant, in seconds). */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerAnchorHoldSeconds = 0.3;

	/** Steer anchoring: the same during play. Slow, so that a deliberate look at one edge is not absorbed before it has done its work. */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerAnchorPlaySeconds = 6.0;

	/** Steer anchoring: during play the offset is only followed while the corrected gaze is within this many table units of the puck. */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerAnchorGate = 4.0;

	/** Steer: the tracker is told the player is looking at the puck this long before the hold ends. */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerHintLeadSeconds = 0.5;

	/** Steer: the weight of that hint. */
	UPROPERTY(EditAnywhere, Category = "MindPuck|Steer")
	double SteerHintWeight = 0.3;

	/** Seconds the GOAL banner stays up before the next level loads. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double GoalSeconds = 1.9;

	/** A gaze sample older than this counts as no gaze. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double GazeMaxAgeSeconds = 0.4;

private:
	void LoadLevel();
	void HandleKeys(APlayerController* Player);
	bool TrackerPresent() const;
	bool UsingMouse() const;
	/** The table point the player is looking at right now. False when the eyes are not tracked. */
	bool PlayerGaze(APlayerController* Player, MindPuck::FVec& OutGaze) const;
	/**
	 * Steer mode: take the standing offset between the gaze point and the puck out of Gaze, and
	 * keep measuring it. Once per attempt, near the end of the hold, tell the tracker the player
	 * is looking at the puck.
	 */
	void AnchorSteer(APlayerController* Player, MindPuck::FVec& Gaze, double Elapsed);
	void MakeBot();
	void Flash(const FString& Text);
	/** Log lines and the command-line switches for checking the game without a person at it. */
	void TickDiagnostics(double DeltaSeconds);

	UFUNCTION()
	void HandleCalibrationFinished(const FGazeCalibrationResult& Result);

	UPROPERTY()
	TObjectPtr<AMindPuckTable> Table;

	UPROPERTY()
	TObjectPtr<UGazeCalibrator> Calibrator;

	UPROPERTY()
	TObjectPtr<UGazeTrackerSubsystem> Tracker;

	uint32 RunSeed = 1;
	int32 LevelIndex = 0;
	MindPuck::FPhysicsParams Params;
	MindPuck::FLevel CurrentLevel;
	// The simulation and the bot refer to CurrentLevel, so they are rebuilt whenever it changes.
	TUniquePtr<MindPuck::FPuckSim> Sim;
	TUniquePtr<MindPuck::FSolverBot> Bot;

	/** Magnetic is the default. The choice is kept in GameUserSettings.ini, section MindPuck. */
	MindPuck::EGazeMode GazeMode = MindPuck::EGazeMode::Attract;
	void ApplyMode();

	bool bMouseStandIn = false;
	bool bFramed = false;
	double HoldRemaining = 0.0;
	double GoalRemaining = 0.0;
	FString GoalDetail;

	bool bGazeValid = false;
	MindPuck::FVec GazeTable;

	/**
	 * Steer mode works from where the gaze is relative to the puck, and an eye tracker is rarely
	 * accurate to within a puck's width. But in steer mode the player is, by definition, looking
	 * at the puck. So the game measures the standing offset between the gaze point and the puck
	 * and takes it out: quickly while the puck is held at the start, slowly during play. What is
	 * left is the part that matters, the small movement of the eyes toward one edge of the puck.
	 * It is kept from one attempt to the next, as on the web.
	 */
	MindPuck::FVec SteerOffset{ 0.0, 0.0 };
	/** The hint has been sent for this attempt. */
	bool bSteerHinted = false;
	/** Whether the offset was measured from the mouse. It is dropped when the gaze changes hands. */
	bool bSteerOffsetFromMouse = false;

	FString FlashText;
	double FlashRemaining = 0.0;

	// Diagnostics.
	double RunSeconds = 0.0;
	int32 LoggedTrackerCount = -1;
	bool bLoggedFirstGaze = false;
	bool bCalibrateAtStart = false;
	FString ShotPath;
	TArray<double> ShotAfter;
	int32 ShotsTaken = 0;
	double QuitAfter = 0.0;
	bool bQuitRequested = false;
};
