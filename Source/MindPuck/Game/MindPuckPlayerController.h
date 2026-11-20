#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MindPuckPlayerController.generated.h"

/** Shows the mouse cursor and leaves it free, since the mouse stands in for the gaze. */
UCLASS()
class AMindPuckPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMindPuckPlayerController();

protected:
	virtual void BeginPlay() override;
};
