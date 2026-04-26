//
//  LoricielsBactron.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <algorithm>

namespace Thomson::FastLoader {

/*

	Fast loader used by, at least, Bactron and MGT.

	Sampling code is:

		Z35CA   LDA     ,U                       ;35CA: A6 C4          '..'
				LSRA                             ;35CC: 44             'D'
				STA     ,-S                      ;35CD: A7 E2          '..'
				STA     ,S                       ;35CF: A7 E4          '..'
				LDA     ,U                       ;35D1: A6 C4          '..'
				LSRA                             ;35D3: 44             'D'
				ADDA    ,S                       ;35D4: AB E4          '..'
				STA     ,S                       ;35D6: A7 E4          '..'
				LDA     ,U                       ;35D8: A6 C4          '..'
				LSRA                             ;35DA: 44             'D'
				ADDA    ,S+                      ;35DB: AB E0          '..'
				EORA    M3507                    ;35DD: B8 35 07       '.5.'
				RTS                              ;35E0: 39             '9'

	Outer loop is:

		Z35AD   BSR     Z35CA                    ;35AD: 8D 1B          '..'
				BPL     Z35AD                    ;35AF: 2A FC          '*.'		; loop until edge?
				LDX     #M0041                   ;35B1: 8E 00 41       '..A'
				BSR     Z35C4                    ;35B4: 8D 0E          '..'		; fixed delay
				BSR     Z35CA                    ;35B6: 8D 12          '..'		; sample again for bit

*/
struct LoricielsBactron: public Loader {
	static constexpr uint8_t sampler[] = {
		0xa6, 0xc4, 0x44, 0xa7, 0xe2, 0xa7, 0xe4, 0xa6, 0xc4, 0x44, 0xab, 0xe4, 0xa7, 0xe4, 0xa6, 0xc4,
		0x44, 0xab, 0xe0, 0xb8, 0x35, 0x07, 0x39
	};

	static constexpr uint8_t outer[] = {
		0x8d, 0x1b, 0x2a, 0xfc, 0x8e, 0x00, 0x41, 0x8d, 0x0e, 0x8d, 0x12
	};

	template <typename MemoryMapT>
	static std::optional<uint16_t> detect(const uint16_t address, const MemoryMapT &memory_map) {
		if(
			address > 2 &&
			std::equal(
				std::begin(sampler),
				std::end(sampler),
				memory_map.iterator(address - 2)
			)
		) {
			// TODO: use 'outer' for validation.
			switch(address) {
				case 0x35cc: return 0x35ad;
				default:
					printf("Probable relocated Loriciels Bactron\n");
				break;
			}
		}
		return std::nullopt;
	}

	TrapAction did_trap(
		const uint16_t,
		MemoryAccess &,
		CPU::M6809::Registers &
	) override {
		// TODO.
		return TrapAction::None;
	}
};

}
