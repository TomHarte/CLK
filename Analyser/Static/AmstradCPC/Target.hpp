//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::AmstradCPC {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, CPC464, CPC664, CPC6128);
	Model model = Model::CPC464;
	std::string loading_command;

	// This is used internally for testing; it therefore isn't exposed reflectively.
	bool catch_ssm_codes = false;

	Target() : Analyser::Static::Target(Machine::AmstradCPC) {
		if(needs_declare()) {
			DeclareField(model);
			AnnounceEnum(Model);
		}
	}
};

}
