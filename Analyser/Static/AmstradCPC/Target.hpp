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
namespace AmstradCPC {

struct Target: public ::Analyser::Static::Target {
	enum class Model {
		CPC464,
		CPC664,
		CPC6128
	};

	Model model;
};

}
}
}


#endif /* Target_h */
