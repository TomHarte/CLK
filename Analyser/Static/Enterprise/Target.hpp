//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Enterprise_Target_h
#define Analyser_Static_Enterprise_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Enterprise {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	Target() : Analyser::Static::Target(Machine::Enterprise) {}

	// TODO: I assume there'll be relevant fields to add here.
};

}
}
}

#endif /* Analyser_Static_Enterprise_Target_h */
