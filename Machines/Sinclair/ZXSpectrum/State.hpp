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

namespace Sinclair {
namespace ZXSpectrum {


struct State: public Reflection::StructImpl<State> {
	CPU::Z80::State z80;

	State() {
		if(needs_declare()) {
			DeclareField(z80);
		}
	}
};

}
}

#endif /* State_h */
