#pragma once

#include "Geometry.h"
#include "Level.h"
#include "NavGrid.h"
#include "Physics.h"
#include "Rng.h"

#include <cstdint>
#include <memory>
#include <vector>

// Port of MindPuck.Web/src/core/bot.ts.

namespace MindPuck
{
	struct FBotOptions
	{
		/** Gaze error, as a standard deviation in table units. A calibrated webcam is around 0.3 to 0.6. */
		double Noise = 0.4;
		/** How slowly the gaze error wanders, in seconds. */
		double NoiseTime = 0.3;
		/** How long the eyes take to reach a new point, in seconds. */
		double Lag = 0.12;
		/** The bot gives up after this many simulated seconds. */
		double TimeLimit = 75.0;
		/** The speed the bot tries to travel at. */
		double Cruise = 3.2;
		uint32_t Seed = 1;
	};

	/**
	 * A stand-in player. It finds a route, then steers the puck along it using nothing but a gaze
	 * point, with the unsteadiness and delay of a real eye tracker. If it can win a level, a person can.
	 *
	 * The bot keeps a reference to the level (and to the grid, when one is passed in): both must
	 * outlive it.
	 */
	class FSolverBot
	{
	public:
		/** Where the bot is looking, noise included: what the simulation is given. */
		FVec Gaze;
		/** The route it is following, for drawing. */
		std::vector<FVec> Path;

		/** With no grid passed in, the bot builds its own. */
		explicit FSolverBot(const FLevel& InLevel, const FPhysicsParams& InParams = FPhysicsParams(),
			const FBotOptions& InOptions = FBotOptions(), const FNavGrid* InGrid = nullptr);

		/** Call once per simulation step, before stepping. Returns the gaze point to apply. */
		const FVec& Update(const FPuckSim& Sim);

	private:
		const FLevel& Level;
		const FPhysicsParams Params;
		const FBotOptions Options;
		std::unique_ptr<FNavGrid> OwnedGrid;
		const FNavGrid* Grid;
		FRng Rng;
		FVec Aim;
		FVec Eye;
		FVec Wobble;
		FVec Target;
		int Tick = 0;

		/** The corner of the table farthest from a point: where to look to stop pushing. */
		FVec FarCorner(const FVec& From) const;
		void Replan(const FVec& From);
		void Decide(const FPuckSim& Sim);
	};

	struct FSolveResult
	{
		bool bSolved = false;
		/** Seconds the bot took, or the time limit if it failed. */
		double Time = 0.0;
	};

	/** Play a level with the bot. This is the proof that the level can be won. */
	FSolveResult SolveLevel(const FLevel& Level, const FPhysicsParams& Params = FPhysicsParams(),
		const FBotOptions& Options = FBotOptions(), const FNavGrid* Grid = nullptr);
}
