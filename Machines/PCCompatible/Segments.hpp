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
	Segments(const InstructionSet::x86::Registers<model> &registers, LinearMemoryT &memory) :
		registers_(registers), memory_(memory) {}

	using Descriptor = InstructionSet::x86::SegmentDescriptor;
	using DescriptorTable = InstructionSet::x86::DescriptorTable;
	using Mode = InstructionSet::x86::Mode;
	using Source = InstructionSet::x86::Source;

	template <bool for_read>
	bool verify(const uint16_t value) {
		try {
			const auto incoming = descriptor(value);
			const auto description = incoming.description();

			if(!is_data_or_code(description.type)) {
				return false;
			}

			if constexpr (for_read) {
				if(!description.readable) {
					return false;
				}
			} else {
				if(!description.writeable) {
					return false;
				}
			}

			// TODO: privilege level?

			return true;
		} catch (const InstructionSet::x86::Exception &e) {
			return false;
		}
	}

	std::optional<uint8_t> load_access_rights(const uint16_t source) {
		try {
			const auto incoming = descriptor(source);
			return incoming.access_rights();
		} catch (const InstructionSet::x86::Exception &e) {
			return std::nullopt;
		}
	}

	std::optional<uint16_t> load_limit(const uint16_t source) {
		try {
			const auto incoming = descriptor(source);
			const auto description = incoming.description();
			using DescriptorType = InstructionSet::x86::DescriptorType;
			if(
				InstructionSet::x86::is_data_or_code(description.type) ||
				description.type == DescriptorType::AvailableTaskStateSegment ||
				description.type == DescriptorType::BusyTaskStateSegment ||
				description.type == DescriptorType::LDT) {
				return incoming.offset();
			} else {
				return std::nullopt;
			}
		} catch (const InstructionSet::x86::Exception &e) {
			return std::nullopt;
		}
	}

	void preauthorise(
		const Source segment,
		const uint16_t value
	) {
#ifndef NDEBUG
		last_source_ = segment;
#endif

		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			return;
		} else {
			if(is_real(mode_)) {
				return;
			}
			const auto incoming = descriptor(value);
			incoming.validate_as(segment);

			// TODO: set descriptor accessed bit in memory.
			// (unless that happens later? But probably not.)
		}
	}

	void preauthorise_task_state(const uint16_t value) {
		// Test value of descriptor.
		const auto incoming = descriptor(value);
		const auto description = incoming.description();
		if(description.type != InstructionSet::x86::DescriptorType::AvailableTaskStateSegment) {
			incoming.throw_gpf();
		}
		set_descriptor_type_flag<Descriptor>(
			memory_,
			descriptor_table(value),
			incoming,
			InstructionSet::x86::DescriptorTypeFlag::Busy
		);
	}

	void preauthorise_call(
		const Source segment,
		const uint16_t value,
		const std::function<void()> &real_callback,
		const std::function<void(const Descriptor &)> &call_callback	// TODO: call gate and task segment callbacks.
	) {
#ifndef NDEBUG
		last_source_ = segment;
#endif

		if constexpr (model <= InstructionSet::x86::Model::i80186) {
			real_callback();
			return;
		} else {
			if(is_real(mode_)) {
				real_callback();
				return;
			}

			const auto incoming = descriptor(value);
			incoming.validate_call(call_callback);
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

	const InstructionSet::x86::DescriptorTablePointer &descriptor_table(const uint16_t value) {
		return (value & 4) ?
			registers_.template get<DescriptorTable::Local>() :
			registers_.template get<DescriptorTable::Global>();
	}

	Descriptor descriptor(const uint16_t value) {
		const auto &table = descriptor_table(value);
		const auto incoming = descriptor_at<Descriptor>(memory_, table, value & ~7, value & 4);
		last_descriptor_ = incoming;
		return last_descriptor_;
	}

	Mode mode_ = Mode::Real;
	const InstructionSet::x86::Registers<model> &registers_;
	LinearMemoryT &memory_;
	Descriptor last_descriptor_;

#ifndef NDEBUG
	std::optional<Source> last_source_;
#endif
};

}
