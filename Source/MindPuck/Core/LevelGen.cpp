#include "LevelGen.h"

#include "Bot.h"
#include "Geometry.h"
#include "NavGrid.h"
#include "Rng.h"

#include <algorithm>
#include <cmath>
#include <vector>

// A note on porting: the original calls rng() inside argument lists, where JavaScript evaluates
// left to right. C++ does not promise an order for function arguments, so every draw here is
// taken into a local first, in the original's order.

namespace MindPuck
{
	namespace
	{
		const int GenMaxAttempts = 60;
		/** Two obstacles either touch or leave at least this much room: the puck plus a margin. */
		const double GenMinGap = PuckR * 2 + 0.25;
		const double GenCupInner = GoalR + 0.15;
		const double GenCupWall = 0.22;
		/** How far the cup's corners reach from the goal's centre. */
		const double GenCupReach = (GenCupInner + GenCupWall) * Sqrt2;

		/** Math.round: halves round toward positive infinity. */
		double JsRound(double V)
		{
			return std::floor(V + 0.5);
		}

		/** Three walls around the goal, leaving the side at Angle open. */
		void AppendCupWalls(std::vector<FPolygon>& Out, const FVec& Goal, double Angle)
		{
			const double C = std::cos(Angle);
			const double S = std::sin(Angle);
			const double H = GenCupInner;
			const double T = GenCupWall;
			auto Wall = [&Goal, C, S, Angle](double Lx, double Ly, double Hw, double Hh)
			{
				return RectPolygon(Goal.X + Lx * C - Ly * S, Goal.Y + Lx * S + Ly * C, Hw, Hh, Angle);
			};
			Out.push_back(Wall(-(H + T / 2), 0, T / 2, H + T)); // back
			Out.push_back(Wall(-T / 2, -(H + T / 2), H + T / 2, T / 2)); // one side
			Out.push_back(Wall(-T / 2, H + T / 2, H + T / 2, T / 2)); // the other side
		}

		FPolygon RandomShape(FRng& Rng, double X, double Y)
		{
			const double Pick = Rng();
			const double Angle = Range(Rng, 0, Pi * 2);
			if (Pick < 0.34)
			{
				const double Hw = Range(Rng, 0.8, 2.1);
				const double Hh = Range(Rng, 0.16, 0.3);
				return RectPolygon(X, Y, Hw, Hh, Angle);
			}
			if (Pick < 0.52)
			{
				const double Radius = Range(Rng, 0.6, 1.1);
				return RegularPolygon(X, Y, Radius, 3, Angle);
			}
			if (Pick < 0.7)
			{
				const double Radius = Range(Rng, 0.5, 1);
				const int Sides = 5 + static_cast<int>(std::floor(Rng() * 2));
				return RegularPolygon(X, Y, Radius, Sides, Angle);
			}
			if (Pick < 0.86)
			{
				const double Arm = Range(Rng, 1, 1.7);
				const double Thick = Range(Rng, 0.3, 0.42);
				return LPolygon(X, Y, Arm, Thick, Angle);
			}
			std::vector<double> Radii;
			const double Base = Range(Rng, 0.55, 1);
			for (int i = 0; i < 7; i++)
			{
				Radii.push_back(Base * Range(Rng, 0.75, 1.15));
			}
			return BlobPolygon(X, Y, Radii, Angle);
		}

