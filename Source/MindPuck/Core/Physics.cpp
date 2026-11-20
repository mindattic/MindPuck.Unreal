#include "Physics.h"

#include <cmath>
#include <limits>

namespace MindPuck
{
	double RepelAccel(double D, double PuckRadius, const FPhysicsParams& P)
	{
		if (D >= P.RepelRange)
		{
			return 0.0;
		}
		const double Reach = 1 - D / P.RepelRange;
		const double Core = P.RepelStrength / (1 + (D / P.RepelSoftness) * (D / P.RepelSoftness));
		const double Edge = P.DeadZone * PuckRadius;
		double Centre = 1.0;
		if (D < Edge)
		{
			const double U = D / Edge;
			Centre = U * U * (3 - 2 * U);
		}
		return Core * Reach * Reach * Centre;
	}

	double WinRadius(const FLevel& Level)
	{
		return Level.Goal.R - Level.PuckRadius * 0.6;
	}

	FPuckSim::FPuckSim(const FLevel& InLevel, const FPhysicsParams& InParams)
		: Level(InLevel)
		, Params(InParams)
	{
		Pos = FVec{ InLevel.Start.X, InLevel.Start.Y };
		for (const FPolygon& Poly : InLevel.Obstacles)
		{
			const size_t Count = Poly.size();
			for (size_t i = 0, j = Count - 1; i < Count; j = i++)
			{
				Segments.push_back(Poly[j].X);
				Segments.push_back(Poly[j].Y);
				Segments.push_back(Poly[i].X);
				Segments.push_back(Poly[i].Y);
			}
		}
	}

	void FPuckSim::Reset()
	{
		Pos = FVec{ Level.Start.X, Level.Start.Y };
		Vel = FVec{ 0.0, 0.0 };
		Time = 0.0;
		Dwell = 0.0;
		bWon = false;
		Impact = 0.0;
		Accumulator = 0.0;
	}

	int FPuckSim::Advance(double Seconds, const FVec* Gaze)
	{
		Impact = 0.0;
		Accumulator += (std::min)(Seconds, 0.1);
		int Steps = 0;
		while (Accumulator >= Params.Dt)
		{
			Accumulator -= Params.Dt;
			Step(Gaze);
			Steps++;
		}
		return Steps;
	}

	void FPuckSim::Step(const FVec* Gaze)
	{
		const FPhysicsParams& P = Params;
		const FLevel& Lv = Level;
		const double R = Lv.PuckRadius;
		const double Dt = P.Dt;

		double Ax = 0.0;
		double Ay = 0.0;
		if (Gaze)
		{
			const double Dx = Pos.X - Gaze->X;
			const double Dy = Pos.Y - Gaze->Y;
			const double D = std::hypot(Dx, Dy);
			if (D > 1e-9)
			{
				const double A = RepelAccel(D, R, P);
				Ax = (Dx / D) * A;
				Ay = (Dy / D) * A;
			}
		}
		Push.X = Ax;
		Push.Y = Ay;
		Vel.X += Ax * Dt;
		Vel.Y += Ay * Dt;

		const double Gx = Pos.X - Lv.Goal.X;
		const double Gy = Pos.Y - Lv.Goal.Y;
		const double GoalDist = std::hypot(Gx, Gy);
		const double Damp = std::exp(-(GoalDist < Lv.Goal.R ? P.PadDrag : P.Drag) * Dt);
		Vel.X *= Damp;
		Vel.Y *= Damp;
		double Speed = std::hypot(Vel.X, Vel.Y);
		if (Speed > 0)
		{
			const double Slowed = (std::min)((std::max)(Speed - P.Friction * Dt, 0.0), P.MaxSpeed);
			Vel.X *= Slowed / Speed;
			Vel.Y *= Slowed / Speed;
			Speed = Slowed;
		}

		Pos.X += Vel.X * Dt;
		Pos.Y += Vel.Y * Dt;

		CollideObstacles();
		CollideArena();

		Time += Dt;
		const bool bResting = std::hypot(Pos.X - Lv.Goal.X, Pos.Y - Lv.Goal.Y) <= WinRadius(Lv) && Speed < P.RestSpeed;
		Dwell = bResting ? Dwell + Dt : 0.0;
		if (Dwell >= P.Dwell)
		{
			bWon = true;
		}
	}

