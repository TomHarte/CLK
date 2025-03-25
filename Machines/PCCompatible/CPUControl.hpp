//
//  CPUControl.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "SegmentedMemory.hpp"
#include "ProcessorByModel.hpp"
#include "Registers.hpp"
#include "Segments.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Outputs/Log.hpp"

namespace PCCompatible {

template <Analyser::Static::PCCompatible::Model model>
class CPUControl {
public:
	CPUControl(
		Registers<processor_model(model)> &registers,
		Segments<processor_model(model)> &segments,
		SegmentedMemory<model> &memory
	) : registers_(registers), segments_(segments), memory_(memory) {}

	void reset() {
		registers_.reset();
		segments_.reset();
	}

	void set_a20_enabled(const bool enabled) {
		// Assumed: this'll be something to set on Memory.
		log_.info().append("A20 line is now: ", enabled);
	}

private:
	Registers<processor_model(model)> &registers_;
	Segments<processor_model(model)> &segments_;
	SegmentedMemory<model> &memory_;

	Log::Logger<Log::Source::PCCompatible> log_;
};


}
