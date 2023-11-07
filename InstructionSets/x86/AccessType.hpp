//
//  AccessType.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef AccessType_h
#define AccessType_h

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

constexpr bool is_writeable(AccessType type) {
	return type == AccessType::ReadModifyWrite || type == AccessType::Write;
}

template <typename IntT, AccessType type> struct Accessor;

// Reads: return a value directly.
template <typename IntT> struct Accessor<IntT, AccessType::Read> { using type = IntT; };
template <typename IntT> struct Accessor<IntT, AccessType::PreauthorisedRead> { using type = IntT; };

// Writes: return a custom type that can be written but not read.
template <typename IntT>
class Writeable {
	public:
		Writeable(IntT &target) : target_(target) {}
		IntT operator=(IntT value) { return target_ = value; }
	private:
		IntT &target_;
};
template <typename IntT> struct Accessor<IntT, AccessType::Write> { using type = Writeable<IntT>; };

// Read-modify-writes: return a reference.
template <typename IntT> struct Accessor<IntT, AccessType::ReadModifyWrite> { using type = IntT &; };

// Shorthands; assumed that preauthorised reads have the same return type as reads.
template<typename IntT> using read_t = typename Accessor<IntT, AccessType::Read>::type;
template<typename IntT> using write_t = typename Accessor<IntT, AccessType::Write>::type;
template<typename IntT> using modify_t = typename Accessor<IntT, AccessType::ReadModifyWrite>::type;
template<typename IntT, AccessType type> using access_t = typename Accessor<IntT, type>::type;

}

#endif /* AccessType_h */
