#include "Rng.h"

namespace MindPuck
{
	double FRng::operator()()
	{
		// Unsigned 32-bit arithmetic wraps exactly as Math.imul and >>> 0 do in the original.
		A += 0x6d2b79f5u;
		uint32_t T = A;
		T = (T ^ (T >> 15)) * (T | 1u);
		T ^= T + (T ^ (T >> 7)) * (T | 61u);
		return static_cast<double>(T ^ (T >> 14)) / 4294967296.0;
	}

	uint32_t MixSeed(uint32_t Seed, int32_t Index, int32_t Attempt)
	{
		return (Seed * 73856093u) ^ (static_cast<uint32_t>(Index) * 19349663u) ^ (static_cast<uint32_t>(Attempt) * 83492791u);
	}

	double Range(FRng& Rng, double Lo, double Hi)
	{
		return Lo + (Hi - Lo) * Rng();
	}

	double Gaussian(FRng& Rng)
	{
		// Drawn one at a time: the order of the draws and of the additions is part of the sequence.
		const double R1 = Rng();
		const double R2 = Rng();
		const double R3 = Rng();
		const double R4 = Rng();
		return (R1 + R2 + R3 + R4 - 2.0) * 1.7320508075688772;
	}
}
