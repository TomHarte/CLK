//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 09/03/2018.
//  Copyright 2018 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Commodore_Target_h
#define Analyser_Static_Commodore_Target_h

#include "../../../Reflection/Enum.hpp"
#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"
#include <string>

namespace Analyser {
namespace Static {
namespace Commodore {

struct Target: public Analyser::Static::Target, public Reflection::StructImpl<Target> {
	enum class MemoryModel {
		Unexpanded,
		EightKB,
		ThirtyTwoKB
	};

	ReflectableEnum(Region,
		American,
		Danish,
		Japanese,
		European,
		Swedish
	);

	/// Maps from a named memory model to a bank enabled/disabled set.
	void set_memory_model(MemoryModel memory_model) {
		// This is correct for unexpanded and 32kb memory models.
		enabled_ram.bank0 = enabled_ram.bank1 =
		enabled_ram.bank2 = enabled_ram.bank3 =
		enabled_ram.bank5 = memory_model == MemoryModel::ThirtyTwoKB;

		// Bank 0 will need to be enabled if this is an 8kb machine.
		enabled_ram.bank0 |= memory_model == MemoryModel::EightKB;
	}
	struct {
		bool bank0 = false;
		bool bank1 = false;
		bool bank2 = false;
		bool bank3 = false;
		bool bank5 = false;
					// Sic. There is no bank 4; this is because the area that logically would be
					// bank 4 is occupied by the character ROM, colour RAM, hardware registers, etc.
	} enabled_ram;

	Region region = Region::European;
	bool has_c1540 = false;
	std::string loading_command;

	Target() : Analyser::Static::Target(Machine::Vic20) {
		if(needs_declare()) {
			DeclareField(enabled_ram.bank0);
			DeclareField(enabled_ram.bank1);
			DeclareField(enabled_ram.bank2);
			DeclareField(enabled_ram.bank3);
			DeclareField(enabled_ram.bank5);
			DeclareField(region);
			DeclareField(has_c1540);
			AnnounceEnum(Region);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Commodore_Target_h */
