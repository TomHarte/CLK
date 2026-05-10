//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"

#include <string>

namespace Analyser::Static::TandyCoCo {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	Target() : Analyser::Static::Target(Machine::TandyCoCo) {}

	std::wstring loading_command;

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {}
};

}
