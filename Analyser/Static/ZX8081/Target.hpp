//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_ZX8081_Target_h
#define Analyser_Static_ZX8081_Target_h

#include "../StaticAnalyser.hpp"
#include <string>

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
	bool is_ZX81 = false;
	bool ZX80_uses_ZX81_ROM = false;
	std::string loading_command;
};

}
}
}

#endif /* Analyser_Static_ZX8081_Target_h */
