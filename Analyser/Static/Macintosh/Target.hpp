//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Macintosh_Target_h
#define Analyser_Static_Macintosh_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Macintosh {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(Model, Mac128k, Mac512k, Mac512ke, MacPlus);
	Model model = Model::MacPlus;

	Target() : Analyser::Static::Target(Machine::Macintosh) {
		// Boilerplate for declaring fields and potential values.
		if(needs_declare()) {
			DeclareField(model);
			AnnounceEnum(Model);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Macintosh_Target_h */
