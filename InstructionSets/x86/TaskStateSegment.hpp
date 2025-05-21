//
//  TaskStateSegment.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/05/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "Mode.hpp"

namespace InstructionSet::x86 {

template <Mode mode>
struct TaskStateSegment;

template <>
struct TaskStateSegment<Mode::Protected286> {
	static constexpr uint32_t Size = 44;

	template <typename LinearMemoryT>
	static TaskStateSegment read(LinearMemoryT &memory, const uint32_t base, const uint32_t limit) {
		uint32_t c = 0;
		const auto read = [&]() {
			const auto value = memory.template access<uint16_t, AccessType::Read>(base + c, limit);
			c += 2;
			return value;
		};

		TaskStateSegment result;
		result.back_link = read();
		result.stacks[0].offset = read();
		result.stacks[0].segment = read();
		result.stacks[1].offset = read();
		result.stacks[1].segment = read();
		result.stacks[2].offset = read();
		result.stacks[2].segment = read();
		result.instruction_pointer = read();
		result.flags = read();

		result.ax = read();
		result.cx = read();
		result.dx = read();
		result.bx = read();

		result.sp = read();
		result.bp = read();
		result.si = read();
		result.di = read();

		result.es_selector = read();
		result.cs_selector = read();
		result.ss_selector = read();
		result.ds_selector = read();

		result.ldt_selector = read();

		return result;
	}

	uint16_t back_link;
	struct StackPointer {
		uint16_t offset;
		uint16_t segment;
	};
	StackPointer stacks[3];	// Indexed by CPL.
	uint16_t instruction_pointer;
	uint16_t flags;
	uint16_t ax, cx, dx, bx;
	uint16_t sp, bp, si, di;
	uint16_t es_selector;
	uint16_t cs_selector;
	uint16_t ss_selector;
	uint16_t ds_selector;
	uint16_t ldt_selector;
};

}
