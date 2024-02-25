//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "OperationMapper.hpp"

namespace InstructionSet::ARM {

namespace ConditionCode {

static constexpr uint32_t Negative		= 1 << 31;
static constexpr uint32_t Zero			= 1 << 30;
static constexpr uint32_t Carry			= 1 << 29;
static constexpr uint32_t Overflow		= 1 << 28;
static constexpr uint32_t IRQDisable	= 1 << 27;
static constexpr uint32_t FIQDisable	= 1 << 26;
static constexpr uint32_t Mode			= (1 << 1) | (1 << 0);

static constexpr uint32_t Address		= FIQDisable - Mode - 1;

}

}
