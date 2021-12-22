//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Analyser_Static_Amiga_Target_h
#define Analyser_Static_Amiga_Target_h

#include "../../../Reflection/Struct.hpp"
#include "../StaticAnalyser.hpp"

namespace Analyser {
namespace Static {
namespace Amiga {

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

	Target() : Analyser::Static::Target(Machine::Amiga) {
		if(needs_declare()) {
			DeclareField(fast_ram);
			DeclareField(chip_ram);
			AnnounceEnum(FastRAM);
			AnnounceEnum(ChipRAM);
		}
	}
};

}
}
}

#endif /* Analyser_Static_Amiga_Target_h */
