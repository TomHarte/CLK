//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Target_h
#define Target_h

#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Atari {

struct Target: public ::Analyser::Static::Target {
	enum class PagingModel {
		None,
		CommaVid,
		Atari8k,
		Atari16k,
		Atari32k,
		ActivisionStack,
		ParkerBros,
		Tigervision,
		CBSRamPlus,
		MNetwork,
		MegaBoy,
		Pitfall2
	};

	// TODO: shouldn't these be properties of the cartridge?
	PagingModel paging_model;
	bool uses_superchip;
};

}
}
}

#endif /* Target_h */
