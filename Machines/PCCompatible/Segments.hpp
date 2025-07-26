//
//  Segments.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/12/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "InstructionSets/x86/AccessType.hpp"
#include "InstructionSets/x86/Descriptors.hpp"
#include "InstructionSets/x86/Instruction.hpp"
#include "InstructionSets/x86/Mode.hpp"
#include "InstructionSets/x86/Model.hpp"
#include "InstructionSets/x86/Registers.hpp"

#include <cassert>
#include <functional>
#include <optional>

namespace PCCompatible {

template <InstructionSet::x86::Model model, typename LinearMemoryT>
class Segments {
public:
	Segments(const InstructionSet::x86::Registers<model> &registers, const LinearMemoryT &memory) :
		registers_(registers), memory_(memory) {}

	using Descriptor = InstructionSet::x86::SegmentDescriptor;
	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using Mode = InstructionSet::x86::Mode;
	using Source = InstructionSet::x86::Source;

	void preauthorise(
		const Source segment,
		const uint16_t value,
		const std::function<void()> &real_callback = {},
		const std::function<void(const Descriptor &)> &protected_callback = {}
	) {
#ifndef NDEBUG
		last_source_ = segment;
#endif

		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			if(real_callback) real_callback();
			return;
		} else {
			if(is_real(mode_)) {
				if(real_callback) real_callback();
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

			// TODO: set descriptor accessed bit in memory if it's a segment.

			// Get and validate descriptor.
			last_descriptor_.validate_as(segment);

			if(protected_callback) protected_callback(incoming);
		}
	}

	/// Posted by @c perform after any operation which affected a segment register.
	void did_update(const Source segment) {
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
	const InstructionSet::x86::Registers<model> &registers_;
	const LinearMemoryT &memory_;
	Descriptor last_descriptor_;

#ifndef NDEBUG
	std::optional<Source> last_source_;
#endif
};

}
