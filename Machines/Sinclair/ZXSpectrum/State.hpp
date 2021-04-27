//
//  State.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef State_hpp
#define State_hpp

#include "../../../Reflection/Struct.hpp"
#include "../../../Processors/Z80/State/State.hpp"

#include "Video.hpp"
#include "../../../Components/AY38910/AY38910.hpp"

namespace Sinclair {
namespace ZXSpectrum {


struct State: public Reflection::StructImpl<State> {
	CPU::Z80::State z80;
	Video::State video;

	// In 16kb or 48kb mode, RAM will be 16kb or 48kb and represent
	// memory in standard linear order. In 128kb mode, RAM will be
	// 128kb with the first 16kb representing bank 0, the next bank 1, etc.
	std::vector<uint8_t> ram;

	// Meaningful for 128kb machines only.
	uint8_t last_7ffd = 0;
	GI::AY38910::State ay;

	// Meaningful for the +2a and +3 only.
	uint8_t last_1ffd = 0;

	State() {
		if(needs_declare()) {
			DeclareField(z80);
			DeclareField(video);
			DeclareField(ram);
			DeclareField(last_7ffd);
			DeclareField(last_1ffd);
			DeclareField(ay);
		}
	}
};

}
}

#endif /* State_h */
