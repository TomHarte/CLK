//
//  Mode.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 22/03/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::x86 {

enum class Mode {
	Real,
	Protected286,
};

constexpr bool is_real(const Mode mode) {
	// Note to future self: this will include virtual 8086 mode in the future.
	return mode == Mode::Real;
}

}
