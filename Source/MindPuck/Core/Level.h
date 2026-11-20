#pragma once

#include "Geometry.h"

#include <cstdint>
#include <vector>

// Port of MindPuck.Web/src/core/level.ts.

namespace MindPuck
{
	/**
	 * World units. The table is 16 by 9 with the origin at its top-left corner and y pointing down,
	 * so a normalized gaze point (0..1) times (16, 9) is a table position.
	 */
	constexpr double ArenaW = 16.0;
	constexpr double ArenaH = 9.0;
	constexpr double PuckR = 0.42;
	constexpr double GoalR = 1.1;

	struct FGoal : FVec
	{
		/** Radius of the goal pad. */
		double R = 0.0;
	};

	/** Zone is an open pad. Cup is a pad inside a box with one side missing. */
	enum class ELevelKind
	{
		Zone,
		Cup,
	};

	struct FLevel
	{
		uint32_t Seed = 0;
		int32_t Index = 0;
		double Width = ArenaW;
		double Height = ArenaH;
		double PuckRadius = PuckR;
		FVec Start;
		FGoal Goal;
		ELevelKind Kind = ELevelKind::Zone;
		/** For a cup, the direction its open side faces, in radians. */
		double CupAngle = 0.0;
		/** Everything the puck bounces off, the cup's walls included. */
		std::vector<FPolygon> Obstacles;
		/** How long the solver took to win in repulsor mode, in seconds. Shown to the player as the time to beat. */
		double Par = 0.0;
		/** The same for magnetic mode. */
		double ParAttract = 0.0;
		/** The same for steer mode. */
		double ParSteer = 0.0;
		/** How many candidates were generated before one passed the solver. */
		int32_t Attempts = 0;
	};
}