		FLevel BuildCandidate(FRng& Rng, uint32_t Seed, int32_t Index)
		{
			const int32_t D = Index;
			const ELevelKind Kind = (D >= 2 && Rng() < 0.5) ? ELevelKind::Cup : ELevelKind::Zone;
			const bool bFlip = Rng() < 0.5;
			const double GoalMargin = Kind == ELevelKind::Cup ? GenCupReach + 0.15 : GoalR + 0.35;
			FVec Start;
			Start.X = Range(Rng, 1.3, ArenaW * 0.24);
			Start.Y = Range(Rng, 1.3, ArenaH - 1.3);
			FVec GoalPos;
			GoalPos.X = Range(Rng, ArenaW * 0.7, ArenaW - GoalMargin);
			GoalPos.Y = Range(Rng, GoalMargin, ArenaH - GoalMargin);
			if (bFlip)
			{
				Start.X = ArenaW - Start.X;
				GoalPos.X = ArenaW - GoalPos.X;
			}

			std::vector<FPolygon> Obstacles;
			double CupAngle = 0.0;
			if (Kind == ELevelKind::Cup)
			{
				// The opening faces one of eight directions. Early on it faces the puck; later it may not.
				const double Toward = std::atan2(Start.Y - GoalPos.Y, Start.X - GoalPos.X);
				const double Step = Pi / 4;
				const double Turn = D < 4 ? 0.0 : std::floor(Range(Rng, -2, 3));
				CupAngle = (JsRound(Toward / Step) + Turn) * Step;
				AppendCupWalls(Obstacles, GoalPos, CupAngle);
			}

			// Gates: a wall across the table with one gap in it.
			int Gates = 0;
			if (D < 4)
			{
				Gates = 0;
			}
			else if (D < 9)
			{
				Gates = Rng() < 0.6 ? 1 : 0;
			}
			else
			{
				Gates = Rng() < 0.5 ? 2 : 1;
			}
			for (int i = 0; i < Gates; i++)
			{
				const double T = Gates == 1 ? Range(Rng, 0.38, 0.62) : i == 0 ? Range(Rng, 0.3, 0.4) : Range(Rng, 0.58, 0.68);
				const double X = Start.X + (GoalPos.X - Start.X) * T;
				const double Half = PuckR * (std::max)(1.7, 2.6 - D * 0.06);
				const double GapY = Range(Rng, Half + 0.6, ArenaH - Half - 0.6);
				const double Thick = 0.16;
				const double Top = GapY - Half;
				const double Bottom = GapY + Half;
				Obstacles.push_back(RectPolygon(X, Top / 2, Thick, Top / 2));
				Obstacles.push_back(RectPolygon(X, (Bottom + ArenaH) / 2, Thick, (ArenaH - Bottom) / 2));
			}

			// Loose obstacles: bars, triangles, polygons, L shapes and rocks.
			const size_t Fixed = Obstacles.size();
			const int Wanted = D == 0 ? 0 : (std::min)(1 + static_cast<int>(std::floor(D * 0.7)), 8);
			const double GoalClear = Kind == ELevelKind::Cup ? GenCupReach + GenMinGap : GoalR + GenMinGap;
			const double Inset = GenMinGap;
			for (int n = 0; n < Wanted; n++)
			{
				for (int Tries = 0; Tries < 24; Tries++)
				{
					const double ShapeX = Range(Rng, 1.2, ArenaW - 1.2);
					const double ShapeY = Range(Rng, 1.2, ArenaH - 1.2);
					const FPolygon Shape = RandomShape(Rng, ShapeX, ShapeY);
					bool bOutside = false;
					for (const FVec& V : Shape)
					{
						if (V.X < Inset || V.Y < Inset || V.X > ArenaW - Inset || V.Y > ArenaH - Inset)
						{
							bOutside = true;
							break;
						}
					}
					if (bOutside)
					{
						continue;
					}
					if (PointPolygonDistance(Start.X, Start.Y, Shape) < PuckR + 1)
					{
						continue;
					}
					if (PointPolygonDistance(GoalPos.X, GoalPos.Y, Shape) < GoalClear)
					{
						continue;
					}
					bool bFits = true;
					for (size_t o = 0; bFits && o < Obstacles.size(); o++)
					{
						const double Gap = PolygonDistance(Shape, Obstacles[o]);
						// Loose obstacles may overlap each other, but never the cup or a gate.
						if (Gap < GenMinGap && (o < Fixed || Gap > 0))
						{
							bFits = false;
						}
					}
					if (!bFits)
					{
						continue;
					}
					Obstacles.push_back(Shape);
					break;
				}
			}

			FLevel Level;
			Level.Seed = Seed;
			Level.Index = Index;
			Level.Width = ArenaW;
			Level.Height = ArenaH;
			Level.PuckRadius = PuckR;
			Level.Start = Start;
			Level.Goal.X = GoalPos.X;
			Level.Goal.Y = GoalPos.Y;
			Level.Goal.R = GoalR;
			Level.Kind = Kind;
			Level.CupAngle = CupAngle;
			Level.Obstacles = std::move(Obstacles);
			Level.Par = 0.0;
			Level.ParAttract = 0.0;
			Level.ParSteer = 0.0;
			Level.Attempts = 0;
			return Level;
		}

		/** Route check first (cheap), then the bot actually plays it, in every mode. Fills in the pars and Attempts. */
		bool Verify(FLevel& Level, const FPhysicsParams& Params, int32_t Attempt)
		{
			const FNavGrid Grid(Level);
			std::vector<FVec> Route;
			if (!Grid.FindPath(Level.Start, Level.Goal, Route))
			{
				return false;
			}
			// The level has to be winnable every way the game can be played.
			FBotOptions Options;
			Options.Seed = MixSeed(Level.Seed, Level.Index, 1000 + Attempt);
			Options.Mode = EGazeMode::Repel;
			const FSolveResult Result = SolveLevel(Level, Params, Options, &Grid);
			if (!Result.bSolved)
			{
				return false;
			}
			Options.Mode = EGazeMode::Attract;
			const FSolveResult Pulled = SolveLevel(Level, Params, Options, &Grid);
			if (!Pulled.bSolved)
			{
				return false;
			}
			Options.Mode = EGazeMode::Steer;
			const FSolveResult Steered = SolveLevel(Level, Params, Options, &Grid);
			if (!Steered.bSolved)
			{
				return false;
			}
			Level.Par = JsRound(Result.Time * 10) / 10;
			Level.ParSteer = JsRound(Steered.Time * 10) / 10;
			Level.ParAttract = JsRound(Pulled.Time * 10) / 10;
			Level.Attempts = Attempt + 1;
			return true;
		}
	}

	FLevel GenerateLevel(uint32_t Seed, int32_t Index, const FPhysicsParams& Params)
	{
		for (int32_t Attempt = 0; Attempt < GenMaxAttempts; Attempt++)
		{
			FRng Rng = Mulberry32(MixSeed(Seed, Index, Attempt));
			FLevel Level = BuildCandidate(Rng, Seed, Index);
			if (Verify(Level, Params, Attempt))
			{
				return Level;
			}
		}
		// Nothing passed (this should not happen): fall back to an open table, which always can be won.
		FRng Rng = Mulberry32(MixSeed(Seed, Index, GenMaxAttempts));
		FLevel Open = BuildCandidate(Rng, Seed, 0);
		Open.Index = Index;
		Open.Attempts = GenMaxAttempts + 1;
		Verify(Open, Params, GenMaxAttempts);
		return Open;
	}
}
