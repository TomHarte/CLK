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

namespace Analyser::Static::Oric {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(ROM,
		BASIC10,
		BASIC11,
		Pravetz
	);

	ReflectableEnum(DiskInterface,
		None,
		Microdisc,
		Pravetz,
		Jasmin,
		BD500
	);

	ReflectableEnum(Processor,
		MOS6502,
		WDC65816
	);

	ROM rom = ROM::BASIC11;
	DiskInterface disk_interface = DiskInterface::None;
	Processor processor = Processor::MOS6502;
	std::string loading_command;
	bool should_start_jasmin = false;

	Target(): Analyser::Static::Target(Machine::Oric) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(rom);
		DeclareField(disk_interface);
		DeclareField(processor);
		AnnounceEnum(ROM);
		AnnounceEnum(DiskInterface);
		AnnounceEnum(Processor);
	}
};

}
