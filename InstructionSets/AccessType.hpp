//
//  AccessType.h
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet {

enum class AccessType {
	None,
	Read,
	Write,
	ReadModifyWrite
};

}
