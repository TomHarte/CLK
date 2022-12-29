//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Sega_Target_h
#define Analyser_Static_Sega_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Sega {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	enum class Model {
		SG1000,
		MasterSystem,
		MasterSystem2,
	};

	ReflectableEnum(Region,
		Japan,
		USA,
		Europe,
		Brazil
	);

	enum class PagingScheme {
		Sega,
		Codemasters
	};

	Model model = Model::MasterSystem;
	Region region = Region::Japan;
	PagingScheme paging_scheme = PagingScheme::Sega;

	Target() : Analyser::Static::Target(Machine::MasterSystem) {
		if(needs_declare()) {
			DeclareField(region);
			AnnounceEnum(Region);
		}
	}
};

constexpr bool is_master_system(Analyser::Static::Sega::Target::Model model) {
	return model >= Analyser::Static::Sega::Target::Model::MasterSystem;
}

}
}
}

#endif /* Analyser_Static_Sega_Target_h */
