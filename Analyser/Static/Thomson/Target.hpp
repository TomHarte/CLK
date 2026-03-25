//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::Thomson {

struct MOTarget: public Analyser::Static::Target, public Reflection::StructImpl<MOTarget> {
	std::string loading_command;

	MOTarget() : Analyser::Static::Target(Machine::ThomsonMO) {}

private:
	friend Reflection::StructImpl<MOTarget>;
	void declare_fields() {
		// None yet.
	}
};

}
