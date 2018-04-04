//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Acorn_Target_h
#define Analyser_Static_Acorn_Target_h

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

#endif /* Analyser_Static_Acorn_Target_h */
