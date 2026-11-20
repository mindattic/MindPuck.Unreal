#include "Bot.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MindPuck
{
	namespace
	{
		const int BotDecideEvery = 4; // simulation steps between decisions: 30 a second at 120 steps a second
		const int BotReplanEvery = 60; // simulation steps between route searches: twice a second
		const double BotMinStandoff = 0.8;
	}

	FSolverBot::FSolverBot(const FLevel& InLevel, const FPhysicsParams& InParams, const FBotOptions& InOptions, const FNavGrid* InGrid)
		: Level(InLevel)
		, Params(InParams)
		, Options(InOptions)
		, Grid(InGrid)
		, Rng(InOptions.Seed)
	{
		if (!Grid)
		{
			OwnedGrid = std::make_unique<FNavGrid>(InLevel);
			Grid = OwnedGrid.get();
		}
		Target = FVec{ InLevel.Goal.X, InLevel.Goal.Y };
		Aim = FarCorner(InLevel.Start);
		Eye = Aim;
		Gaze = Aim;
	}

	FVec FSolverBot::FarCorner(const FVec& From) const
	{
		return FVec{
			From.X < Level.Width / 2 ? Level.Width - 0.2 : 0.2,
			From.Y < Level.Height / 2 ? Level.Height - 0.2 : 0.2,
		};
	}

	const FVec& FSolverBot::Update(const FPuckSim& Sim)
	{
		const double Dt = Params.Dt;
		if (Tick % BotReplanEvery == 0)
		{
			Replan(Sim.Pos);
		}
		if (Tick % BotDecideEvery == 0)
		{
			Decide(Sim);
		}
		Tick++;

		// The eye travels toward where the bot wants to look, and the tracker's error wanders around it.
		const double Follow = 1 - std::exp(-Dt / Options.Lag);
		Eye.X += (Aim.X - Eye.X) * Follow;
		Eye.Y += (Aim.Y - Eye.Y) * Follow;
		const double Tau = Options.NoiseTime;
		const double Kick = Options.Noise * std::sqrt((2 * Dt) / Tau);
		// One draw per axis, x first: the order is part of the sequence.
		const double KickX = Kick * Gaussian(Rng);
		Wobble.X += (-Wobble.X / Tau) * Dt + KickX;
		const double KickY = Kick * Gaussian(Rng);
		Wobble.Y += (-Wobble.Y / Tau) * Dt + KickY;
		Gaze.X = Eye.X + Wobble.X;
		Gaze.Y = Eye.Y + Wobble.Y;
		return Gaze;
	}

	void FSolverBot::Replan(const FVec& From)
	{
		std::vector<FVec> Found;
		if (!Grid->FindPath(From, Level.Goal, Found) || Found.empty())
		{
			return;
		}
		Path = Found;
		// Head for the farthest point on the route that can be reached in a straight line.
		FVec Pick = Path[(std::min)(static_cast<size_t>(1), Path.size() - 1)];
		for (size_t i = Path.size() - 1; i >= 1; i--)
		{
			if (Grid->LineClear(From, Path[i]))
			{
				Pick = Path[i];
				break;
			}
		}
		Target = Pick;
	}

	void FSolverBot::Decide(const FPuckSim& Sim)
	{
		const FLevel& Lv = Level;
		const FPhysicsParams& P = Params;
		const FVec& Pos = Sim.Pos;
		const FVec& Vel = Sim.Vel;
		const double ToGoal = std::hypot(Lv.Goal.X - Pos.X, Lv.Goal.Y - Pos.Y);
		const double Speed = std::hypot(Vel.X, Vel.Y);

		// Magnetic: simply look where the puck should go next. The pull does the rest.
		if (Options.Mode == EGazeMode::Attract)
		{
			Aim.X = Target.X;
			Aim.Y = Target.Y;
			return;
		}

		// Sitting on the pad: look away and let it settle.
		if (ToGoal < WinRadius(Lv) * 0.7 && Speed < P.RestSpeed * 0.8)
		{
			Aim = FarCorner(Pos);
			return;
		}

		double Tx = Target.X - Pos.X;
		double Ty = Target.Y - Pos.Y;
		const double Td = std::hypot(Tx, Ty);
		if (Td > 1e-6)
		{
			Tx /= Td;
			Ty /= Td;
		}
		const bool bFinalLeg = std::hypot(Target.X - Lv.Goal.X, Target.Y - Lv.Goal.Y) < 0.3;
		// Cruise along the route; on the last leg, slow down so as to arrive rather than fly through.
		const double Wanted = bFinalLeg ? (std::min)(Options.Cruise, 1.3 * Td) : Options.Cruise;
		double Ax = (Tx * Wanted - Vel.X) * 3;
		double Ay = (Ty * Wanted - Vel.Y) * 3;
		double A = std::hypot(Ax, Ay);
		if (A < 0.08)
		{
			Aim = FarCorner(Pos);
			return;
		}
		Ax /= A;
		Ay /= A;
		A = (std::min)(A, 18.0);

		// Find the standoff distance whose push matches the acceleration wanted. The push falls with
		// distance, so a bisection finds it.
		double Lo = BotMinStandoff;
		double Hi = P.RepelRange;
		if (RepelAccel(Lo, Lv.PuckRadius, P) <= A)
		{
			Hi = Lo;
		}
		else
		{
			for (int i = 0; i < 18; i++)
			{
				const double Mid = (Lo + Hi) / 2;
				if (RepelAccel(Mid, Lv.PuckRadius, P) > A)
				{
					Lo = Mid;
				}
				else
				{
					Hi = Mid;
				}
			}
		}
		double D = Hi;

		// The gaze point sits behind the puck, opposite the way it should accelerate. It has to be on
		// the table: if the wall is closer than that, look at the wall's edge (a harder push than
		// wanted, which the next decision corrects).
		const double Edge = 0.1;
		double Room = std::numeric_limits<double>::infinity();
		if (Ax > 1e-6)
		{
			Room = (std::min)(Room, (Pos.X - Edge) / Ax);
		}
		if (Ax < -1e-6)
		{
			Room = (std::min)(Room, (Pos.X - (Lv.Width - Edge)) / Ax);
		}
		if (Ay > 1e-6)
		{
			Room = (std::min)(Room, (Pos.Y - Edge) / Ay);
		}
		if (Ay < -1e-6)
		{
			Room = (std::min)(Room, (Pos.Y - (Lv.Height - Edge)) / Ay);
		}
		if (Room < D)
		{
			D = (std::max)(Room, 0.05);
		}
		Aim.X = Pos.X - Ax * D;
		Aim.Y = Pos.Y - Ay * D;
	}

	FSolveResult SolveLevel(const FLevel& Level, const FPhysicsParams& Params, const FBotOptions& Options, const FNavGrid* Grid)
	{
		FPuckSim Sim(Level, Params);
		Sim.Mode = Options.Mode;
		FSolverBot Bot(Level, Params, Options, Grid);
		const int MaxSteps = static_cast<int>(std::ceil(Options.TimeLimit / Params.Dt));
		for (int i = 0; i < MaxSteps; i++)
		{
			Sim.Step(&Bot.Update(Sim));
			if (Sim.bWon)
			{
				return FSolveResult{ true, Sim.Time };
			}
		}
		return FSolveResult{ false, Options.TimeLimit };
	}
}
