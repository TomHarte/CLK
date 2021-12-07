//
//  Bitplanes.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/11/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "Bitplanes.hpp"
#include "Chipset.hpp"

using namespace Amiga;

namespace {

/// Expands @c source so that b7 is the least-significant bit of the most-significant byte of the result,
/// b6 is the least-significant bit of the next most significant byte, etc. b0 stays in place.
constexpr uint64_t expand_bitplane_byte(uint8_t source) {
	uint64_t result = source;									// 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 0000 abcd efgh
	result = (result | (result << 28)) & 0x0000'000f'0000'000f;	// 0000 0000 0000 0000 0000 0000 0000 abcd 0000 0000 0000 0000 0000 0000 0000 efgh
	result = (result | (result << 14)) & 0x0003'0003'0003'0003;	// 0000 0000 0000 00ab 0000 0000 0000 00cd 0000 0000 0000 00ef 0000 0000 0000 00gh
	result = (result | (result << 7)) & 0x0101'0101'0101'0101;	// 0000 000a 0000 000b 0000 000c 0000 000d 0000 000e 0000 000f 0000 000g 0000 000h
	return result;
}

// A very small selection of test cases.
static_assert(expand_bitplane_byte(0xff) == 0x01'01'01'01'01'01'01'01);
static_assert(expand_bitplane_byte(0x55) == 0x00'01'00'01'00'01'00'01);
static_assert(expand_bitplane_byte(0xaa) == 0x01'00'01'00'01'00'01'00);
static_assert(expand_bitplane_byte(0x00) == 0x00'00'00'00'00'00'00'00);

}

// MARK: - BitplaneShifter.

void BitplaneShifter::set(const BitplaneData &previous, const BitplaneData &next, int odd_delay, int even_delay) {
	const uint16_t planes[6] = {
		uint16_t(((previous[0] << 16) | next[0]) >> even_delay),
		uint16_t(((previous[1] << 16) | next[1]) >> odd_delay),
		uint16_t(((previous[2] << 16) | next[2]) >> even_delay),
		uint16_t(((previous[3] << 16) | next[3]) >> odd_delay),
		uint16_t(((previous[4] << 16) | next[4]) >> even_delay),
		uint16_t(((previous[5] << 16) | next[5]) >> odd_delay),
	};

	// Swizzle bits into the form:
	//
	//	[b5 b3 b1 b4 b2 b0]
	//
	// ... and assume a suitably adjusted palette is in use elsewhere.
	// This makes dual playfields very easy to separate.
	data_[0] =
		(expand_bitplane_byte(uint8_t(planes[0])) << 0) |
		(expand_bitplane_byte(uint8_t(planes[2])) << 1) |
		(expand_bitplane_byte(uint8_t(planes[4])) << 2) |
		(expand_bitplane_byte(uint8_t(planes[1])) << 3) |
		(expand_bitplane_byte(uint8_t(planes[3])) << 4) |
		(expand_bitplane_byte(uint8_t(planes[5])) << 5);

	data_[1] =
		(expand_bitplane_byte(uint8_t(planes[0] >> 8)) << 0) |
		(expand_bitplane_byte(uint8_t(planes[2] >> 8)) << 1) |
		(expand_bitplane_byte(uint8_t(planes[4] >> 8)) << 2) |
		(expand_bitplane_byte(uint8_t(planes[1] >> 8)) << 3) |
		(expand_bitplane_byte(uint8_t(planes[3] >> 8)) << 4) |
		(expand_bitplane_byte(uint8_t(planes[5] >> 8)) << 5);
}

// MARK: - Bitplanes.

bool Bitplanes::advance_dma(int cycle) {
#define BIND_CYCLE(offset, plane)								\
	case offset:												\
		if(plane_count_ > plane) {								\
			next[plane] = ram_[pointer_[plane] & ram_mask_];	\
			++pointer_[plane];									\
			if constexpr (!plane) {								\
				chipset_.post_bitplanes(next);					\
			}													\
			return true;										\
		}														\
	return false;

	if(is_high_res_) {
		switch(cycle&3) {
			default: return false;
			BIND_CYCLE(0, 3);
			BIND_CYCLE(1, 1);
			BIND_CYCLE(2, 2);
			BIND_CYCLE(3, 0);
		}
	} else {
		switch(cycle&7) {
			default: return false;
			/* Omitted: 0. */
			BIND_CYCLE(1, 3);
			BIND_CYCLE(2, 5);
			BIND_CYCLE(3, 1);
			/* Omitted: 4. */
			BIND_CYCLE(5, 2);
			BIND_CYCLE(6, 4);
			BIND_CYCLE(7, 0);
		}
	}

	return false;

#undef BIND_CYCLE
}

void Bitplanes::do_end_of_line() {
	// Apply modulos here. Posssibly correct?
	pointer_[0] += modulos_[1];
	pointer_[2] += modulos_[1];
	pointer_[4] += modulos_[1];

	pointer_[1] += modulos_[0];
	pointer_[3] += modulos_[0];
	pointer_[5] += modulos_[0];
}

void Bitplanes::set_control(uint16_t control) {
	is_high_res_ = control & 0x8000;
	plane_count_ = (control >> 12) & 7;

	// TODO: who really has responsibility for clearing the other
	// bit plane fields?
	std::fill(next.begin() + plane_count_, next.end(), 0);
	if(plane_count_ == 7) {
		plane_count_ = 4;
	}
}
