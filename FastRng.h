#ifndef FAST_RNG_H
#define FAST_RNG_H

#include <random>

#include "CoreTypes.h"
#include "Utility.h"

// the number of fast rng elements - must be a power of 2
#define FAST_RNG_COUNT 16

namespace CSX64
{
	static_assert(IsPowerOf2(FAST_RNG_COUNT), "FAST_RNG_COUNT must be a power of 2");

	// represents a fast random number generator - produces poor results but is very fast about it.
	class FastRNG
	{
	private: // -- data -- //

		// the random elements
		u64 elems[FAST_RNG_COUNT];

		// current position in the elems array
		u64 pos;

	public: // -- ctor / dtor / asgn -- //

		// constructs a new FastRNG from the given seed
		FastRNG(unsigned int seed)
		{
			// create a real random object to supply our values
			std::default_random_engine rand(seed);
			
			// fill the random array
			for (u64 i = 0; i < FAST_RNG_COUNT; ++i)
				elems[i] = ((u64)rand() << 32) | rand();

			// set start position
			pos = 0;
		}

		FastRNG(const FastRNG&) = default;
		FastRNG(FastRNG&&) = default;

		FastRNG &operator=(const FastRNG&) = default;
		FastRNG &operator=(FastRNG&&) = default;

	public: // -- interface -- //

		// returns the next 64-bit random number.
		// in the spirit of being fast, calls are not thread-safe
		u64 operator()() noexcept { return elems[pos = (pos + 1) & (FAST_RNG_COUNT - 1)]; }
	};
}

#endif
