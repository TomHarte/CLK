//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_ZXSpectrum_Target_h
#define Analyser_Static_ZXSpectrum_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace ZXSpectrum {

struct Target: public ::Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model,
		Plus2a,
		Plus3,
	);

	Model model = Model::Plus2a;
	bool should_hold_enter = false;

	Target(): Analyser::Static::Target(Machine::ZXSpectrum) {
		if(needs_declare()) {
			DeclareField(model);
			AnnounceEnum(Model);
		}
	}
};

}
}
}

#endif /* Target_h */
