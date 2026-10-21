//
//  RegisterSizes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include <bit>
#include <compare>
#include <cstdint>

#pragma pack(push, 1)

// Unions below assume a modern consumer architecture,
// and that this compiler offers C-style anonymous structs.

namespace CPU {

template <typename ValueT, std::endian = std::endian::native> struct Halves;
template <typename ValueT> struct Halves<ValueT, std::endian::little> {
	ValueT low, high;
};
template <typename ValueT> struct Halves<ValueT, std::endian::big> {
	ValueT high, low;
};

/// Provides access to all intermediate parts of a larger int.
template <typename Full, typename Half> union alignas(Full) alignas(Half) RegisterPair {
	constexpr RegisterPair(const Full v) noexcept : full(v) {}
	RegisterPair() = default;

	Full full;
	Halves<Half> halves;

	auto operator <=>(const RegisterPair rhs) const {
		return full <=> rhs.full;
	}
	auto operator ==(const RegisterPair rhs) const {
		return full == rhs.full;
	}
};

using RegisterPair16 = RegisterPair<uint16_t, uint8_t>;
using RegisterPair32 = RegisterPair<uint32_t, RegisterPair16>;

static_assert(sizeof(RegisterPair32) == 4);
static_assert(
	(
		std::endian::native == std::endian::little &&
		offsetof(RegisterPair32, halves.low) == 0 &&
		offsetof(RegisterPair32, halves.high) == 2
	) ||
	(
		std::endian::native == std::endian::big &&
		offsetof(RegisterPair32, halves.low) == 2 &&
		offsetof(RegisterPair32, halves.high) == 0
	)
);

static_assert(sizeof(RegisterPair16) == 2);
static_assert(
	(
		std::endian::native == std::endian::little &&
		offsetof(RegisterPair16, halves.low) == 0 &&
		offsetof(RegisterPair16, halves.high) == 1
	) ||
	(
		std::endian::native == std::endian::big &&
		offsetof(RegisterPair16, halves.low) == 1 &&
		offsetof(RegisterPair16, halves.high) == 0
	)
);

/// Provides access to lower portions of a larger int.
template <typename IntT, std::endian = std::endian::native> union SlicedInt {};

template <> union SlicedInt<uint16_t, std::endian::little> {
	uint16_t w;
	uint8_t b;

	auto operator <=>(const SlicedInt rhs) const {
		return w <=> rhs.w;
	}
};

template <> union SlicedInt<uint16_t, std::endian::big> {
	uint16_t w;
	struct {
		uint8_t __padding[1];
		uint8_t b;
	};

	auto operator <=>(const SlicedInt rhs) const {
		return w <=> rhs.w;
	}
};

template <> union SlicedInt<uint32_t, std::endian::little> {
	uint32_t l;
	uint16_t w;
	uint8_t b;

	struct {
		SlicedInt<uint16_t> low, high;
	};

	auto operator <=>(const SlicedInt &rhs) const {
		return l <=> rhs.l;
	}
};

template <> union SlicedInt<uint32_t, std::endian::big> {
	uint32_t l;
	struct {
		uint16_t __padding_w[1];
		uint16_t w;
	};
	struct {
		uint8_t __padding_b[3];
		uint8_t b;
	};
	struct {
		SlicedInt<uint16_t> high, low;
	};

	auto operator <=>(const SlicedInt rhs) const {
		return l <=> rhs.l;
	}
};

using SlicedInt16 = SlicedInt<uint16_t>;
using SlicedInt32 = SlicedInt<uint32_t>;

static_assert(sizeof(SlicedInt16) == 2);
static_assert(
	(
		std::endian::native == std::endian::little &&
		offsetof(SlicedInt16, b) == 0
	) ||
	(
		std::endian::native == std::endian::big &&
		offsetof(SlicedInt16, b) == 1
	)
);

static_assert(sizeof(SlicedInt32) == 4);
static_assert(
	(
		std::endian::native == std::endian::little &&
		offsetof(SlicedInt16, b) == 0 &&
		offsetof(SlicedInt16, w) == 0
	) ||
	(
		std::endian::native == std::endian::big &&
		offsetof(SlicedInt16, b) == 3 &&
		offsetof(SlicedInt16, w) == 2
	)
);

}

#pragma pack(pop)
