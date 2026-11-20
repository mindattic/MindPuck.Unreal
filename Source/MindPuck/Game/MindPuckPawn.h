#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MindPuckPawn.generated.h"

class UCameraComponent;

/**
 * The player's view: a camera above the table, looking down with a slight tilt. Each frame it
 * backs off just far enough for the whole table to fit the viewport, whatever its shape.
 */
UCLASS()
class AMindPuckPawn : public APawn
{
	GENERATED_BODY()

public:
	AMindPuckPawn();

	virtual void Tick(float DeltaSeconds) override;

	/** The point the camera looks at (the table's centre) and the table's size, in world units. */
	void Frame(const FVector& InCentre, double InWidth, double InHeight);

	/** Degrees the camera leans back from straight down. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double TiltDegrees = 7.0;

	/** Horizontal field of view, in degrees. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double FieldOfView = 35.0;

	/** Extra room around the table, as a fraction. */
	UPROPERTY(EditAnywhere, Category = "MindPuck")
	double Margin = 0.14;

	/** The exposure the view is held at, in EV100. Higher is darker. */
	static constexpr float FixedExposure = 2.5f;

private:
	void Place();

	UPROPERTY()
	TObjectPtr<UCameraComponent> Camera;

	FVector Centre = FVector::ZeroVector;
	double FrameWidth = 1600.0;
	double FrameHeight = 900.0;
};
