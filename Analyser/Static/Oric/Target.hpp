//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Oric_Target_h
#define Analyser_Static_Oric_Target_h

#include "../../../Reflection/Enum.h"
#include "../../../Reflection/Struct.h"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Oric {

struct Target: public ::Analyser::Static::Target, public Reflection::Struct<Target> {
	ReflectableEnum(ROM,
		BASIC10,
		BASIC11,
		Pravetz
	);

	ReflectableEnum(DiskInterface,
		None,
		Microdisc,
		Pravetz,
		Jasmin,
		BD500
	);

	ROM rom = ROM::BASIC11;
	DiskInterface disk_interface = DiskInterface::None;
	std::string loading_command;
	bool should_start_jasmin = false;

	Target() {
		if(needs_declare()) {
			DeclareField(rom);
			DeclareField(disk_interface);
			AnnounceEnum(ROM);
			AnnounceEnum(DiskInterface);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Oric_Target_h */
