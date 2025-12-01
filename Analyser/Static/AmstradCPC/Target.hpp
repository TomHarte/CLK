//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::AmstradCPC {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, CPC464, CPC664, CPC6128);
	Model model = Model::CPC464;
	std::string loading_command;

//	ReflectableEnum(CRTCType, Type0, Type1, Type2, Type3);
//	CRTCType crtc_type = CRTCType::Type2;

	// This is used internally for testing; it therefore isn't exposed reflectively.
	bool catch_ssm_codes = false;

	Target() : Analyser::Static::Target(Machine::AmstradCPC) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(model);
//		DeclareField(crtc_type);
		AnnounceEnum(Model);
//		AnnounceEnum(CRTCType);
	}
};

}
