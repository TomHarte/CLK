//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Target_h
#define Target_h

#include "../../../Reflection/Enum.h"
#include "../../../Reflection/Struct.h"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace AppleII {

struct Target: public ::Analyser::Static::Target, public Reflection::Struct<Target> {
	ReflectableEnum(Model,
		II,
		IIplus,
		IIe,
		EnhancedIIe
	);
	ReflectableEnum(DiskController,
		None,
		SixteenSector,
		ThirteenSector
	);

	Model model = Model::IIe;
	DiskController disk_controller = DiskController::None;

	Target() {
		if(needs_declare()) {
			DeclareField(model);
			DeclareField(disk_controller);
			AnnounceEnum(Model);
			AnnounceEnum(DiskController);
		}
	}
};

}
}
}

#endif /* Target_h */
