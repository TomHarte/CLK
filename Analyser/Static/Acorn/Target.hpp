//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Acorn_Target_h
#define Analyser_Static_Acorn_Target_h

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Acorn {

struct Target: public ::Analyser::Static::Target, public Reflection::StructImpl<Target> {
	bool has_acorn_adfs = false;
	bool has_pres_adfs = false;
	bool has_dfs = false;
	bool has_ap6_rom = false;
	bool has_sideways_ram = false;
	bool should_shift_restart = false;
	std::string loading_command;

	Target() : Analyser::Static::Target(Machine::Electron) {
		if(needs_declare()) {
			DeclareField(has_pres_adfs);
			DeclareField(has_acorn_adfs);
			DeclareField(has_dfs);
			DeclareField(has_ap6_rom);
			DeclareField(has_sideways_ram);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Acorn_Target_h */
