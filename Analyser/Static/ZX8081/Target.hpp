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
namespace ZX8081 {

struct Target: public ::Analyser::Static::Target {
	enum class MemoryModel {
		Unexpanded,
		SixteenKB,
		SixtyFourKB
	};

	MemoryModel memory_model = MemoryModel::Unexpanded;
	bool isZX81 = false;
};

}
}
}

#endif /* Target_h */
