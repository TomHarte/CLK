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
namespace Commodore {

struct Target: public ::Analyser::Static::Target {
	enum class MemoryModel {
		Unexpanded,
		EightKB,
		ThirtyTwoKB
	};

	MemoryModel memory_model;
	bool has_c1540;
};

}
}
}

#endif /* Target_h */
