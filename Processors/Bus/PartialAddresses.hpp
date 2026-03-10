//
//  PartialAddresses.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <concepts>

namespace Bus::Address {

template <std::unsigned_integral AddressT>
struct Literal {
	constexpr Literal(const AddressT address) noexcept : address_(address) {}
	operator AddressT() const {
		return address_;
	}

private:
	AddressT address_;
};

template <std::unsigned_integral AddressT, AddressT value>
struct Fixed {
	operator AddressT() const {
		return value;
	}
};

template <
	std::unsigned_integral AddressT,
	std::unsigned_integral VariableT,
	std::unsigned_integral FixedT,
	FixedT Page
>
struct FixedPage {
	FixedPage(const VariableT address) noexcept : address_(address) {}
	operator AddressT() const {
		return (Page << 8*sizeof(VariableT)) | address_;
	}

private:
	VariableT address_;
};

}  // namespace Bus::Address
