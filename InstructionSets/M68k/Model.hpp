//
//  Model.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/04/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::M68k {

enum class Model {
	M68000,
	M68010,
	M68020,
	M68030,
	M68040,
};

}
