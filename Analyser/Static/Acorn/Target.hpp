//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser::Static::Acorn {

struct ElectronTarget: public ::Analyser::Static::Target, public Reflection::StructImpl<ElectronTarget> {
	bool has_acorn_adfs = false;
	bool has_pres_adfs = false;
	bool has_dfs = false;
	bool has_ap6_rom = false;
	bool has_sideways_ram = false;
	bool should_shift_restart = false;
	std::string loading_command;

	ElectronTarget() : Analyser::Static::Target(Machine::Electron) {
		if(needs_declare()) {
			DeclareField(has_pres_adfs);
			DeclareField(has_acorn_adfs);
			DeclareField(has_dfs);
			DeclareField(has_ap6_rom);
			DeclareField(has_sideways_ram);
		}
	}
};

struct ArchimedesTarget: public ::Analyser::Static::Target, public Reflection::StructImpl<ArchimedesTarget> {
	std::string main_program;

	ArchimedesTarget() : Analyser::Static::Target(Machine::Archimedes) {}
};

}
