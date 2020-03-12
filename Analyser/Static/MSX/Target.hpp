//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_MSX_Target_h
#define Analyser_Static_MSX_Target_h

#include "../../../Reflection/Enum.h"
#include "../../../Reflection/Struct.h"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace MSX {

struct Target: public ::Analyser::Static::Target, public Reflection::Struct<Target> {
	bool has_disk_drive = false;
	std::string loading_command;

	ReflectableEnum(Region, int,
		Japan,
		USA,
		Europe
	);
	Region region = Region::USA;

	Target() {
		if(needs_declare()) {
			DeclareField(has_disk_drive);
			DeclareField(region);
			AnnounceEnum(Region);
		}
	}
};

}
}
}

#endif /* Analyser_Static_MSX_Target_h */
