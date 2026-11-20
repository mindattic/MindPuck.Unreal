#include "MindPuckTable.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Physics.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MindPuckMesh.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const double TableObstacleHeight = 50.0;
	const double TablePuckHeight = 20.0;
	const double TableRailWidth = 0.4; // table units
	const double TableRailHeight = 35.0;
	const double TableSlabDepth = 30.0;
	const double TableReticleRadius = 0.16; // table units

	const FLinearColor TableSurfaceColor(0.015f, 0.035f, 0.07f);
	const FLinearColor TableRailColor(0.10f, 0.14f, 0.22f);
	const FLinearColor TableObstacleColor(0.42f, 0.52f, 0.68f);
	const FLinearColor TableGoalColor(0.03f, 0.30f, 0.14f);
	const FLinearColor TableGoalCoreColor(0.06f, 0.55f, 0.24f);
	const FLinearColor TableGoalLitColor(0.35f, 1.0f, 0.5f);
	const FLinearColor TablePuckColor(1.0f, 0.42f, 0.08f);
	const FLinearColor TableReticleColor(0.5f, 0.83f, 1.0f);

	UStaticMeshComponent* MakeDisc(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Disc = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Disc->SetupAttachment(Parent);
		Disc->SetStaticMesh(Mesh);
		Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Disc->SetMobility(EComponentMobility::Movable);
		Disc->SetCastShadow(false);
		return Disc;
	}
}

AMindPuckTable::AMindPuckTable()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	BaseMaterial = MaterialFinder.Object;
	CylinderMesh = CylinderFinder.Object;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SurfaceMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Surface"));
	SurfaceMesh->SetupAttachment(Root);
	SurfaceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ObstacleMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Obstacles"));
	ObstacleMesh->SetupAttachment(Root);
	ObstacleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GoalPad = MakeDisc(this, Root, TEXT("GoalPad"), CylinderMesh);
	GoalCore = MakeDisc(this, Root, TEXT("GoalCore"), CylinderMesh);
	Puck = MakeDisc(this, Root, TEXT("Puck"), CylinderMesh);
	Puck->SetCastShadow(true);
	Reticle = MakeDisc(this, Root, TEXT("Reticle"), CylinderMesh);

	KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(Root);
	KeyLight->SetMobility(EComponentMobility::Movable);
	KeyLight->SetRelativeRotation(FRotator(-62.0, 35.0, 0.0));
	KeyLight->SetIntensity(5.0f);

	FillLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(Root);
	FillLight->SetMobility(EComponentMobility::Movable);
	FillLight->SetRelativeRotation(FRotator(-50.0, -140.0, 0.0));
	FillLight->SetIntensity(2.0f);
	FillLight->SetCastShadows(false);
}

void AMindPuckTable::BeginPlay()
{
	Super::BeginPlay();
	EnsureMaterials();
	BuildSurface();
}

UMaterialInstanceDynamic* AMindPuckTable::MakeMaterial(const FLinearColor& Color)
{
	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Material)
	{
		// The engine's basic shape material has one colour parameter, named Color.
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	return Material;
}

void AMindPuckTable::EnsureMaterials()
{
	if (SurfaceMaterial || !BaseMaterial)
	{
		return;
	}
	SurfaceMaterial = MakeMaterial(TableSurfaceColor);
	RailMaterial = MakeMaterial(TableRailColor);
	ObstacleMaterial = MakeMaterial(TableObstacleColor);
	GoalMaterial = MakeMaterial(TableGoalColor);
	GoalCoreMaterial = MakeMaterial(TableGoalCoreColor);
	PuckMaterial = MakeMaterial(TablePuckColor);
	ReticleMaterial = MakeMaterial(TableReticleColor);

	GoalPad->SetMaterial(0, GoalMaterial);
	GoalCore->SetMaterial(0, GoalCoreMaterial);
	Puck->SetMaterial(0, PuckMaterial);
	Reticle->SetMaterial(0, ReticleMaterial);
}

FVector2D AMindPuckTable::TableToLocal(double TableX, double TableY) const
{
	return FVector2D((TableHeight * 0.5 - TableY) * UnitScale, (TableX - TableWidth * 0.5) * UnitScale);
}

FVector AMindPuckTable::TableToWorld(double TableX, double TableY, double Height) const
{
	const FVector2D Local = TableToLocal(TableX, TableY);
	return GetActorTransform().TransformPosition(FVector(Local.X, Local.Y, Height));
}

FVector2D AMindPuckTable::WorldToTable(const FVector& World) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(World);
	return FVector2D(Local.Y / UnitScale + TableWidth * 0.5, TableHeight * 0.5 - Local.X / UnitScale);
}

bool AMindPuckTable::RayToTable(const FVector& Origin, const FVector& Direction, FVector2D& OutTable) const
{
	const FTransform& Transform = GetActorTransform();
	const FVector LocalOrigin = Transform.InverseTransformPosition(Origin);
	const FVector LocalDirection = Transform.InverseTransformVector(Direction);
	if (FMath::Abs(LocalDirection.Z) < 1e-6)
	{
		return false;
	}
	const double T = (GazePlaneHeight - LocalOrigin.Z) / LocalDirection.Z;
	if (T <= 0.0)
	{
		return false;
	}
	const FVector Hit = LocalOrigin + LocalDirection * T;
	OutTable = FVector2D(Hit.Y / UnitScale + TableWidth * 0.5, TableHeight * 0.5 - Hit.X / UnitScale);
	return true;
}

