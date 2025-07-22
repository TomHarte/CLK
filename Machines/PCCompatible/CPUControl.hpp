//
//  CPUControl.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "LinearMemory.hpp"
#include "ProcessorByModel.hpp"
#include "Segments.hpp"
#include "SegmentedMemory.hpp"

#include "InstructionSets/x86/Registers.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Outputs/Log.hpp"

namespace PCCompatible {

template <Analyser::Static::PCCompatible::Model model>
class CPUControl {
public:
	CPUControl(
		InstructionSet::x86::Registers<processor_model(model)> &registers,
		Segments<processor_model(model)> &segments,
		SegmentedMemory<processor_model(model)> &segmented_memory,
		LinearMemory<processor_model(model)> &linear_memory
	) :
		registers_(registers),
		segments_(segments),
		segmented_memory_(segmented_memory),
		linear_memory_(linear_memory) {}

	using Mode = InstructionSet::x86::Mode;
	void reset() {
		set_mode(Mode::Real);
		registers_.reset();
		segments_.reset();
	}

	void set_a20_enabled(const bool enabled) {
		// Assumed: this'll be something to set on Memory.
		log_.info().append("A20 line is now: %d", enabled);
		linear_memory_.set_a20_enabled(enabled);
	}

	void set_mode(const Mode mode) {
		mode_ = mode;
		if constexpr (processor_model(model) >= InstructionSet::x86::Model::i80286) {
			segments_.set_mode(mode);
			segmented_memory_.set_mode(mode);
		}
	}

	Mode mode() const {
		return mode_;
	}

private:
	InstructionSet::x86::Registers<processor_model(model)> &registers_;
	Segments<processor_model(model)> &segments_;
	SegmentedMemory<processor_model(model)> &segmented_memory_;
	LinearMemory<processor_model(model)> &linear_memory_;

	Log::Logger<Log::Source::PCCompatible> log_;
	Mode mode_ = Mode::Real;
};


}
