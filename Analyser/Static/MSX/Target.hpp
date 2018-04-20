//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/04/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_MSX_Target_h
#define Analyser_Static_MSX_Target_h

#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace MSX {

struct Target: public ::Analyser::Static::Target {
	bool has_disk_drive = false;
	std::string loading_command;
};

}
}
}

#endif /* Analyser_Static_MSX_Target_h */
