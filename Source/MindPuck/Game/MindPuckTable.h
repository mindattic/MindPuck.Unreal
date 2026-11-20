#pragma once

#include "CoreMinimal.h"
#include "Core/Level.h"
#include "GameFramework/Actor.h"
#include "MindPuckTable.generated.h"

class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * The air-hockey table and everything on it, built from code when a level loads: the surface and
 * rails, the obstacles (the level's polygons pushed up into solid shapes), the goal pad, the puck
 * and the gaze reticle. It also carries the two lights, so the engine's empty map is enough.
 *
 * Table units map to the world like this: the table lies in the XY plane, centred on the actor.
 * Table x runs along world +Y and table y along world -X, so a camera looking straight down with
 * world +X at the top of the screen sees the table the way the web version draws it.
 */
UCLASS()
class AMindPuckTable : public AActor
{
	GENERATED_BODY()

public:
	AMindPuckTable();

	/** World units (centimetres) per table unit. */
	static constexpr double UnitScale = 100.0;
	/** Height of the plane the gaze is projected onto: the middle of the puck. */
	static constexpr double GazePlaneHeight = 10.0;

	/** Rebuild the obstacles and move the goal pad for a new level. */
	void BuildLevel(const MindPuck::FLevel& Level);

	void SetPuck(double TableX, double TableY);
	void SetReticle(bool bVisible, double TableX, double TableY);
	/** The reticle is blue and full size for the repulsor, pink and smaller for the magnet. */
	void SetReticleStyle(bool bMagnetic);
	/** Brighten the goal pad, for the moment the level is won. */
	void SetGoalLit(bool bLit);

	FVector TableToWorld(double TableX, double TableY, double Height = 0.0) const;
	FVector2D WorldToTable(const FVector& World) const;

	/** Where a ray meets the gaze plane, in table units. False when it runs parallel or away. */
	bool RayToTable(const FVector& Origin, const FVector& Direction, FVector2D& OutTable) const;

protected:
	virtual void BeginPlay() override;

private:
	FVector2D TableToLocal(double TableX, double TableY) const;
	void BuildSurface();
	void EnsureMaterials();
	UMaterialInstanceDynamic* MakeMaterial(const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> SurfaceMesh;

	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> ObstacleMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GoalPad;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> GoalCore;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Puck;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Reticle;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> KeyLight;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> FillLight;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CylinderMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SurfaceMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> RailMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ObstacleMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GoalMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> GoalCoreMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PuckMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ReticleMaterial;

	double TableWidth = MindPuck::ArenaW;
	double TableHeight = MindPuck::ArenaH;
	bool bSurfaceBuilt = false;
};