	void FPuckSim::Bounce(double Nx, double Ny)
	{
		const double Vn = Vel.X * Nx + Vel.Y * Ny;
		if (Vn >= 0)
		{
			return;
		}
		// Split the velocity into the part going into the wall, which bounces, and the part sliding
		// along it, which loses a little to friction.
		const double Tx = Vel.X - Vn * Nx;
		const double Ty = Vel.Y - Vn * Ny;
		const double Keep = 1 - Params.WallFriction;
		Vel.X = Tx * Keep - Vn * Params.Restitution * Nx;
		Vel.Y = Ty * Keep - Vn * Params.Restitution * Ny;
		if (-Vn > Impact)
		{
			Impact = -Vn;
		}
	}

	void FPuckSim::CollideArena()
	{
		const double R = Level.PuckRadius;
		if (Pos.X < R)
		{
			Pos.X = R;
			Bounce(1, 0);
		}
		else if (Pos.X > Level.Width - R)
		{
			Pos.X = Level.Width - R;
			Bounce(-1, 0);
		}
		if (Pos.Y < R)
		{
			Pos.Y = R;
			Bounce(0, 1);
		}
		else if (Pos.Y > Level.Height - R)
		{
			Pos.Y = Level.Height - R;
			Bounce(0, -1);
		}
	}

	void FPuckSim::CollideObstacles()
	{
		const double R = Level.PuckRadius;

		// The step is far smaller than the puck, so its centre cannot cross a wall in one step. If it
		// ever ends up inside a shape anyway (pinched in a corner), put it back on the nearest face.
		for (const FPolygon& Poly : Level.Obstacles)
		{
			if (!PointInPolygon(Pos.X, Pos.Y, Poly))
			{
				continue;
			}
			double Best = std::numeric_limits<double>::infinity();
			double Qx = Pos.X;
			double Qy = Pos.Y;
			const size_t Count = Poly.size();
			for (size_t i = 0, j = Count - 1; i < Count; j = i++)
			{
				const FVec Q = ClosestOnSegment(Pos.X, Pos.Y, Poly[j].X, Poly[j].Y, Poly[i].X, Poly[i].Y);
				const double D = std::hypot(Pos.X - Q.X, Pos.Y - Q.Y);
				if (D < Best)
				{
					Best = D;
					Qx = Q.X;
					Qy = Q.Y;
				}
			}
			if (Best > 1e-9)
			{
				const double Nx = (Qx - Pos.X) / Best;
				const double Ny = (Qy - Pos.Y) / Best;
				Pos.X = Qx + Nx * R;
				Pos.Y = Qy + Ny * R;
				Bounce(Nx, Ny);
			}
		}

		// Resolve against every segment the puck overlaps. A corner is just the end of a segment, so
		// the same closest-point test handles faces and corners. A few passes settle the puck when it
		// touches two walls at once.
		for (int Pass = 0; Pass < 4; Pass++)
		{
			bool bTouched = false;
			for (size_t i = 0; i < Segments.size(); i += 4)
			{
				const FVec Q = ClosestOnSegment(Pos.X, Pos.Y, Segments[i], Segments[i + 1], Segments[i + 2], Segments[i + 3]);
				const double Dx = Pos.X - Q.X;
				const double Dy = Pos.Y - Q.Y;
				const double D2 = Dx * Dx + Dy * Dy;
				if (D2 >= R * R || D2 < 1e-18)
				{
					continue;
				}
				const double D = std::sqrt(D2);
				const double Nx = Dx / D;
				const double Ny = Dy / D;
				Pos.X += Nx * (R - D);
				Pos.Y += Ny * (R - D);
				Bounce(Nx, Ny);
				bTouched = true;
			}
			if (!bTouched)
			{
				break;
			}
		}
	}
}
