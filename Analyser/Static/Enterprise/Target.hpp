//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Enterprise_Target_h
#define Analyser_Static_Enterprise_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Enterprise {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, Enterprise64, Enterprise128, Enterprise256);
	ReflectableEnum(EXOSVersion, v10, v20, v21, v23, Any);
	ReflectableEnum(BASICVersion, v10, v11, v21, Any, None);
	ReflectableEnum(DOS, EPDOS, EXDOS, None);

	Model model = Model::Enterprise128;
	EXOSVersion exos_version = EXOSVersion::Any;
	BASICVersion basic_version = BASICVersion::None;
	DOS dos = DOS::None;

	Target() : Analyser::Static::Target(Machine::Enterprise) {
		if(needs_declare()) {
			AnnounceEnum(Model);
			AnnounceEnum(EXOSVersion);
			AnnounceEnum(BASICVersion);
			AnnounceEnum(DOS);

			DeclareField(model);
			DeclareField(exos_version);
			DeclareField(basic_version);
			DeclareField(dos);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Enterprise_Target_h */
