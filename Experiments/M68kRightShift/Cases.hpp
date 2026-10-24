// SPDX-License-Identifier: MIT
// Original, firmware-free stimuli and a one-bit-at-a-time specification model.
#pragma once
#include <cstdint>
#include <vector>

namespace RightShiftTest {
struct Case {
	uint32_t value, count, result;
	uint16_t ccr, opcode, initial_ccr;
	unsigned width;
	bool logical, immediate;
};
inline std::vector<Case> cases(bool logical) {
	std::vector<Case> out;
	for(unsigned width : {8u, 16u, 32u}) {
		const uint32_t mask = uint32_t(0xffffffffu >> (32-width));
		const uint32_t sign = uint32_t(1) << (width-1);
		for(uint32_t low : {0u, 1u, sign-1, sign, mask, sign | 0x25u})
		for(unsigned x : {0u, 1u})
		for(bool immediate : {false, true}) {
			const std::vector<uint32_t> counts = immediate ?
				std::vector<uint32_t>{1,2,3,4,5,6,7,8} :
				std::vector<uint32_t>{0,1,width-1,width,width+1,63,64,65,128,0xddb92f2c};
			for(uint32_t count : counts) {
				uint32_t v = low & mask, carry = 0;
				const unsigned n = count & 63;
				for(unsigned i=0; i<n; ++i) {
					carry = v & 1;
					v = (v >> 1) | (!logical ? v & sign : 0);
				}
				const uint16_t ccr = uint16_t(((n ? carry : x)<<4) |
					((v & sign) ? 8 : 0) | (!v ? 4 : 0) | (n ? carry : 0));
				// Preserve upper D3 bits for byte/word operations.
				const uint32_t upper = 0x5aa55aa5u & ~mask;
				const uint16_t size = width == 8 ? 0 : width == 16 ? 0x40 : 0x80;
				const uint16_t opcode = uint16_t(0xe003 | size | (logical ? 8 : 0) |
					(immediate ? (count & 7)<<9 : 0x820));
				out.push_back({upper | low, count, upper | v, ccr, opcode,
					uint16_t(0x0f | x<<4), width, logical, immediate});
			}
		}
	}
	return out;
}
}
