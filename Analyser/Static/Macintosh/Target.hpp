//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Macintosh_Target_h
#define Analyser_Static_Macintosh_Target_h

#include "../../../Reflection/Enum.h"
#include "../../../Reflection/Struct.h"

namespace Analyser {
namespace Static {
namespace Macintosh {

struct Target: public ::Analyser::Static::Target, public Reflection::Struct<Target> {
	ReflectableEnum(Model, int, Mac128k, Mac512k, Mac512ke, MacPlus);

	Target() {
		// Boilerplate for declaring fields and potential values.
		if(needs_declare()) {
			declare(&model, "model");
			Reflection::Enum::declare<Model>(EnumDeclaration(Model));
		}
	}

	Model model = Model::MacPlus;
};

}
}
}

#endif /* Analyser_Static_Macintosh_Target_h */
