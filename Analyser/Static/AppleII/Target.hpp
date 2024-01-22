//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser::Static::AppleII {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
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
	ReflectableEnum(SCSIController,
		None,
		AppleSCSI
	);

	Model model = Model::IIe;
	DiskController disk_controller = DiskController::None;
	SCSIController scsi_controller = SCSIController::None;

	Target() : Analyser::Static::Target(Machine::AppleII) {
		if(needs_declare()) {
			DeclareField(model);
			DeclareField(disk_controller);
			DeclareField(scsi_controller);

			AnnounceEnum(Model);
			AnnounceEnum(DiskController);
			AnnounceEnum(SCSIController);
		}
	}
};

}
