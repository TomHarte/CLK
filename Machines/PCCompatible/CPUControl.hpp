//
//  CPUControl.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Memory.hpp"
#include "ProcessorByModel.hpp"
#include "Registers.hpp"
#include "Segments.hpp"

#include "Analyser/Static/PCCompatible/Target.hpp"

namespace PCCompatible {

template <Analyser::Static::PCCompatible::Model model>
class CPUControl {
public:
	CPUControl(
		Registers<processor_model(model)> &registers,
		Segments<processor_model(model)> &segments,
		Memory<model> &memory
	) : registers_(registers), segments_(segments), memory_(memory) {}

	void reset() {
		registers_.reset();
		segments_.reset();
	}

	void set_a20_enabled(bool) {
		// Assumed: this'll be something to set on Memory.
	}

private:
	Registers<processor_model(model)> &registers_;
	Segments<processor_model(model)> &segments_;
	Memory<model> &memory_;
};


}
