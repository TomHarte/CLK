//
//  ProcessorByModel.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/PCCompatible/Target.hpp"
#include "InstructionSets/x86/Model.hpp"

namespace PCCompatible {

constexpr InstructionSet::x86::Model processor_model(Analyser::Static::PCCompatible::Model model) {
	switch(model) {
		default:											return InstructionSet::x86::Model::i8086;
		case Analyser::Static::PCCompatible::Model::AT:		return InstructionSet::x86::Model::i80286;
	}
}

}