void AMindPuckTable::BuildSurface()
{
	if (bSurfaceBuilt)
	{
		return;
	}
	bSurfaceBuilt = true;

	auto TableRect = [this](double X0, double Y0, double X1, double Y1)
	{
		TArray<FVector2D> Outline;
		Outline.Add(TableToLocal(X0, Y0));
		Outline.Add(TableToLocal(X1, Y0));
		Outline.Add(TableToLocal(X1, Y1));
		Outline.Add(TableToLocal(X0, Y1));
		return Outline;
	};

	const double W = TableWidth;
	const double H = TableHeight;
	const double Rail = TableRailWidth;

	// The playing surface: a slab whose top is at height zero, reaching under the rails.
	FMindPuckMeshData Slab;
	Slab.AddPrism(TableRect(-Rail, -Rail, W + Rail, H + Rail), -TableSlabDepth, 0.0);
	Slab.CreateSection(SurfaceMesh, 0);
	SurfaceMesh->SetMaterial(0, SurfaceMaterial);

	// The rails stand just outside the arena, so their inner faces are the walls the puck hits.
	FMindPuckMeshData Rails;
	Rails.AddPrism(TableRect(-Rail, -Rail, W + Rail, 0.0), 0.0, TableRailHeight);
	Rails.AddPrism(TableRect(-Rail, H, W + Rail, H + Rail), 0.0, TableRailHeight);
	Rails.AddPrism(TableRect(-Rail, 0.0, 0.0, H), 0.0, TableRailHeight);
	Rails.AddPrism(TableRect(W, 0.0, W + Rail, H), 0.0, TableRailHeight);
	Rails.CreateSection(SurfaceMesh, 1);
	SurfaceMesh->SetMaterial(1, RailMaterial);
}

void AMindPuckTable::BuildLevel(const MindPuck::FLevel& Level)
{
	TableWidth = Level.Width;
	TableHeight = Level.Height;
	EnsureMaterials();
	BuildSurface();

	FMindPuckMeshData Solids;
	for (const MindPuck::FPolygon& Polygon : Level.Obstacles)
	{
		TArray<FVector2D> Outline;
		Outline.Reserve(static_cast<int32>(Polygon.size()));
		for (const MindPuck::FVec& Point : Polygon)
		{
			Outline.Add(TableToLocal(Point.X, Point.Y));
		}
		Solids.AddPrism(Outline, 0.0, TableObstacleHeight);
	}
	ObstacleMesh->ClearAllMeshSections();
	if (Solids.Vertices.Num() > 0)
	{
		Solids.CreateSection(ObstacleMesh, 0);
		ObstacleMesh->SetMaterial(0, ObstacleMaterial);
	}

	// The engine cylinder is 100 across and 100 tall, centred on its origin.
	const FVector2D GoalAt = TableToLocal(Level.Goal.X, Level.Goal.Y);
	const double PadScale = Level.Goal.R * 2.0 * UnitScale / 100.0;
	GoalPad->SetRelativeLocation(FVector(GoalAt.X, GoalAt.Y, 0.5));
	GoalPad->SetRelativeScale3D(FVector(PadScale, PadScale, 0.01));
	const double CoreScale = MindPuck::WinRadius(Level) * 2.0 * UnitScale / 100.0;
	GoalCore->SetRelativeLocation(FVector(GoalAt.X, GoalAt.Y, 1.0));
	GoalCore->SetRelativeScale3D(FVector(CoreScale, CoreScale, 0.01));

	const double PuckScale = Level.PuckRadius * 2.0 * UnitScale / 100.0;
	Puck->SetRelativeScale3D(FVector(PuckScale, PuckScale, TablePuckHeight / 100.0));
	SetPuck(Level.Start.X, Level.Start.Y);

	Reticle->SetVisibility(false);
	SetGoalLit(false);
}

void AMindPuckTable::SetPuck(double TableX, double TableY)
{
	const FVector2D At = TableToLocal(TableX, TableY);
	Puck->SetRelativeLocation(FVector(At.X, At.Y, TablePuckHeight * 0.5));
}

void AMindPuckTable::SetReticle(bool bVisible, double TableX, double TableY)
{
	Reticle->SetVisibility(bVisible);
	if (bVisible)
	{
		const FVector2D At = TableToLocal(TableX, TableY);
		Reticle->SetRelativeLocation(FVector(At.X, At.Y, 2.0));
	}
}

void AMindPuckTable::SetReticleStyle(const FLinearColor& Color, double Size)
{
	// The engine cylinder is 100 across.
	const double ReticleScale = TableReticleRadius * Size * 2.0 * UnitScale / 100.0;
	Reticle->SetRelativeScale3D(FVector(ReticleScale, ReticleScale, 0.01));
	if (ReticleMaterial)
	{
		ReticleMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AMindPuckTable::SetGoalLit(bool bLit)
{
	if (GoalCoreMaterial)
	{
		GoalCoreMaterial->SetVectorParameterValue(TEXT("Color"), bLit ? TableGoalLitColor : TableGoalCoreColor);
	}
}
