//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::Amiga {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(ChipRAM,
		FiveHundredAndTwelveKilobytes,
		OneMegabyte,
		TwoMegabytes);
	ReflectableEnum(FastRAM,
		None,
		OneMegabyte,
		TwoMegabytes,
		FourMegabytes,
		EightMegabytes);

	ChipRAM chip_ram = ChipRAM::FiveHundredAndTwelveKilobytes;
	FastRAM fast_ram = FastRAM::EightMegabytes;

	Target() : Analyser::Static::Target(Machine::Amiga) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(fast_ram);
		DeclareField(chip_ram);
		AnnounceEnum(FastRAM);
		AnnounceEnum(ChipRAM);
	}
};

}
