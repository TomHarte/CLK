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


namespace Analyser {
namespace Static {
namespace Macintosh {


struct Target: public ::Analyser::Static::Target {
	ReflectiveEnum(Model, int, Mac128k, Mac512k, Mac512ke, MacPlus);

	Target() {
//		Model m;
//		printf("%s\n", __declaration(m));
		printf("%zu\n", Reflection::Enum<Model>::size());
//		for(size_t c = 0; c < Reflection::Enum<Model>::size(); ++c) {
//			const auto name = Reflection::Enum<Model>::toString(Model(c));
//			printf("%.*s\n", int(name.size()), name.data());
//		}
	}

	Model model = Model::MacPlus;
};

}
}
}


#endif /* Analyser_Static_Macintosh_Target_h */
