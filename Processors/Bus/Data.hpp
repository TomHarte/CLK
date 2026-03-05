//
//  Data.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <concepts>

namespace Bus::Data {

/// A value that can be read from or written to, without effect.
template <std::unsigned_integral DataT>
struct NoValue {
	operator DataT() const { return DataT(~0); }
	NoValue() = default;
	constexpr NoValue(DataT) noexcept {}
};

/// A value that can be written only, not read. With a DEBUG build test that it is written before it is read.
template <std::unsigned_integral DataT>
class Writeable {
public:
	DataT operator=(const DataT value) {
		#ifndef NDEBUG
		did_write_ = true;
		#endif
		return result_ = value;
	}
	operator DataT() const {
		assert(did_write_);
		return result_;
	}

private:
	DataT result_;

	#ifndef NDEBUG
	bool did_write_ = false;
	#endif
};

enum class AccessType {
	Read,
	Write,
	NoData
};

template <std::unsigned_integral DataT, AccessType> struct Value;

template <std::unsigned_integral DataT> struct Value<DataT, AccessType::Read> {
	using type = Writeable<DataT> &;
};
template <std::unsigned_integral DataT> struct Value<DataT, AccessType::Write> {
	using type = const DataT;
};
template <std::unsigned_integral DataT> struct Value<DataT, AccessType::NoData> {
	using type = const NoValue<DataT>;
};

template <std::unsigned_integral DataT, AccessType operation>
using data_t = typename Data::Value<DataT, operation>::type;

}	// namespace Bus::Data
