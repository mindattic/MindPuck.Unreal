#pragma once

#include "Geometry.h"
#include "Level.h"

#include <algorithm>
#include <vector>

// Port of MindPuck/src/core/physics.ts.

namespace MindPuck
{
	/** Every number that shapes how the table feels. The web repo's docs/DESIGN.md explains each one. */
	struct FPhysicsParams
	{
		/** Peak push from the gaze point, in units per second squared. */
		double RepelStrength = 40.0;
		/** Distance at which the push has fallen to roughly half. Larger means a softer, wider field. */
		double RepelSoftness = 2.0;
		/** Beyond this distance the gaze does not push at all, so looking away lets the puck coast. */
		double RepelRange = 6.0;
		/** Looking at the very centre of the puck pushes nowhere; the push fades in over this fraction of its radius. */
		double DeadZone = 0.3;
		/** Air drag: the fraction of speed lost per second while gliding. */
		double Drag = 0.55;
		/** Drag on the goal pad, which is what lets the puck come to rest there. */
		double PadDrag = 3.5;
		/** A small constant deceleration, so a slow puck actually stops. */
		double Friction = 0.06;
		/** Fraction of speed kept, along the wall's normal, after a bounce. */
		double Restitution = 0.82;
		/** Fraction of sliding speed lost along the wall in a bounce. */
		double WallFriction = 0.03;
		double MaxSpeed = 14.0;
		/** Fixed simulation step, in seconds. */
		double Dt = 1.0 / 120.0;
		/** How long the puck must sit on the pad to win. */
		double Dwell = 0.5;
		/** The puck counts as resting on the pad below this speed. */
		double RestSpeed = 1.5;
	};

	/** The defaults, the same values as DEFAULT_PARAMS in the original. */
	inline FPhysicsParams DefaultParams() { return FPhysicsParams(); }

	/**
	 * How hard the gaze pushes the puck from distance D: strongest close in, fading with distance,
	 * nothing past the range, and nothing at dead centre (where there is no direction to push in).
	 */
	double RepelAccel(double D, double PuckRadius, const FPhysicsParams& P);

	/** The win radius: how close the puck's centre must be to the goal's centre. */
	double WinRadius(const FLevel& Level);

	/**
	 * The puck on the table. A fixed-step simulation: gaze repulsion, drag, and circle-against-polygon
	 * collisions with every wall segment, so the puck bounces correctly off faces and corners alike.
	 *
	 * The simulation keeps a reference to the level: the level must outlive it.
	 */
	class FPuckSim
	{
	public:
		FVec Pos;
		FVec Vel;
		/** Seconds simulated since the last reset. */
		double Time = 0.0;
		/** Seconds the puck has been resting on the pad. */
		double Dwell = 0.0;
		bool bWon = false;
		/** The hardest hit during the last Advance, as speed into the wall. For sound and effects. */
		double Impact = 0.0;
		/** The push applied in the last step, for drawing. */
		FVec Push;

		const FLevel& Level;
		const FPhysicsParams Params;

		explicit FPuckSim(const FLevel& InLevel, const FPhysicsParams& InParams = FPhysicsParams());

		void Reset();

		/** Advance by real elapsed time, in whole fixed steps. Gaze is null when the eyes are not tracked. */
		int Advance(double Seconds, const FVec* Gaze);

		/**
		 * Advance like Advance, asking GazeForStep for the gaze point before each step. This is the
		 * frame loop of the original's main.ts, used when the solver bot plays live. It stops early
		 * once the level is won. GazeForStep returns a const FVec* (null for no gaze).
		 */
		template <typename TGazeForStep>
		int AdvanceWith(double Seconds, TGazeForStep&& GazeForStep)
		{
			Impact = 0.0;
			Accumulator += (std::min)(Seconds, 0.1);
			int Steps = 0;
			while (Accumulator >= Params.Dt && !bWon)
			{
				Accumulator -= Params.Dt;
				Step(GazeForStep());
				Steps++;
			}
			return Steps;
		}

		/** One fixed step. */
		void Step(const FVec* Gaze);

	private:
		/** Every wall segment, flattened: ax, ay, bx, by. */
		std::vector<double> Segments;
		double Accumulator = 0.0;

		void Bounce(double Nx, double Ny);
		void CollideArena();
		void CollideObstacles();
	};
}
