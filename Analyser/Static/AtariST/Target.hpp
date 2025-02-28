//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Analyser/Static/StaticAnalyser.hpp"
#include "Reflection/Struct.hpp"

namespace Analyser::Static::AtariST {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	ReflectableEnum(MemorySize,
		FiveHundredAndTwelveKilobytes,
		OneMegabyte,
		FourMegabytes);
	MemorySize memory_size = MemorySize::OneMegabyte;

	Target() : Analyser::Static::Target(Machine::AtariST) {}

private:
	friend Reflection::StructImpl<Target>;
	void declare_fields() {
		DeclareField(memory_size);
		AnnounceEnum(MemorySize);
	}
};

}
