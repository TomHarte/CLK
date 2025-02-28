//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"

#include <string>

namespace Analyser::Static::Enterprise {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, Enterprise64, Enterprise128, Enterprise256);
	ReflectableEnum(EXOSVersion, v10, v20, v21, v23, Any);
	ReflectableEnum(BASICVersion, v10, v11, v21, Any, None);
	ReflectableEnum(DOS, EXDOS, None);
	ReflectableEnum(Speed, FourMHz, SixMHz);

	Model model = Model::Enterprise128;
	EXOSVersion exos_version = EXOSVersion::Any;
	BASICVersion basic_version = BASICVersion::None;
	DOS dos = DOS::None;
	Speed speed = Speed::FourMHz;
	std::string loading_command;

	Target() : Analyser::Static::Target(Machine::Enterprise) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		AnnounceEnum(Model);
		AnnounceEnum(EXOSVersion);
		AnnounceEnum(BASICVersion);
		AnnounceEnum(DOS);
		AnnounceEnum(Speed);

		DeclareField(model);
		DeclareField(exos_version);
		DeclareField(basic_version);
		DeclareField(dos);
		DeclareField(speed);
	}
};

}
