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
#include <list>
#include <map>
#include <queue>
#include <unordered_map>

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
	/// Provides the type of Instruction to expect.
	typename InstructionType,
	/// Indicates whether instructions should be treated as ephemeral or included in the cache.
	bool retain_instructions
> class CachingExecutor {
	public:
		using Performer = void (Executor::*)();
		using PerformerIndex = typename MinIntTypeValue<max_performer_count>::type;
		using ProgramCounterType = typename MinIntTypeValue<max_address>::type;

		// MARK: - Parser call-ins.

		void announce_overflow(ProgramCounterType) {
			/*
				Should be impossible for now; this is intended to provide information
				when page caching.
			*/
		}
		void announce_instruction(ProgramCounterType, InstructionType instruction) {
			// Dutifully map the instruction to a performer and keep it.
			program_.push_back(static_cast<Executor *>(this)->action_for(instruction));

			if constexpr (retain_instructions) {
				// TODO.
			}
		}

	protected:

		// Storage for the statically-allocated list of performers. It's a bit more
		// work for executors to fill this array, but subsequently performers can be
		// indexed by array position, which is a lot more compact than a generic pointer.
		std::array<Performer, max_performer_count> performers_;

		// TODO: should I include a program counter at all? Are there platforms
		// for which the expense of retaining opcode length doesn't buy any benefit?
		ProgramCounterType program_counter_;


		/*!
			Moves the current point of execution to @c address, updating necessary performer caches
			and doing any translation as is necessary.
		*/
		void set_program_counter(ProgramCounterType address) {
			// Temporary implementation: just interpret.
			program_.clear();
			static_cast<Executor *>(this)->parse(address, ProgramCounterType(max_address));

//			const auto page = find_page(address);
//			const auto entry = page->entry_points.find(address);
//			if(entry == page->entry_points.end()) {
//				// Requested segment wasn't found; check whether it was
//				// within the recently translated list and otherwise
//				// translate it.
//			}
		}

	private:
		std::vector<PerformerIndex> program_;

		/* TODO: almost below here can be shoved off into an LRUCache object, or similar. */

//		static constexpr size_t max_cached_pages = 64;

//		struct Page {
//			std::map<ProgramCounterType, PerformerIndex> entry_points;

			// TODO: can I statically these two? Should I?
//			std::vector<PerformerIndex> actions_;
//			std::vector<typename std::enable_if<!std::is_same<InstructionType, void>::value, InstructionType>::type> instructions_;
//		};
//		std::array<Page, max_cached_pages> pages_;

		// Maps from page numbers to pages.
//		std::unordered_map<ProgramCounterType, Page *> cached_pages_;

		// Maintains an LRU of recently-used pages in case of a need for reuse.
//		std::list<ProgramCounterType> touched_pages_;

		/*!
			Finds or creates the page that contains @c address.
		*/
/*		Page *find_page(ProgramCounterType address) {
			// TODO: are 1kb pages always appropriate? Is 64 the correct amount to keep?
			const auto page_address = ProgramCounterType(address >> 10);

			auto page = cached_pages_.find(page_address);
			if(page == cached_pages_.end()) {
				// Page wasn't found; either allocate a new one or
				// reuse one that already exists.
				if(cached_pages_.size() == max_cached_pages) {

				} else {

				}
			} else {
				// Page was found; LRU shuffle it.
			}

			return nullptr;
		}*/
};

}

#endif /* CachingExecutor_hpp */
