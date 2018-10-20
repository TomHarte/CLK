//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/09/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Sega_Target_h
#define Analyser_Static_Sega_Target_h

namespace Analyser {
namespace Static {
namespace Sega {

struct Target: public ::Analyser::Static::Target {
	enum class Model {
		MasterSystem,
		SG1000
	};

	enum class Region {
		Japan,
		USA,
		Europe
	};

	enum class PagingScheme {
		Sega,
		Codemasters
	};

	Model model = Model::MasterSystem;
	Region region = Region::Japan;
	PagingScheme paging_scheme = PagingScheme::Sega;
};

}
}
}

#endif /* Analyser_Static_Sega_Target_h */
