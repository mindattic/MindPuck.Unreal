#pragma once

#include "Level.h"
#include "Physics.h"

#include <cstdint>

// Port of MindPuck.Web/src/core/levelgen.ts.

namespace MindPuck
{
	/**
	 * Build level number Index of the run identified by Seed. Levels get harder as the index
	 * grows. Every level returned has been won by the solver bot (see Bot.h), with an unsteady gaze,
	 * in every gaze mode, so it is known to be possible; Par, ParAttract and ParSteer are the bot's times.
	 */
	FLevel GenerateLevel(uint32_t Seed, int32_t Index, const FPhysicsParams& Params = FPhysicsParams());
}
