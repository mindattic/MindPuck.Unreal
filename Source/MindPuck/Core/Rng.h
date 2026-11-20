#pragma once

#include <cstdint>

// Port of MindPuck/src/core/rng.ts.

namespace MindPuck
{
	/**
	 * mulberry32: a tiny 32-bit generator. It uses only 32-bit integer arithmetic, so this port
	 * produces the same sequence as the TypeScript original for the same seed.
	 */
	class FRng
	{
	public:
		explicit FRng(uint32_t Seed) : A(Seed) {}

		/** The next value in [0, 1). */
		double operator()();

	private:
		uint32_t A;
	};

	inline FRng Mulberry32(uint32_t Seed) { return FRng(Seed); }

	/** Mix a seed, a level number and an attempt number into one 32-bit seed. */
	uint32_t MixSeed(uint32_t Seed, int32_t Index, int32_t Attempt);

	double Range(FRng& Rng, double Lo, double Hi);

	/** Roughly normal with unit variance: the sum of four uniforms, rescaled. Cheap and portable. */
	double Gaussian(FRng& Rng);
}
