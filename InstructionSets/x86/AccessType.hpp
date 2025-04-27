//
//  AccessType.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

namespace InstructionSet::x86 {

/// Explains the type of access that `perform` intends to perform; is provided as a template parameter to whatever
/// the caller supplies as `MemoryT` and `RegistersT` when obtaining a reference to whatever the processor
/// intends to reference.
///
/// `perform` guarantees to validate all accesses before modifying any state, giving the caller opportunity to generate
/// any exceptions that might be applicable.
enum class AccessType {
	/// The requested value will be read from.
	Read,
	/// The requested value will be written to.
	Write,
	/// The requested value will be read from and then written to.
	ReadModifyWrite,
	/// The requested value has already been authorised for whatever form of access is now intended, so there's no
	/// need further to inspect. This is done e.g. by operations that will push multiple values to the stack to verify that
	/// all necessary stack space is available ahead of pushing anything, though each individual push will then result in
	/// a further `Preauthorised` access.
	PreauthorisedRead,
};

constexpr const char *to_string(const AccessType type) {
	switch(type) {
		case AccessType::Read:				return "read";
		case AccessType::Write:				return "write";
		case AccessType::ReadModifyWrite:	return "read-modify-write";
		case AccessType::PreauthorisedRead:	return "preauthorised read";
	}
}

constexpr bool is_writeable(const AccessType type) {
	return type == AccessType::ReadModifyWrite || type == AccessType::Write;
}

// Allow only 8-, 16- and 32-bit unsigned int accesses.
template <typename IntT>
concept is_x86_data_type
	= std::is_same_v<IntT, uint8_t> || std::is_same_v<IntT, uint16_t> || std::is_same_v<IntT, uint32_t>;

template <typename IntT, AccessType type> struct Accessor;

// Reads: return a value directly.
template <typename IntT>
requires is_x86_data_type<IntT>
struct Accessor<IntT, AccessType::Read> { using type = const IntT; };

template <typename IntT>
requires is_x86_data_type<IntT>
struct Accessor<IntT, AccessType::PreauthorisedRead> { using type = const IntT; };

// Writes: return a custom type that can be written but not read.
template <typename IntT>
requires is_x86_data_type<IntT>
class Writeable {
public:
	constexpr Writeable(IntT &target) noexcept : target_(target) {}
	IntT operator=(const IntT value) { return target_ = value; }
private:
	IntT &target_;
};

template <typename IntT>
requires is_x86_data_type<IntT>
struct Accessor<IntT, AccessType::Write> { using type = Writeable<IntT>; };

// Read-modify-writes: return a reference.
template <typename IntT>
requires is_x86_data_type<IntT>
struct Accessor<IntT, AccessType::ReadModifyWrite> { using type = IntT &; };

// Shorthands; assumed that preauthorised reads have the same return type as reads.
template<typename IntT> using read_t = typename Accessor<IntT, AccessType::Read>::type;
template<typename IntT> using write_t = typename Accessor<IntT, AccessType::Write>::type;
template<typename IntT> using modify_t = typename Accessor<IntT, AccessType::ReadModifyWrite>::type;
template<typename IntT, AccessType type> using access_t = typename Accessor<IntT, type>::type;

}
