//
//  Segments.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "LinearMemory.hpp"
#include "Registers.hpp"

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Instruction.hpp"
#include "InstructionSets/x86/Mode.hpp"
#include "InstructionSets/x86/Model.hpp"

namespace PCCompatible {

template <InstructionSet::x86::Model model>
class Segments {
public:
	Segments(const Registers<model> &registers, const LinearMemory<model> &memory) :
		registers_(registers), memory_(memory) {}

	using Descriptor = InstructionSet::x86::Descriptor;
	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using Mode = InstructionSet::x86::Mode;
	using Source = InstructionSet::x86::Source;

	/// Posted by @c perform after any operation which *might* have affected a segment register.
	void did_update(const Source segment) {
		if(!is_segment_register(segment)) return;
		const auto value = registers_.segment(segment);
		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			descriptors[segment].set_segment(value);
			return;
		} else {
			switch(mode_) {
				case Mode::Real:
					descriptors[segment].set_segment(value);
				break;

				case Mode::Protected286: {
					// Check privilege level.
					const auto requested_privilege_level = value & 3;
					if(requested_privilege_level < descriptors[Source::CS].privilege_level()) {
						printf("TODO: privilege exception.");
					}

					// Check segment range.
					const auto &table =
						(value & 4) ?
							registers_.template get<DescriptorTable::Local>() :
							registers_.template get<DescriptorTable::Global>();
					const uint32_t table_address = table.base + (value & ~7);
					if(table_address > table.base + table.limit - 8) {
						printf("TODO: descriptor table overrun exception.\n");
					}

					// Get descriptor contents.
					using AccessType = InstructionSet::x86::AccessType;
					const uint32_t table_end = table.base + table.limit;
					const uint16_t entry[] = {
						memory_.template access<uint16_t, AccessType::Read>(table_address, table_end),
						memory_.template access<uint16_t, AccessType::Read>(table_address + 2, table_end),
						memory_.template access<uint16_t, AccessType::Read>(table_address + 4, table_end),
						memory_.template access<uint16_t, AccessType::Read>(table_address + 6, table_end)
					};

//					printf("%s [%04x -> %08x]: ", InstructionSet::x86::to_string(segment, InstructionSet::x86::DataSize::Word).c_str(), value, table_address);
					const Descriptor incoming(entry);

//					switch(segment) {
//						case Source::CS:
//						break;
//					}

					// TODO: is this descriptor privilege within reach?
					// TODO: is this an empty descriptor*? If so: exception!
					// (* other than the 0 descriptor, which can be loaded to DS or ES)
					// TODO:
					//	bool is_compatible(const Source reg) const {
					//		switch(reg)
					//			DS/ES: either readable code, or any sort of data
					//			SS: writeable data
					//			CS: any sort of code
					//	}

					descriptors[segment] = incoming;
					// TODO: set descriptor accessed bit in memory if it's a segment.
				} break;
			}
		}
	}

	void did_update(DescriptorTable) {}

	void set_mode(const Mode mode) {
		mode_ = mode;
	}

	void reset() {
		did_update(Source::ES);
		did_update(Source::CS);
		did_update(Source::DS);
		did_update(Source::SS);
		did_update(Source::FS);
		did_update(Source::GS);
	}

	InstructionSet::x86::SegmentRegisterSet<Descriptor> descriptors;

	bool operator ==(const Segments &rhs) const {
		return descriptors == rhs.descriptors;
	}

private:
	Mode mode_ = Mode::Real;
	const Registers<model> &registers_;
	const LinearMemory<model> &memory_;
};

}
