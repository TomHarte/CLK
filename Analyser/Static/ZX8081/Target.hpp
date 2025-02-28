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

namespace Analyser::Static::ZX8081 {

struct Target: public ::Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(MemoryModel,
		Unexpanded,
		SixteenKB,
		SixtyFourKB
	);

	MemoryModel memory_model = MemoryModel::Unexpanded;
	bool is_ZX81 = false;
	bool ZX80_uses_ZX81_ROM = false;
	std::string loading_command;

	Target(): Analyser::Static::Target(Machine::ZX8081) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(memory_model);
		DeclareField(is_ZX81);
		DeclareField(ZX80_uses_ZX81_ROM);
		AnnounceEnum(MemoryModel);
	}
};

}
