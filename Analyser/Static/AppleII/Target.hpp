//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 21/04/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Target_h
#define Target_h

#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace AppleII {

struct Target: public ::Analyser::Static::Target {
	enum class Model {
		II,
		IIplus,
		IIe
	};
	enum class DiskController {
		None,
		SixteenSector,
		ThirteenSector
	};

	Model model = Model::IIe;
	DiskController disk_controller = DiskController::None;
};

}
}
}

#endif /* Target_h */
