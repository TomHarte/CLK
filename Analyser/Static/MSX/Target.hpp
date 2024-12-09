//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::MSX {

struct Target: public ::Analyser::Static::Target, public Reflection::StructImpl<Target> {
	bool has_disk_drive = false;
	bool has_msx_music = true;
	std::string loading_command;

	ReflectableEnum(Model,
		MSX1,
		MSX2
	);
	Model model = Model::MSX2;

	ReflectableEnum(Region,
		Japan,
		USA,
		Europe
	);
	Region region = Region::USA;

	Target(): Analyser::Static::Target(Machine::MSX) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(has_disk_drive);
		DeclareField(has_msx_music);
		DeclareField(region);
		AnnounceEnum(Region);
		DeclareField(model);
		AnnounceEnum(Model);
	}
};

}
