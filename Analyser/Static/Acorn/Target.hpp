//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
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

	ElectronTarget() : Analyser::Static::Target(Machine::Electron) {}

private:
	friend Reflection::StructImpl<ElectronTarget>;
	void declare_fields() {
		DeclareField(has_pres_adfs);
		DeclareField(has_acorn_adfs);
		DeclareField(has_dfs);
		DeclareField(has_ap6_rom);
		DeclareField(has_sideways_ram);
	}
};

struct BBCMicroTarget: public ::Analyser::Static::Target, public Reflection::StructImpl<BBCMicroTarget> {
	std::string loading_command;
	bool should_shift_restart = false;

	bool has_1770dfs = false;
	bool has_adfs = false;
	bool has_sideways_ram = true;

	ReflectableEnum(TubeProcessor, None, MOS6502);
	TubeProcessor tube_processor = TubeProcessor::None;

	BBCMicroTarget() : Analyser::Static::Target(Machine::BBCMicro) {}

private:
	friend Reflection::StructImpl<BBCMicroTarget>;
	void declare_fields() {
		DeclareField(has_1770dfs);
		DeclareField(has_adfs);
		DeclareField(has_sideways_ram);
		AnnounceEnum(TubeProcessor);
		DeclareField(tube_processor);
	}
};

struct ArchimedesTarget: public ::Analyser::Static::Target, public Reflection::StructImpl<ArchimedesTarget> {
	std::string main_program;

	ArchimedesTarget() : Analyser::Static::Target(Machine::Archimedes) {}

private:
	friend Reflection::StructImpl<ArchimedesTarget>;
	void declare_fields() {}
};

}
