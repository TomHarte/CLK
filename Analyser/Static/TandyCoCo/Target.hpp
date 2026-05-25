//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Reflection/Enum.hpp"
#include "Reflection/Struct.hpp"
#include "Analyser/Static/StaticAnalyser.hpp"

#include <string>

namespace Analyser::Static::TandyCoCo {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	Target() : Analyser::Static::Target(Machine::TandyCoCo) {}

	ReflectableEnum(ROMVersion,
		V10,
		V11,
		V12,
		V13,
		Any,
		V11OrAbove,
	);
	ReflectableEnum(Model,
		TandyCoCo,
		Dragon,
	);
	ReflectableEnum(MemorySize,
		ThirtyTwoKB,
		SixtyFourKB,
	);

	std::wstring loading_command;
	MemorySize memory_size = MemorySize::SixtyFourKB;
	Model model = Model::TandyCoCo;
	ROMVersion rom_version = ROMVersion::Any;
	bool has_disk_drive = false;

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		AnnounceEnum(Model);
		AnnounceEnum(MemorySize);
		AnnounceEnum(ROMVersion);

		DeclareField(model);
		DeclareField(memory_size);
		DeclareField(has_disk_drive);
		DeclareField(rom_version);
	}
};

constexpr bool is_pal(const Target::Model model) {
	return model == Target::Model::Dragon;
}

constexpr bool is_dragon(const Target::Model model) {
	return model == Target::Model::Dragon;
}

}
