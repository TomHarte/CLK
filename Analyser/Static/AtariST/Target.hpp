//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_AtariST_Target_h
#define Analyser_Static_AtariST_Target_h

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace AtariST {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	Target() : Analyser::Static::Target(Machine::AtariST) {}
};

}
}
}

#endif /* Analyser_Static_AtariST_Target_h */
