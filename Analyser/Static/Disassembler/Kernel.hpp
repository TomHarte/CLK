//
//  Kernel.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/12/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Kernel_hpp
#define Kernel_hpp

namespace Analyser::Static::Disassembly {

template <typename D, typename S> struct PartialDisassembly {
	D disassembly;
	std::vector<S> remaining_entry_points;
	std::vector<bool> touched;
};

template <typename D, typename S, typename Disassembler> D Disassemble(
	const std::vector<uint8_t> &memory,
	const std::function<std::size_t(S)> &address_mapper,
	std::vector<S> entry_points,
	bool exhaustive) {
	PartialDisassembly<D, S> partial_disassembly;
	partial_disassembly.remaining_entry_points = entry_points;
	partial_disassembly.touched.resize(memory.size());

	while(true) {
		// Do a recursive-style disassembly for all current entry points.
		while(!partial_disassembly.remaining_entry_points.empty()) {
			// Pull the next entry point from the back of the vector.
			S next_entry_point = partial_disassembly.remaining_entry_points.back();
			partial_disassembly.remaining_entry_points.pop_back();

			// If that address has already been visited, forget about it.
			if(	partial_disassembly.disassembly.instructions_by_address.find(next_entry_point)
				!= partial_disassembly.disassembly.instructions_by_address.end()) continue;

			// If it's outgoing, log it as such and forget about it; otherwise disassemble.
			std::size_t mapped_entry_point = address_mapper(next_entry_point);
			if(mapped_entry_point >= memory.size())
				partial_disassembly.disassembly.outward_calls.insert(next_entry_point);
			else
				Disassembler::AddToDisassembly(partial_disassembly, memory, address_mapper, next_entry_point);
		}

		// If this is not an exhaustive disassembly, that's your lot.
		if(!exhaustive) {
			break;
		}

		// Otherwise, find the first address that isn't yet disassembled and chuck it onto the list.
		auto first_untouched = std::find(partial_disassembly.touched.begin(), partial_disassembly.touched.end(), false);
		if(first_untouched == partial_disassembly.touched.end()) {
			break;
		}
		partial_disassembly.remaining_entry_points.push_back(
			static_cast<S>(first_untouched - partial_disassembly.touched.begin())
		);
	}

	return partial_disassembly.disassembly;
}

}

#endif /* Kernel_hpp */
