#include "Core/MindPuckCore.h"
#include "Game/MindPuckMesh.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// The expected numbers below were printed by the web core (MindPuck.Web/src/core) for the same inputs.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMindPuckRngTest, "MindPuck.Core.Rng",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMindPuckRngTest::RunTest(const FString& Parameters)
{
	MindPuck::FRng Rng = MindPuck::Mulberry32(1);
	TestEqual(TEXT("mulberry32(1) first"), Rng(), 0.62707394058816135, 1e-16);
	TestEqual(TEXT("mulberry32(1) second"), Rng(), 0.00273572118021548, 1e-16);
	TestEqual(TEXT("mulberry32(1) third"), Rng(), 0.52744703995995224, 1e-16);
	TestEqual(TEXT("mulberry32(1) fourth"), Rng(), 0.98105096747167408, 1e-16);
	TestTrue(TEXT("mixSeed(1, 2, 3)"), MindPuck::MixSeed(1, 2, 3) == 147163718u);
	TestTrue(TEXT("mixSeed(12345, 7, 1003)"), MindPuck::MixSeed(12345, 7, 1003) == 1070857489u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMindPuckGeometryTest, "MindPuck.Core.Geometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMindPuckGeometryTest::RunTest(const FString& Parameters)
{
	const MindPuck::FPolygon Square = MindPuck::RectPolygon(0, 0, 1, 1);
	TestTrue(TEXT("centre is inside"), MindPuck::PointInPolygon(0, 0, Square));
	TestFalse(TEXT("far point is outside"), MindPuck::PointInPolygon(3, 0, Square));
	TestEqual(TEXT("distance to a face"), MindPuck::PointPolygonDistance(3, 0, Square), 2.0, 1e-12);

	// The notch of the L is outside the shape.
	const MindPuck::FPolygon Ell = MindPuck::LPolygon(0, 0, 2, 0.5);
	TestTrue(TEXT("the corner of the L is inside"), MindPuck::PointInPolygon(-0.8, -0.8, Ell));
	TestFalse(TEXT("the notch of the L is outside"), MindPuck::PointInPolygon(0.5, 0.5, Ell));

	const MindPuck::FPolygon Other = MindPuck::RectPolygon(4, 0, 1, 1);
	TestEqual(TEXT("gap between two squares"), MindPuck::PolygonDistance(Square, Other), 2.0, 1e-12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMindPuckPhysicsTest, "MindPuck.Core.Physics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMindPuckPhysicsTest::RunTest(const FString& Parameters)
{
	MindPuck::FLevel Level;
	Level.Start = MindPuck::FVec{ 4.0, 4.5 };
	Level.Goal.X = 12.0;
	Level.Goal.Y = 4.5;
	Level.Goal.R = MindPuck::GoalR;

	// No gaze: the puck stays where it is.
	MindPuck::FPuckSim Still(Level);
	TestTrue(TEXT("magnetic is the default mode"), Still.Mode == MindPuck::EGazeMode::Attract);
	Still.Advance(0.1, nullptr);
	TestEqual(TEXT("no gaze, no motion"), Still.Pos.X, 4.0, 1e-12);

	// Repulsor: looking to the left of the puck pushes it to the right.
	MindPuck::FPuckSim Pushed(Level);
	Pushed.Mode = MindPuck::EGazeMode::Repel;
	const MindPuck::FVec Gaze{ 2.5, 4.5 };
	for (int32 i = 0; i < 60; i++)
	{
		Pushed.Step(&Gaze);
	}
	TestTrue(TEXT("the gaze repels"), Pushed.Pos.X > 4.0 && Pushed.Vel.X > 0.0);
	TestEqual(TEXT("no sideways drift"), Pushed.Pos.Y, 4.5, 1e-9);

	// Past the range the gaze does nothing; at dead centre it does nothing either.
	const MindPuck::FPhysicsParams Params;
	TestEqual(TEXT("no push past the range"), MindPuck::RepelAccel(Params.RepelRange, MindPuck::PuckR, Params), 0.0, 0.0);
	TestEqual(TEXT("no push at dead centre"), MindPuck::RepelAccel(0.0, MindPuck::PuckR, Params), 0.0, 0.0);

	// Magnetic: the same gaze pulls the puck to the left, toward itself, and it settles there.
	MindPuck::FPuckSim Pulled(Level);
	Pulled.Mode = MindPuck::EGazeMode::Attract;
	Pulled.Step(&Gaze);
	TestTrue(TEXT("the gaze attracts"), Pulled.Vel.X < 0.0 && Pulled.Push.X < 0.0);
	for (int32 i = 0; i < 1200; i++)
	{
		Pulled.Step(&Gaze);
	}
	TestEqual(TEXT("the puck settles on the gaze point"), Pulled.Pos.X, Gaze.X, 0.05);
	TestEqual(TEXT("no sideways drift when pulled"), Pulled.Pos.Y, 4.5, 1e-9);

	// The pull grows like a spring up to PullSoftness, is flat beyond it and has no range limit.
	TestEqual(TEXT("no pull on the gaze point"), MindPuck::PullAccel(0.0, Params), 0.0, 0.0);
	TestEqual(TEXT("half pull at half the softness"), MindPuck::PullAccel(Params.PullSoftness * 0.5, Params), Params.PullStrength * 0.5, 1e-12);
	TestEqual(TEXT("full pull far away"), MindPuck::PullAccel(50.0, Params), Params.PullStrength, 0.0);

	// Steer: nothing at the centre of the puck, full from SteerFull radii out to SteerRange radii,
	// then fading to nothing at one and a half times that.
	const double PuckRadius = MindPuck::PuckR;
	TestEqual(TEXT("no steer at the centre"), MindPuck::SteerAccel(0.0, PuckRadius, Params), 0.0, 0.0);
	TestEqual(TEXT("full steer at the full radius"), MindPuck::SteerAccel(PuckRadius * Params.SteerFull, PuckRadius, Params), Params.SteerStrength, 1e-9);
	TestEqual(TEXT("full steer at the range"), MindPuck::SteerAccel(PuckRadius * Params.SteerRange, PuckRadius, Params), Params.SteerStrength, 0.0);
	TestEqual(TEXT("no steer past the fade"), MindPuck::SteerAccel(PuckRadius * Params.SteerRange * 1.5, PuckRadius, Params), 0.0, 0.0);
	TestEqual(TEXT("no steer far away"), MindPuck::SteerAccel(20.0, PuckRadius, Params), 0.0, 0.0);

	// Looking at the right edge of the puck drives it to the right; looking at its centre does nothing.
	MindPuck::FPuckSim Steered(Level);
	Steered.Mode = MindPuck::EGazeMode::Steer;
	for (int32 i = 0; i < 60; i++)
	{
		const MindPuck::FVec Edge{ Steered.Pos.X + PuckRadius, Steered.Pos.Y };
		Steered.Step(&Edge);
	}
	TestTrue(TEXT("steer drives toward the side looked at"), Steered.Pos.X > 4.0 && Steered.Vel.X > 0.0);
	MindPuck::FPuckSim Rested(Level);
	Rested.Mode = MindPuck::EGazeMode::Steer;
	for (int32 i = 0; i < 60; i++)
	{
		Rested.Step(&Rested.Pos);
	}
	TestEqual(TEXT("looking at the centre lets the puck be"), Rested.Pos.X, 4.0, 1e-12);

	// The order F1 steps through.
	TestTrue(TEXT("mode order"), MindPuck::GazeModes[0] == MindPuck::EGazeMode::Attract &&
		MindPuck::GazeModes[1] == MindPuck::EGazeMode::Steer && MindPuck::GazeModes[2] == MindPuck::EGazeMode::Repel);

	// The walls keep the puck on the table.
	MindPuck::FPuckSim Thrown(Level);
	Thrown.Vel = MindPuck::FVec{ -14.0, -9.0 };
	for (int32 i = 0; i < 600; i++)
	{
		Thrown.Step(nullptr);
	}
	TestTrue(TEXT("the puck stays on the table"),
		Thrown.Pos.X >= Level.PuckRadius - 1e-9 && Thrown.Pos.X <= Level.Width - Level.PuckRadius + 1e-9 &&
		Thrown.Pos.Y >= Level.PuckRadius - 1e-9 && Thrown.Pos.Y <= Level.Height - Level.PuckRadius + 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMindPuckLevelGenTest, "MindPuck.Core.LevelGen",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMindPuckLevelGenTest::RunTest(const FString& Parameters)
{
	// Seed 12345 on the web: level 1 is an open zone, level 6 a cup with seven obstacles.
	const MindPuck::FLevel First = MindPuck::GenerateLevel(12345, 0);
	TestTrue(TEXT("level 1 is a zone"), First.Kind == MindPuck::ELevelKind::Zone);
	TestEqual(TEXT("level 1 has no obstacles"), static_cast<int32>(First.Obstacles.size()), 0);
	TestEqual(TEXT("level 1 start x"), First.Start.X, 12.201043, 1e-6);
	TestEqual(TEXT("level 1 start y"), First.Start.Y, 1.771419, 1e-6);
	TestEqual(TEXT("level 1 goal x"), First.Goal.X, 4.095201, 1e-6);
	TestEqual(TEXT("level 1 goal y"), First.Goal.Y, 5.970526, 1e-6);
	TestEqual(TEXT("level 1 par"), First.Par, 4.1, 1e-9);
	TestEqual(TEXT("level 1 candidates"), First.Attempts, 1);

	const MindPuck::FLevel Sixth = MindPuck::GenerateLevel(12345, 5);
	TestTrue(TEXT("level 6 is a cup"), Sixth.Kind == MindPuck::ELevelKind::Cup);
	TestEqual(TEXT("level 6 obstacles"), static_cast<int32>(Sixth.Obstacles.size()), 7);
	TestEqual(TEXT("level 6 start x"), Sixth.Start.X, 14.697560, 1e-6);
	TestEqual(TEXT("level 6 goal y"), Sixth.Goal.Y, 4.865961, 1e-6);
	TestEqual(TEXT("level 6 par"), Sixth.Par, 4.9, 1e-9);
	TestEqual(TEXT("level 1 magnetic par"), First.ParAttract, 3.3, 1e-9);
	TestEqual(TEXT("level 6 magnetic par"), Sixth.ParAttract, 4.5, 1e-9);

	// Whatever the generator returns, the bot can win it again with the seed that proved it, in
	// both modes. The bot's own default is the repulsor.
	MindPuck::FBotOptions Options;
	TestTrue(TEXT("the bot defaults to the repulsor"), Options.Mode == MindPuck::EGazeMode::Repel);
	Options.Seed = MindPuck::MixSeed(Sixth.Seed, Sixth.Index, 1000 + Sixth.Attempts - 1);
	const MindPuck::FSolveResult Replay = MindPuck::SolveLevel(Sixth, MindPuck::FPhysicsParams(), Options);
	TestTrue(TEXT("the proving run repeats"), Replay.bSolved);
	TestEqual(TEXT("and takes the par time"), FMath::RoundToDouble(Replay.Time * 10.0) / 10.0, Sixth.Par, 1e-9);
	Options.Mode = MindPuck::EGazeMode::Attract;
	const MindPuck::FSolveResult MagneticReplay = MindPuck::SolveLevel(Sixth, MindPuck::FPhysicsParams(), Options);
	TestTrue(TEXT("the magnetic proving run repeats"), MagneticReplay.bSolved);
	TestEqual(TEXT("and takes the magnetic par time"), FMath::RoundToDouble(MagneticReplay.Time * 10.0) / 10.0, Sixth.ParAttract, 1e-9);

	TestEqual(TEXT("level 1 steer par"), First.ParSteer, 4.7, 1e-9);
	TestEqual(TEXT("level 6 steer par"), Sixth.ParSteer, 6.1, 1e-9);
	Options.Mode = MindPuck::EGazeMode::Steer;
	const MindPuck::FSolveResult SteerReplay = MindPuck::SolveLevel(Sixth, MindPuck::FPhysicsParams(), Options);
	TestTrue(TEXT("the steer proving run repeats"), SteerReplay.bSolved);
	TestEqual(TEXT("and takes the steer par time"), FMath::RoundToDouble(SteerReplay.Time * 10.0) / 10.0, Sixth.ParSteer, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMindPuckTriangulateTest, "MindPuck.Game.Triangulate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FMindPuckTriangulateTest::RunTest(const FString& Parameters)
{
	// The L is concave: six corners make four triangles whose areas add up to the L's own area.
	const MindPuck::FPolygon Ell = MindPuck::LPolygon(0, 0, 2, 0.5, 0.7);
	TArray<FVector2D> Outline;
	for (const MindPuck::FVec& Point : Ell)
	{
		Outline.Add(FVector2D(Point.X, Point.Y));
	}
	TArray<int32> Indices;
	MindPuckMesh::Triangulate(Outline, Indices);
	TestEqual(TEXT("four triangles"), Indices.Num(), 12);

	double Area = 0.0;
	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		const FVector2D& A = Outline[Indices[i]];
		const FVector2D& B = Outline[Indices[i + 1]];
		const FVector2D& C = Outline[Indices[i + 2]];
		Area += FMath::Abs((B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X)) * 0.5;
	}
	TestEqual(TEXT("the triangles cover the L exactly"), Area, 2.0 * 0.5 + 1.5 * 0.5, 1e-9);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
