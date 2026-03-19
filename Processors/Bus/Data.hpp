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
template <std::unsigned_integral DataT, bool VerifyWrites>
class Writeable {
public:
	DataT operator=(const DataT value) {
		if constexpr (VerifyWrites) {
			#ifndef NDEBUG
			did_write_ = true;
			#endif
		}
		return result_ = value;
	}
	operator DataT() const {
		if constexpr (VerifyWrites) {
			assert(did_write_);
		}
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

template <std::unsigned_integral DataT, bool VerifyWrites, AccessType> struct Value;

template <std::unsigned_integral DataT, bool VerifyWrites> struct Value<DataT, VerifyWrites, AccessType::Read> {
	using type = Writeable<DataT, VerifyWrites> &;
};
template <std::unsigned_integral DataT, bool VerifyWrites> struct Value<DataT, VerifyWrites, AccessType::Write> {
	using type = const DataT;
};
template <std::unsigned_integral DataT, bool VerifyWrites> struct Value<DataT, VerifyWrites, AccessType::NoData> {
	using type = const NoValue<DataT>;
};

template <std::unsigned_integral DataT, bool VerifyWrites, AccessType operation>
using data_t = typename Data::Value<DataT, VerifyWrites, operation>::type;

}	// namespace Bus::Data
