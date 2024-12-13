//
//  Pager.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/12/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

enum PagerSide {
	Read = 1,
	Write = 2,
	ReadWrite = Read | Write,
};

template <typename AddressT, typename DataT, int NumPages>
class Pager {
public:
	DataT read(AddressT address) {
		return read_[address >> Shift][address];
	}
	DataT &write(AddressT address) {
		return write_[address >> Shift][address];
	}

	template <int side, size_t start, size_t length>
	void page(uint8_t *data) {
		static_assert(!(start % PageSize), "Start address must be a multiple of the page size");
		static_assert(!(length % PageSize), "Data length must be a multiple of the page size");

		for(size_t slot = start >> Shift; slot < (start + length) >> Shift; slot++) {
			if constexpr (side & PagerSide::Write) write_[slot] = data - (slot << Shift);
			if constexpr (side & PagerSide::Read) read_[slot] = data - (slot << Shift);
			data += PageSize;
		}
	}

private:
	std::array<DataT *, NumPages> write_{};
	std::array<const DataT *, NumPages> read_{};

	static constexpr auto AddressBits = sizeof(AddressT) * 8;
	static constexpr auto PageSize = (1 << AddressBits) / NumPages;
	static_assert(!(PageSize & (PageSize - 1)), "Pages must be a power of two in size");

	static constexpr int ln2(int value) {
		int result = 0;
		while(value != 1) {
			value >>= 1;
			++result;
		}
		return result;
	}
	static constexpr auto Shift = ln2(PageSize);
};

namespace Commodore::Plus4 {

using Pager = Pager<uint16_t, uint8_t, 4>;

}
