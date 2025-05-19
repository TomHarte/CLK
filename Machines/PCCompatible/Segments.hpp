//
//  Segments.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "Descriptors.hpp"
#include "LinearMemory.hpp"
#include "Registers.hpp"

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Instruction.hpp"
#include "InstructionSets/x86/Mode.hpp"
#include "InstructionSets/x86/Model.hpp"

#include <cassert>
#include <optional>

namespace PCCompatible {

template <InstructionSet::x86::Model model>
class Segments {
public:
	Segments(const Registers<model> &registers, const LinearMemory<model> &memory) :
		registers_(registers), memory_(memory) {}

	using Descriptor = InstructionSet::x86::SegmentDescriptor;
	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using Mode = InstructionSet::x86::Mode;
	using Source = InstructionSet::x86::Source;

	void preauthorise(const Source segment, const uint16_t value) {
#ifndef NDEBUG
		last_source_ = segment;
#endif

		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			return;
		} else {
			if(is_real(mode_)) {
				return;
			}

			// Cache descriptor for preauthorisation.
			const auto &table =
				(value & 4) ?
					registers_.template get<DescriptorTable::Local>() :
					registers_.template get<DescriptorTable::Global>();
			const auto incoming = descriptor_at<Descriptor>(memory_, table, value & ~7);
			last_descriptor_ = incoming;

			// TODO: authorise otherwise, i.e. test anything telse that should cause a throw
			// prior to modification of the segment register.

			// Check privilege level.
	//		const auto requested_privilege_level = value & 3;
	//		if(requested_privilege_level < descriptors[Source::CS].privilege_level()) {
	//			printf("TODO: privilege exception.\n");
	//			assert(false);
	//		}

			// Get and validate descriptor.

			switch(segment) {
				case Source::DS:
				case Source::ES:
					if(!incoming.code_or_data() || (incoming.executable() && !incoming.readable())) {
						printf("TODO: throw for unreadable DS or ES source.\n");
						assert(false);
					}
				break;

				case Source::SS:
					if(!incoming.code_or_data() || incoming.executable() || !incoming.writeable()) {
						printf("TODO: throw for invalid SS target.\n");
						assert(false);
					}
				break;

				case Source::CS:
					if(!incoming.code_or_data() || !incoming.executable()) {
						// TODO: throw.
						printf("TODO: throw for illegal CS destination.\n");
						assert(false);
					}

					if(!incoming.code_or_data()) {
						printf("TODO: handle jump to system descriptor of type %d\n", int(incoming.type()));
						assert(false);
					}
				break;

				default: break;
			}

			// TODO: is this descriptor privilege within reach?
			// TODO: is this an empty descriptor*? If so: exception!

			// TODO: set descriptor accessed bit in memory if it's a segment.
		}
	}

	/// Posted by @c perform after any operation which *might* have affected a segment register.
	void did_update(const Source segment) {
		if(!is_segment_register(segment)) return;

#ifndef NDEBUG
		assert(last_source_.has_value() && *last_source_ == segment);
		last_source_ = std::nullopt;
#endif

		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			load_real(segment);
			return;
		} else {
			if(is_real(mode_)) {
				load_real(segment);
				return;
			}

			descriptors[segment] = last_descriptor_;
		}
	}

	void did_update(DescriptorTable) {}

	void set_mode(const Mode mode) {
		mode_ = mode;
	}

	void reset() {
		load_real(Source::ES);
		load_real(Source::CS);
		load_real(Source::DS);
		load_real(Source::SS);
		load_real(Source::FS);
		load_real(Source::GS);
	}

	InstructionSet::x86::SegmentRegisterSet<Descriptor> descriptors;

	bool operator ==(const Segments &rhs) const {
		return descriptors == rhs.descriptors;
	}

private:
	void load_real(const Source segment) {
		const auto value = registers_.segment(segment);
		descriptors[segment].set_segment(value);
	}

	Mode mode_ = Mode::Real;
	const Registers<model> &registers_;
	const LinearMemory<model> &memory_;
	Descriptor last_descriptor_;

#ifndef NDEBUG
	std::optional<Source> last_source_;
#endif
};

}
