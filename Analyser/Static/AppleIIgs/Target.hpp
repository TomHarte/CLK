//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/10/2020.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_AppleIIgs_Target_h
#define Analyser_Static_AppleIIgs_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace AppleIIgs {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model,
		ROM00,
		ROM01,
		ROM03
	);
	ReflectableEnum(MemoryModel,
		TwoHundredAndFiftySixKB,
		OneMB,
		EightMB
	);

	Model model = Model::ROM01;
	MemoryModel memory_model = MemoryModel::EightMB;

	Target() : Analyser::Static::Target(Machine::AppleIIgs) {
		if(needs_declare()) {
			DeclareField(model);
			DeclareField(memory_model);
			AnnounceEnum(Model);
			AnnounceEnum(MemoryModel);
		}
	}
};

}
}
}

#endif /* Analyser_Static_AppleIIgs_Target_h */
