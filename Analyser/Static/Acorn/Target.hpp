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
namespace Acorn {

struct Target: public ::Analyser::Static::Target {
	bool has_adfs = false;
	bool has_dfs = false;
	bool should_shift_restart = false;
};

}
}
}

#endif /* Target_h */
