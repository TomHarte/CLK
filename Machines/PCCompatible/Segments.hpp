//
//  Segments.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Registers.hpp"

#include "InstructionSets/x86/Instruction.hpp"
#include "InstructionSets/x86/Model.hpp"

namespace PCCompatible {

template <InstructionSet::x86::Model model>
class Segments {
public:
	Segments(const Registers<model> &registers) : registers_(registers) {}

	using Source = InstructionSet::x86::Source;
	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using Descriptor = InstructionSet::x86::Descriptor;

	/// Posted by @c perform after any operation which *might* have affected a segment register.
	void did_update(const Source segment) {
		if(!is_segment_register(segment)) return;
		descriptors[segment].set_segment(registers_.segment(segment));
	}

	void did_update(DescriptorTable) {}

	void reset() {
		did_update(Source::ES);
		did_update(Source::CS);
		did_update(Source::DS);
		did_update(Source::SS);
	}

	InstructionSet::x86::SegmentRegisterSet<Descriptor> descriptors;

	bool operator ==(const Segments &rhs) const {
		return descriptors == rhs.descriptors_;
	}

private:
	const Registers<model> &registers_;
};

}
