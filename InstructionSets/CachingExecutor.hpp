//
//  CachingExecutor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef CachingExecutor_hpp
#define CachingExecutor_hpp

#include <array>
#include <cstdint>
#include <limits>

namespace InstructionSet {

/*!
	Maps to the smallest of the following integers that can contain max_value:

	* uint8_t;
	* uint16_t;
	* uint32_t; or
	* uint64_t.
*/
template <uint64_t max_value> struct MinIntTypeValue {
	using type =
		std::conditional_t<
			max_value <= std::numeric_limits<uint8_t>::max(), uint8_t,
			std::conditional_t<
				max_value <= std::numeric_limits<uint16_t>::max(), uint16_t,
				std::conditional_t<
					max_value <= std::numeric_limits<uint32_t>::max(), uint32_t,
					uint64_t
				>
			>
		>;
};


/*!
	A caching executor makes use of an instruction set-specific executor to cache 'performers' (i.e. function pointers)
	that result from decoding.

	In other words, it's almost a JIT compiler, but producing threaded code (in the Forth sense) and then incurring whatever
	costs sit behind using the C ABI for calling. Since there'll always be exactly one parameter, being the specific executor,
	hopefully the calling costs are acceptable.

	Intended usage is for specific executors to subclass from this and declare it a friend.

	TODO: determine promises re: interruption, amongst other things.
*/
template <
	/// Indicates the Executor for this platform.
	typename Executor,
	/// Indicates the greatest value the program counter might take.
	uint64_t max_address,
	/// Indicates the maximum number of potential performers that will be provided.
	uint64_t max_performer_count,
	/// Indicates whether performers will acess their decoded instructions; if @c false then instructions are not retained.
	bool keep_instruction_structs
> class CachingExecutor {
	public:

	protected:
		using Performer = void (Executor::*)();
		using PerformerIndex = typename MinIntTypeValue<max_performer_count>::type;

		std::array<Performer, max_performer_count> performers_;
		typename MinIntTypeValue<max_address>::type program_counter_;

	private:
		PerformerIndex actions_[100];
};

}

#endif /* CachingExecutor_hpp */
