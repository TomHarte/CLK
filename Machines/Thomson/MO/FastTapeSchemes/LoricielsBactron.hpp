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

		Z35CA	LDA     ,U                       ;35CA: A6 C4          '..'
				LSRA                             ;35CC: 44             'D'
				STA     ,-S                      ;35CD: A7 E2          '..'
				STA     ,S                       ;35CF: A7 E4          '..'		; [S], [S+1] = tape in b6
				LDA     ,U                       ;35D1: A6 C4          '..'
				LSRA                             ;35D3: 44             'D'
				ADDA    ,S                       ;35D4: AB E4          '..'
				STA     ,S                       ;35D6: A7 E4          '..'		; [S] = two-bit tape average in b6:b7
				LDA     ,U                       ;35D8: A6 C4          '..'
				LSRA                             ;35DA: 44             'D'
				ADDA    ,S+                      ;35DB: AB E0          '..'
				EORA    M3507                    ;35DD: B8 35 07       '.5.'
				RTS                              ;35E0: 39             '9'

	Outer loop is:

		Z35AD	BSR     Z35CA                    ;35AD: 8D 1B          '..'
				BPL     Z35AD                    ;35AF: 2A FC          '*.'		; loop until sample differs from 3507
				LDX     #M0041                   ;35B1: 8E 00 41       '..A'	; bit length
				BSR     Z35C4                    ;35B4: 8D 0E          '..'		; fixed delay: 7µs + [as per 35C4]
				BSR     Z35CA                    ;35B6: 8D 12          '..'		; sample again for bit
				BPL     Z35BF                    ;35B8: 2A 05          '*.'		; BPL -> 43 = COMA, which sets carry
				COM     M3507                    ;35BA: 73 35 07       's5.'
				CLRA                             ;35BD: 4F             'O'		; clear carry
				BRN     Z3603                    ;35BE: 21 43          '!C'
				ROL     M3508                    ;35C0: 79 35 08       'y5.'
				RTS                              ;35C3: 39             '9'

	Delay loop is:

		Z35C4	DEX                              ;35C4: 30 1F          '0.'		; 5 µs
				BNE     Z35C4                    ;35C6: 26 FC          '&.'		; 3 µs
				CLRA                             ;35C8: 4F             'O'		; 2 µs
				RTS                              ;35C9: 39             '9'		; 5 µs

			i.e. length is 8*X + 7 µs.

*/
struct LoricielsBactron: public Loader {
	static constexpr uint8_t sampler[] = {
		0xa6, 0xc4, 0x44, 0xa7, 0xe2, 0xa7, 0xe4, 0xa6, 0xc4, 0x44, 0xab, 0xe4, 0xa7, 0xe4, 0xa6, 0xc4,
		0x44, 0xab, 0xe0, 0xb8, 0x35, 0x07, 0x39
	};

	static constexpr uint8_t outer[] = {
		0x8d, 0x1b, 0x2a, 0xfc, 0x8e, 0x00, 0x41, 0x8d, 0x0e, 0x8d, 0x12
	};

	static std::optional<uint16_t> detect(const uint16_t address, const MemoryAccess &memory) {
		if(
			address > 2 &&
			std::equal(
				std::begin(sampler),
				std::end(sampler),
				memory.iterator(address - 2)
			)
		) {
			switch(address) {
				case 0x35cc: return 0x35ad;
				default:
					printf("Probable relocated Loriciels Bactron\n");
				break;
			}
		}
		return std::nullopt;
	}

	std::pair<TrapAction, bool> did_trap(
		const uint16_t address,
		MemoryAccess &memory,
		CPU::M6809::Registers &,
		Storage::Tape::BinaryTapePlayer &player,
		Storage::Tape::TapeSerialiser &
	) override {
		// Check that this is still the expected routine.
		if(!std::equal(
				std::begin(outer),
				std::end(outer),
				memory.iterator(address)
			)
		) {
			return std::make_pair(TrapAction::None, false);
		}

		player.run_for(Storage::Time(14 + 8*0x41, 1'000'000));

		// Sample current level.
		const bool start_level = memory[0x3507] & 0x80;

		// Proceed to next edge; time can be skipped here.
		while(player.input() == start_level) {
			player.complete_pulse();
		}

		// Wait for 14 + 8*(0x41) µs.
		player.run_for(Storage::Time(14 + 8*0x41, 1'000'000));

		// Sample current level to determine bit, shift and complement as appropriate.
		const bool final_level = player.input();
		memory[0x3508] <<= 1;
		if(final_level == start_level) {
			memory[0x3508] |= 1;
		} else {
			memory[0x3507] ^= 0xff;
		}

		return std::make_pair(TrapAction::RTS, true);
	}
};

}
