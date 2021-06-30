//
//  Disassembler.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Disassembler_hpp
#define Disassembler_hpp

#include "../Numeric/Sizes.hpp"

#include <list>
#include <map>
#include <set>

namespace InstructionSet {

template <
	/// Indicates the Parser for this platform.
	template<typename, bool> class ParserType,
	/// Indicates the greatest value the program counter might take.
	uint64_t max_address,
	/// Provides the type of Instruction to expect.
	typename InstructionType,
	/// Provides the storage size used for memory.
	typename MemoryWord,
	/// Provides the addressing range of memory.
	typename AddressType
> class Disassembler {
	public:
		using ProgramCounterType = typename MinIntTypeValue<max_address>::type;

		/*!
			Adds the result of disassembling @c memory which is @c length @c MemoryWords long from @c start_address
			to the current net total of instructions and recorded memory accesses.
		*/
		void disassemble(const MemoryWord *memory, ProgramCounterType location, ProgramCounterType length, ProgramCounterType start_address) {
			// TODO: possibly, move some of this stuff to instruction-set specific disassemblers, analogous to
			// the Executor's ownership of the Parser. That would allow handling of stateful parsing.
			ParserType<decltype(*this), true> parser;
			pending_entry_points_.push_back(start_address);
			entry_points_.insert(start_address);

			while(!pending_entry_points_.empty()) {
				const auto next_entry_point = pending_entry_points_.front();
				pending_entry_points_.pop_front();

				if(next_entry_point >= location) {
					parser.parse(*this, memory - location, next_entry_point & max_address, length + location);
				}
			}
		}

		const std::map<ProgramCounterType, InstructionType> &instructions() const {
			return instructions_;
		}

		const std::set<ProgramCounterType> &entry_points() const {
			return entry_points_;
		}

		void announce_overflow(ProgramCounterType) {}
		void announce_instruction(ProgramCounterType address, InstructionType instruction) {
			instructions_[address] = instruction;
		}
		void add_entry(ProgramCounterType address) {
			if(entry_points_.find(address) == entry_points_.end()) {
				pending_entry_points_.push_back(address);
				entry_points_.insert(address);
			}
		}
		void add_access(AddressType address, AccessType access_type) {
			// TODO.
			(void)address;
			(void)access_type;
		}

	private:
		std::map<ProgramCounterType, InstructionType> instructions_;
		std::set<ProgramCounterType> entry_points_;

		std::list<ProgramCounterType> pending_entry_points_;
};

}

#endif /* Disassembler_h */
