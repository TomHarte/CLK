//
//  Kernel.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Kernel_hpp
#define Kernel_hpp

namespace Analyser {
namespace Static {
namespace Disassembly {

template <typename D, typename S> struct PartialDisassembly {
	D disassembly;
	std::vector<S> remaining_entry_points;
};

template <typename D, typename S, typename Disassembler> D Disassemble(
	const std::vector<uint8_t> &memory,
	const std::function<std::size_t(S)> &address_mapper,
	std::vector<S> entry_points) {
	PartialDisassembly<D, S> partial_disassembly;
	partial_disassembly.remaining_entry_points = entry_points;

	while(!partial_disassembly.remaining_entry_points.empty()) {
		// pull the next entry point from the back of the vector
		S next_entry_point = partial_disassembly.remaining_entry_points.back();
		partial_disassembly.remaining_entry_points.pop_back();

		// if that address has already been visited, forget about it
		if(	partial_disassembly.disassembly.instructions_by_address.find(next_entry_point)
			!= partial_disassembly.disassembly.instructions_by_address.end()) continue;

		// if it's outgoing, log it as such and forget about it; otherwise disassemble
		std::size_t mapped_entry_point = address_mapper(next_entry_point);
		if(mapped_entry_point >= memory.size())
			partial_disassembly.disassembly.outward_calls.insert(next_entry_point);
		else
			Disassembler::AddToDisassembly(partial_disassembly, memory, address_mapper, next_entry_point);
	}

	return partial_disassembly.disassembly;
}

}
}
}

#endif /* Kernel_hpp */
