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
		switch(segment) {
			default: break;
			case Source::ES:	es_.set_segment(registers_.es());	break;
			case Source::CS:	cs_.set_segment(registers_.cs());	break;
			case Source::DS:	ds_.set_segment(registers_.ds());	break;
			case Source::SS:	ss_.set_segment(registers_.ss());	break;
		}
	}

	void did_update(DescriptorTable) {}

	void reset() {
		did_update(Source::ES);
		did_update(Source::CS);
		did_update(Source::DS);
		did_update(Source::SS);
	}

	Descriptor es_, cs_, ds_, ss_;

	bool operator ==(const Segments &rhs) const {
		return
			es_ == rhs.es_ &&
			cs_ == rhs.cs_ &&
			ds_ == rhs.ds_ &&
			ss_ == rhs.ss_;
	}

private:
	const Registers<model> &registers_;
};

}
