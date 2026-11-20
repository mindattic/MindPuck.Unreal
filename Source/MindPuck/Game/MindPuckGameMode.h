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
 * the counterpart of MindPuck/src/main.ts.
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
	/** Start the calibration, or cancel the one in progress. */
	void Calibrate();

	// What the HUD shows.
	int32 GetLevelNumber() const { return LevelIndex + 1; }
	uint32 GetSeed() const { return RunSeed; }
	double GetTime() const { return Sim ? Sim->Time : 0.0; }
	double GetPar() const { return CurrentLevel.Par; }
	bool IsWon() const { return Sim && Sim->bWon; }
	bool IsBotPlaying() const { return Bot.IsValid(); }
	bool IsHolding() const { return HoldRemaining > 0.0; }
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
	void MakeBot();
	void Flash(const FString& Text);

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

	bool bMouseStandIn = false;
	bool bFramed = false;
	double HoldRemaining = 0.0;
	double GoalRemaining = 0.0;
	FString GoalDetail;

	bool bGazeValid = false;
	MindPuck::FVec GazeTable;

	FString FlashText;
	double FlashRemaining = 0.0;
};
