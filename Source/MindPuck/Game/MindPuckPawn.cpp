#include "MindPuckPawn.h"

#include "Camera/CameraComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

AMindPuckPawn::AMindPuckPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->bConstrainAspectRatio = false;
	Camera->SetFieldOfView(static_cast<float>(FieldOfView));
}

void AMindPuckPawn::Frame(const FVector& InCentre, double InWidth, double InHeight)
{
	Centre = InCentre;
	FrameWidth = InWidth;
	FrameHeight = InHeight;
	Place();
}

void AMindPuckPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Place();
}

void AMindPuckPawn::Place()
{
	double Aspect = 16.0 / 9.0;
	if (const UWorld* World = GetWorld())
	{
		if (UGameViewportClient* Viewport = World->GetGameViewport())
		{
			FVector2D Size(0.0, 0.0);
			Viewport->GetViewportSize(Size);
			if (Size.X > 1.0 && Size.Y > 1.0)
			{
				Aspect = Size.X / Size.Y;
			}
		}
	}

	// The field of view is horizontal. Back off until both the width and the height fit.
	Camera->SetFieldOfView(static_cast<float>(FieldOfView));
	const double TanHalfH = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5));
	const double TanHalfV = TanHalfH / Aspect;
	const double Distance = (1.0 + Margin) * FMath::Max(FrameWidth * 0.5 / TanHalfH, FrameHeight * 0.5 / TanHalfV);

	// Pitch -90 looks straight down with world +X at the top of the screen. Leaning back by the
	// tilt moves the camera toward the bottom edge of the table.
	const FRotator Rotation(-90.0 + TiltDegrees, 0.0, 0.0);
	const FVector Location = Centre - Rotation.Vector() * Distance;
	SetActorLocationAndRotation(Location, Rotation);
	if (AController* Driver = GetController())
	{
		Driver->SetControlRotation(Rotation);
	}
}
